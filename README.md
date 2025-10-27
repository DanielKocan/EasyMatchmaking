# EasyMatchmaking Plugin for Unreal Engine 🎮

Easy-to-use multiplayer plugin powered by Epic Online Services (EOS). Handle lobbies, sessions, and dedicated servers - all with Blueprint or C++ support, no additional subsystems required!

## What is This?

EasyMatchmaking is a plugin for Unreal Engine that lets you create **lobbies with sessions for dedicated servers!** 

Unlike the official Online Subsystem EOS Plugin, this is **pure C++ with no extra dependencies**, giving you **full control** of your network. You can generate your server `.exe` file, run it on your machine, or send it to a cloud service and it'll host the server for you.

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
