#pragma once

#include <windows.h>
#include <cstdint>

/* PresenceSettings: Container for all Discord Rich Presence data fields */
struct PresenceSettings {
    // Application Identification
    char clientId[64] = "";

    // Text Content
    char details[128] = "";
    char state[128] = "";

    /* Activity Type: Discord RPC supports 0=Playing, 2=Listening, 3=Watching, 5=Competing */
    int32_t activityType = 0;

    /* Timestamp Settings: timerMode 0 = Elapsed, 1 = Remaining */
    int32_t timerMode = 0;
    int32_t days = 0;
    int32_t hours = 0;
    int32_t minutes = 0;
    int32_t seconds = 0;

    /* Image Assets: Must match Discord Developer Portal keys */
    char largeImageKey[64] = "";
    char largeImageText[128] = "";
    char smallImageKey[64] = "";
    char smallImageText[128] = "";

    /* Party Settings: Multiplayer/Group status */
    char partyId[64] = "";
    int32_t partySize = 0;
    int32_t partyMax = 0;

    /* Button Settings: Supported by the overhauled discord-rpc fork */
    char button1Label[32] = "";
    char button1Url[256] = "";
    char button2Label[32] = "";
    char button2Url[256] = "";
};

/* Windows System Constants: Custom Window Messages and Resource IDs */
#define WM_TRAYICON            (WM_USER + 1)
#define ID_TRAY_EXIT           1001
#define ID_TRAY_SHOW           1002
#define ID_TRAY_PROFILES_START 5000

/* Global Shared State: Defined in main.cpp */
extern PresenceSettings g_Settings;
extern bool g_ExitRequested;
