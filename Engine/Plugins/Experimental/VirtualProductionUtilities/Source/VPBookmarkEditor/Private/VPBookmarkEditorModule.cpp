// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VPBookmarkEditorModule.h"
#include "VPBookmarkTypeActions.h"
#include "Modules/ModuleManager.h"

#include "Bookmarks/IBookmarkTypeTools.h"


DEFINE_LOG_CATEGORY(LogVPBookmarkEditor);


void FVPBookmarkEditorModule::StartupModule()
{
	BookmarkTypeActions = MakeShared<FVPBookmarkTypeActions>();
	IBookmarkTypeTools::Get().RegisterBookmarkTypeActions(BookmarkTypeActions.ToSharedRef());
}


void FVPBookmarkEditorModule::ShutdownModule()
{
	IBookmarkTypeTools::Get().UnregisterBookmarkTypeActions(BookmarkTypeActions.ToSharedRef());
	BookmarkTypeActions.Reset();
}


IMPLEMENT_MODULE(FVPBookmarkEditorModule, VPBookmarkEditor)
