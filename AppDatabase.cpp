#include "AppDatabase.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

AppDatabase g_AppDatabase;

namespace {
    constexpr const char* kGamesRawUrl = "https://gist.githubusercontent.com/Cynosphere/c1e77f77f0e565ddaac2822977961e76/raw/gameslist.json";
    constexpr const wchar_t* kGamesRawUrlW = L"https://gist.githubusercontent.com/Cynosphere/c1e77f77f0e565ddaac2822977961e76/raw/gameslist.json";
    constexpr const wchar_t* kGistApiUrlW = L"https://api.github.com/gists/c1e77f77f0e565ddaac2822977961e76";
    constexpr const wchar_t* kDiscordRpcUrlPrefixW = L"https://discord.com/api/v10/applications/";
    constexpr const wchar_t* kDiscordRpcUrlSuffixW = L"/rpc";

    fs::path CachePath(const fs::path& appDataPath) {
        return appDataPath / "AppDatabase.json";
    }

    fs::path MetadataPath(const fs::path& appDataPath) {
        return appDataPath / "AppDatabase.meta";
    }

    std::wstring WidenAscii(const std::string& value) {
        return std::wstring(value.begin(), value.end());
    }

    std::string TrimCopy(std::string value) {
        auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char c) { return !isSpace(c); }));
        value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char c) { return !isSpace(c); }).base(), value.end());
        return value;
    }

    std::string LowerCopy(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
            });
        return value;
    }

    bool HttpGet(const wchar_t* url, std::string& body, std::string& error) {
        body.clear();
        error.clear();

        URL_COMPONENTSW parts = {};
        parts.dwStructSize = sizeof(parts);
        wchar_t host[256] = {};
        wchar_t path[4096] = {};
        parts.lpszHostName = host;
        parts.dwHostNameLength = ARRAYSIZE(host);
        parts.lpszUrlPath = path;
        parts.dwUrlPathLength = ARRAYSIZE(path);
        parts.dwSchemeLength = static_cast<DWORD>(-1);
        parts.dwExtraInfoLength = static_cast<DWORD>(-1);

        if (!WinHttpCrackUrl(url, 0, 0, &parts)) {
            error = "Could not parse URL.";
            return false;
        }

        std::wstring requestPath(parts.lpszUrlPath, parts.dwUrlPathLength);
        if (parts.lpszExtraInfo && parts.dwExtraInfoLength > 0) {
            requestPath.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        }

        HINTERNET session = WinHttpOpen(L"ZombieRPC/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) {
            error = "Could not start WinHTTP session.";
            return false;
        }

        DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

        HINTERNET connect = WinHttpConnect(session, std::wstring(parts.lpszHostName, parts.dwHostNameLength).c_str(),
            parts.nPort, 0);
        if (!connect) {
            WinHttpCloseHandle(session);
            error = "Could not connect to remote host.";
            return false;
        }

        DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET request = WinHttpOpenRequest(connect, L"GET", requestPath.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!request) {
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            error = "Could not open HTTP request.";
            return false;
        }

        const wchar_t* headers = L"Accept: application/json\r\nUser-Agent: ZombieRPC/1.0\r\n";
        bool ok = WinHttpSendRequest(request, headers, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(request, nullptr);

        if (ok) {
            DWORD status = 0;
            DWORD statusSize = sizeof(status);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
            if (status < 200 || status >= 300) {
                error = "Remote server returned HTTP " + std::to_string(status) + ".";
                ok = false;
            }
        }

        while (ok) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available)) {
                error = "Could not read HTTP response.";
                ok = false;
                break;
            }
            if (available == 0) break;

            std::string chunk(available, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, chunk.data(), available, &read)) {
                error = "Could not read HTTP response bytes.";
                ok = false;
                break;
            }
            chunk.resize(read);
            body += chunk;
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return ok;
    }

    void SkipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    }

    bool Consume(const std::string& json, size_t& pos, char expected) {
        SkipWhitespace(json, pos);
        if (pos >= json.size() || json[pos] != expected) return false;
        ++pos;
        return true;
    }

    bool ParseJsonString(const std::string& json, size_t& pos, std::string& out) {
        SkipWhitespace(json, pos);
        if (pos >= json.size() || json[pos] != '"') return false;
        ++pos;
        out.clear();

        while (pos < json.size()) {
            char ch = json[pos++];
            if (ch == '"') return true;
            if (ch != '\\') {
                out += ch;
                continue;
            }

            if (pos >= json.size()) return false;
            char esc = json[pos++];
            switch (esc) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u':
                if (pos + 4 <= json.size()) pos += 4;
                out += '?';
                break;
            default:
                out += esc;
                break;
            }
        }
        return false;
    }

    bool SkipJsonValue(const std::string& json, size_t& pos);

    bool SkipJsonArray(const std::string& json, size_t& pos) {
        if (!Consume(json, pos, '[')) return false;
        SkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ']') {
            ++pos;
            return true;
        }

        while (pos < json.size()) {
            if (!SkipJsonValue(json, pos)) return false;
            SkipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') {
                ++pos;
                continue;
            }
            if (pos < json.size() && json[pos] == ']') {
                ++pos;
                return true;
            }
            return false;
        }
        return false;
    }

    bool SkipJsonObject(const std::string& json, size_t& pos) {
        if (!Consume(json, pos, '{')) return false;
        SkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '}') {
            ++pos;
            return true;
        }

        while (pos < json.size()) {
            std::string key;
            if (!ParseJsonString(json, pos, key)) return false;
            if (!Consume(json, pos, ':')) return false;
            if (!SkipJsonValue(json, pos)) return false;
            SkipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') {
                ++pos;
                continue;
            }
            if (pos < json.size() && json[pos] == '}') {
                ++pos;
                return true;
            }
            return false;
        }
        return false;
    }

    bool SkipJsonValue(const std::string& json, size_t& pos) {
        SkipWhitespace(json, pos);
        if (pos >= json.size()) return false;
        if (json[pos] == '"') {
            std::string ignored;
            return ParseJsonString(json, pos, ignored);
        }
        if (json[pos] == '{') return SkipJsonObject(json, pos);
        if (json[pos] == '[') return SkipJsonArray(json, pos);

        while (pos < json.size() && json[pos] != ',' && json[pos] != ']' && json[pos] != '}') {
            ++pos;
        }
        return true;
    }

    bool ParseStringArray(const std::string& json, size_t& pos, std::vector<std::string>& out) {
        if (!Consume(json, pos, '[')) return false;
        SkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ']') {
            ++pos;
            return true;
        }

        while (pos < json.size()) {
            std::string value;
            if (json[pos] == '"') {
                if (!ParseJsonString(json, pos, value)) return false;
                if (!TrimCopy(value).empty()) out.push_back(TrimCopy(value));
            }
            else {
                if (!SkipJsonValue(json, pos)) return false;
            }

            SkipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') {
                ++pos;
                SkipWhitespace(json, pos);
                continue;
            }
            if (pos < json.size() && json[pos] == ']') {
                ++pos;
                return true;
            }
            return false;
        }
        return false;
    }

    bool ParseSkuArray(const std::string& json, size_t& pos, std::vector<std::string>& out) {
        if (!Consume(json, pos, '[')) return false;
        SkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ']') {
            ++pos;
            return true;
        }

        while (pos < json.size()) {
            if (json[pos] == '{') {
                std::string distributor;
                std::string id;
                if (!Consume(json, pos, '{')) return false;
                while (pos < json.size()) {
                    SkipWhitespace(json, pos);
                    if (pos < json.size() && json[pos] == '}') {
                        ++pos;
                        break;
                    }

                    std::string key;
                    std::string value;
                    if (!ParseJsonString(json, pos, key)) return false;
                    if (!Consume(json, pos, ':')) return false;
                    SkipWhitespace(json, pos);
                    if (key == "distributor" && json[pos] == '"') ParseJsonString(json, pos, distributor);
                    else if (key == "id" && json[pos] == '"') ParseJsonString(json, pos, id);
                    else SkipJsonValue(json, pos);

                    SkipWhitespace(json, pos);
                    if (pos < json.size() && json[pos] == ',') ++pos;
                }

                if (!distributor.empty() && !id.empty()) out.push_back(distributor + ":" + id);
            }
            else {
                if (!SkipJsonValue(json, pos)) return false;
            }

            SkipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') {
                ++pos;
                SkipWhitespace(json, pos);
                continue;
            }
            if (pos < json.size() && json[pos] == ']') {
                ++pos;
                return true;
            }
            return false;
        }
        return false;
    }

    void SetPresetField(PresencePreset& preset, const std::string& key, const std::string& value) {
        if (key == "name" || key == "label") preset.name = value;
        else if (key == "details") preset.details = value;
        else if (key == "state") preset.state = value;
        else if (key == "largeImageKey" || key == "large_image") preset.largeImageKey = value;
        else if (key == "largeImageText" || key == "large_text") preset.largeImageText = value;
        else if (key == "smallImageKey" || key == "small_image") preset.smallImageKey = value;
        else if (key == "smallImageText" || key == "small_text") preset.smallImageText = value;
        else if (key == "button1Label") preset.button1Label = value;
        else if (key == "button1Url" || key == "button1URL") preset.button1Url = value;
        else if (key == "button2Label") preset.button2Label = value;
        else if (key == "button2Url" || key == "button2URL") preset.button2Url = value;
        else if (key == "activityType") {
            try {
                preset.activityType = std::stoi(value);
            }
            catch (...) {
                preset.activityType = -1;
            }
        }
    }

    bool ParsePresetArray(const std::string& json, size_t& pos, std::vector<PresencePreset>& out) {
        if (!Consume(json, pos, '[')) return false;
        SkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ']') {
            ++pos;
            return true;
        }

        while (pos < json.size()) {
            if (json[pos] == '"') {
                std::string state;
                if (!ParseJsonString(json, pos, state)) return false;
                state = TrimCopy(state);
                if (!state.empty()) {
                    PresencePreset preset;
                    preset.name = state;
                    preset.state = state;
                    out.push_back(preset);
                }
            }
            else if (json[pos] == '{') {
                PresencePreset preset;
                if (!Consume(json, pos, '{')) return false;
                while (pos < json.size()) {
                    SkipWhitespace(json, pos);
                    if (pos < json.size() && json[pos] == '}') {
                        ++pos;
                        break;
                    }

                    std::string key;
                    if (!ParseJsonString(json, pos, key)) return false;
                    if (!Consume(json, pos, ':')) return false;

                    std::string value;
                    SkipWhitespace(json, pos);
                    if (pos < json.size() && json[pos] == '"') {
                        if (!ParseJsonString(json, pos, value)) return false;
                        SetPresetField(preset, key, value);
                    }
                    else {
                        size_t start = pos;
                        if (!SkipJsonValue(json, pos)) return false;
                        SetPresetField(preset, key, TrimCopy(json.substr(start, pos - start)));
                    }

                    SkipWhitespace(json, pos);
                    if (pos < json.size() && json[pos] == ',') ++pos;
                }

                if (preset.name.empty()) {
                    preset.name = !preset.state.empty() ? preset.state : preset.details;
                }
                if (!preset.name.empty()) out.push_back(preset);
            }
            else {
                if (!SkipJsonValue(json, pos)) return false;
            }

            SkipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') {
                ++pos;
                SkipWhitespace(json, pos);
                continue;
            }
            if (pos < json.size() && json[pos] == ']') {
                ++pos;
                return true;
            }
            return false;
        }
        return false;
    }

    bool ParseAppObject(const std::string& json, size_t& pos, AppEntry& entry) {
        if (!Consume(json, pos, '{')) return false;

        while (pos < json.size()) {
            SkipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == '}') {
                ++pos;
                return !entry.id.empty() && !entry.name.empty();
            }

            std::string key;
            if (!ParseJsonString(json, pos, key)) return false;
            if (!Consume(json, pos, ':')) return false;
            SkipWhitespace(json, pos);

            if (key == "id" && pos < json.size() && json[pos] == '"') ParseJsonString(json, pos, entry.id);
            else if (key == "name" && pos < json.size() && json[pos] == '"') ParseJsonString(json, pos, entry.name);
            else if (key == "description" && pos < json.size() && json[pos] == '"') ParseJsonString(json, pos, entry.description);
            else if (key == "aliases" && pos < json.size() && json[pos] == '[') ParseStringArray(json, pos, entry.aliases);
            else if (key == "thirdPartySkus" && pos < json.size() && json[pos] == '[') ParseSkuArray(json, pos, entry.skus);
            else if ((key == "presets" || key == "states" || key == "statePresets" || key == "richPresencePresets") &&
                pos < json.size() && json[pos] == '[') {
                ParsePresetArray(json, pos, entry.presets);
            }
            else SkipJsonValue(json, pos);

            SkipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') {
                ++pos;
                continue;
            }
            if (pos < json.size() && json[pos] == '}') {
                ++pos;
                return !entry.id.empty() && !entry.name.empty();
            }
            return false;
        }

        return false;
    }

    bool ExtractJsonStringByKey(const std::string& json, const std::string& key, std::string& out) {
        std::string needle = "\"" + key + "\":";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos += needle.size();
        return ParseJsonString(json, pos, out);
    }
}

bool AppDatabase::EnsureLoaded(const fs::path& appDataPath) {
    if (loaded) return true;
    if (loadAttempted) return false;
    loadAttempted = true;
    std::string error;
    return LoadFromFile(appDataPath, error);
}

bool AppDatabase::LoadFromFile(const fs::path& appDataPath, std::string& error) {
    std::ifstream ifs(CachePath(appDataPath), std::ios::binary);
    if (!ifs.is_open()) {
        error = "App database cache has not been downloaded yet.";
        return false;
    }

    std::ostringstream buffer;
    buffer << ifs.rdbuf();
    if (!ParseEntries(buffer.str(), error)) return false;

    loaded = true;
    lastLoadedFrom = CachePath(appDataPath).string();
    LoadCacheMetadata(appDataPath);
    return true;
}

bool AppDatabase::RefreshFromRemote(const fs::path& appDataPath, std::string& error) {
    std::string metaJson;
    std::string updatedAt;
    if (HttpGet(kGistApiUrlW, metaJson, error)) {
        ExtractJsonStringByKey(metaJson, "updated_at", updatedAt);
    }

    std::string body;
    if (!HttpGet(kGamesRawUrlW, body, error)) return false;
    if (!ParseEntries(body, error)) return false;

    fs::create_directories(appDataPath);
    std::ofstream ofs(CachePath(appDataPath), std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        error = "Could not write app database cache.";
        return false;
    }
    ofs.write(body.data(), static_cast<std::streamsize>(body.size()));

    sourceUpdatedAt = updatedAt;
    lastLoadedFrom = kGamesRawUrl;
    loaded = true;
    loadAttempted = true;
    SaveCacheMetadata(appDataPath);
    return true;
}

bool AppDatabase::VerifyWithDiscord(const std::string& id, std::string& verifiedName, std::string& error) {
    std::wstring url = std::wstring(kDiscordRpcUrlPrefixW) + WidenAscii(id) + kDiscordRpcUrlSuffixW;
    std::string body;
    if (!HttpGet(url.c_str(), body, error)) return false;

    std::string returnedId;
    std::string returnedName;
    if (!ExtractJsonStringByKey(body, "id", returnedId) || returnedId != id ||
        !ExtractJsonStringByKey(body, "name", returnedName) || returnedName.empty()) {
        error = "Discord RPC endpoint did not return a matching application.";
        return false;
    }

    verifiedName = returnedName;
    return true;
}

bool AppDatabase::ParseEntries(const std::string& json, std::string& error) {
    std::vector<AppEntry> parsed;
    size_t pos = 0;
    if (!Consume(json, pos, '[')) {
        error = "App database JSON is not an array.";
        return false;
    }

    while (pos < json.size()) {
        SkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ']') {
            ++pos;
            entries = std::move(parsed);
            error.clear();
            return !entries.empty();
        }

        AppEntry entry;
        if (!ParseAppObject(json, pos, entry)) {
            error = "Could not parse an app entry from the database.";
            return false;
        }
        parsed.push_back(std::move(entry));

        SkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ',') ++pos;
    }

    error = "App database JSON ended before the array closed.";
    return false;
}

std::vector<const AppEntry*> AppDatabase::Search(const std::string& query, size_t limit) const {
    std::vector<const AppEntry*> results;
    std::string normalized = LowerCopy(TrimCopy(query));
    if (normalized.empty()) return results;

    auto matches = [&](const AppEntry& entry) {
        if (LowerCopy(entry.name).find(normalized) != std::string::npos) return true;
        if (entry.id.find(normalized) != std::string::npos) return true;
        for (const std::string& alias : entry.aliases) {
            if (LowerCopy(alias).find(normalized) != std::string::npos) return true;
        }
        return false;
        };

    for (const AppEntry& entry : entries) {
        if (matches(entry)) {
            results.push_back(&entry);
            if (results.size() >= limit) break;
        }
    }
    return results;
}

const AppEntry* AppDatabase::FindById(const std::string& id) const {
    auto it = std::find_if(entries.begin(), entries.end(), [&](const AppEntry& entry) {
        return entry.id == id;
        });
    return it == entries.end() ? nullptr : &(*it);
}

void AppDatabase::SaveCacheMetadata(const fs::path& appDataPath) const {
    std::ofstream ofs(MetadataPath(appDataPath), std::ios::trunc);
    if (!ofs.is_open()) return;
    ofs << "Source: " << kGamesRawUrl << "\n";
    ofs << "UpdatedAt: " << sourceUpdatedAt << "\n";
}

void AppDatabase::LoadCacheMetadata(const fs::path& appDataPath) {
    std::ifstream ifs(MetadataPath(appDataPath));
    if (!ifs.is_open()) return;

    std::string line;
    while (std::getline(ifs, line)) {
        size_t sep = line.find(": ");
        if (sep == std::string::npos) continue;
        std::string key = line.substr(0, sep);
        std::string value = TrimCopy(line.substr(sep + 2));
        if (key == "UpdatedAt") sourceUpdatedAt = value;
    }
}
