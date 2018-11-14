// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformMisc.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformMisc.h"
#elif PLATFORM_PS4
#include "PS4/PS4Misc.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneMisc.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformMisc.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformMisc.h"
#elif PLATFORM_LUMIN
#include "Lumin/LuminPlatformMisc.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidMisc.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformMisc.h"
#elif PLATFORM_QUAIL
#include "Quail/QuailPlatformMisc.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformMisc.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformMisc.h"
#endif

#ifndef UE_DEBUG_BREAK
#error UE_DEBUG_BREAK is not defined for this platform
#endif

#ifndef PLATFORM_USES_ANSI_STRING_FOR_EXTERNAL_PROFILING
#error PLATFORM_USES_ANSI_STRING_FOR_EXTERNAL_PROFILING is not defined.
#endif

#ifndef PLATFORM_LIMIT_PROFILER_UNIQUE_NAMED_EVENTS
#if defined(FRAMEPRO_ENABLED) && FRAMEPRO_ENABLED
 // If framepro is enabled, we need to limit the number of unique events
 // This define prevents us emitting "Frame N" events, etc
 #define PLATFORM_LIMIT_PROFILER_UNIQUE_NAMED_EVENTS 1
#else
 #define PLATFORM_LIMIT_PROFILER_UNIQUE_NAMED_EVENTS 0
#endif
#endif

// Master switch for scoped named events
#define ENABLE_NAMED_EVENTS (!UE_BUILD_SHIPPING && 1)

#if ENABLE_NAMED_EVENTS

class CORE_API FScopedNamedEvent
{
public:

	FScopedNamedEvent(const struct FColor& Color, const TCHAR* Text)
	{
		FPlatformMisc::BeginNamedEvent(Color, Text);
	}

	FScopedNamedEvent(const struct FColor& Color, const ANSICHAR* Text)
	{
		FPlatformMisc::BeginNamedEvent(Color, Text);
	}

	~FScopedNamedEvent()
	{
		FPlatformMisc::EndNamedEvent();
	}	
};

class CORE_API FScopedProfilerColor
{
public:

	FScopedProfilerColor(const struct FColor& Color)
	{
		FPlatformMisc::BeginProfilerColor(Color);
	}

	~FScopedProfilerColor()
	{
		FPlatformMisc::EndProfilerColor();
	}
};


//
// Scoped named event class for constant (compile-time) strings literals.
//
// BeginNamedEventStatic works the same as BeginNamedEvent, but should only be passed a compile-time string literal.
// Some platform profilers can optimize the case where strings for certain events are constant.
//
class CORE_API FScopedNamedEventStatic
{
public:

	FScopedNamedEventStatic(const struct FColor& Color, const TCHAR* Text)
	{
#if PLATFORM_IMPLEMENTS_BeginNamedEventStatic
		FPlatformMisc::BeginNamedEventStatic(Color, Text);
#else
		FPlatformMisc::BeginNamedEvent(Color, Text);
#endif
	}

	FScopedNamedEventStatic(const struct FColor& Color, const ANSICHAR* Text)
	{
#if PLATFORM_IMPLEMENTS_BeginNamedEventStatic
		FPlatformMisc::BeginNamedEventStatic(Color, Text);
#else
		FPlatformMisc::BeginNamedEvent(Color, Text);
#endif
	}

	~FScopedNamedEventStatic()
	{
		FPlatformMisc::EndNamedEvent();
	}
};


// Lightweight scoped named event separate from stats system.  Will be available in test builds.  
// Events cost profiling overhead so use them judiciously in final code.

#if PLATFORM_USES_ANSI_STRING_FOR_EXTERNAL_PROFILING
#define NAMED_EVENT_STR(x) x
#else
#define NAMED_EVENT_STR(x) L##x
#endif

#define SCOPED_NAMED_EVENT(Name, Color)          FScopedNamedEventStatic ANONYMOUS_VARIABLE(NamedEvent_##Name##_)(Color, NAMED_EVENT_STR(#Name));
#define SCOPED_NAMED_EVENT_FSTRING(Text, Color)  FScopedNamedEvent       ANONYMOUS_VARIABLE(NamedEvent_)         (Color, *Text);
#define SCOPED_NAMED_EVENT_TEXT(Text, Color)     FScopedNamedEventStatic ANONYMOUS_VARIABLE(NamedEvent_)         (Color, NAMED_EVENT_STR(Text));
#define SCOPED_NAMED_EVENT_F(Format, Color, ...) FScopedNamedEvent       ANONYMOUS_VARIABLE(NamedEvent_)         (Color, *FString::Printf(Format, __VA_ARGS__));
#define SCOPED_PROFILER_COLOR(Color)			 FScopedProfilerColor    ANONYMOUS_VARIABLE(ProfilerColor_##Name##_)(Color);


#else

class CORE_API FScopedNamedEvent
{
public:
	DEPRECATED(4.19, "FScopedNamedEvent is compiled out in shipping builds, use SCOPED_NAMED_EVENT or variant instead to compile correctly for all targets.")
	FScopedNamedEvent(const struct FColor& Color, const TCHAR* Text)
	{
	}

	DEPRECATED(4.19, "FScopedNamedEvent is compiled out in shipping builds, use SCOPED_NAMED_EVENT or variant instead to compile correctly for all targets.")
	FScopedNamedEvent(const struct FColor& Color, const ANSICHAR* Text)
	{
	}
};

class CORE_API FScopedNamedEventStatic
{
public:
	DEPRECATED(4.19, "FScopedNamedEventStatic is compiled out in shipping builds, use SCOPED_NAMED_EVENT or variant instead to compile correctly for all targets.")
	FScopedNamedEventStatic(const struct FColor& Color, const TCHAR* Text)
	{
	}

	DEPRECATED(4.19, "FScopedNamedEventStatic is compiled out in shipping builds, use SCOPED_NAMED_EVENT or variant instead to compile correctly for all targets.")
	FScopedNamedEventStatic(const struct FColor& Color, const ANSICHAR* Text)
	{
	}
};

#define SCOPED_NAMED_EVENT(...)
#define SCOPED_NAMED_EVENT_FSTRING(...)
#define SCOPED_NAMED_EVENT_TEXT(...)
#define SCOPED_NAMED_EVENT_F(...)
#define SCOPED_PROFILER_COLOR(...)

#endif
