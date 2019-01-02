// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"
#include "UI/MediaFrameworkUtilitiesEditorStyle.h"

class FMediaProfileCommands : public TCommands<FMediaProfileCommands>
{
public:

	FMediaProfileCommands()
		: TCommands<FMediaProfileCommands>(TEXT("ToolbarIcon"), NSLOCTEXT("Contexts", "ToolbarIcon", "Media Profile"), NAME_None, FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName())
	{}
	
	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

	/** Applies changes to the original Media Profile.*/
	TSharedPtr<FUICommandInfo> Apply;
};
