// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Linux/LinuxPlatformProcess.h"
#include "Containers/StringConv.h"
#include "CoreGlobals.h"
#include "Logging/LogMacros.h"

const TCHAR* FLinuxPlatformProcess::BaseDir()
{
	static bool bHaveResult = false;
	static TCHAR CachedResult[MAX_PATH];
	if (!bHaveResult)
	{
		char SelfPath[MAX_PATH] = {0};
		if (readlink( "/proc/self/exe", SelfPath, ARRAY_COUNT(SelfPath) - 1) == -1)
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Fatal, TEXT("readlink() failed with errno = %d (%s)"), ErrNo,
				StringCast< TCHAR >(strerror(ErrNo)).Get());
			// unreachable
			return CachedResult;
		}
		SelfPath[ARRAY_COUNT(SelfPath) - 1] = 0;

		FCString::Strncpy(CachedResult, UTF8_TO_TCHAR(dirname(SelfPath)), ARRAY_COUNT(CachedResult) - 1);
		CachedResult[ARRAY_COUNT(CachedResult) - 1] = 0;
		FCString::Strncat(CachedResult, TEXT("/"), ARRAY_COUNT(CachedResult) - 1);
		bHaveResult = true;
	}
	return CachedResult;
}

const TCHAR* FLinuxPlatformProcess::GetBinariesSubdirectory()
{
	return TEXT("Linux");
}
