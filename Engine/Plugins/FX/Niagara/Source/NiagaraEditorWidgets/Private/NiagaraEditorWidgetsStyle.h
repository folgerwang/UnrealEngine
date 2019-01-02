// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Styling/ISlateStyle.h"

/** Manages the style which provides resources for niagara editor widgets. */
class FNiagaraEditorWidgetsStyle
{
public:

	static void Initialize();

	static void Shutdown();

	/** reloads textures used by slate renderer */
	static void ReloadTextures();

	/** @return The Slate style set for niagara editor widgets */
	static const ISlateStyle& Get();

	static FName GetStyleSetName();

private:

	static TSharedRef< class FSlateStyleSet > Create();

private:

	static TSharedPtr< class FSlateStyleSet > NiagaraEditorWidgetsStyleInstance;
};
