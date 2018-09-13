// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	UnixPlatformString.h: Unix platform string classes, mostly implemented with ANSI C++
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/StandardPlatformString.h"
#include "GenericPlatform/GenericWidePlatformString.h"
/**
* Unix string implementation
**/
struct FUnixPlatformString : public 
#if PLATFORM_TCHAR_IS_CHAR16
	FGenericWidePlatformString
#else
	FStandardPlatformString
#endif
{
	template<typename CHAR>
	static FORCEINLINE int32 Strlen(const CHAR* String)
	{
		if(!String)
			return 0;

		int Len = 0;
		while(String[Len])
		{
			++Len;
		}

		return Len;
	}
};

typedef FUnixPlatformString FPlatformString;
