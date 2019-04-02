// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "TimecodeSynchronizer.h"
#include "UObject/WeakObjectPtr.h"

struct FAssetData;
class FExtender;
class FMenuBuilder;
class FToolBarBuilder;
class SWidget;

//////////////////////////////////////////////////////////////////////////
// FTimecodeSynchronizerEditorLevelToolbar

class FTimecodeSynchronizerEditorLevelToolbar
{
public:
	FTimecodeSynchronizerEditorLevelToolbar();
	~FTimecodeSynchronizerEditorLevelToolbar();

private:
	void ExtendLevelEditorToolbar();
	void FillToolbar(FToolBarBuilder& ToolbarBuilder);

	void AddObjectSubMenu(FMenuBuilder& MenuBuilder);
	TSharedRef<SWidget> GenerateMenuContent();
	void OpenCurrentTimecodeSynchronizer();
	void CreateNewTimecodeSynchronizer();
	void NewTimecodeSynchronizerSelected(const FAssetData& AssetData);


private:
	TSharedPtr<FExtender> LevelToolbarExtender;
	TWeakObjectPtr<UTimecodeSynchronizer> CurrentTimecodeSynchronizer;
};

