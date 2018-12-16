// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
// Copyright 2017 Magic Leap, Inc. All Rights Reserved.

#include "Lumin/LuminOutputDeviceDebug.h"
#include "Lumin/LuminPlatformMisc.h"
#include "CoreGlobals.h"
#include "Misc/OutputDeviceHelper.h"

void FLuminOutputDeviceDebug::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time )
{
	static bool Entry=false;
	if( !GIsCriticalError || Entry )
	{
		if (Verbosity != ELogVerbosity::SetColor)
		{
			FLuminPlatformMisc::LowLevelOutputDebugStringfWithVerbosity(Verbosity, TEXT("%s%s"),*FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Data, GPrintLogTimes, Time),LINE_TERMINATOR);
		}
	}
	else
	{
		Entry=true;
		FOutputDeviceDebug::Serialize( Data, Verbosity, Category );
		Entry=false;
	}
}
