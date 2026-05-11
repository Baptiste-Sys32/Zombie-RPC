#pragma once

#include <windows.h>
#include "dependencies/imgui/imgui.h"

/* Interface: Contains the main UI rendering logic using Dear ImGui */
namespace Interface {
    /*
       Renders the application frame
       done: Reference to the main loop control flag
       hwnd: Handle to the application window for system interactions
    */
    void Render(bool& done, HWND hwnd);
}