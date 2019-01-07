// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

class FRegexPatternImplementation;
class FRegexMatcherImplementation;

/**
 * Implements a regular expression pattern.
 * @note DO NOT use this class as a file-level variable as its construction relies on the internationalization system being initialized!
 */
class CORE_API FRegexPattern
{
	friend class FRegexMatcher;

public:
	FRegexPattern(const FString& SourceString);

private:
	TSharedRef<FRegexPatternImplementation> Implementation;
};

/**
 * Implements a regular expression pattern matcher.
 * @note DO NOT use this class as a file-level variable as its construction relies on the internationalization system being initialized!
 */
class CORE_API FRegexMatcher
{
public:
	FRegexMatcher(const FRegexPattern& Pattern, const FString& Input);

	bool FindNext();

	int32 GetMatchBeginning();
	int32 GetMatchEnding();

	int32 GetCaptureGroupBeginning(const int32 Index);
	int32 GetCaptureGroupEnding(const int32 Index);
	FString GetCaptureGroup(const int32 Index);

	int32 GetBeginLimit();
	int32 GetEndLimit();
	void SetLimits(const int32 BeginIndex, const int32 EndIndex);

private:
	TSharedRef<FRegexMatcherImplementation> Implementation;
};
