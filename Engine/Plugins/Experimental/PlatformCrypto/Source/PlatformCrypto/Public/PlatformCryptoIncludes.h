// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_XBOXONE
	#include "EncryptionContextBCrypt.h"
#elif PLATFORM_SWITCH
	#include "EncryptionContextSwitch.h"
#else
	#include "EncryptionContextOpenSSL.h"
#endif