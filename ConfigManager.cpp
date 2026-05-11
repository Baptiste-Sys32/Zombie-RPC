#include "ConfigManager.h"
#include "Logger.h"
#include "resource.h"
#include <fstream>
#include <shlobj.h>
#include <windows.h>

ConfigManager g_Config;

/* Helper to safely convert string to int with a fallback to prevent crashes on empty/bad data */
static int SafeStoi(const std::string& val, int fallback = 0) {
    try {
        if (val.empty()) return fallback;
        return std::stoi(val);
    }
    catch (...) {
        return fallback;
    }
}

/* Helper to trim trailing whitespace/carriage returns from strings read from files */
static void TrimString(std::string& s) {
    if (s.empty()) return;
    size_t last = s.find_last_not_of(" \n\r\t");
    if (last == std::string::npos) {
        s.clear();
    }
    else {
        s.erase(last + 1);
    }
}

ConfigManager::ConfigManager() {
    memset(&cachedState, 0, sizeof(StateMemory));

    /* Retrieve the Roaming AppData path for the current user */
    wchar_t* path_tmp;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path_tmp) == S_OK) {
        appDataPath = fs::path(path_tmp) / "Zombie-RPC";
        profilesPath = appDataPath / "Profiles";
        memoryFilePath = appDataPath / "Memory.zrpc";
        CoTaskMemFree(path_tmp);
    }

    EnsureDirectoryExists();
    bool firstRun = !fs::exists(memoryFilePath);
    if (firstRun) {
        ExtractDefaultPreset();
    }
    LoadState();
    UpdateStateMemory();
}

/* Ensures necessary folders exist on the filesystem */
void ConfigManager::EnsureDirectoryExists() {
    if (!fs::exists(appDataPath)) fs::create_directories(appDataPath);
    if (!fs::exists(profilesPath)) fs::create_directories(profilesPath);
}

static void ExtractPresetResource(int resourceId, const fs::path& targetPath, const std::string& displayName) {
    if (fs::exists(targetPath)) return;

    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!hRes) return;

    HGLOBAL hData = LoadResource(NULL, hRes);
    void* pData = LockResource(hData);
    DWORD size = SizeofResource(NULL, hRes);

    std::ofstream ofs(targetPath, std::ios::trunc);
    if (ofs.is_open()) {
        ofs.write(reinterpret_cast<const char*>(pData), size);
        g_Logger.AddLog("[System] Factory default profile '" + displayName + "' created.", ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
    }
}

/* Extracts embedded default profiles during first-run setup only */
void ConfigManager::ExtractDefaultPreset() {
    ExtractPresetResource(IDR_DEFAULT_PRESET, profilesPath / "Zombie-RPC.zrpc", "Zombie-RPC");
    ExtractPresetResource(IDR_ALWAYS_WATCHING_PRESET, profilesPath / "Always Watching.zrpc", "Always Watching");
}

/* Parses the persistent state file into cachedState */
void ConfigManager::LoadState() {
    std::ifstream ifs(memoryFilePath);
    if (!ifs.is_open()) {
        memset(&cachedState, 0, sizeof(StateMemory));
        strncpy_s(cachedState.defaultProfile, "Always Watching", 64);
        return;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        size_t sep = line.find(": ");
        if (sep == std::string::npos) continue;

        std::string key = line.substr(0, sep);
        std::string val = line.substr(sep + 2);
        TrimString(val);

        if (key == "CurrentPath") strncpy_s(cachedState.currentExePath, val.c_str(), MAX_PATH);
        else if (key == "PreviousPath") strncpy_s(cachedState.previousExePath, val.c_str(), MAX_PATH);
        else if (key == "RegisteredPath") strncpy_s(cachedState.registeredPath, val.c_str(), MAX_PATH);
        else if (key == "DefaultProfile") strncpy_s(cachedState.defaultProfile, val.c_str(), 64);
        else if (key == "StartupEnabled") cachedState.startupRegistered = (val == "1");
        else if (key == "StartMinimized") cachedState.startMinimized = (val == "1");
    }
}

/* Writes the current cachedState to the persistence file */
void ConfigManager::SaveState() {
    std::ofstream ofs(memoryFilePath, std::ios::trunc);
    if (ofs.is_open()) {
        ofs << "CurrentPath: " << cachedState.currentExePath << "\n";
        ofs << "PreviousPath: " << cachedState.previousExePath << "\n";
        ofs << "RegisteredPath: " << cachedState.registeredPath << "\n";
        ofs << "DefaultProfile: " << cachedState.defaultProfile << "\n";
        ofs << "StartupEnabled: " << (cachedState.startupRegistered ? "1" : "0") << "\n";
        ofs << "StartMinimized: " << (cachedState.startMinimized ? "1" : "0") << "\n";
    }
}

/* Synchronizes the stored EXE path with the current process location */
void ConfigManager::UpdateStateMemory() {
    wchar_t actualPathW[MAX_PATH];
    GetModuleFileNameW(NULL, actualPathW, MAX_PATH);
    std::string actualPath = fs::path(actualPathW).string();

    if (actualPath != cachedState.currentExePath) {
        strncpy_s(cachedState.previousExePath, cachedState.currentExePath, MAX_PATH);
        strncpy_s(cachedState.currentExePath, actualPath.c_str(), MAX_PATH);

        if (cachedState.startupRegistered) {
            SetStartup(true);
            g_Logger.AddLog("[System] EXE move detected; registry paths synchronized.", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        }
        SaveState();
    }
}

/* Formatted text save of PresenceSettings to a .zrpc profile file */
void ConfigManager::SavePreset(const std::string& name, const PresenceSettings& settings) {
    if (_stricmp(name.c_str(), "default") == 0) {
        g_Logger.AddLog("[System] Blocked attempt to overwrite reserved 'default' profile.", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        return;
    }

    fs::path filePath = profilesPath / (name + ".zrpc");
    std::ofstream ofs(filePath, std::ios::trunc);

    if (ofs.is_open()) {
        ofs << "ClientID: " << settings.clientId << "\n";
        ofs << "Details: " << settings.details << "\n";
        ofs << "State: " << settings.state << "\n";
        ofs << "ActivityType: " << settings.activityType << "\n";
        ofs << "TimerMode: " << settings.timerMode << "\n";
        ofs << "Days: " << settings.days << "\n";
        ofs << "Hours: " << settings.hours << "\n";
        ofs << "Minutes: " << settings.minutes << "\n";
        ofs << "Seconds: " << settings.seconds << "\n";
        ofs << "LargeImageKey: " << settings.largeImageKey << "\n";
        ofs << "LargeImageText: " << settings.largeImageText << "\n";
        ofs << "SmallImageKey: " << settings.smallImageKey << "\n";
        ofs << "SmallImageText: " << settings.smallImageText << "\n";
        ofs << "PartyID: " << settings.partyId << "\n";
        ofs << "PartySize: " << settings.partySize << "\n";
        ofs << "PartyMax: " << settings.partyMax << "\n";
        ofs << "Button1Label: " << settings.button1Label << "\n";
        ofs << "Button1URL: " << settings.button1Url << "\n";
        ofs << "Button2Label: " << settings.button2Label << "\n";
        ofs << "Button2URL: " << settings.button2Url << "\n";

        g_Logger.AddLog("[Zombie-RPC] Saved preset: " + name, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    }
}

/* Parses the text-based .zrpc profile into a PresenceSettings struct with safety checks */
bool ConfigManager::LoadPreset(const std::string& name, PresenceSettings& outSettings) {
    fs::path filePath = profilesPath / (name + ".zrpc");
    if (!fs::exists(filePath)) return false;

    std::ifstream ifs(filePath);
    if (!ifs.is_open()) return false;

    outSettings = PresenceSettings();

    std::string line;
    while (std::getline(ifs, line)) {
        size_t sep = line.find(": ");
        if (sep == std::string::npos) continue;

        std::string key = line.substr(0, sep);
        std::string val = (sep + 2 < line.length()) ? line.substr(sep + 2) : "";
        TrimString(val);

        if (key == "ClientID") strncpy_s(outSettings.clientId, val.c_str(), 64);
        else if (key == "Details") strncpy_s(outSettings.details, val.c_str(), 128);
        else if (key == "State") strncpy_s(outSettings.state, val.c_str(), 128);
        else if (key == "ActivityType") {
            int type = SafeStoi(val);
            outSettings.activityType = (type == 2 || type == 3 || type == 5) ? type : 0;
        }
        else if (key == "LargeImageKey") strncpy_s(outSettings.largeImageKey, val.c_str(), 64);
        else if (key == "LargeImageText") strncpy_s(outSettings.largeImageText, val.c_str(), 128);
        else if (key == "SmallImageKey") strncpy_s(outSettings.smallImageKey, val.c_str(), 64);
        else if (key == "SmallImageText") strncpy_s(outSettings.smallImageText, val.c_str(), 128);
        else if (key == "PartyID") strncpy_s(outSettings.partyId, val.c_str(), 64);
        else if (key == "Button1Label") strncpy_s(outSettings.button1Label, val.c_str(), 32);
        else if (key == "Button1URL") strncpy_s(outSettings.button1Url, val.c_str(), 256);
        else if (key == "Button2Label") strncpy_s(outSettings.button2Label, val.c_str(), 32);
        else if (key == "Button2URL") strncpy_s(outSettings.button2Url, val.c_str(), 256);
        else if (key == "TimerMode") outSettings.timerMode = SafeStoi(val);
        else if (key == "Days") outSettings.days = SafeStoi(val);
        else if (key == "Hours") outSettings.hours = SafeStoi(val);
        else if (key == "Minutes") outSettings.minutes = SafeStoi(val);
        else if (key == "Seconds") outSettings.seconds = SafeStoi(val);
        else if (key == "PartySize") outSettings.partySize = SafeStoi(val);
        else if (key == "PartyMax") outSettings.partyMax = SafeStoi(val);
    }
    return true;
}

/* Deletes a saved .zrpc profile from disk */
bool ConfigManager::DeletePreset(const std::string& name) {
    if (_stricmp(name.c_str(), "default") == 0) {
        g_Logger.AddLog("[System] Blocked attempt to delete reserved 'default' profile.", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        return false;
    }

    fs::path filePath = profilesPath / (name + ".zrpc");
    std::error_code ec;
    bool removed = fs::remove(filePath, ec);

    if (!removed || ec) {
        g_Logger.AddLog("[Zombie-RPC] Failed to delete preset: " + name, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        return false;
    }

    if (_stricmp(cachedState.defaultProfile, name.c_str()) == 0) {
        memset(cachedState.defaultProfile, 0, sizeof(cachedState.defaultProfile));
        SaveState();
    }

    g_Logger.AddLog("[Zombie-RPC] Deleted preset: " + name, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
    return true;
}

/* Returns a list of all saved profiles in the Profiles directory */
std::vector<std::string> ConfigManager::GetAvailablePresets() {
    std::vector<std::string> presets;
    if (fs::exists(profilesPath)) {
        for (const auto& entry : fs::directory_iterator(profilesPath)) {
            if (entry.path().extension() == ".zrpc") {
                presets.push_back(entry.path().stem().string());
            }
        }
    }
    return presets;
}

void ConfigManager::SetDefaultPresetName(const std::string& name) {
    strncpy_s(cachedState.defaultProfile, name.c_str(), 64);
    SaveState();
    g_Logger.AddLog("[Zombie-RPC] Default startup preset set to: " + name, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
}

std::string ConfigManager::GetDefaultPresetName() {
    return std::string(cachedState.defaultProfile);
}

/* Manages Windows Registry keys for Run on Startup and Task Manager approval */
void ConfigManager::SetStartup(bool enable) {
    HKEY hKey;
    HKEY hApproveKey;
    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, startupRegistryPath, 0, KEY_SET_VALUE, &hKey);

    if (status != ERROR_SUCCESS) {
        g_Logger.AddLog("[Zombie-RPC] Error: Registry access denied.", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        return;
    }

    bool wasAlreadyRegistered = cachedState.startupRegistered;
    std::string fullRunPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    std::string fullApprovePath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";

    if (enable) {
        bool wasOverrideActive = IsDisabledInTaskManager();
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);

        std::wstring cmdLine = L"\"" + std::wstring(exePath) + L"\"";
        if (cachedState.startMinimized) {
            cmdLine += L" -minimized";
        }

        status = RegSetValueExW(hKey, appRegistryName, 0, REG_SZ, (BYTE*)cmdLine.c_str(), (DWORD)((cmdLine.length() + 1) * sizeof(wchar_t)));

        if (status == ERROR_SUCCESS) {
            cachedState.startupRegistered = true;
            std::string fullRegEntry = fullRunPath + "\\ZombieRPC";
            strncpy_s(cachedState.registeredPath, fullRegEntry.c_str(), MAX_PATH);

            if (wasAlreadyRegistered) {
                std::string msg = "[Zombie-RPC] Updated startup arguments (Minimized: " + std::string(cachedState.startMinimized ? "Yes" : "No") + ")";
                g_Logger.AddLog(msg, cachedState.startMinimized ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            }
            else {
                if (wasOverrideActive) {
                    if (RegOpenKeyExW(HKEY_CURRENT_USER, approvedRegistryPath, 0, KEY_SET_VALUE, &hApproveKey) == ERROR_SUCCESS) {
                        BYTE enabledBlob[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
                        RegSetValueExW(hApproveKey, appRegistryName, 0, REG_BINARY, enabledBlob, sizeof(enabledBlob));
                        RegCloseKey(hApproveKey);
                    }
                    g_Logger.AddLog("[Zombie-RPC] Enabled Startup (Modified: " + fullApprovePath + ")", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                }
                else {
                    g_Logger.AddLog("[Zombie-RPC] Enabled Startup (Created: " + fullRunPath + ")", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                }
            }
        }
    }
    else {
        RegDeleteValueW(hKey, appRegistryName);
        if (RegOpenKeyExW(HKEY_CURRENT_USER, approvedRegistryPath, 0, KEY_SET_VALUE, &hApproveKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hApproveKey, appRegistryName);
            RegCloseKey(hApproveKey);
        }
        cachedState.startupRegistered = false;
        memset(cachedState.registeredPath, 0, MAX_PATH);
        g_Logger.AddLog("[Zombie-RPC] Disabled Startup (Removed: " + fullRunPath + ")", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    }

    SaveState();
    RegCloseKey(hKey);
}

void ConfigManager::SetStartMinimized(bool enable) {
    cachedState.startMinimized = enable;
    SaveState();
}

bool ConfigManager::ShouldStartMinimized() {
    return cachedState.startMinimized;
}

bool ConfigManager::IsStartupEnabled() {
    return cachedState.startupRegistered;
}

/* Checks if the user manually disabled the app via the Task Manager Startup tab */
bool ConfigManager::IsDisabledInTaskManager() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, approvedRegistryPath, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) return false;

    BYTE data[12];
    DWORD dataSize = sizeof(data);
    LSTATUS status = RegQueryValueExW(hKey, appRegistryName, NULL, NULL, data, &dataSize);
    RegCloseKey(hKey);

    if (status == ERROR_SUCCESS) return (data[0] & 1) == 1;
    return false;
}
