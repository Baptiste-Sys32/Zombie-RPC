#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "dependencies/imgui/imgui.h"

/* LogMessage: A single entry in the application log */
struct LogMessage {
    std::string text;  // The log message content
    ImVec4 color;      // RGBA color for ImGui rendering
};

/* Logger: Thread-safe logging system for the ZombieRPC interface */
class Logger {
public:
    // Adds a new message to the log buffer
    void AddLog(const std::string& text, ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    // Retrieves the current list of log messages
    const std::vector<LogMessage>& GetLogs() const;

    // Clears all entries from the log buffer
    void Clear();

private:
    std::vector<LogMessage> logs;    // Internal storage of log entries
    mutable std::mutex logMutex;     // Ensures thread-safety for Discord callbacks
};

/* Global Logger Instance */
extern Logger g_Logger;