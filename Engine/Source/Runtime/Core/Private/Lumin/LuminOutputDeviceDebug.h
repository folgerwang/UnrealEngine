// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
// Copyright 2017 Magic Leap, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/OutputDeviceDebug.h"

class CORE_API FLuminOutputDeviceDebug : public FOutputDeviceDebug
{
public:
	/**
	* Serializes the passed in data unless the current event is suppressed.
	*
	* @param	Data	Text to log
	* @param	Event	Event name used for suppression purposes
	*/
	virtual void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time) override;
};

