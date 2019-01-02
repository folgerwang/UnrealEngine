// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AISubsystem.h"
#include "AISystem.h"
#include "Misc/App.h"


DEFINE_LOG_CATEGORY_STATIC(LogAISub, Log, All);

UAISubsystem::UAISubsystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		AISystem = Cast<UAISystem>(GetOuter());
		UE_CLOG(AISystem == nullptr, LogAISub, Error, TEXT("%s is an invalid outer for UAISubsystem instance %s")
			, *GetName(), *GetNameSafe(GetOuter()));
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (AISystem == nullptr 
			&& GetOuter()
			&& (IsRunningCommandlet() 
				|| (GIsEditor && GetWorld() && GetWorld()->WorldType == EWorldType::Editor)))
		{
			// not calling MarkPackageDirty on this because it might be marked as transient 
			GetOuter()->MarkPackageDirty();
		}		
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	}
}

UWorld* UAISubsystem::GetWorld() const
{
	return GetWorldFast();
}

ETickableTickType UAISubsystem::GetTickableTickType() const
{
	return (HasAnyFlags(RF_ClassDefaultObject) || AISystem == nullptr)
		? ETickableTickType::Never
		: ETickableTickType::Always;
}

TStatId UAISubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAISubsystem, STATGROUP_Tickables);
}
