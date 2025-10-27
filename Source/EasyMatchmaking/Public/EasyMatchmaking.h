// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IEOSSDKManager.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

// This class will initialize settings for EOS and setup button with button,
// main functionallity is inside class will be EOSManager.cpp
class FEasyMatchmakingModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command. */
	void PluginButtonClicked();

	static IEOSPlatformHandlePtr GetCachedEOSPlatform();
	
private:

	void RegisterMenus();

	void InitializeEOSUserSettings();
	void InitializeEOSForDedicatedServer();
	
	// For Test ONLY!
	TSharedPtr<FSlateDynamicImageBrush> ImageBrush;
	TWeakPtr<SWindow> PluginWindow; // Track the window, we want only one window open
	TSharedPtr<class FUICommandList> PluginCommands;

	IEOSPlatformHandlePtr InstanceCachedEOSPlatform;
};

/*
Main Workflow Example:
A dedicated server is available to host a game and has registered a session.
Player A and Player B are in a lobby together.
Player A selects a game mode and indicates that they are ready to play.The EOS Lobby service then shares this update, as a lobby attribute, with Player B in real - time.
Player B indicates that they are ready to play.The EOS Lobby service notifies Player A that Player B is ready.
Player A searches for a game session and finds the registered session on the dedicated server.
Player A adds the dedicated server's IP address as a new value to the lobby attribute data. The EOS Lobby service shares this data update with all players in the lobby (Player B in this case). For more information on how to update lobby data, see the section on: Lobby and Lobby Member Properties in the Modify a Lobby page.
Both players connect to the dedicated server, which registers them both to the session.
Player A and B start the game.
The dedicated server updates the session state to “In Game”.Other players that search for a session can see that players are in the game.For more information, see the documentation : Modify a Session.*/