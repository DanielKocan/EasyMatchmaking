// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "EasyMatchmakingStyle.h"

class FEasyMatchmakingCommands : public TCommands<FEasyMatchmakingCommands>
{
public:

	FEasyMatchmakingCommands()
		: TCommands<FEasyMatchmakingCommands>(TEXT("EasyMatchmaking"), NSLOCTEXT("Contexts", "EasyMatchmaking", "EasyMatchmaking Plugin"), NAME_None, FEasyMatchmakingStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
