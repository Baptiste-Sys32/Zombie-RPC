#include "Interface.h"
#include "Global.h"
#include "DiscordManager.h"
#include "ConfigManager.h"
#include "AppDatabase.h"
#include "Logger.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <shellapi.h>
#include <string>
#include <vector>

namespace {
    const ImVec4 kTextMuted = ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
    const ImVec4 kAccent = ImVec4(0.00f, 0.43f, 1.00f, 1.0f);
    const ImVec4 kGood = ImVec4(0.00f, 0.92f, 0.42f, 1.0f);
    const ImVec4 kWarn = ImVec4(1.00f, 0.68f, 0.10f, 1.0f);
    const ImVec4 kBad = ImVec4(1.00f, 0.18f, 0.22f, 1.0f);

    void ApplyModernStyle() {
        static bool applied = false;
        if (applied) return;
        applied = true;

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.ChildRounding = 8.0f;
        style.FrameRounding = 7.0f;
        style.PopupRounding = 8.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 8.0f;
        style.TabRounding = 7.0f;
        style.TabBarOverlineSize = 0.0f;
        style.WindowPadding = ImVec2(18.0f, 14.0f);
        style.FramePadding = ImVec2(10.0f, 5.0f);
        style.ItemSpacing = ImVec2(10.0f, 7.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 5.0f);
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 1.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.98f, 0.98f, 0.98f, 1.0f);
        colors[ImGuiCol_TextDisabled] = kTextMuted;
        colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.020f, 0.020f, 0.020f, 1.0f);
        colors[ImGuiCol_Border] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.045f, 0.045f, 0.045f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.115f, 0.115f, 0.115f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.155f, 0.155f, 0.155f, 1.0f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        colors[ImGuiCol_Button] = ImVec4(0.055f, 0.055f, 0.055f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.140f, 0.140f, 0.140f, 1.0f);
        colors[ImGuiCol_ButtonActive] = kAccent;
        colors[ImGuiCol_Header] = ImVec4(0.050f, 0.050f, 0.050f, 1.0f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.120f, 0.120f, 0.120f, 1.0f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.006f, 0.431f, 1.0f, 1.0f);
        colors[ImGuiCol_Tab] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.115f, 0.115f, 0.115f, 1.0f);
        colors[ImGuiCol_TabSelected] = ImVec4(0.060f, 0.060f, 0.060f, 1.0f);
        colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.66f, 0.66f, 0.66f, 1.0f);
        colors[ImGuiCol_TabDimmed] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.060f, 0.060f, 0.060f, 1.0f);
        colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.66f, 0.66f, 0.66f, 1.0f);
        colors[ImGuiCol_Separator] = ImVec4(0.66f, 0.66f, 0.66f, 1.0f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.72f, 0.72f, 0.72f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabActive] = kAccent;
        colors[ImGuiCol_CheckMark] = kAccent;
        colors[ImGuiCol_SliderGrab] = kAccent;
    }

    void TextMuted(const char* text) {
        ImGui::TextColored(kTextMuted, "%s", text);
    }

    bool LooksLikeDiscordId(const char* value) {
        if (!value) return false;
        size_t length = strlen(value);
        if (length < 16 || length > 21) return false;

        for (size_t i = 0; i < length; ++i) {
            if (!std::isdigit(static_cast<unsigned char>(value[i]))) return false;
        }
        return true;
    }

    bool LooksLikeHttpUrl(const char* value) {
        if (!value) return false;
        std::string url(value);
        std::transform(url.begin(), url.end(), url.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
            });
        return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
    }

    void CopySetting(char* destination, size_t destinationSize, const std::string& value) {
        if (value.empty()) return;
        strncpy_s(destination, destinationSize, value.c_str(), _TRUNCATE);
    }

    void ApplyAppEntry(const AppEntry& entry) {
        CopySetting(g_Settings.clientId, IM_ARRAYSIZE(g_Settings.clientId), entry.id);
        g_Logger.AddLog("[App DB] Selected " + entry.name + " (" + entry.id + ").", ImVec4(0.45f, 0.85f, 1.0f, 1.0f));
    }

    void ApplyPresencePreset(const PresencePreset& preset) {
        CopySetting(g_Settings.details, IM_ARRAYSIZE(g_Settings.details), preset.details);
        CopySetting(g_Settings.state, IM_ARRAYSIZE(g_Settings.state), preset.state);
        CopySetting(g_Settings.largeImageKey, IM_ARRAYSIZE(g_Settings.largeImageKey), preset.largeImageKey);
        CopySetting(g_Settings.largeImageText, IM_ARRAYSIZE(g_Settings.largeImageText), preset.largeImageText);
        CopySetting(g_Settings.smallImageKey, IM_ARRAYSIZE(g_Settings.smallImageKey), preset.smallImageKey);
        CopySetting(g_Settings.smallImageText, IM_ARRAYSIZE(g_Settings.smallImageText), preset.smallImageText);
        CopySetting(g_Settings.button1Label, IM_ARRAYSIZE(g_Settings.button1Label), preset.button1Label);
        CopySetting(g_Settings.button1Url, IM_ARRAYSIZE(g_Settings.button1Url), preset.button1Url);
        CopySetting(g_Settings.button2Label, IM_ARRAYSIZE(g_Settings.button2Label), preset.button2Label);
        CopySetting(g_Settings.button2Url, IM_ARRAYSIZE(g_Settings.button2Url), preset.button2Url);
        if (preset.activityType == 0 || preset.activityType == 2 || preset.activityType == 3 || preset.activityType == 5) {
            g_Settings.activityType = preset.activityType;
        }
        g_Logger.AddLog("[App DB] Applied presence preset: " + preset.name, ImVec4(0.45f, 0.85f, 1.0f, 1.0f));
    }

    bool IsPayloadValid(std::vector<std::string>* errors = nullptr) {
        std::vector<std::string> localErrors;
        std::vector<std::string>& out = errors ? *errors : localErrors;
        out.clear();

        if (!LooksLikeDiscordId(g_Settings.clientId)) {
            out.push_back("Application ID must be 16-21 digits.");
        }

        if (!(g_Settings.activityType == 0 || g_Settings.activityType == 2 || g_Settings.activityType == 3 || g_Settings.activityType == 5)) {
            out.push_back("Activity Type is not supported by native RPC.");
        }

        if (strlen(g_Settings.partyId) > 0) {
            if (strlen(g_Settings.partyId) < 2) {
                out.push_back("Party ID must be at least 2 characters.");
            }
            if (g_Settings.partySize <= 0 || g_Settings.partyMax <= 0) {
                out.push_back("Party needs positive current/max sizes.");
            }
            else if (g_Settings.partySize > g_Settings.partyMax) {
                out.push_back("Party current size cannot exceed max size.");
            }
        }

        bool button1Partial = (strlen(g_Settings.button1Label) > 0) != (strlen(g_Settings.button1Url) > 0);
        bool button2Partial = (strlen(g_Settings.button2Label) > 0) != (strlen(g_Settings.button2Url) > 0);

        if (button1Partial) out.push_back("Button 1 needs both label and URL.");
        if (button2Partial) out.push_back("Button 2 needs both label and URL.");
        if (strlen(g_Settings.button1Url) > 0 && !LooksLikeHttpUrl(g_Settings.button1Url)) out.push_back("Button 1 URL must be http/https.");
        if (strlen(g_Settings.button2Url) > 0 && !LooksLikeHttpUrl(g_Settings.button2Url)) out.push_back("Button 2 URL must be http/https.");

        return out.empty();
    }

    void RenderPayloadStatus() {
        std::vector<std::string> errors;
        if (IsPayloadValid(&errors)) {
            ImGui::TextColored(kGood, "%s", "Payload checks OK");
            return;
        }

        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.36f, 1.0f), "%s", errors.front().c_str());
        if (errors.size() > 1) {
            ImGui::SameLine();
            ImGui::TextColored(kTextMuted, "(+%d)", static_cast<int>(errors.size() - 1));
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                for (const std::string& error : errors) {
                    ImGui::TextUnformatted(error.c_str());
                }
                ImGui::EndTooltip();
            }
        }
    }

    bool TextField(const char* label, char* value, size_t valueSize) {
        TextMuted(label);
        ImGui::PushID(label);
        ImGui::PushItemWidth(-1.0f);
        bool changed = ImGui::InputText("##value", value, valueSize);
        ImGui::PopItemWidth();
        ImGui::PopID();
        return changed;
    }

    bool TextFieldCell(const char* label, char* value, size_t valueSize) {
        TextMuted(label);
        ImGui::PushID(label);
        ImGui::PushItemWidth(-1.0f);
        bool changed = ImGui::InputText("##value", value, valueSize);
        ImGui::PopItemWidth();
        ImGui::PopID();
        return changed;
    }

    bool ComboField(const char* label, int* value, const char* const items[], int itemCount) {
        TextMuted(label);
        ImGui::PushID(label);
        ImGui::PushItemWidth(-1.0f);
        bool changed = ImGui::Combo("##value", value, items, itemCount);
        ImGui::PopItemWidth();
        ImGui::PopID();
        return changed;
    }

    int ActivityTypeToComboIndex(int32_t type) {
        switch (type) {
        case 2: return 1;
        case 3: return 2;
        case 5: return 3;
        default: return 0;
        }
    }

    int32_t ComboIndexToActivityType(int index) {
        switch (index) {
        case 1: return 2;
        case 2: return 3;
        case 3: return 5;
        default: return 0;
        }
    }

    void ActivityTypeField() {
        const char* activityTypes[] = { "Playing", "Listening", "Watching", "Competing" };
        int selectedType = ActivityTypeToComboIndex(g_Settings.activityType);
        if (ComboField("Activity Type", &selectedType, activityTypes, IM_ARRAYSIZE(activityTypes))) {
            g_Settings.activityType = ComboIndexToActivityType(selectedType);
        }
    }

    void IntFieldCell(const char* label, int32_t* value) {
        TextMuted(label);
        ImGui::PushID(label);
        ImGui::PushItemWidth(-1.0f);
        ImGui::InputInt("##value", value, 0, 0);
        ImGui::PopItemWidth();
        ImGui::PopID();
    }

    void RenderTimerFields() {
        if (ImGui::BeginTable("TimerFields", 4, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn();
            IntFieldCell("Days", &g_Settings.days);
            ImGui::TableNextColumn();
            IntFieldCell("Hours", &g_Settings.hours);
            ImGui::TableNextColumn();
            IntFieldCell("Minutes", &g_Settings.minutes);
            ImGui::TableNextColumn();
            IntFieldCell("Seconds", &g_Settings.seconds);
            ImGui::EndTable();
        }
    }

    void RenderPartyFields() {
        if (ImGui::BeginTable("PartyFields", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn();
            IntFieldCell("Current Size", &g_Settings.partySize);
            ImGui::TableNextColumn();
            IntFieldCell("Max Size", &g_Settings.partyMax);
            ImGui::EndTable();
        }
    }

    void RenderButtonFields() {
        if (ImGui::BeginTable("ButtonFields", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn();
            TextFieldCell("Button 1 Label", g_Settings.button1Label, IM_ARRAYSIZE(g_Settings.button1Label));
            ImGui::TableNextColumn();
            TextFieldCell("Button 1 URL", g_Settings.button1Url, IM_ARRAYSIZE(g_Settings.button1Url));
            ImGui::TableNextColumn();
            TextFieldCell("Button 2 Label", g_Settings.button2Label, IM_ARRAYSIZE(g_Settings.button2Label));
            ImGui::TableNextColumn();
            TextFieldCell("Button 2 URL", g_Settings.button2Url, IM_ARRAYSIZE(g_Settings.button2Url));
            ImGui::EndTable();
        }
    }

    void SectionTitle(const char* title, const char* hint = nullptr) {
        ImGui::TextColored(ImVec4(0.92f, 0.95f, 0.98f, 1.0f), "%s", title);
        if (hint && hint[0] != '\0') {
            ImGui::SameLine();
            ImGui::TextColored(kTextMuted, "%s", hint);
        }
        ImGui::Separator();
    }

    bool BeginPanel(const char* id, const ImVec2& size = ImVec2(0, 0)) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.76f, 0.76f, 0.76f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        return ImGui::BeginChild(id, size, ImGuiChildFlags_Borders);
    }

    void EndPanel() {
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    void DrawTabDivider() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float x1 = ImGui::GetWindowPos().x + style.WindowPadding.x;
        float x2 = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - style.WindowPadding.x;
        float y = cursor.y - 4.0f;

        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(x1, y),
            ImVec2(x2, y),
            ImGui::GetColorU32(ImVec4(0.90f, 0.90f, 0.90f, 1.0f)),
            1.0f
        );
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
    }

    void StatusLabel() {
        const bool connected = g_Discord.IsConnected();
        const bool ready = g_Discord.IsReady();
        const char* label = ready ? "Live" : (connected ? "Connecting" : "Offline");
        ImVec4 color = ready ? kGood : (connected ? kWarn : kBad);

        ImGui::TextColored(color, "%s", label);
    }

    void HeaderStatusLabel() {
        const bool connected = g_Discord.IsConnected();
        const bool ready = g_Discord.IsReady();
        const char* label = ready ? "Live" : (connected ? "Connecting" : "Offline");
        ImVec4 color = ready ? kGood : (connected ? kWarn : kBad);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(color, "%s", label);
    }

    bool ActionButton(const char* label, const ImVec2& size, const ImVec4& color, bool enabled = true) {
        ImVec4 buttonColor = enabled ? color : ImVec4(0.055f, 0.055f, 0.055f, 1.0f);
        const float hoverR = buttonColor.x + 0.18f > 1.0f ? 1.0f : buttonColor.x + 0.18f;
        const float hoverG = buttonColor.y + 0.18f > 1.0f ? 1.0f : buttonColor.y + 0.18f;
        const float hoverB = buttonColor.z + 0.18f > 1.0f ? 1.0f : buttonColor.z + 0.18f;
        const float activeR = buttonColor.x - 0.12f < 0.0f ? 0.0f : buttonColor.x - 0.12f;
        const float activeG = buttonColor.y - 0.12f < 0.0f ? 0.0f : buttonColor.y - 0.12f;
        const float activeB = buttonColor.z - 0.12f < 0.0f ? 0.0f : buttonColor.z - 0.12f;

        if (!enabled) ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Border, enabled ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.38f, 0.38f, 0.38f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(hoverR, hoverG, hoverB, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(activeR, activeG, activeB, 1.0f));
        bool pressed = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        if (!enabled) ImGui::EndDisabled();
        return enabled && pressed;
    }

    void SanitizeTimestampInputs() {
        if (g_Settings.days < 0) g_Settings.days = 0;
        if (g_Settings.hours < 0 || g_Settings.hours > 23) g_Settings.hours = 0;
        if (g_Settings.minutes < 0 || g_Settings.minutes > 59) g_Settings.minutes = 0;
        if (g_Settings.seconds < 0 || g_Settings.seconds > 59) g_Settings.seconds = 0;
    }

    void RenderConnectionPanel() {
        SectionTitle("Connection");

        TextField("Application ID", g_Settings.clientId, IM_ARRAYSIZE(g_Settings.clientId));
        ActivityTypeField();

        ImGui::Spacing();
        SectionTitle("Game Lookup");

        static char gameSearch[128] = "";
        static std::string selectedAppId;
        static std::string verifiedAppName;
        static std::string verifyMessage;

        std::string loadError;
        g_AppDatabase.EnsureLoaded(g_Config.GetAppDataPath());

        TextField("Game Name", gameSearch, IM_ARRAYSIZE(gameSearch));
        auto matches = g_AppDatabase.Search(gameSearch, 12);

        const AppEntry* selectedEntry = selectedAppId.empty() ? nullptr : g_AppDatabase.FindById(selectedAppId);
        std::string preview = selectedEntry ? selectedEntry->name + " (" + selectedEntry->id + ")" : "Select match";
        ImGui::PushItemWidth(-1.0f);
        if (ImGui::BeginCombo("##GameMatches", preview.c_str())) {
            if (matches.empty()) {
                ImGui::BeginDisabled();
                ImGui::Selectable(g_AppDatabase.IsLoaded() ? "No matches" : "Database not downloaded");
                ImGui::EndDisabled();
            }
            for (const AppEntry* entry : matches) {
                std::string label = entry->name + "##" + entry->id;
                bool isSelected = selectedAppId == entry->id;
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedAppId = entry->id;
                    selectedEntry = entry;
                    verifiedAppName.clear();
                    verifyMessage.clear();
                    ApplyAppEntry(*entry);
                }
                if (!entry->aliases.empty() && ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Application ID: %s", entry->id.c_str());
                    ImGui::Text("Alias: %s", entry->aliases.front().c_str());
                    if (!entry->skus.empty()) ImGui::Text("SKU: %s", entry->skus.front().c_str());
                    ImGui::EndTooltip();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        if (selectedEntry) {
            ImGui::TextColored(kTextMuted, "%s", selectedEntry->id.c_str());
            if (!selectedEntry->skus.empty()) {
                ImGui::SameLine();
                ImGui::TextColored(kTextMuted, "%s", selectedEntry->skus.front().c_str());
            }

            if (!selectedEntry->presets.empty()) {
                static int selectedPreset = 0;
                if (selectedPreset >= static_cast<int>(selectedEntry->presets.size())) selectedPreset = 0;
                std::vector<const char*> presetNames;
                presetNames.reserve(selectedEntry->presets.size());
                for (const PresencePreset& preset : selectedEntry->presets) {
                    presetNames.push_back(preset.name.c_str());
                }
                if (ComboField("Predefined State", &selectedPreset, presetNames.data(), static_cast<int>(presetNames.size()))) {
                    ApplyPresencePreset(selectedEntry->presets[selectedPreset]);
                }
            }
            else {
                TextMuted("No predefined states in this database entry.");
            }
        }

        const float lookupWidth = ImGui::GetContentRegionAvail().x;
        if (selectedEntry && ImGui::Button("Check Selected ID with Discord", ImVec2(lookupWidth, 30.0f))) {
            std::string error;
            if (g_AppDatabase.VerifyWithDiscord(selectedEntry->id, verifiedAppName, error)) {
                verifyMessage = "Discord RPC match: " + verifiedAppName;
                g_Logger.AddLog("[App DB] " + verifyMessage, ImVec4(0.45f, 0.95f, 0.55f, 1.0f));
            }
            else {
                verifyMessage = "Discord check failed: " + error;
                g_Logger.AddLog("[App DB] " + verifyMessage, ImVec4(1.0f, 0.55f, 0.20f, 1.0f));
            }
        }

        if (!verifyMessage.empty()) {
            ImGui::TextColored(verifiedAppName.empty() ? kWarn : kGood, "%s", verifyMessage.c_str());
        }

        ImGui::Spacing();
        ImGui::Text("Status");
        ImGui::SameLine();
        StatusLabel();
        RenderPayloadStatus();

        ImGui::Spacing();
        const float fullWidth = ImGui::GetContentRegionAvail().x;
        if (!g_Discord.IsConnected()) {
            const bool canConnect = LooksLikeDiscordId(g_Settings.clientId);
            if (ActionButton("Connect", ImVec2(fullWidth, 42.0f), kAccent, canConnect)) {
                g_Discord.Initialize(g_Settings.clientId);
            }
        }
        else if (!g_Discord.IsReady()) {
            ImGui::BeginDisabled();
            ImGui::Button("Connecting...", ImVec2(fullWidth, 42.0f));
            ImGui::EndDisabled();

            if (ActionButton("Cancel", ImVec2(fullWidth, 36.0f), ImVec4(0.74f, 0.12f, 0.14f, 1.0f))) {
                g_Discord.Shutdown();
            }
        }
        else {
            bool canPush = IsPayloadValid();
            if (ActionButton("Validate & Push Update", ImVec2(fullWidth, 42.0f), kAccent, canPush)) {
                if (g_Discord.GetCurrentClientID() != g_Settings.clientId) {
                    g_Discord.Reconnect(g_Settings.clientId);
                }
                else {
                    g_Discord.Update(g_Settings);
                }
            }
            if (ActionButton("Disconnect", ImVec2(fullWidth, 36.0f), ImVec4(0.74f, 0.12f, 0.14f, 1.0f))) {
                g_Discord.Shutdown();
            }
        }

        ImGui::Spacing();
        if (ActionButton("Reset All Fields", ImVec2(fullWidth, 34.0f), ImVec4(0.62f, 0.24f, 0.18f, 1.0f))) {
            g_Settings = PresenceSettings();
            g_Logger.AddLog("[Zombie-RPC] All fields have been cleared.", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        }

        ImGui::Spacing();
        SectionTitle("Resources");
        if (ActionButton("Discord Developer Portal", ImVec2(fullWidth, 34.0f), ImVec4(0.090f, 0.090f, 0.090f, 1.0f))) {
            ShellExecuteA(NULL, "open", "https://discord.com/developers/home", NULL, NULL, SW_SHOWNORMAL);
        }
    }

    void RenderCorePresencePanel() {
        SectionTitle("Presence Modifier");
        TextField("Details", g_Settings.details, IM_ARRAYSIZE(g_Settings.details));
        TextField("State", g_Settings.state, IM_ARRAYSIZE(g_Settings.state));

        ImGui::Spacing();
        SectionTitle("Image Assets");
        TextField("Large Image Key", g_Settings.largeImageKey, IM_ARRAYSIZE(g_Settings.largeImageKey));
        TextField("Large Image Text", g_Settings.largeImageText, IM_ARRAYSIZE(g_Settings.largeImageText));
        TextField("Small Image Key", g_Settings.smallImageKey, IM_ARRAYSIZE(g_Settings.smallImageKey));
        TextField("Small Image Text", g_Settings.smallImageText, IM_ARRAYSIZE(g_Settings.smallImageText));
    }

    void RenderAdvancedPresencePanel() {
        SectionTitle("Timer");
        const char* timerModes[] = { "Time Elapsed (Count Up)", "Time Remaining (Count Down)" };
        ComboField("Display Mode", &g_Settings.timerMode, timerModes, IM_ARRAYSIZE(timerModes));

        RenderTimerFields();
        SanitizeTimestampInputs();

        ImGui::Spacing();
        SectionTitle("Party");
        TextField("Party ID", g_Settings.partyId, IM_ARRAYSIZE(g_Settings.partyId));

        RenderPartyFields();

        ImGui::Spacing();
        SectionTitle("Buttons");
        RenderButtonFields();
    }

    void RenderPresenceTab() {
        ImGui::Spacing();
        const float width = ImGui::GetContentRegionAvail().x;
        const bool compact = width < 940.0f;
        const float leftWidth = compact ? width : 330.0f;
        const float rightWidth = compact ? width : width - leftWidth - ImGui::GetStyle().ItemSpacing.x;

        if (BeginPanel("ConnectionPanel", ImVec2(leftWidth, compact ? 350.0f : 0.0f))) {
            RenderConnectionPanel();
        }
        EndPanel();

        if (!compact) ImGui::SameLine();

        const bool split = rightWidth > 720.0f && !compact;
        if (split) {
            const float columnWidth = (rightWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
            if (BeginPanel("CorePresence", ImVec2(columnWidth, 0.0f))) {
                RenderCorePresencePanel();
            }
            EndPanel();

            ImGui::SameLine();

            if (BeginPanel("AdvancedPresence", ImVec2(0.0f, 0.0f))) {
                RenderAdvancedPresencePanel();
            }
            EndPanel();
        }
        else {
            if (BeginPanel("CorePresence", ImVec2(rightWidth, compact ? 0.0f : 300.0f))) {
                RenderCorePresencePanel();
            }
            EndPanel();

            if (BeginPanel("AdvancedPresence", ImVec2(rightWidth, 0.0f))) {
                RenderAdvancedPresencePanel();
            }
            EndPanel();
        }
    }

    void RenderDatabaseSettings() {
        SectionTitle("Verified App ID Database");
        g_AppDatabase.EnsureLoaded(g_Config.GetAppDataPath());

        const float width = ImGui::GetContentRegionAvail().x;
        if (ActionButton("Update ID Database", ImVec2(width, 32.0f), ImVec4(0.090f, 0.090f, 0.090f, 1.0f))) {
            std::string error;
            if (g_AppDatabase.RefreshFromRemote(g_Config.GetAppDataPath(), error)) {
                g_Logger.AddLog("[App DB] Updated " + std::to_string(g_AppDatabase.Count()) + " verified app IDs from GitHub.", ImVec4(0.45f, 0.95f, 0.55f, 1.0f));
            }
            else {
                g_Logger.AddLog("[App DB] Update failed: " + error, ImVec4(1.0f, 0.35f, 0.30f, 1.0f));
            }
        }

        if (ActionButton("Open Verified App IDs List", ImVec2(width, 32.0f), ImVec4(0.090f, 0.090f, 0.090f, 1.0f))) {
            ShellExecuteA(NULL, "open", "https://gist.github.com/Cynosphere/c1e77f77f0e565ddaac2822977961e76", NULL, NULL, SW_SHOWNORMAL);
        }

        if (g_AppDatabase.IsLoaded()) {
            std::string dbStatus = std::to_string(g_AppDatabase.Count()) + " IDs cached";
            if (!g_AppDatabase.SourceUpdatedAt().empty()) dbStatus += " | GitHub " + g_AppDatabase.SourceUpdatedAt();
            TextMuted(dbStatus.c_str());
        }
        else {
            TextMuted("Click Update ID Database to download the verified list.");
        }
    }
}

/* Renders the main GUI using Dear ImGui */
void Interface::Render(bool& done, HWND hwnd) {
    IM_UNUSED(done);
    ApplyModernStyle();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    // Lock window movement and resizing to fit the Win32 host window perfectly
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("ZombieRPC_Internal", nullptr, window_flags);

    const float headerY = ImGui::GetCursorPosY();
    ImGui::TextColored(ImVec4(0.93f, 0.96f, 0.98f, 1.0f), "ZombieRPC");
    ImGui::SameLine();
    TextMuted("Rich Presence Control Panel");

    const float closeWidth = 36.0f;
    const float closeHeight = 26.0f;
    const float closeX = ImGui::GetWindowWidth() - closeWidth - 18.0f;
    const float statusX = closeX - 132.0f;
    ImGui::SetCursorPos(ImVec2(statusX, headerY + (closeHeight - ImGui::GetTextLineHeight()) * 0.5f));
    StatusLabel();
    ImGui::SetCursorPos(ImVec2(closeX, headerY));
    if (ActionButton("X", ImVec2(closeWidth, closeHeight), ImVec4(0.72f, 0.08f, 0.10f, 1.0f))) {
        ShowWindow(hwnd, SW_HIDE);
    }
    ImGui::SetCursorPosY(headerY + closeHeight + 8.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

    if (ImGui::BeginTabBar("MainTabBar")) {

        /* TAB: Presence Configuration */
        if (ImGui::BeginTabItem("Presence")) {
            DrawTabDivider();
            RenderPresenceTab();
            ImGui::EndTabItem();
        }

        /* TAB: Presets Management */
        if (ImGui::BeginTabItem("Presets")) {
            DrawTabDivider();
            static char newPresetName[64] = "";
            ImGui::InputText("New Preset Name", newPresetName, 64);

            bool isReserved = (_stricmp(newPresetName, "default") == 0);

            if (isReserved) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::TextWrapped("Error: 'Default' is a reserved name for your default profile.");
                ImGui::PopStyleColor();
                ImGui::BeginDisabled();
            }

            if (ImGui::Button("Save Current Setup")) {
                if (strlen(newPresetName) > 0) {
                    g_Config.SavePreset(newPresetName, g_Settings);
                    memset(newPresetName, 0, sizeof(newPresetName));
                }
            }

            if (isReserved) ImGui::EndDisabled();

            ImGui::Separator();
            ImGui::Text("Available Presets:");

            auto presets = g_Config.GetAvailablePresets();
            std::string currentDefault = g_Config.GetDefaultPresetName();
            bool deletedPreset = false;

            for (const auto& name : presets) {
                ImGui::PushID(name.c_str());
                if (ImGui::Selectable(name.c_str(), false, 0, ImVec2(ImGui::GetWindowWidth() - 150, 0))) {
                    if (g_Config.LoadPreset(name, g_Settings)) {
                        g_Logger.AddLog("[Zombie-RPC] Loaded preset: " + name, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
                    }
                }

                if (ImGui::BeginPopupContextItem("PresetContext")) {
                    ImGui::TextUnformatted(name.c_str());
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete Preset")) {
                        deletedPreset = g_Config.DeletePreset(name);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                ImGui::SameLine(ImGui::GetWindowWidth() - 130);

                if (name == currentDefault) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[DEFAULT]");
                }
                else {
                    if (ImGui::Button("Set as Default")) g_Config.SetDefaultPresetName(name);
                }
                ImGui::PopID();

                if (deletedPreset) {
                    break;
                }
            }
            ImGui::EndTabItem();
        }

        /* TAB: System Settings & Logs */
        if (ImGui::BeginTabItem("Settings")) {
            DrawTabDivider();
            static bool startup = g_Config.IsStartupEnabled();
            static bool startMinimized = g_Config.ShouldStartMinimized();
            static bool lastOverrideState = false;

            // Sync UI state if Task Manager override was detected
            bool isOverride = g_Config.IsDisabledInTaskManager();
            if (isOverride && !lastOverrideState) {
                startup = false;
                g_Logger.AddLog("[Zombie-RPC] Startup disabled via Task Manager.", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            }
            lastOverrideState = isOverride;

            if (ImGui::Checkbox("Run on Startup", &startup)) {
                g_Config.SetStartup(startup);
            }

            ImGui::SameLine();

            if (ImGui::Checkbox("Start Minimized", &startMinimized)) {
                g_Config.SetStartMinimized(startMinimized);
                if (startup) g_Config.SetStartup(true); // Refresh registry flag
            }

            ImGui::Separator();
            RenderDatabaseSettings();

            ImGui::Separator();
            ImGui::Text("Application Logs:");

            ImGui::BeginChild("LogRegion", ImVec2(0, 0), true);
            for (const auto& log : g_Logger.GetLogs()) {
                ImGui::TextColored(log.color, log.text.c_str());
            }

            // Auto-scroll to bottom
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
