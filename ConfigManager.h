#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include "Global.h"

namespace fs = std::filesystem;

/* StateMemory: Persistent application state stored in AppData */
struct StateMemory {
    char currentExePath[MAX_PATH];
    char previousExePath[MAX_PATH];
    char registeredPath[MAX_PATH];
    char defaultProfile[64];
    bool startupRegistered;
    bool startMinimized;
};

/* ConfigManager: Handles profile I/O, registry startup, and state persistence */
class ConfigManager {
public:
    ConfigManager();

    // Profile Management
    void SavePreset(const std::string& name, const PresenceSettings& settings);
    bool LoadPreset(const std::string& name, PresenceSettings& outSettings);
    bool DeletePreset(const std::string& name);
    std::vector<std::string> GetAvailablePresets();

    // Default Profile Logic
    void SetDefaultPresetName(const std::string& name);
    std::string GetDefaultPresetName();

    // Path Utilities
    fs::path GetAppDataPath() const { return appDataPath; }

    // Windows Integration (Registry & Startup)
    void SetStartup(bool enable);
    void SetStartMinimized(bool enable);
    bool IsStartupEnabled();
    bool IsDisabledInTaskManager();
    bool ShouldStartMinimized();

private:
    void EnsureDirectoryExists();
    void ExtractDefaultPreset(); // Added for the resource extraction
    void UpdateStateMemory();

    // Internal state persistence
    void LoadState();
    void SaveState();

    // Persistent cache for state data
    StateMemory cachedState;

    // File System Paths
    fs::path appDataPath;
    fs::path profilesPath;   // Added this identifier
    fs::path memoryFilePath;

    // Registry Keys for Windows Startup
    const wchar_t* appRegistryName = L"ZombieRPC";
    const wchar_t* startupRegistryPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t* approvedRegistryPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
};

/* Global Configuration Instance */
extern ConfigManager g_Config;
