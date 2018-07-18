// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FSimpleParse
{
	static bool MatchZeroOrMoreWhitespace(const TCHAR*& InOutPtr);
	static bool MatchChar(const TCHAR*& InOutPtr, TCHAR Ch);
	static bool ParseString(const TCHAR*& InOutPtr, FString& OutStr);
	static bool ParseUnsignedNumber(const TCHAR*& InOutPtr, int32& OutNumber);
};
