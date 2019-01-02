// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CurveTableEditorUtils.h"

#define LOCTEXT_NAMESPACE "CurveTableEditorUtils"

FCurveTableEditorUtils::FCurveTableEditorManager& FCurveTableEditorUtils::FCurveTableEditorManager::Get()
{
	static TSharedRef< FCurveTableEditorManager > EditorManager(new FCurveTableEditorManager());
	return *EditorManager;
}

void FCurveTableEditorUtils::BroadcastPreChange(UCurveTable* CurveTable, ECurveTableChangeInfo Info)
{
	FCurveTableEditorManager::Get().PreChange(CurveTable, Info);
}

void FCurveTableEditorUtils::BroadcastPostChange(UCurveTable* CurveTable, ECurveTableChangeInfo Info)
{
	FCurveTableEditorManager::Get().PostChange(CurveTable, Info);
}

#undef LOCTEXT_NAMESPACE
