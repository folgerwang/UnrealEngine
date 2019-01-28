// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "MagicLeapScreensTypes.h"

enum EScreensMsgType
{
	Request,
	Response,
};

enum EScreensTaskType
{
	None,
	GetHistory,
	AddToHistory,
	UpdateEntry,
};

struct FScreensMessage
{
	EScreensMsgType Type;
	EScreensTaskType TaskType;
	bool bSuccess;
	TArray<FScreensWatchHistoryEntry> WatchHistory;
	FScreensEntryRequestResultDelegate EntryDelegate;
	FScreensHistoryRequestResultDelegate HistoryDelegate;

	FScreensMessage()
		: Type(EScreensMsgType::Request)
		, TaskType(EScreensTaskType::None)
		, bSuccess(false)
	{}
};