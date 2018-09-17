// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "targetver.h"
#include "assert.h"

#include <string>
#include <sstream>

#include <memory>
#include <array>
#include <vector>

// Windows Header Files:
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <comutil.h>
#include "../DeckLinkAPI_h.h"

#ifdef _DEBUG
#define VideoIOCHECK(FUNCTION) if (!FUNCTION) { *reinterpret_cast<char*>(0) = 0; }
#define ComCheck(FUNCTION) if ((FUNCTION) != S_OK) { *reinterpret_cast<char*>(0) = 0; }
#else
#define VideoIOCHECK(FUNCTION) (FUNCTION)
#define ComCheck(FUNCTION) (FUNCTION)
#endif

#include "Thread.h"
#include "RefCount.h"

#include "BlackmagicLib.h"
#include "VideoIOPrivate.h"

#include "VideoIOLog.h"

