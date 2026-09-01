#include <pch.h>
#include "fileassoc.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>

static bool HasRegisteredMarker()
{
    HKEY hKey = nullptr;
    const char* subKey = "Software\\rsx";
    if (RegOpenKeyExA(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

static void SetRegisteredMarker()
{
    HKEY hKey = nullptr;
    const char* subKey = "Software\\rsx";
    if (RegCreateKeyExA(HKEY_CURRENT_USER, subKey, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        const char* val = "1";
        RegSetValueExA(hKey, "RegisteredFileTypes", 0, REG_SZ, (const BYTE*)val, static_cast<DWORD>(strlen(val) + 1));
        RegCloseKey(hKey);
    }
}

static void RegisterSingleAssociation(const std::string& extension, const std::string& progId, const std::string& description, const std::string& exePath)
{
    std::string extKey = std::string("Software\\Classes\\") + extension;
    HKEY hExt = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, extKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hExt, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExA(hExt, nullptr, 0, REG_SZ, (const BYTE*)progId.c_str(), static_cast<DWORD>(progId.size() + 1));
        RegCloseKey(hExt);
    }

    std::string progKey = std::string("Software\\Classes\\") + progId;
    HKEY hProg = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, progKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hProg, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExA(hProg, nullptr, 0, REG_SZ, (const BYTE*)description.c_str(), static_cast<DWORD>(description.size() + 1));

        HKEY hShell = nullptr;
        if (RegCreateKeyExA(hProg, "shell\\open\\command", 0, nullptr, 0, KEY_WRITE, nullptr, &hShell, nullptr) == ERROR_SUCCESS)
        {
            std::string command = std::string("\"") + exePath + "\" \"%1\"";
            RegSetValueExA(hShell, nullptr, 0, REG_SZ, (const BYTE*)command.c_str(), static_cast<DWORD>(command.size() + 1));
            RegCloseKey(hShell);
        }

        RegCloseKey(hProg);
    }
}

void RegisterFileAssociationsIfNeeded()
{
    if (HasRegisteredMarker())
        return;

    char modulePath[MAX_PATH] = {0};
    if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) == 0)
        return;

    const std::string exePath = modulePath;

    // Register .rpak
    RegisterSingleAssociation(".rpak", "rsx.rpak", "reSource RPAK File", exePath);
    // Register .mbnk (audio containers)
    RegisterSingleAssociation(".mbnk", "rsx.mbnk", "reSource MBNK File", exePath);

    // Notify shell of the change so icons/associations update without reboot
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    SetRegisteredMarker();
}
