#pragma once

#include <string>
#include <functional>

struct UpdateInfo_t
{
    bool hasUpdate;
    std::string currentVersion;
    std::string latestVersion;
    std::string downloadUrl;        // Release page URL
    std::string assetDownloadUrl;   // Direct .exe download URL
    std::string releaseNotes;
    std::string error;
};

class AutoUpdater
{
public:
    static AutoUpdater& GetInstance();

    void CheckForUpdatesAsync(std::function<void(const UpdateInfo_t&)> callback);
    UpdateInfo_t CheckForUpdates();

    // Download and apply update (restarts application)
    void DownloadAndUpdateAsync(std::function<void(bool success, const std::string& errorMsg)> callback);

    bool IsDownloading() const { return m_isDownloading; }
    bool IsChecking() const { return m_isChecking; }
    const UpdateInfo_t& GetLastResult() const { return m_lastResult; }
    float GetDownloadProgress() const { return m_downloadProgress; }

private:
    AutoUpdater() = default;

    bool m_isChecking = false;
    bool m_isDownloading = false;
    float m_downloadProgress = 0.0f;
    UpdateInfo_t m_lastResult;

    static const char* GITHUB_REPO;
    static const char* CURRENT_VERSION;

    // Get repository from ini settings (defaults to kralrindo/rsx)
    static std::string GetRepoPath();
    static std::string GetCurrentModulePath();
    static bool CreateUpdateScript(const std::string& tempUpdatePath, const std::string& currentExePath);
    static bool DownloadFile(const std::string& url, const std::string& destinationPath, std::atomic<float>& progress);
};

extern AutoUpdater* g_pAutoUpdater;
