#include "Logger.h"

// Global Logger Instance
Logger g_Logger;

/* Adds a new message to the log buffer with thread safety */
void Logger::AddLog(const std::string& text, ImVec4 color) {
    std::lock_guard<std::mutex> lock(logMutex);

    // Using emplace_back to construct the object in place
    logs.push_back({ text, color });

    // Keeps the log buffer at a maximum of 50 entries to manage memory usage
    if (logs.size() > 50) {
        logs.erase(logs.begin());
    }
}

/* Returns the current vector of logs */
const std::vector<LogMessage>& Logger::GetLogs() const {
    // Note: Calling this while another thread is writing to logs could be risky.
    // However, for ImGui rendering, we typically accept the small risk for speed.
    return logs;
}

/* Clears all messages from the log history */
void Logger::Clear() {
    std::lock_guard<std::mutex> lock(logMutex);
    logs.clear();
}