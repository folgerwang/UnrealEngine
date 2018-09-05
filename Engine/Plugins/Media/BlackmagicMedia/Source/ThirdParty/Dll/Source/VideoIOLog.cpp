// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "stdafx.h"
#include <string>

#include "VideoIOLog.h"

/*
 * Global logging callbacks
 */

namespace BlackmagicDevice
{

	LoggingCallbackPtr GLogInfo = nullptr;
	LoggingCallbackPtr GLogWarning = nullptr;
	LoggingCallbackPtr GLogError = nullptr;

};