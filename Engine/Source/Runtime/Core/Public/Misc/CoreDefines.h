// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#ifndef STUBBED
#define STUBBED(x)	\
	do																								\
	{																								\
		static bool AlreadySeenThisStubbedSection = false;											\
		if (!AlreadySeenThisStubbedSection)															\
		{																							\
			AlreadySeenThisStubbedSection = true;													\
			fprintf(stderr, "STUBBED: %s at %s:%d (%s)\n", x, __FILE__, __LINE__, __FUNCTION__);	\
		}																							\
	} while (0)
#endif

/*----------------------------------------------------------------------------
Metadata macros.
----------------------------------------------------------------------------*/

#define CPP       1
#define STRUCTCPP 1
#define DEFAULTS  0


/*-----------------------------------------------------------------------------
Seek-free defines.
-----------------------------------------------------------------------------*/

#define STANDALONE_SEEKFREE_SUFFIX	TEXT("_SF")


/*-----------------------------------------------------------------------------
Macros for enabling heap storage instead of inline storage on delegate types.
Can be overridden by setting to 1 or 0 in the project's .Target.cs files.
-----------------------------------------------------------------------------*/

#ifndef USE_SMALL_DELEGATES
	#define USE_SMALL_DELEGATES 1
#endif

#ifndef USE_SMALL_MULTICAST_DELEGATES
	#define USE_SMALL_MULTICAST_DELEGATES 1
#endif

#ifndef USE_SMALL_TFUNCTIONS
	#define USE_SMALL_TFUNCTIONS 0
#endif
