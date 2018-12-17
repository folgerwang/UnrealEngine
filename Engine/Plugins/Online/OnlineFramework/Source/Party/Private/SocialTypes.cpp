// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SocialTypes.h"
#include "OnlineSubsystemUtils.h"

//////////////////////////////////////////////////////////////////////////
// FUserPlatform
//////////////////////////////////////////////////////////////////////////

bool FUserPlatform::operator==(const FString& OtherStr) const
{
	return PlatformStr == OtherStr;
}

bool FUserPlatform::operator==(const FUserPlatform& Other) const
{
	return PlatformStr == Other.PlatformStr;
}

bool FUserPlatform::IsValid() const
{
	return !PlatformStr.IsEmpty();
}

bool FUserPlatform::IsDesktop() const
{
	return PlatformStr == OSS_PLATFORM_NAME_WINDOWS || PlatformStr == OSS_PLATFORM_NAME_MAC || PlatformStr == OSS_PLATFORM_NAME_LINUX;
}

bool FUserPlatform::IsMobile() const
{
	return PlatformStr == OSS_PLATFORM_NAME_ANDROID || PlatformStr == OSS_PLATFORM_NAME_IOS;
}

bool FUserPlatform::IsConsole() const
{
	return PlatformStr == OSS_PLATFORM_NAME_XBOX || PlatformStr == OSS_PLATFORM_NAME_PS4 || PlatformStr == OSS_PLATFORM_NAME_SWITCH;
}

bool FUserPlatform::RequiresCrossplayOptIn() const
{
	return PlatformStr == OSS_PLATFORM_NAME_XBOX || PlatformStr == OSS_PLATFORM_NAME_PS4;
}

bool FUserPlatform::IsCrossplayWith(const FUserPlatform& OtherPlatform) const
{
	if (*this != OtherPlatform)
	{
		// Any difference in platform qualifies as crossplay for a console platform
		// Desktops and mobile aren't considered crossplay within themselves (i.e. Android+iOS or Mac+PC don't count)
		return IsConsole() || IsDesktop() != OtherPlatform.IsDesktop() || IsMobile() != OtherPlatform.IsMobile();
	}
	return false;
}

bool FUserPlatform::IsCrossplayWith(const FString& OtherPlatformStr) const
{
	return IsCrossplayWith(FUserPlatform(OtherPlatformStr));
}

bool FUserPlatform::IsCrossplayWithLocalPlatform() const
{
	return IsCrossplayWith(IOnlineSubsystem::GetLocalPlatformName());
}

//////////////////////////////////////////////////////////////////////////
// FSocialActionTimeTracker
//////////////////////////////////////////////////////////////////////////

void FSocialActionTimeTracker::BeginStep(FName StepName)
{
	UE_LOG(LogParty, VeryVerbose, TEXT("Beginning social action step [%s]"), *StepName.ToString());

	FSocialActionStep& NewStep = ActionSteps[ActionSteps.Add(FSocialActionStep())];
	NewStep.StepName = StepName;
}

void FSocialActionTimeTracker::CompleteStep(FName StepName)
{
	//@todo DanH Social: Enforce that this is the currently running step? #suggested
	if (FSocialActionStep* Step = ActionSteps.FindByKey(StepName))
	{
		Step->EndTime = FPlatformTime::Seconds();

		UE_LOG(LogParty, VeryVerbose, TEXT("Finished social action step [%s] in %.2fs"), *StepName.ToString(), Step->GetDurationMs());
	}
}

double FSocialActionTimeTracker::GetActionStartTime() const
{
	return ActionSteps.Num() > 0 ? ActionSteps[0].StartTime : 0.0;
}

double FSocialActionTimeTracker::GetTotalDurationMs() const
{
	double TotalDuration = 0.0;
	for (const FSocialActionStep& Step : ActionSteps)
	{
		TotalDuration += Step.GetDurationMs();
	}
	return TotalDuration;
}

FName FSocialActionTimeTracker::GetCurrentStepName() const
{
	return ActionSteps.Num() > 0 ? ActionSteps.Last().StepName : NAME_None;
}

double FSocialActionTimeTracker::GetStepDurationMs(FName StepName) const
{
	if (const FSocialActionStep* Step = ActionSteps.FindByKey(StepName))
	{
		return Step->GetDurationMs();
	}
	return 0.0;
}

double FSocialActionTimeTracker::FSocialActionStep::GetDurationMs() const
{
	double TotalSeconds = (EndTime != 0.0 ? EndTime : FPlatformTime::Seconds()) - StartTime;
	return TotalSeconds * 1000.0;
}
