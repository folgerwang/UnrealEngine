// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditorUtilityContext.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "ContentBrowserModule.h"
#include "Modules/ModuleManager.h"
#include "IContentBrowserSingleton.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"


#define LOCTEXT_NAMESPACE "BlutilityLevelEditorExtensions"

UEditorUtilityContext::UEditorUtilityContext(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


#undef LOCTEXT_NAMESPACE
