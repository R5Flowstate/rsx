#include <pch.h>
#include <core/render/preview/audio_preview.h>

#pragma comment(lib, "winmm.lib")

// Global callback wrapper that uses a critical section
static CRITICAL_SECTION g_audioCallbackLock;
static bool g_audioCallbackInitialized = false;

AudioPreview::AudioPreview()
    : m_waveOut(nullptr)
    , m_waveHeaders(nullptr)
    , m_numHeaders(0)
    , m_sampleRate(48000)
    , m_channels(2)
    , m_isPlaying(false)
    , m_stopRequested(false)
    , m_currentSample(0)
    , m_totalSamples(0)
    , m_callbackSafe(true)
    , m_playbackStartTime(0)
    , m_duration(0.0f)
{
    if (!g_audioCallbackInitialized)
    {
        InitializeCriticalSection(&g_audioCallbackLock);
        g_audioCallbackInitialized = true;
    }
}

AudioPreview::~AudioPreview()
{
    Stop();
    Cleanup();
}

void AudioPreview::Cleanup()
{
    // Mark callback as unsafe to prevent accessing freed resources
    m_callbackSafe = false;

    // Store handle locally before clearing member
    HWAVEOUT waveOut = m_waveOut;
    WAVEHDR* headers = m_waveHeaders;
    UINT numHeaders = m_numHeaders;

    // Clear member variables first
    m_waveOut = nullptr;
    m_waveHeaders = nullptr;
    m_numHeaders = 0;

    // Small delay to let any pending callbacks finish
    Sleep(10);

    if (waveOut && headers)
    {
        for (UINT i = 0; i < numHeaders; i++)
        {
            if (headers[i].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(waveOut, &headers[i], sizeof(WAVEHDR));
        }
        delete[] headers;

        waveOutClose(waveOut);
    }

    m_audioData.clear();
    m_pcmData.clear();
}

bool AudioPreview::Play(const std::vector<float>& audioData, uint32_t sampleRate, uint16_t channels)
{
    return Play(audioData.data(), audioData.size(), sampleRate, channels);
}

bool AudioPreview::Play(const float* audioData, size_t sampleCount, uint32_t sampleRate, uint16_t channels)
{
    // Stop any existing playback
    Stop();

    if (!audioData || sampleCount == 0)
    {
        Log("AUDIO: Play() called with null data or zero samples\n");
        return false;
    }

    Log("AUDIO: Play() - %zu samples, %d Hz, %d channels\n", sampleCount, sampleRate, channels);

    // Mark callback as safe - we're about to start new playback
    m_callbackSafe = true;

    // Store audio data
    m_audioData.assign(audioData, audioData + sampleCount);
    m_sampleRate = sampleRate;
    m_channels = channels;
    m_totalSamples = sampleCount / channels;  // Store frame count, not sample count
    m_currentSample = 0;
    m_stopRequested = false;

    // Calculate duration for progress estimation
    m_duration = static_cast<float>(m_totalSamples) / static_cast<float>(sampleRate);
    m_playbackStartTime = GetTickCount();  // Will be set when playback actually starts

    // Convert float32 to int16 PCM
    // sampleCount is already total samples (interleaved channels)
    m_pcmData.resize(sampleCount * sizeof(short));

    Log("AUDIO: Converted to %zu bytes of PCM data\n", m_pcmData.size());

    short* pcmData = reinterpret_cast<short*>(m_pcmData.data());
    for (size_t i = 0; i < sampleCount; i++)
    {
        // Apply volume, clamp and convert to 16-bit
        float sample = audioData[i] * m_volume;
        sample = std::max(-1.0f, std::min(1.0f, sample));
        pcmData[i] = static_cast<short>(sample * 32767.0f);
    }

    // Set up WAVEFORMATEX
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = channels;
    wfx.nSamplesPerSec = sampleRate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    // Open wave out device
    MMRESULT result = waveOutOpen(&m_waveOut, WAVE_MAPPER, &wfx,
        reinterpret_cast<DWORD_PTR>(AudioPreview::WaveOutCallback),
        reinterpret_cast<DWORD_PTR>(this),
        CALLBACK_FUNCTION);

    if (result != MMSYSERR_NOERROR)
    {
        Log("AUDIO: Failed to open wave out device: error %d\n", result);
        Cleanup();
        return false;
    }

    Log("AUDIO: Opened wave out device successfully\n");

    // Use a single buffer for simplicity
    m_numHeaders = 1;
    m_waveHeaders = new WAVEHDR[m_numHeaders];
    memset(m_waveHeaders, 0, sizeof(WAVEHDR) * m_numHeaders);

    m_waveHeaders[0].lpData = m_pcmData.data();
    m_waveHeaders[0].dwBufferLength = static_cast<DWORD>(m_pcmData.size());

    result = waveOutPrepareHeader(m_waveOut, &m_waveHeaders[0], sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR)
    {
        Log("AUDIO: Failed to prepare header: error %d\n", result);
        Cleanup();
        return false;
    }

    result = waveOutWrite(m_waveOut, &m_waveHeaders[0], sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR)
    {
        Log("AUDIO: Failed to write audio buffer: error %d\n", result);
        Cleanup();
        return false;
    }

    // Set the start time now that playback has begun
    m_playbackStartTime = GetTickCount();

    Log("AUDIO: Started playback of %u bytes (duration: %.2fs)\n", m_waveHeaders[0].dwBufferLength, m_duration);

    m_isPlaying = true;
    return true;
}

void AudioPreview::Stop()
{
    m_stopRequested = true;

    if (m_waveOut)
    {
        waveOutReset(m_waveOut);
    }

    // Wait a bit for callbacks to complete
    Sleep(50);

    Cleanup();

    m_isPlaying = false;
    m_stopRequested = false;
}

void AudioPreview::SetVolume(float volume)
{
    m_volume = std::clamp(volume, 0.0f, 1.0f);
    UpdateVolume();
}

void AudioPreview::UpdateVolume()
{
    if (m_waveOut && m_isPlaying)
    {
        // Convert 0.0-1.0 to 0x0000-0xFFFF for both channels
        DWORD volumeValue = static_cast<DWORD>(m_volume * 0xFFFF);
        waveOutSetVolume(m_waveOut, volumeValue | (volumeValue << 16));
    }
}

float AudioPreview::GetProgress() const
{
    if (!m_isPlaying || m_duration <= 0.0f)
        return 0.0f;

    // Estimate progress based on elapsed time
    DWORD elapsed = GetTickCount() - m_playbackStartTime;
    float progress = static_cast<float>(elapsed) / 1000.0f / m_duration;

    // Clamp between 0 and 1
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    return progress;
}

void CALLBACK AudioPreview::WaveOutCallback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    (void)hwo;
    (void)dwParam1;
    (void)dwParam2;
    AudioPreview* preview = reinterpret_cast<AudioPreview*>(dwInstance);

    // Early exit if preview is null or callback is marked unsafe
    if (!preview || !preview->m_callbackSafe.load())
        return;

    // Handle WOM_DONE for natural playback completion
    if (uMsg == WOM_DONE && !preview->m_stopRequested.load())
    {
        if (preview->m_loop)
        {
            // Loop: restart playback
            if (preview->m_waveOut && preview->m_waveHeaders)
            {
                waveOutWrite(preview->m_waveOut, &preview->m_waveHeaders[0], sizeof(WAVEHDR));
                preview->m_playbackStartTime = GetTickCount();
                Log("AUDIO: Looping playback\n");
            }
        }
        else
        {
            preview->m_isPlaying = false;
            Log("AUDIO: Playback complete\n");
        }
    }
}

// Global instance initialization
AudioPreview* g_pAudioPreview = new AudioPreview();
