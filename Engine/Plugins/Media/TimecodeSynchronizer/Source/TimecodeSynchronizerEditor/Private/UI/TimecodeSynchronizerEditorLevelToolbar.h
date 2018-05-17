// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

//////////////////////////////////////////////////////////////////////////
// FTimecodeSynchronizerEditorLevelToolbar

class FTimecodeSynchronizerEditorLevelToolbar
{
public:
	FTimecodeSynchronizerEditorLevelToolbar();
	~FTimecodeSynchronizerEditorLevelToolbar();

private:
	void ExtendLevelEditorToolbar();
	void FillToolbar(class FToolBarBuilder& ToolbarBuilder);

private:
	TSharedPtr<class FExtender>			LevelToolbarExtender;
};

