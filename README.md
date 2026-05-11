# 🧟 Zombie-RPC
**A lightweight, granular Discord Rich Presence controller for Windows.**

Zombie-RPC is a C++ utility designed to give you total control over your Discord status. Unlike standard RPC tools, this project focuses on a "zero-bloat" philosophy using **Dear ImGui** and **DirectX 11**, ensuring it stays out of the way of your system resources while providing professional-grade features.

---

## 🚀 Features
* **Complete Customization:** Modify Client IDs, state, details, party sizes, and image keys on the fly.
* **Interactive Buttons:** Supports up to two custom buttons with redirect URLs (requires a button-compatible Discord-RPC fork).
* **Smart Timestamps:** Toggle between "Time Elapsed" and "Time Remaining" with DD:HH:MM:SS precision.
* **Preset Management:** * Saves profiles as human-readable `.zrpc` files.
    * Storage localized in `%appdata%\Zombie-RPC\Profiles`.
    * Quick-swap between profiles directly from the System Tray.
* **Windows Native Integration:**
    * **Run on Startup:** Managed via the Windows Registry.
    * **Minimize to Tray:** "X" hides the app to the system tray to keep your taskbar clean.
    * **Single Instance Lock:** Mutex-based protection ensures only one instance runs at a time.

---

## 🛠️ Technical Stack
* **Language:** C++17
* **Graphics API:** DirectX 11
* **GUI Framework:** [Dear ImGui](https://github.com/ocornut/imgui)
* **Backend:** Discord-RPC (Embedded as a resource and extracted at runtime)

---

## 📂 Project Structure
* `main.cpp`: Application lifecycle, Win32 entry point, and D3D11 initialization.
* `Interface.cpp/h`: The ImGui-based control panel logic.
* `ConfigManager.cpp/h`: Handles the heavy lifting for Registry I/O and `.zrpc` parsing.
* `DiscordManager.cpp/h`: Wrapper for the Discord-RPC lifecycle.
* `Resources/`: Contains the application icon and the factory-default `Zombie-RPC.zrpc` profile.

---

## 📦 Building from Source
1. Open `Discord-RPC.slnx` in Visual Studio 2022.
2. Ensure the **DirectX SDK** is available (included in modern Windows SDKs).
3. Set the build configuration to **Release | x64**.
4. Build Solution (`Ctrl+Shift+B`).
5. The executable will handle the extraction of necessary `.dll` dependencies to AppData automatically on the first run.

---

## 📜 Usage Notes
* **First Run:** The app will extract a default profile and create the necessary folder structure in your Roaming AppData.
* **Client IDs:** You must create an application in the [Discord Developer Portal](https://discord.com/developers/applications) to get a Client ID.
* **Assets:** Large and Small image keys must match the names uploaded to your Discord Developer Dashboard.

---

### 🛡️ Privacy & Safety
This project is designed for personal use. It does not store Discord tokens or private user data; it only communicates with the local Discord client via the standard RPC socket.
