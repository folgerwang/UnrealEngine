// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Net/NetworkGranularMemoryLogging.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#include "Serialization/ArchiveCountMem.h"
#include "EngineLogs.h"
#include "HAL/IConsoleManager.h"

namespace GranularNetworkMemoryTrackingPrivate
{
	static TAutoConsoleVariable<int32> CVarUseGranularNetworkTracking(
		TEXT("Net.UseGranularNetworkTracking"),
		0,
		TEXT("When enabled, Obj List will print out highly detailed information about Network Memory Usage")
	);

	FHelper::FHelper(FArchive& InAr, FString&& InScopeName) :
		Ar(InAr),
		ScopeName(InScopeName),
		bShouldTrack(CVarUseGranularNetworkTracking.GetValueOnAnyThread() && FString(TEXT("FArchiveCountMem")).Equals(Ar.GetArchiveName()))
	{
	}

	void FHelper::BeginWork()
	{
		if (bShouldTrack)
		{
			PreWorkPos = ((FArchiveCountMem&)Ar).GetMax();
		}
	}

	void FHelper::EndWork(const FString& WorkName)
	{
		if (bShouldTrack)
		{
			UE_LOG(LogNet, Log, TEXT("%s: %s is %llu bytes"), *ScopeName, *WorkName, ((FArchiveCountMem&)Ar).GetMax() - PreWorkPos);
		}
	}
};

#endif