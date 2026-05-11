#pragma once

#include "Global.h"
#include <string>

/* DiscordManager: Handles the lifecycle and communication with the Discord-RPC API */
class DiscordManager {
public:
    // Initializes the connection to Discord using the provided Client ID
    void Initialize(const char* clientId);

    // Updates the current Rich Presence data based on user settings
    void Update(const PresenceSettings& settings);

    // Restarts the Discord connection after a short delay so app ID changes stick
    void Reconnect(const char* clientId);

    // Properly shuts down the connection and cleans up resources
    void Shutdown();

    // State Accessors
    bool IsConnected() const { return connected; }
    bool IsReady() const { return ready; }
    std::string GetCurrentClientID() const;
    void SetReady(bool state) { ready = state; }

private:
    bool connected = false; // True if the library is initialized
    bool ready = false;     // True if the Discord client has acknowledged the connection
};

/* Global Discord Manager Instance */
extern DiscordManager g_Discord;
