#include "DiscordManager.h"
#include "Logger.h"
#include "Global.h"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Global instance and internal tracking for the current Client ID
DiscordManager g_Discord;

namespace {
    HANDLE g_IpcPipe = INVALID_HANDLE_VALUE;
    std::mutex g_IpcMutex;
    std::string g_CurrentClientID;
    std::vector<std::chrono::steady_clock::time_point> g_ActivityPushes;

    constexpr uint32_t OPCODE_HANDSHAKE = 0;
    constexpr uint32_t OPCODE_FRAME = 1;
    constexpr uint32_t OPCODE_PING = 3;
    constexpr uint32_t OPCODE_PONG = 4;

    bool IsValidDiscordId(const char* value) {
        if (!value) return false;
        size_t length = strlen(value);
        if (length < 16 || length > 21) return false;

        for (size_t i = 0; i < length; ++i) {
            if (!std::isdigit(static_cast<unsigned char>(value[i]))) return false;
        }
        return true;
    }

    bool StartsWithHttpUrl(const char* value) {
        if (!value) return false;
        std::string url(value);
        std::transform(url.begin(), url.end(), url.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
            });
        return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
    }

    std::string JsonEscape(const char* value) {
        std::string escaped;
        if (!value) return escaped;

        for (const unsigned char ch : std::string(value)) {
            switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (ch < 0x20) {
                    char buffer[7];
                    sprintf_s(buffer, "\\u%04x", ch);
                    escaped += buffer;
                }
                else {
                    escaped += static_cast<char>(ch);
                }
                break;
            }
        }

        return escaped;
    }

    std::string JsonEscape(const std::string& value) {
        return JsonEscape(value.c_str());
    }

    void AppendStringField(std::ostringstream& json, bool& first, const char* key, const char* value) {
        if (!value || value[0] == '\0') return;
        if (!first) json << ",";
        first = false;
        json << "\"" << key << "\":\"" << JsonEscape(value) << "\"";
    }

    void AppendIntField(std::ostringstream& json, bool& first, const char* key, int64_t value) {
        if (!first) json << ",";
        first = false;
        json << "\"" << key << "\":" << value;
    }

    bool WaitForAvailableBytes(HANDLE pipe, DWORD needed, int timeoutMs) {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            DWORD available = 0;
            if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
                return false;
            }

            if (available >= needed) {
                return true;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeoutMs) {
                return false;
            }

            Sleep(20);
        }
    }

    bool ReadBytes(HANDLE pipe, void* buffer, DWORD length, int timeoutMs) {
        char* out = static_cast<char*>(buffer);
        DWORD totalRead = 0;

        while (totalRead < length) {
            DWORD remaining = length - totalRead;
            if (!WaitForAvailableBytes(pipe, remaining, timeoutMs)) {
                return false;
            }

            DWORD bytesRead = 0;
            if (!ReadFile(pipe, out + totalRead, remaining, &bytesRead, nullptr) || bytesRead == 0) {
                return false;
            }
            totalRead += bytesRead;
        }

        return true;
    }

    bool WriteBytes(HANDLE pipe, const void* buffer, DWORD length) {
        const char* input = static_cast<const char*>(buffer);
        DWORD totalWritten = 0;

        while (totalWritten < length) {
            DWORD bytesWritten = 0;
            if (!WriteFile(pipe, input + totalWritten, length - totalWritten, &bytesWritten, nullptr) || bytesWritten == 0) {
                return false;
            }
            totalWritten += bytesWritten;
        }

        return true;
    }

    bool WriteFrame(HANDLE pipe, uint32_t opcode, const std::string& payload) {
        uint32_t header[2] = { opcode, static_cast<uint32_t>(payload.size()) };
        return WriteBytes(pipe, header, sizeof(header)) &&
            WriteBytes(pipe, payload.data(), static_cast<DWORD>(payload.size()));
    }

    bool ReadFrame(HANDLE pipe, uint32_t& opcode, std::string& payload, int timeoutMs) {
        uint32_t header[2] = {};
        if (!ReadBytes(pipe, header, sizeof(header), timeoutMs)) {
            return false;
        }

        opcode = header[0];
        uint32_t length = header[1];
        if (length > 1024 * 1024) {
            return false;
        }

        payload.assign(length, '\0');
        if (length == 0) {
            return true;
        }

        return ReadBytes(pipe, payload.data(), length, timeoutMs);
    }

    void ClosePipe() {
        if (g_IpcPipe != INVALID_HANDLE_VALUE) {
            CloseHandle(g_IpcPipe);
            g_IpcPipe = INVALID_HANDLE_VALUE;
        }
    }

    HANDLE OpenDiscordPipe() {
        for (int i = 0; i < 10; ++i) {
            std::string path = "\\\\?\\pipe\\discord-ipc-" + std::to_string(i);
            HANDLE pipe = CreateFileA(
                path.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

            if (pipe != INVALID_HANDLE_VALUE) {
                DWORD mode = PIPE_READMODE_BYTE;
                SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);
                return pipe;
            }
        }

        return INVALID_HANDLE_VALUE;
    }

    std::string ExtractJsonString(const std::string& json, const char* key) {
        std::string marker = std::string("\"") + key + "\":\"";
        size_t start = json.find(marker);
        if (start == std::string::npos) return "";

        start += marker.size();
        std::string value;
        bool escaped = false;

        for (size_t i = start; i < json.size(); ++i) {
            char ch = json[i];
            if (escaped) {
                value += ch;
                escaped = false;
            }
            else if (ch == '\\') {
                escaped = true;
            }
            else if (ch == '"') {
                break;
            }
            else {
                value += ch;
            }
        }

        return value;
    }

    bool CanSendActivityCommand() {
        const auto now = std::chrono::steady_clock::now();
        g_ActivityPushes.erase(
            std::remove_if(g_ActivityPushes.begin(), g_ActivityPushes.end(), [now](const auto& timestamp) {
                return std::chrono::duration_cast<std::chrono::seconds>(now - timestamp).count() >= 20;
                }),
            g_ActivityPushes.end());

        if (g_ActivityPushes.size() >= 5) {
            return false;
        }

        g_ActivityPushes.push_back(now);
        return true;
    }

    int32_t NormalizeActivityType(int32_t type) {
        return (type == 2 || type == 3 || type == 5) ? type : 0;
    }

    bool ValidatePresence(const PresenceSettings& settings, std::vector<std::string>& errors) {
        if (!IsValidDiscordId(settings.clientId)) {
            errors.push_back("Application ID must be a 16-21 digit Discord application ID.");
        }

        if (NormalizeActivityType(settings.activityType) != settings.activityType) {
            errors.push_back("Activity Type must be Playing, Listening, Watching, or Competing.");
        }

        if (strlen(settings.partyId) > 0) {
            if (strlen(settings.partyId) < 2) {
                errors.push_back("Party ID must be at least 2 characters long.");
            }
            if (settings.partySize <= 0 || settings.partyMax <= 0) {
                errors.push_back("Party size requires Current Size and Max Size to both be greater than 0.");
            }
            else if (settings.partySize > settings.partyMax) {
                errors.push_back("Party Current Size cannot be greater than Max Size.");
            }
        }

        const bool button1Partial = (strlen(settings.button1Label) > 0) != (strlen(settings.button1Url) > 0);
        const bool button2Partial = (strlen(settings.button2Label) > 0) != (strlen(settings.button2Url) > 0);

        if (button1Partial) {
            errors.push_back("Button 1 needs both a label and a URL.");
        }
        if (button2Partial) {
            errors.push_back("Button 2 needs both a label and a URL.");
        }
        if (strlen(settings.button1Url) > 0 && !StartsWithHttpUrl(settings.button1Url)) {
            errors.push_back("Button 1 URL must start with http:// or https://.");
        }
        if (strlen(settings.button2Url) > 0 && !StartsWithHttpUrl(settings.button2Url)) {
            errors.push_back("Button 2 URL must start with http:// or https://.");
        }

        return errors.empty();
    }

    std::string BuildActivityJson(const PresenceSettings& settings) {
        std::ostringstream activity;
        bool first = true;

        AppendIntField(activity, first, "type", NormalizeActivityType(settings.activityType));
        AppendStringField(activity, first, "details", settings.details);
        AppendStringField(activity, first, "state", settings.state);

        auto now = std::chrono::system_clock::now();
        int64_t epochSeconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        int64_t totalSeconds = (settings.days * 86400LL) +
            (settings.hours * 3600LL) +
            (settings.minutes * 60LL) +
            static_cast<int64_t>(settings.seconds);

        if (settings.timerMode == 0) {
            if (!first) activity << ",";
            first = false;
            activity << "\"timestamps\":{\"start\":" << (epochSeconds - totalSeconds) << "}";
        }
        else if (totalSeconds > 0) {
            if (!first) activity << ",";
            first = false;
            activity << "\"timestamps\":{\"end\":" << (epochSeconds + totalSeconds) << "}";
        }

        if (strlen(settings.largeImageKey) > 0 || strlen(settings.smallImageKey) > 0) {
            if (!first) activity << ",";
            first = false;
            activity << "\"assets\":{";
            bool firstAsset = true;
            AppendStringField(activity, firstAsset, "large_image", settings.largeImageKey);
            if (strlen(settings.largeImageKey) > 0) {
                AppendStringField(activity, firstAsset, "large_text", settings.largeImageText);
            }
            AppendStringField(activity, firstAsset, "small_image", settings.smallImageKey);
            if (strlen(settings.smallImageKey) > 0) {
                AppendStringField(activity, firstAsset, "small_text", settings.smallImageText);
            }
            activity << "}";
        }

        if (strlen(settings.partyId) > 0) {
            if (!first) activity << ",";
            first = false;
            activity << "\"party\":{\"id\":\"" << JsonEscape(settings.partyId)
                << "\",\"size\":[" << settings.partySize << "," << settings.partyMax << "]}";
        }

        bool hasButton1 = strlen(settings.button1Label) > 0 && strlen(settings.button1Url) > 0;
        bool hasButton2 = strlen(settings.button2Label) > 0 && strlen(settings.button2Url) > 0;
        if (hasButton1 || hasButton2) {
            if (!first) activity << ",";
            first = false;
            activity << "\"buttons\":[";
            bool firstButton = true;

            if (hasButton1) {
                activity << "{\"label\":\"" << JsonEscape(settings.button1Label)
                    << "\",\"url\":\"" << JsonEscape(settings.button1Url) << "\"}";
                firstButton = false;
            }

            if (hasButton2) {
                if (!firstButton) activity << ",";
                activity << "{\"label\":\"" << JsonEscape(settings.button2Label)
                    << "\",\"url\":\"" << JsonEscape(settings.button2Url) << "\"}";
            }

            activity << "]";
        }

        return "{" + activity.str() + "}";
    }

    bool SendSetActivity(const PresenceSettings* settings, bool countRateLimit) {
        if (g_IpcPipe == INVALID_HANDLE_VALUE) {
            g_Logger.AddLog("[Zombie-RPC] Error: Discord IPC pipe is not connected.", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            return false;
        }

        if (countRateLimit && !CanSendActivityCommand()) {
            g_Logger.AddLog("[Zombie-RPC] Push blocked: Discord allows 5 activity updates per 20 seconds. Wait a moment.", ImVec4(1.0f, 0.55f, 0.20f, 1.0f));
            return false;
        }

        std::string activity = settings ? BuildActivityJson(*settings) : "null";
        std::ostringstream payload;
        payload << "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" << GetCurrentProcessId()
            << ",\"activity\":" << activity << "},\"nonce\":\"zrpc-" << GetTickCount64() << "\"}";

        if (!WriteFrame(g_IpcPipe, OPCODE_FRAME, payload.str())) {
            g_Logger.AddLog("[Zombie-RPC] Error: failed to write SET_ACTIVITY to Discord.", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            return false;
        }

        uint32_t opcode = 0;
        std::string response;
        if (ReadFrame(g_IpcPipe, opcode, response, 900)) {
            if (opcode == OPCODE_PING) {
                WriteFrame(g_IpcPipe, OPCODE_PONG, response);
            }
            else if (response.find("\"evt\":\"ERROR\"") != std::string::npos) {
                std::string code = ExtractJsonString(response, "code");
                std::string message = ExtractJsonString(response, "message");
                g_Logger.AddLog("[Zombie-RPC] Discord rejected presence: " + code + " " + message, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                return false;
            }
        }

        return true;
    }
}

/* Initializes or re-initializes the Discord connection */
void DiscordManager::Initialize(const char* clientId) {
    if (!IsValidDiscordId(clientId)) {
        g_Logger.AddLog("[Zombie-RPC] Error: enter a valid Discord Application ID before connecting.", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        return;
    }

    bool shouldPushInitialPresence = false;

    {
        std::lock_guard<std::mutex> lock(g_IpcMutex);

        // Avoid re-initializing if we are already connected to the same ID
        if (connected && ready && g_CurrentClientID == clientId) {
            return;
        }

        if (connected) {
            if (ready) {
                SendSetActivity(nullptr, false);
            }
            ClosePipe();
            connected = false;
            ready = false;
            g_CurrentClientID.clear();
        }

        SetReady(false);
        HANDLE pipe = OpenDiscordPipe();
        if (pipe == INVALID_HANDLE_VALUE) {
            g_Logger.AddLog("[Zombie-RPC] Error: Discord IPC pipe not found. Start or restart Discord desktop.", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            return;
        }

        g_IpcPipe = pipe;
        g_CurrentClientID = clientId;
        connected = true;

        std::string handshake = "{\"v\":1,\"client_id\":\"" + JsonEscape(clientId) + "\"}";
        if (!WriteFrame(g_IpcPipe, OPCODE_HANDSHAKE, handshake)) {
            g_Logger.AddLog("[Zombie-RPC] Error: failed to send Discord handshake.", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ClosePipe();
            connected = false;
            g_CurrentClientID.clear();
            return;
        }

        uint32_t opcode = 0;
        std::string response;
        if (!ReadFrame(g_IpcPipe, opcode, response, 5000) || response.find("\"evt\":\"READY\"") == std::string::npos) {
            g_Logger.AddLog("[Zombie-RPC] Error: Discord did not accept the RPC handshake.", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ClosePipe();
            connected = false;
            g_CurrentClientID.clear();
            return;
        }

        SetReady(true);
        std::string username = ExtractJsonString(response, "username");
        if (!username.empty()) {
            g_Logger.AddLog("[Zombie-RPC] Handshake Complete! Connected as: " + username, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        }
        else {
            g_Logger.AddLog("[Zombie-RPC] Handshake Complete! Connected to Discord.", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        }
        shouldPushInitialPresence = true;
    }

    if (shouldPushInitialPresence) {
        Update(g_Settings);
    }
}

/* Pushes local PresenceSettings to the Discord client */
void DiscordManager::Update(const PresenceSettings& settings) {
    std::vector<std::string> errors;
    if (!ValidatePresence(settings, errors)) {
        for (const std::string& error : errors) {
            g_Logger.AddLog("[Zombie-RPC] Push blocked: " + error, ImVec4(1.0f, 0.35f, 0.3f, 1.0f));
        }
        return;
    }

    std::string reconnectClientId;

    {
        std::lock_guard<std::mutex> lock(g_IpcMutex);

        if (!connected || !ready) {
            g_Logger.AddLog("[Zombie-RPC] Error: Connect to Discord first!", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            return;
        }

        if (g_CurrentClientID != settings.clientId) {
            reconnectClientId = settings.clientId;
        }
        else if (SendSetActivity(&settings, true)) {
            bool hasButtons = strlen(settings.button1Label) > 0 || strlen(settings.button2Label) > 0;
            g_Logger.AddLog("[Zombie-RPC] Presence accepted by local Discord RPC.", ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
            if (hasButtons) {
                g_Logger.AddLog("[Zombie-RPC] Buttons sent. Your own Discord client may hide them; verify with another account.", ImVec4(0.65f, 0.85f, 1.0f, 1.0f));
            }
        }
    }

    if (!reconnectClientId.empty()) {
        g_Logger.AddLog("[Zombie-RPC] Application ID changed. Reconnecting before pushing presence...", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        Reconnect(reconnectClientId.c_str());
    }
}

/* Reconnects after Discord has had time to tear down the previous activity */
void DiscordManager::Reconnect(const char* clientId) {
    if (!IsValidDiscordId(clientId)) {
        g_Logger.AddLog("[Zombie-RPC] Error: enter a valid Application ID before reconnecting.", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        return;
    }

    std::string requestedClientId = clientId;
    Shutdown();

    g_Logger.AddLog("[Zombie-RPC] Waiting for Discord to clear the previous activity...", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
    std::thread([requestedClientId]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (strcmp(g_Settings.clientId, requestedClientId.c_str()) == 0) {
            g_Discord.Initialize(requestedClientId.c_str());
        }
        }).detach();
}

/* Terminates the connection and resets state */
void DiscordManager::Shutdown() {
    std::lock_guard<std::mutex> lock(g_IpcMutex);

    if (connected) {
        if (ready) {
            SendSetActivity(nullptr, false);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        ClosePipe();
        connected = false;
        SetReady(false);
        g_CurrentClientID.clear();
    }
}

std::string DiscordManager::GetCurrentClientID() const {
    return g_CurrentClientID;
}
