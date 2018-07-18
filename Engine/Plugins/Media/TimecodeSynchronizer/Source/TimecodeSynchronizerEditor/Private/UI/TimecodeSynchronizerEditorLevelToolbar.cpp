// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UI/TimecodeSynchronizerEditorLevelToolbar.h"

#include "UI/TimecodeSynchronizerEditorCommand.h"
#include "UI/TimecodeSynchronizerEditorStyle.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Toolkits/AssetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "TimecodeSynchronizerEditor"

//////////////////////////////////////////////////////////////////////////
// FTimecodeSynchronizerEditorLevelToolbar

FTimecodeSynchronizerEditorLevelToolbar::FTimecodeSynchronizerEditorLevelToolbar()
{
	ExtendLevelEditorToolbar();
}

FTimecodeSynchronizerEditorLevelToolbar::~FTimecodeSynchronizerEditorLevelToolbar()
{
	if (UObjectInitialized() && LevelToolbarExtender.IsValid() && !GIsRequestingExit)
	{
		// Add a TimecodeSynchronizer toolbar section after the settings section of the level editor
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		if (LevelEditorModule.GetToolBarExtensibilityManager().IsValid())
		{
			LevelEditorModule.GetToolBarExtensibilityManager()->RemoveExtender(LevelToolbarExtender);
		}
	}
}

void FTimecodeSynchronizerEditorLevelToolbar::ExtendLevelEditorToolbar()
{
	check(!LevelToolbarExtender.IsValid());

	// Create Toolbar extension
	LevelToolbarExtender = MakeShareable(new FExtender);
	LevelToolbarExtender->AddToolBarExtension(
		"Settings",
		EExtensionHook::After,
		FTimecodeSynchronizerEditorCommand::Get().CommandActionList,
		FToolBarExtensionDelegate::CreateRaw(this, &FTimecodeSynchronizerEditorLevelToolbar::FillToolbar)
		);

	// Add a TimecodeSynchronizer toolbar section after the settings section of the level editor
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(LevelToolbarExtender);
}

void FTimecodeSynchronizerEditorLevelToolbar::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("TimecodeSynchronizer");
	{
		// Add a button to open a TimecodeSynchronizer Editor
		ToolbarBuilder.AddToolBarButton(
			FTimecodeSynchronizerEditorCommand::Get().OpenEditorCommand,
			NAME_None,
			TAttribute< FText >::Create(TAttribute< FText >::FGetter::CreateLambda([this]() { return FTimecodeSynchronizerEditorCommand::Get().OpenEditorCommand->GetLabel(); })),
			TAttribute< FText >::Create(TAttribute< FText >::FGetter::CreateLambda([this]() { return FTimecodeSynchronizerEditorCommand::Get().OpenEditorCommand->GetDescription(); })),
			FSlateIcon(FTimecodeSynchronizerEditorStyle::GetStyleSetName(), TEXT("Console"))
		);
	}
	ToolbarBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE
