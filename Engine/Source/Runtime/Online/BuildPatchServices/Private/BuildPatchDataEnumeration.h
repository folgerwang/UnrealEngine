// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FBuildDataEnumeration
{
public:
	static bool EnumeratePatchData(const FString& InputFile, const FString& OutputFile, bool bIncludeSizes);
};
