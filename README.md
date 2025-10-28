# EasyMatchmaking Plugin for Unreal Engine 🎮

Easy-to-use multiplayer plugin built on Epic Online Services (EOS). Handle lobbies, sessions, and dedicated servers - all with Blueprint or C++ support, no additional subsystems required!

## What is This?

EasyMatchmaking is a plugin for Unreal Engine that lets you create **lobbies with sessions for dedicated servers!** 

Unlike the official Online Subsystem EOS Plugin, this is **pure C++ for UE with no extra plugins dependencies**, giving you **full control** of your network. You can generate your server `.exe` file, run it on your machine, or send it to a cloud service and it'll host the server for you.

## Motivation

Why did I make this?

- **Ready-to-use UI** - Example widgets included, just plug and play
- **Blueprint accessible** - All functions exposed to Blueprint
- **Fully expandable C++** - No module dependencies, simpler to understand how EOS actually works
- **Learning focused** - With the official EOS Online Subsystem plugin, you can't easily modify or understand the C++ implementation. This plugin is meant to be readable and hackable!

Feel free to add or modify files as you need! (Would be nice to give me credits if possible 🫶)

## Features

✨ **What's Included:**

- `.uasset` files for UI examples
- Blueprint functions for using EOS
- Authentication via Epic Games website
- Dedicated server `.exe` generation
- Lobby system with real-time updates
- Chat in lobbies
- Session creation and management
- Auto-join functionality for party members
- Fully accessible C++ - everything in one place!
- Real-time notifications for lobby updates
- Error logs

## Supported Platforms

- **Windows** (tested and working)

## Requirements

- **Unreal Engine**: 5.6.1 or later
- **Epic Online Services**: Free EOS Developer account
- **Visual Studio 2022**: For C++ compilation

## Installation

1. Download or clone this repository
2. Copy the `EasyMatchmaking` folder to your project's `Plugins/` directory:
```
   YourProject/
   └── Plugins/
       └── EasyMatchmaking/  <--- Put it like that
```
3. Right-click your `.uproject` file -> **Generate Visual Studio project files**
4. Open your project in Unreal Engine
5. When prompted, **compile** the plugin
6. Restart the editor

## Contributing

Contributions welcome! This is a learning project, so:
- 🐛 Bug reports appreciated
- 💡 Feature requests welcome
- 🔧 Pull requests encouraged
- 📖 Documentation improvements needed!

### Possible Additions

- [ ] Full documentation with examples
- [ ] Voice chat integration
- [ ] Matchmaking rating system
- [ ] Cross-platform support (Linux, Mac)
- [ ] Steam integration alongside EOS

---

Made with ❤️ for the Unreal Engine community

*"Because dealing with Unreal Engine Network shouldn't require a PhD"*