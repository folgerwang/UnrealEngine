// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LuminARFunctionLibrary.h"
#include "UnrealEngine.h"
#include "Engine/Engine.h"
#include "LatentActions.h"
#include "ARBlueprintLibrary.h"

#include "LuminARDevice.h"
#include "LuminARTrackingSystem.h"


struct FLuminARStartSessionAction : public FPendingLatentAction
{
public:
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	FLuminARStartSessionAction(const FLatentActionInfo& InLatentInfo)
		: FPendingLatentAction()
		, ExecutionFunction(InLatentInfo.ExecutionFunction)
		, OutputLink(InLatentInfo.Linkage)
		, CallbackTarget(InLatentInfo.CallbackTarget)
	{}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		bool bSessionStartedFinished = FLuminARDevice::GetInstance()->GetStartSessionRequestFinished();
		Response.FinishAndTriggerIf(bSessionStartedFinished, ExecutionFunction, OutputLink, CallbackTarget);
	}
#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return FString::Printf(TEXT("Starting LuminAR tracking session"));
	}
#endif
};

void ULuminARSessionFunctionLibrary::StartLuminARSession(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, ULuminARSessionConfig* Configuration)
{
	UE_LOG(LogTemp, Log, TEXT("ULuminARSessionFunctionLibrary::StartLuminARSession"));
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentManager = World->GetLatentActionManager();
		if (LatentManager.FindExistingAction<FLuminARStartSessionAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			UARBlueprintLibrary::StartARSession(static_cast<UARSessionConfig*>(Configuration));
			FLuminARStartSessionAction* NewAction = new FLuminARStartSessionAction(LatentInfo);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
	}
}

/************************************************************************/
/*  ULuminARFrameFunctionLibrary                                   */
/************************************************************************/
ELuminARTrackingState ULuminARFrameFunctionLibrary::GetTrackingState()
{
	return FLuminARDevice::GetInstance()->GetTrackingState();
}

//void ULuminARFrameFunctionLibrary::GetPose(FTransform& LastePose)
//{
//	LastePose = FLuminARDevice::GetInstance()->GetLatestPose();
//}

bool ULuminARFrameFunctionLibrary::LuminARLineTrace(UObject* WorldContextObject, const FVector2D& ScreenPosition, TSet<ELuminARLineTraceChannel> TraceChannels, TArray<FARTraceResult>& OutHitResults)
{
	ELuminARLineTraceChannel TraceChannelValue = ELuminARLineTraceChannel::None;
	for (ELuminARLineTraceChannel Channel : TraceChannels)
	{
		TraceChannelValue = TraceChannelValue | Channel;
	}

	FLuminARDevice::GetInstance()->ARLineTrace(ScreenPosition, TraceChannelValue, OutHitResults);
	return OutHitResults.Num() > 0;
}

void ULuminARFrameFunctionLibrary::GetLightEstimation(FLuminARLightEstimate& LightEstimation)
{
	LightEstimation = FLuminARDevice::GetInstance()->GetLatestLightEstimate();
}

