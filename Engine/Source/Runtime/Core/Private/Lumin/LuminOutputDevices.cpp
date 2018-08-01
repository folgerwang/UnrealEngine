// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
// Copyright 2017 Magic Leap, Inc. All Rights Reserved.

#include "Lumin/LuminOutputDevices.h"
#include "Lumin/LuminOutputDeviceDebug.h"
#include "CoreGlobals.h"
#include "Misc/Parse.h"
#include "Templates/ScopedPointer.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceMemory.h"
#include "Misc/OutputDeviceFile.h"
#include "Misc/OutputDeviceDebug.h"
#include "Misc/OutputDeviceAnsiError.h"
#include "Misc/App.h"
#include "HAL/FeedbackContextAnsi.h"
#include "Misc/OutputDeviceConsole.h"

void FLuminOutputDevices::SetupOutputDevices()
{
	check(GLog);

	CachedAbsoluteFilename[0] = 0;

	GLog->AddOutputDevice(FPlatformOutputDevices::GetLog());

#if !NO_LOGGING
	// if console is enabled add an output device, unless the commandline says otherwise...
	if (GLogConsole && !FParse::Param(FCommandLine::Get(), TEXT("NOCONSOLE")))
	{
		GLog->AddOutputDevice(GLogConsole);
	}
	
	// If the platform has a separate debug output channel (e.g. OutputDebugString) then add an output device
	// unless logging is turned off
	if (FPlatformMisc::HasSeparateChannelForDebugOutput())
	{
        // Use FLuminOutputDeviceDebug instead of the default FOutputDeviceDebug so that ml_log can respect the verbosity in the output
		GLog->AddOutputDevice(new FLuminOutputDeviceDebug());
	}
#endif

	GLog->AddOutputDevice(FPlatformOutputDevices::GetEventLog());
}
