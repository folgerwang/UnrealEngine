// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"

class FUnrealSourceFile;

enum class EHeaderProviderSourceType
{
	ClassName,
	FileName,
	Resolved
};

class FHeaderProvider
{
	friend bool operator==(const FHeaderProvider& A, const FHeaderProvider& B);
public:
	FHeaderProvider(EHeaderProviderSourceType Type, FString&& Id);

	FUnrealSourceFile* Resolve();

	FString ToString() const;

	const FString& GetId() const;

private:
	EHeaderProviderSourceType Type;
	FString Id;
	FUnrealSourceFile* Cache;
};

bool operator==(const FHeaderProvider& A, const FHeaderProvider& B);
