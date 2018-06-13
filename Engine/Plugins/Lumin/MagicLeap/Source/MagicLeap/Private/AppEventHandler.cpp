// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AppEventHandler.h"
#include "AppFramework.h"
#include "Engine/Engine.h"
#include "MagicLeapHMD.h"

namespace MagicLeap
{
	IAppEventHandler::IAppEventHandler()
	:  bWasSystemEnabledOnPause(false)
	{
		FAppFramework::AddEventHandler(this);
	}

	IAppEventHandler::~IAppEventHandler()
	{
		FAppFramework::RemoveEventHandler(this);
	}

	void IAppEventHandler::AsyncDestroy()
	{
		FAppFramework::AsyncDestroy(this);
	}
}