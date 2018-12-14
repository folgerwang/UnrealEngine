// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

struct FSimpleParse
{
	static bool MatchZeroOrMoreWhitespace(const TCHAR*& InOutPtr);
	static bool MatchChar(const TCHAR*& InOutPtr, TCHAR Ch);
	static bool ParseString(const TCHAR*& InOutPtr, FString& OutStr);
	static bool ParseUnsignedNumber(const TCHAR*& InOutPtr, int32& OutNumber);
};
