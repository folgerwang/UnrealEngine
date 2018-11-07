// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkTimeSynchronizationSource.h"
#include "LiveLinkClient.h"
#include "Features/IModularFeatures.h"
#include "Math/NumericLimits.h"

ULiveLinkTimeSynchronizationSource::ULiveLinkTimeSynchronizationSource()
{
	if (!HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject))
	{
		IModularFeatures& ModularFeatures = IModularFeatures::Get();
		ModularFeatures.OnModularFeatureRegistered().AddUObject(this, &ThisClass::OnModularFeatureRegistered);
		ModularFeatures.OnModularFeatureUnregistered().AddUObject(this, &ThisClass::OnModularFeatureUnregistered);

		if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			LiveLinkClient = &ModularFeatures.GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		}
	}
}

FFrameTime ULiveLinkTimeSynchronizationSource::GetNewestSampleTime() const
{
	UpdateCachedState();
	return CachedData.NewestSampleTime + FrameOffset;
}

FFrameTime ULiveLinkTimeSynchronizationSource::GetOldestSampleTime() const
{
	UpdateCachedState();
	return CachedData.OldestSampleTime + FrameOffset;
}

FFrameRate ULiveLinkTimeSynchronizationSource::GetFrameRate() const
{
	UpdateCachedState();
	return CachedData.Settings.FrameRate;
}

bool ULiveLinkTimeSynchronizationSource::IsReady() const
{
	UpdateCachedState();
	return LiveLinkClient && CachedData.bIsValid && (ESyncState::NotSynced == State || LastUpdateGuid == CachedData.SkeletonGuid);
}

bool ULiveLinkTimeSynchronizationSource::Open(const FTimeSynchronizationOpenData& OpenData)
{
	UE_LOG(LogLiveLink, Log, TEXT("ULiveLinkTimeSynchronizationSource::Open %s"), *SubjectName.ToString());
	if (ensure(LiveLinkClient != nullptr) && IsReady())
	{
		State = ESyncState::Opened;
		LastUpdateGuid = CachedData.SkeletonGuid;
		LiveLinkClient->OnStartSynchronization(SubjectName, OpenData, FrameOffset);
		return true;
	}
	else
	{
		State = ESyncState::NotSynced;
		return false;
	}
}

void ULiveLinkTimeSynchronizationSource::Start(const FTimeSynchronizationStartData& StartData)
{
	UE_LOG(LogLiveLink, Log, TEXT("ULiveLinkTimeSynchronizationSource::Start %s"), *SubjectName.ToString());
	if (ensure(LiveLinkClient != nullptr))
	{
		State = ESyncState::Synced;
		LiveLinkClient->OnSynchronizationEstablished(SubjectName, StartData);
	}
	else
	{
		State = ESyncState::NotSynced;
	}
}

void ULiveLinkTimeSynchronizationSource::Close()
{
	UE_LOG(LogLiveLink, Log, TEXT("ULiveLinkTimeSynchronizationSource::Close %s"), *SubjectName.ToString());
	if (ensure(LiveLinkClient != nullptr))
	{
		LiveLinkClient->OnStopSynchronization(SubjectName);
	}

	State = ESyncState::NotSynced;
}

FString ULiveLinkTimeSynchronizationSource::GetDisplayName() const
{
	return SubjectName.ToString();
}

void ULiveLinkTimeSynchronizationSource::OnModularFeatureRegistered(const FName& FeatureName, class IModularFeature* Feature)
{
	if (FeatureName == ILiveLinkClient::ModularFeatureName)
	{
		LiveLinkClient = static_cast<FLiveLinkClient*>(Feature);
	}
}

void ULiveLinkTimeSynchronizationSource::OnModularFeatureUnregistered(const FName& FeatureName, class IModularFeature* Feature)
{
	if (FeatureName == ILiveLinkClient::ModularFeatureName && (LiveLinkClient != nullptr) && ensure(Feature == LiveLinkClient))
	{
		LiveLinkClient = nullptr;
	}
}

void ULiveLinkTimeSynchronizationSource::UpdateCachedState() const
{
	if (LastUpdateFrame != GFrameCounter && LiveLinkClient != nullptr)
	{
		LastUpdateFrame = GFrameCounter;
		CachedData = LiveLinkClient->GetTimeSyncData(SubjectName);
	}
}