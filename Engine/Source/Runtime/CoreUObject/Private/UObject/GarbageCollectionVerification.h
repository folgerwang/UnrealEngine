// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GarbageCollectionVerification.h: Unreal realtime garbage collection helpers
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

// UE_BUILD_SHIPPING has GShouldVerifyGCAssumptions=false by default
#define VERIFY_DISREGARD_GC_ASSUMPTIONS			!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if VERIFY_DISREGARD_GC_ASSUMPTIONS

/** Verifies Disregard for GC assumptions */
void VerifyGCAssumptions();

/** Verifies GC Cluster assumptions */
void VerifyClustersAssumptions();

#endif // VERIFY_DISREGARD_GC_ASSUMPTIONS
