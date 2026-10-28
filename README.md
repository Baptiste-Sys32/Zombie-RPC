# Zombie-RPC

Windows app for custom Discord Rich Presence. Built with C++17, DirectX 11, and Dear ImGui. Paste an application ID, set title, details, buttons, images and timestamps, and save named profiles to switch between.

## Build

Visual Studio 2022 (toolset v143) with the Windows 10 SDK. Open `Discord-RPC.slnx` and build x64. Dependencies (Dear ImGui, discord-rpc) are vendored under `dependencies/`.

## Run

Launch the built exe, paste your Discord application ID, fill in the presence fields, and connect. Profiles save as `.zrpc` files under AppData. The two samples in `resources/` use the placeholder `YOUR_DISCORD_CLIENT_ID`.

## Note

The starting codebase came from a friend and was adapted for personal use. Both of us fully vibe coded it, so do not expect clean architecture. It is not sustainable and will never receive updates. It works as is (barely works, but when it does work, it does its job).

## License

This project is licensed under the MIT License. See the [LICENSE.md](LICENSE.md) file for details.
