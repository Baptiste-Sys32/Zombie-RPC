#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct PresencePreset {
    std::string name;
    std::string details;
    std::string state;
    std::string largeImageKey;
    std::string largeImageText;
    std::string smallImageKey;
    std::string smallImageText;
    std::string button1Label;
    std::string button1Url;
    std::string button2Label;
    std::string button2Url;
    int activityType = -1;
};

struct AppEntry {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> aliases;
    std::vector<std::string> skus;
    std::vector<PresencePreset> presets;
    bool verifiedLive = false;
    std::string verifiedName;
};

class AppDatabase {
public:
    bool EnsureLoaded(const fs::path& appDataPath);
    bool RefreshFromRemote(const fs::path& appDataPath, std::string& error);
    bool VerifyWithDiscord(const std::string& id, std::string& verifiedName, std::string& error);

    std::vector<const AppEntry*> Search(const std::string& query, size_t limit) const;
    const AppEntry* FindById(const std::string& id) const;

    size_t Count() const { return entries.size(); }
    bool IsLoaded() const { return loaded; }
    const std::string& SourceUpdatedAt() const { return sourceUpdatedAt; }
    const std::string& LastLoadedFrom() const { return lastLoadedFrom; }

private:
    bool LoadFromFile(const fs::path& appDataPath, std::string& error);
    bool ParseEntries(const std::string& json, std::string& error);
    void SaveCacheMetadata(const fs::path& appDataPath) const;
    void LoadCacheMetadata(const fs::path& appDataPath);

    std::vector<AppEntry> entries;
    bool loaded = false;
    bool loadAttempted = false;
    std::string sourceUpdatedAt;
    std::string lastLoadedFrom;
};

extern AppDatabase g_AppDatabase;
