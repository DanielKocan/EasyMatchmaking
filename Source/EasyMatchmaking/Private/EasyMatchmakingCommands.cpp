// Copyright Epic Games, Inc. All Rights Reserved.

#include "EasyMatchmakingCommands.h"

#define LOCTEXT_NAMESPACE "FEasyMatchmakingModule"

void FEasyMatchmakingCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "EasyMatchmaking", "Execute EasyMatchmaking action", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
