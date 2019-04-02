// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

struct FLiveCodingManifest
{
	FString LinkerPath;
	TMap<FString, TArray<FString>> BinaryToObjectFiles;

	bool Read(const TCHAR* FileName, FString& OutFailReason);
	bool Parse(FJsonObject& Object, FString& OutFailReason);
};
