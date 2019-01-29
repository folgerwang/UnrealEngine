// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapScreensComponent.h"
#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "RenderUtils.h"
#include "MagicLeapPluginUtil.h"
#include "MagicLeapHMD.h"
#include "MagicLeapUtils.h"
#include "IMagicLeapScreensPlugin.h"
#include "MagicLeapScreensPlugin.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Misc/CoreDelegates.h"
#include "MagicLeapScreensWorker.h"
#include "MagicLeapScreensMsg.h"
#if PLATFORM_LUMIN
#include "Lumin/LuminAffinity.h"
#endif // PLATFORM_LUMIN
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_image.h>
#include <ml_screens.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

DEFINE_LOG_CATEGORY_STATIC(LogScreensComponent, Display, All);

UScreensComponent::UScreensComponent()
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick = true;
}

UScreensComponent::~UScreensComponent()
{ }

void UScreensComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!FMagicLeapScreensPlugin::Impl->OutgoingMessages.IsEmpty())
	{
		FScreensMessage Msg;
		FMagicLeapScreensPlugin::Impl->OutgoingMessages.Dequeue(Msg);

		if (Msg.Type == EScreensMsgType::Request)
		{
			UE_LOG(LogScreensComponent, Error, TEXT("Unexpected EScreensMsgType::Request received from worker thread!"));
		}
		else if (Msg.Type == EScreensMsgType::Response)
		{
			switch (Msg.TaskType)
			{
			case EScreensTaskType::None: break;
			case EScreensTaskType::GetHistory:
			{
				if (Msg.bSuccess)
				{
					GetWatchHistorySuccess.Broadcast(Msg.WatchHistory);
				}
				else
				{
					GetWatchHistoryFailure.Broadcast();
				}
			}
			break;
			case EScreensTaskType::AddToHistory:
			{
				if (Msg.WatchHistory.Num() > 0)
				{
					FScreensWatchHistoryEntry Entry = Msg.WatchHistory[0];
					AddToWatchHistoryResult.Broadcast(Entry, Msg.bSuccess);
				}
				else
				{
					UE_LOG(LogScreensComponent, Error, TEXT("Unexpected empty watch history in an AddToHistory response from the worker thread"));
				}
			}
			break;
			case EScreensTaskType::UpdateEntry:
			{
				if (Msg.WatchHistory.Num() > 0)
				{
					FScreensWatchHistoryEntry Entry = Msg.WatchHistory[0];
					UpdateWatchHistoryEntryResult.Broadcast(Entry, Msg.bSuccess);
				}
				else
				{
					UE_LOG(LogScreensComponent, Error, TEXT("Unexpected empty watch history in an UpdateEntry response from the worker thread"));
				}
			}
			break;
			}
		}
	}
}

void UScreensComponent::GetWatchHistoryAsync()
{
	FMagicLeapScreensPlugin::GetWatchHistoryEntriesAsync(TOptional<FScreensHistoryRequestResultDelegate>());
}

void UScreensComponent::AddWatchHistoryEntryAsync(const FScreensWatchHistoryEntry& WatchHistoryEntry)
{
	FMagicLeapScreensPlugin::AddToWatchHistoryAsync(WatchHistoryEntry, TOptional<FScreensEntryRequestResultDelegate>());
}

void UScreensComponent::UpdateWatchHistoryEntryAsync(const FScreensWatchHistoryEntry& WatchHistoryEntry)
{
	FMagicLeapScreensPlugin::UpdateWatchHistoryEntryAsync(WatchHistoryEntry, TOptional<FScreensEntryRequestResultDelegate>());
}

bool UScreensComponent::RemoveWatchHistoryEntry(const FScreenID& ID)
{
	return FMagicLeapScreensPlugin::RemoveWatchHistoryEntry(ID);
}

bool UScreensComponent::ClearWatchHistory()
{
	return FMagicLeapScreensPlugin::ClearWatchHistory();
}

bool UScreensComponent::GetScreensTransforms(TArray<FScreenTransform>& ScreensTransforms)
{
#if WITH_MLSDK
	return FMagicLeapScreensPlugin::GetScreensTransforms(ScreensTransforms);
#endif // WITH_MLSDK
	return false;
}