# Zombie-RPC

Simple Discord Rich Presence controller for Windows.

## Features

- Edit client ID, state, details, party size, and image keys.
- Add up to two buttons with URLs.
- Use elapsed or remaining timestamps.
- Save and load `.zrpc` profiles.
- Minimize to the system tray.
- Optionally run on Windows startup.

## Build

1. Open `Discord-RPC.slnx` in Visual Studio 2022.
2. Select `Release | x64`.
3. Build the solution.

The app uses C++17, Win32, DirectX 11, Dear ImGui, and Discord RPC.

## Notes

- Profiles are stored in `%appdata%\Zombie-RPC\Profiles`.
- You need a Discord application client ID from the Discord Developer Portal.
- Image keys must match the asset names in your Discord application.
- The app does not use Discord tokens.
