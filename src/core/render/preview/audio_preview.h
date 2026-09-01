#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <atomic>

// Audio playback state
class AudioPreview
{
public:
    AudioPreview();
    ~AudioPreview();

    // Play audio data (PCM float, 32-bit)
    bool Play(const std::vector<float>& audioData, uint32_t sampleRate, uint16_t channels);
    bool Play(const float* audioData, size_t sampleCount, uint32_t sampleRate, uint16_t channels);

    // Stop playback
    void Stop();

    // Check if currently playing
    bool IsPlaying() const { return m_isPlaying; }

    // Get playback progress (0.0 to 1.0)
    float GetProgress() const;

    // Get audio duration in seconds
    float GetDuration() const { return m_duration; }

    // Check if has valid audio data
    bool HasAudio() const { return !m_audioData.empty(); }

    // Volume control (0.0 to 1.0)
    void SetVolume(float volume);
    float GetVolume() const { return m_volume; }
    void UpdateVolume(); // Apply volume change to active playback

    // Loop control
    void SetLoop(bool loop) { m_loop = loop; }
    bool IsLooping() const { return m_loop; }

    // Auto-play control
    void SetAutoPlay(bool autoPlay) { m_autoPlay = autoPlay; }
    bool IsAutoPlayEnabled() const { return m_autoPlay; }

    // Static callback for waveOut (must be public/static for CALLBACK_FUNCTION)
    static void CALLBACK WaveOutCallback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);

private:
    void Cleanup();

    std::vector<float> m_audioData;
    std::vector<char> m_pcmData;  // 16-bit PCM data for waveOut
    HWAVEOUT m_waveOut = nullptr;
    WAVEHDR* m_waveHeaders = nullptr;
    UINT m_numHeaders = 0;
    uint32_t m_sampleRate = 48000;
    uint16_t m_channels = 2;

    std::atomic<bool> m_isPlaying;
    std::atomic<bool> m_stopRequested;
    std::atomic<bool> m_callbackSafe;  // Set to false when cleaning up
    size_t m_currentSample = 0;
    size_t m_totalSamples = 0;
    DWORD m_playbackStartTime = 0;  // For progress estimation
    float m_duration = 0.0f;  // Duration in seconds
    float m_volume = 1.0f;  // Volume (0.0 to 1.0)
    bool m_loop = false;  // Loop playback
    bool m_autoPlay = false;  // Auto-play when selecting audio
};

// Global audio preview instance
extern AudioPreview* g_pAudioPreview;
