// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "USDLevelInfo.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

AUSDLevelInfo::AUSDLevelInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FileScale = 1.0;
}