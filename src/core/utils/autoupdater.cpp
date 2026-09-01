#include <pch.h>
#include <core/utils/autoupdater.h>

// Current version (matches FILEVERSION in rsx.rc)
const char* AutoUpdater::CURRENT_VERSION = "1.2.1";

// Default repository
const char* AutoUpdater::GITHUB_REPO = "kralrindo/rsx";

AutoUpdater* g_pAutoUpdater = &AutoUpdater::GetInstance();

AutoUpdater& AutoUpdater::GetInstance()
{
    static AutoUpdater instance;
    return instance;
}

#include <windows.h>
#include <wininet.h>
#include <format>
#include <filesystem>
#include <atomic>

#pragma comment(lib, "wininet.lib")

static std::string HttpGet(const std::string& url)
{
    HINTERNET hInternet = InternetOpenA("RSX-AutoUpdater", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet)
        return "";

    HINTERNET hRequest = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_AUTH | INTERNET_FLAG_NO_COOKIES, NULL);
    if (!hRequest)
    {
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string result;
    char buffer[4096];
    DWORD bytesRead = 0;

    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        result += buffer;
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hInternet);

    return result;
}

std::string AutoUpdater::GetCurrentModulePath()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return std::string(path);
}

bool AutoUpdater::DownloadFile(const std::string& url, const std::string& destinationPath, std::atomic<float>& progress)
{
    HINTERNET hInternet = InternetOpenA("RSX-AutoUpdater", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet)
        return false;

    HINTERNET hRequest = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_AUTH | INTERNET_FLAG_NO_COOKIES, NULL);
    if (!hRequest)
    {
        InternetCloseHandle(hInternet);
        return false;
    }

    // Get content length for progress
    DWORD contentSize = 0;
    DWORD bufferSize = sizeof(contentSize);
    HttpQueryInfoA(hRequest, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentSize, &bufferSize, NULL);

    // Open file for writing
    HANDLE hFile = CreateFileA(destinationPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hInternet);
        return false;
    }

    std::string result;
    char buffer[8192];
    DWORD bytesRead = 0;
    DWORD totalBytesRead = 0;

    while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
    {
        DWORD bytesWritten;
        if (!WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL) || bytesWritten != bytesRead)
        {
            CloseHandle(hFile);
            InternetCloseHandle(hRequest);
            InternetCloseHandle(hInternet);
            DeleteFileA(destinationPath.c_str());
            return false;
        }

        totalBytesRead += bytesRead;
        if (contentSize > 0)
            progress = static_cast<float>(totalBytesRead) / static_cast<float>(contentSize);
    }

    CloseHandle(hFile);
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hInternet);

    return true;
}

bool AutoUpdater::CreateUpdateScript(const std::string& tempUpdatePath, const std::string& currentExePath)
{
    // Get the directory of the current executable
    size_t lastSlash = currentExePath.find_last_of("\\/");
    std::string exeDir = currentExePath.substr(0, lastSlash);
    std::string exeName = currentExePath.substr(lastSlash + 1);

    // Extract just the exe name without extension for taskkill
    std::string exeBaseName = exeName;
    size_t dotPos = exeBaseName.find_last_of('.');
    if (dotPos != std::string::npos)
        exeBaseName = exeBaseName.substr(0, dotPos);

    // Create batch script path
    std::string scriptPath = exeDir + "\\update_rsx.bat";

    HANDLE hFile = CreateFileA(scriptPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    const char* scriptContent =
        "@echo off\r\n"
        "taskkill /F /IM %s.exe >nul 2>&1\r\n"  // Close all RSX instances
        "timeout /t 1 /nobreak >nul\r\n"         // Wait for processes to close
        "copy /Y \"%s\" \"%s\" >nul\r\n"         // Copy new exe over old one
        "del \"%s\" >nul\r\n"                    // Delete temp file
        "start \"\" \"%s\"\r\n"                  // Start new version
        "del \"%~f0\"\r\n";                      // Delete this script

    char scriptBuffer[1024];
    snprintf(scriptBuffer, sizeof(scriptBuffer), scriptContent,
        exeBaseName.c_str(),
        tempUpdatePath.c_str(),
        currentExePath.c_str(),
        tempUpdatePath.c_str(),
        currentExePath.c_str());

    DWORD bytesWritten;
    bool success = WriteFile(hFile, scriptBuffer, static_cast<DWORD>(strlen(scriptBuffer)), &bytesWritten, NULL) != FALSE;
    CloseHandle(hFile);

    return success;
}

// Helper function to compare semantic versions
// Returns true if latestVersion is newer than currentVersion
static bool IsNewerVersion(const std::string& latestVersion, const std::string& currentVersion)
{
    auto parseVersion = [](const std::string& version) -> std::tuple<int, int, int> {
        int major = 0, minor = 0, patch = 0;
        if (sscanf_s(version.c_str(), "%d.%d.%d", &major, &minor, &patch) != 3)
        {
            if (sscanf_s(version.c_str(), "%d.%d", &major, &minor) != 2)
            {
                sscanf_s(version.c_str(), "%d", &major);
            }
        }
        return std::make_tuple(major, minor, patch);
    };

    auto [currentMajor, currentMinor, currentPatch] = parseVersion(currentVersion);
    auto [latestMajor, latestMinor, latestPatch] = parseVersion(latestVersion);

    if (latestMajor > currentMajor) return true;
    if (latestMajor < currentMajor) return false;
    if (latestMinor > currentMinor) return true;
    if (latestMinor < currentMinor) return false;
    return latestPatch > currentPatch;
}

UpdateInfo_t AutoUpdater::CheckForUpdates()
{
    UpdateInfo_t info = { false, CURRENT_VERSION, "", "", "", "" };

    const std::string apiUrl = std::format("https://api.github.com/repos/{}/releases/latest", GITHUB_REPO);
    std::string response = HttpGet(apiUrl);

    if (response.empty())
    {
        info.error = "Failed to connect to GitHub. Check your internet connection.";
        return info;
    }

    // Simple JSON parsing (avoiding dependency on external JSON lib)
    // Look for "tag_name": "vX.Y.Z" and "html_url": "..." and "body": "..."
    // Also look for "browser_download_url" in assets for the .exe file

    size_t tagPos = response.find("\"tag_name\":");
    if (tagPos == std::string::npos)
    {
        info.error = "Failed to parse release information.";
        return info;
    }

    size_t tagStart = response.find("\"", tagPos + 11) + 1;
    size_t tagEnd = response.find("\"", tagStart);
    if (tagStart == std::string::npos || tagEnd == std::string::npos)
    {
        info.error = "Failed to parse version tag.";
        return info;
    }

    info.latestVersion = response.substr(tagStart, tagEnd - tagStart);

    // Remove 'v' prefix if present
    if (!info.latestVersion.empty() && info.latestVersion[0] == 'v')
        info.latestVersion = info.latestVersion.substr(1);

    // Get download URL (release page)
    size_t urlPos = response.find("\"html_url\":");
    if (urlPos != std::string::npos)
    {
        size_t urlStart = response.find("\"", urlPos + 11) + 1;
        size_t urlEnd = response.find("\"", urlStart);
        if (urlStart != std::string::npos && urlEnd != std::string::npos)
            info.downloadUrl = response.substr(urlStart, urlEnd - urlStart);
    }

    // Get asset download URL (find the .exe file)
    size_t assetsPos = response.find("\"assets\":");
    if (assetsPos != std::string::npos)
    {
        // Find the end of the assets array (closing bracket)
        size_t arrayStart = response.find("[", assetsPos);
        if (arrayStart != std::string::npos)
        {
            int depth = 0;
            size_t arrayEnd = arrayStart;
            while (arrayEnd < response.length())
            {
                if (response[arrayEnd] == '[') depth++;
                else if (response[arrayEnd] == ']')
                {
                    depth--;
                    if (depth == 0)
                    {
                        arrayEnd++;
                        break;
                    }
                }
                arrayEnd++;
            }

            // Look for browser_download_url within the assets array
            size_t searchPos = arrayStart;
            while (searchPos < arrayEnd)
            {
                size_t downloadUrlPos = response.find("\"browser_download_url\":", searchPos);
                if (downloadUrlPos == std::string::npos || downloadUrlPos >= arrayEnd)
                    break;

                size_t urlStart = response.find("\"", downloadUrlPos + 22) + 1;
                size_t urlEnd = response.find("\"", urlStart);
                if (urlStart != std::string::npos && urlEnd != std::string::npos)
                {
                    std::string assetUrl = response.substr(urlStart, urlEnd - urlStart);
                    // Check if this is the .exe file
                    if (assetUrl.find(".exe") != std::string::npos)
                    {
                        info.assetDownloadUrl = assetUrl;
                        break;
                    }
                }
                searchPos = urlEnd + 1;
            }
        }
    }

    // Get release notes (body)
    size_t bodyPos = response.find("\"body\":");
    if (bodyPos != std::string::npos)
    {
        size_t bodyStart = response.find("\"", bodyPos + 8) + 1;
        size_t bodyEnd = bodyStart;
        int escapeDepth = 0;

        // Parse JSON string with escape sequences
        while (bodyEnd < response.length())
        {
            if (response[bodyEnd] == '\\' && escapeDepth == 0)
            {
                escapeDepth = 1;
            }
            else if (response[bodyEnd] == '"')
            {
                if (escapeDepth == 0)
                    break;
                else
                    escapeDepth = 0;
            }
            bodyEnd++;
        }

        if (bodyEnd < response.length())
            info.releaseNotes = response.substr(bodyStart, bodyEnd - bodyStart);
    }

    // Compare versions (semantic versioning)
    // Current: 1.2.1, Latest: could be 1.2.2, 1.2.3, etc.
    // Only report update if latest version is actually NEWER (not just different)

    info.hasUpdate = IsNewerVersion(info.latestVersion, CURRENT_VERSION);

    return info;
}

void AutoUpdater::CheckForUpdatesAsync(std::function<void(const UpdateInfo_t&)> callback)
{
    if (m_isChecking)
        return;

    m_isChecking = true;

    std::thread([this, callback]()
    {
        UpdateInfo_t result = CheckForUpdates();
        m_lastResult = result;
        m_isChecking = false;

        if (callback)
            callback(result);
    }).detach();
}

void AutoUpdater::DownloadAndUpdateAsync(std::function<void(bool, const std::string&)> callback)
{
    if (m_isDownloading || m_lastResult.assetDownloadUrl.empty())
    {
        if (callback)
            callback(false, m_lastResult.assetDownloadUrl.empty() ? "No download URL available" : "Already downloading");
        return;
    }

    m_isDownloading = true;
    m_downloadProgress = 0.0f;

    std::thread([this, callback]()
    {
        std::string currentExePath = GetCurrentModulePath();
        std::string tempUpdatePath = currentExePath + ".new";

        // Download the update
        std::atomic<float> progress(0.0f);
        bool downloadSuccess = DownloadFile(m_lastResult.assetDownloadUrl, tempUpdatePath, progress);

        if (!downloadSuccess)
        {
            m_isDownloading = false;
            if (callback)
                callback(false, "Failed to download update");
            return;
        }

        // Create update script
        bool scriptSuccess = CreateUpdateScript(tempUpdatePath, currentExePath);

        if (!scriptSuccess)
        {
            m_isDownloading = false;
            DeleteFileA(tempUpdatePath.c_str());
            if (callback)
                callback(false, "Failed to create update script");
            return;
        }

        m_isDownloading = false;
        if (callback)
            callback(true, "");

        // Launch the update script and exit
        std::string scriptPath = currentExePath.substr(0, currentExePath.find_last_of("\\/")) + "\\update_rsx.bat";
        ShellExecuteA(NULL, "open", scriptPath.c_str(), NULL, NULL, SW_HIDE);

        // Exit the application
        exit(0);
    }).detach();
}
