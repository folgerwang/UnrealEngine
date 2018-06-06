// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ScreensComponent.h"
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Engine/Texture2D.h"
#include "Containers/Queue.h"
#include "Containers/Array.h"
#include "RenderUtils.h"
#include "MagicLeapPluginUtil.h"
#include "MagicLeapHMD.h"
#include "MagicLeapUtils.h"
#include "IMagicLeapScreensPlugin.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Misc/CoreDelegates.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogScreens, Display, All);

class FMagicLeapScreensPlugin : public IMagicLeapScreensPlugin
{
public:
	void StartupModule() override
	{
		IModuleInterface::StartupModule();
		APISetup.Startup();
		APISetup.LoadDLL(TEXT("ml_screens"));

		bEngineLoopInitComplete = false;
		FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FMagicLeapScreensPlugin::OnEngineLoopInitComplete);
	}

	void ShutdownModule() override
	{
		FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
		APISetup.Shutdown();
		IModuleInterface::ShutdownModule();
	}

	bool IsEngineLoopInitComplete() const override
	{
		return bEngineLoopInitComplete;
	}

	void OnEngineLoopInitComplete() override
	{
		bEngineLoopInitComplete = true;
	}

private:
	FMagicLeapAPISetup APISetup;
	bool bEngineLoopInitComplete;
};

IMPLEMENT_MODULE(FMagicLeapScreensPlugin, MagicLeapScreens);

//////////////////////////////////////////////////////////////////////////

#define MAX_TEXTURE_SIZE 450 * 450 * 4 // currently limited by binder implementation

enum ScreensMsgType
{
	Request,
	Response,
};

enum ScreensTaskType
{
	None,
	GetHistory,
};

struct FScreensMessage
{
	ScreensMsgType Type;
	ScreensTaskType TaskType;
	bool bSuccess;
	TArray<FScreensWatchHistoryEntry> WatchHistory;

	FScreensMessage()
		: Type(ScreensMsgType::Request)
		, TaskType(ScreensTaskType::None)
		, bSuccess(false)
	{}
};

class FScreensImpl : public FRunnable, public MagicLeap::IAppEventHandler
{
public:
	FScreensImpl()
		: Thread(nullptr)
		, StopTaskCounter(0)
		, Semaphore(nullptr)
	{
#if WITH_MLSDK
		DefaultThumbnail.width = 2;
		DefaultThumbnail.height = 2;
		DefaultThumbnail.image_type = MLImageType_RGBA32;
		DefaultThumbnail.alignment = 1;
		const SIZE_T DataSize = DefaultThumbnail.width * DefaultThumbnail.height * 4;
		uint8* Data = new uint8[DataSize];
		FMemory::Memset(Data, 255, DataSize);
		DefaultThumbnail.data = Data;
#endif //WITH_MLSDK

		PixelDataMemPool.Reserve(MAX_TEXTURE_SIZE);
	}

	~FScreensImpl()
	{
		StopTaskCounter.Increment();
		if (Semaphore != nullptr)
		{
			Semaphore->Trigger();
			Thread->WaitForCompletion();
			FGenericPlatformProcess::ReturnSynchEventToPool(Semaphore);
			Semaphore = nullptr;
			delete Thread;
			Thread = nullptr;
		}

#if WITH_MLSDK
		delete[] DefaultThumbnail.data;
		DefaultThumbnail.data = nullptr;
#endif //WITH_MLSDK
	}

	void EngineInited()
	{
		if (Semaphore == nullptr)
		{
			Semaphore = FGenericPlatformProcess::GetSynchEventFromPool(false);
#if PLATFORM_LUMIN
			Thread = FRunnableThread::Create(this, TEXT("FScreensWorker"), 0, TPri_BelowNormal, FLuminAffinity::GetPoolThreadMask());
#else
			Thread = FRunnableThread::Create(this, TEXT("FScreensWorker"), 0, TPri_BelowNormal);
#endif // PLATFORM_LUMIN
		}
		// wake up the worker to process the event
		Semaphore->Trigger();
	}

	virtual uint32 Run() override
	{
		while (StopTaskCounter.GetValue() == 0)
		{
			if (IncomingMessages.Dequeue(CurrentMessage))
			{
				DoScreensTasks();
			}

			Semaphore->Wait();
		}

		return 0;
	}

	void ProcessMessage(const FScreensMessage& InMsg)
	{
		IncomingMessages.Enqueue(InMsg);
		if (Semaphore != nullptr)
		{
			// wake up the worker to process the event
			Semaphore->Trigger();
		}
	}

	void DoScreensTasks()
	{
		switch (CurrentMessage.TaskType)
		{
		case ScreensTaskType::None: break;
		case ScreensTaskType::GetHistory: GetWatchHistory(); break;
		}
	}

	void GetWatchHistory()
	{
#if WITH_MLSDK
		FScreensMessage Msg;
		Msg.Type = ScreensMsgType::Response;
		Msg.TaskType = ScreensTaskType::GetHistory;
		MLScreensWatchHistoryList WatchHistoryList;

		if (MLScreensGetWatchHistoryList(&WatchHistoryList))
		{
			for (uint32 i = 0; i < WatchHistoryList.count; ++i)
			{
				FScreensWatchHistoryEntry WatchHistoryEntry;
				MLWatchHistoryEntryToUnreal(WatchHistoryList.entries[i], WatchHistoryEntry);
				Msg.WatchHistory.Add(WatchHistoryEntry);
			}
			MLScreensReleaseWatchHistoryList(&WatchHistoryList);
			Msg.bSuccess = true;
		}
		else
		{
			UE_LOG(LogScreens, Error, TEXT("MLScreensGetWatchHistoryList failed!"));
		}

		OutgoingMessages.Enqueue(Msg);
#endif //WITH_MLSDK
	}

#if WITH_MLSDK
	UTexture2D* MLImageToUTexture2D(const MLImage& Source)
	{
		UTexture2D* Thumbnail = UTexture2D::CreateTransient(Source.width, Source.height, EPixelFormat::PF_R8G8B8A8);
		// No need to add this UTexture2D to root as it is assigned to FScreensWatchHistoryEntry::Thumbnail, which is a UPROPERTY
		// Thumbnail->AddToRoot();
		Thumbnails.Add(Thumbnail);
		FTexture2DMipMap& Mip = Thumbnail->PlatformData->Mips[0];
		void* PixelData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		const uint32 PixelDataSize = Mip.BulkData.GetBulkDataSize();
		FMemory::Memcpy(PixelData, Source.data, PixelDataSize);
		UE_LOG(LogScreens, Log, TEXT("MLImageToUTexture2D width = %u height = %u size = %u"), Source.width, Source.height, PixelDataSize);
		Thumbnail->SRGB = true;
		Mip.BulkData.Unlock();
		Thumbnail->UpdateResource();

		return Thumbnail;
	}

	void MLWatchHistoryEntryToUnreal(const MLScreensWatchHistoryEntry& InEntry, FScreensWatchHistoryEntry& OutEntry)
	{
		FScreensWatchHistoryEntry WatchHistoryEntry;
		OutEntry.ID.ID = InEntry.id;
		OutEntry.Title = FString(UTF8_TO_TCHAR(InEntry.title));
		OutEntry.Subtitle = FString(UTF8_TO_TCHAR(InEntry.subtitle));
		OutEntry.PlaybackPosition = FTimespan(InEntry.playback_position_ms * ETimespan::TicksPerMillisecond);
		OutEntry.PlaybackDuration = FTimespan(InEntry.playback_duration_ms * ETimespan::TicksPerMillisecond);
		OutEntry.CustomData = FString(UTF8_TO_TCHAR(InEntry.custom_data));

		MLImage MLThumbnail;

		if (!MLScreensGetWatchHistoryThumbnail(InEntry.id, &MLThumbnail))
		{
			UE_LOG(LogScreens, Error, TEXT("Failed to get thumbnail for screen ID %u"), (uint32)InEntry.id);
			MLThumbnail = DefaultThumbnail;
		}

		OutEntry.Thumbnail = MLImageToUTexture2D(MLThumbnail);
	}
#endif //WITH_MLSDK

	bool IsSupportedFormat(EPixelFormat InPixelFormat)
	{
		if (InPixelFormat == PF_B8G8R8A8 || InPixelFormat == PF_R8G8B8A8)
		{
			return true;
		}

		UE_LOG(LogScreens, Error, TEXT("Unsupported pixel format!"));// '%s'!"), GetPixelFormatString(InPixelFormat));

		return false;
	}

#if WITH_MLSDK
	bool UTexture2DToMLImage(const UTexture2D& Source, MLImage& Target)
	{
		bool bSuccess = false;
		FTexture2DMipMap& Mip = Source.PlatformData->Mips[0];
		void* PixelData = Mip.BulkData.Lock(LOCK_READ_ONLY);
		const int32 size = Mip.BulkData.GetBulkDataSize();

		if (size <= MAX_TEXTURE_SIZE)
		{
			UE_LOG(LogScreens, Log, TEXT("UTexture2DToMLImage width = %d height = %d size = %d"), Mip.SizeX, Mip.SizeY, size);
			Target.width = Mip.SizeX;
			Target.height = Mip.SizeY;
			Target.image_type = MLImageType_RGBA32;
			Target.alignment = 1;
			Target.data = PixelDataMemPool.GetData();
			FMemory::Memcpy(Target.data, PixelData, size);

			if (Source.GetPixelFormat() == EPixelFormat::PF_B8G8R8A8)
			{
				check((size % 4) == 0);

				for (int32 i = 0; i < size - 4; i += 4)
				{
					Swap<uint8>(Target.data[i], Target.data[i+2]);
				}
			}

			bSuccess = true;
		}
		else
		{
			UE_LOG(LogScreens, Error, TEXT("Texture size (%d) exceeds max texture size (%d)"), size, MAX_TEXTURE_SIZE);
		}

		Mip.BulkData.Unlock();

		return bSuccess;
	}
#endif //WITH_MLSDK

	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;
	TQueue<FScreensMessage, EQueueMode::Spsc> IncomingMessages;
	TQueue<FScreensMessage, EQueueMode::Spsc> OutgoingMessages;
	FEvent* Semaphore;
	FScreensMessage CurrentMessage;
#if WITH_MLSDK
	MLImage DefaultThumbnail;
#endif //WITH_MLSDK
	TArray<UTexture2D*> Thumbnails;
	TArray<uint8> PixelDataMemPool;
};

UScreensComponent::UScreensComponent()
	: Impl(new FScreensImpl())
	, bWorkerBusy(false)
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick = true;
}

UScreensComponent::~UScreensComponent()
{
#if PLATFORM_LUMIN
	if (Impl)
	{
		Impl->AsyncDestroy();
		Impl = nullptr;
	}
#else
	delete Impl;
	Impl = nullptr;
#endif // PLATFORM_LUMIN	
}

void UScreensComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Impl->Semaphore == nullptr)
	{
		if (IMagicLeapScreensPlugin::Get().IsEngineLoopInitComplete())
		{
			Impl->EngineInited();
		}
	}

	if (!Impl->OutgoingMessages.IsEmpty())
	{
		FScreensMessage Msg;
		Impl->OutgoingMessages.Dequeue(Msg);
		bWorkerBusy = false;

		if (Msg.Type == ScreensMsgType::Request)
		{
			UE_LOG(LogScreens, Error, TEXT("Unexpected ScreensMsgType::Request received from worker thread!"));
		}
		else if (Msg.Type == ScreensMsgType::Response)
		{
			switch (Msg.TaskType)
			{
			case ScreensTaskType::None: break;
			case ScreensTaskType::GetHistory:
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
			}
		}
	}
}

UScreensComponent::FScreensGetWatchHistorySuccess& UScreensComponent::OnScreensGetWatchHistorySuccess()
{
	return GetWatchHistorySuccess;
}

UScreensComponent::FScreensGetWatchHistoryFailure& UScreensComponent::OnScreensGetWatchHistoryFailure()
{
	return GetWatchHistoryFailure;
}

bool UScreensComponent::GetWatchHistoryAsync()
{
	if (!bWorkerBusy)
	{
		bWorkerBusy = true;
		FScreensMessage Msg;
		Msg.Type = ScreensMsgType::Request;
		Msg.TaskType = ScreensTaskType::GetHistory;
		Impl->ProcessMessage(Msg);
		return true;
	}

	UE_LOG(LogScreens, Warning, TEXT("Worker thread is already busy!"));
	return false;
}

bool UScreensComponent::AddWatchHistoryEntry(const FScreensWatchHistoryEntry& WatchHistoryEntry, FScreenID& ID)
{
#if WITH_MLSDK
	MLScreensWatchHistoryEntry Entry;
	Entry.id = WatchHistoryEntry.ID.ID;
	Entry.title = TCHAR_TO_UTF8(*WatchHistoryEntry.Title);
	Entry.subtitle = TCHAR_TO_UTF8(*WatchHistoryEntry.Subtitle);
	Entry.playback_position_ms = static_cast<uint32>(WatchHistoryEntry.PlaybackPosition.GetTotalMilliseconds());
	Entry.playback_duration_ms = static_cast<uint32>(WatchHistoryEntry.PlaybackDuration.GetTotalMilliseconds());
	Entry.custom_data = TCHAR_TO_UTF8(*WatchHistoryEntry.CustomData);
	MLImage MLThumbnail;

	if (!WatchHistoryEntry.Thumbnail ||
		!Impl->IsSupportedFormat(WatchHistoryEntry.Thumbnail->GetPixelFormat()) ||
		!Impl->UTexture2DToMLImage(*WatchHistoryEntry.Thumbnail, MLThumbnail))
	{
		MLThumbnail = Impl->DefaultThumbnail;
	}

	if (MLScreensInsertWatchHistoryEntry(&Entry, &MLThumbnail))
	{
		ID.ID = Entry.id;
		return true;
	}

	UE_LOG(LogScreens, Error, TEXT("MLScreensInsertWatchHistoryEntry failed!"));
#endif //WITH_MLSDK
	return false;
}

bool UScreensComponent::UpdateWatchHistoryEntry(const FScreensWatchHistoryEntry& WatchHistoryEntry)
{
#if WITH_MLSDK
	MLScreensWatchHistoryEntry Entry;
	Entry.id = WatchHistoryEntry.ID.ID;
	Entry.title = TCHAR_TO_UTF8(*WatchHistoryEntry.Title);
	Entry.subtitle = TCHAR_TO_UTF8(*WatchHistoryEntry.Subtitle);
	Entry.playback_position_ms = static_cast<uint32>(WatchHistoryEntry.PlaybackPosition.GetTotalMilliseconds());
	Entry.playback_duration_ms = static_cast<uint32>(WatchHistoryEntry.PlaybackDuration.GetTotalMilliseconds());
	Entry.custom_data = TCHAR_TO_UTF8(*WatchHistoryEntry.CustomData);
	MLImage MLThumbnail;

	if (!WatchHistoryEntry.Thumbnail ||
		!Impl->IsSupportedFormat(WatchHistoryEntry.Thumbnail->GetPixelFormat()) ||
		!Impl->UTexture2DToMLImage(*WatchHistoryEntry.Thumbnail, MLThumbnail))
	{
		MLThumbnail = Impl->DefaultThumbnail;
	}

	if (MLScreensUpdateWatchHistoryEntry(&Entry, &MLThumbnail))
	{
		return true;
	}

	UE_LOG(LogScreens, Error, TEXT("MLScreensUpdateWatchHistoryEntry failed!"));
#endif //WITH_MLSDK
	return false;
}

bool UScreensComponent::RemoveWatchHistoryEntry(const FScreenID& ID)
{
#if WITH_MLSDK
	return MLScreensRemoveWatchHistoryEntry(ID.ID);
#else
	return false;
#endif //WITH_MLSDK
}

bool UScreensComponent::ClearWatchHistory()
{
	bool bResult = false;
#if WITH_MLSDK
	MLScreensWatchHistoryList WatchHistoryList;
	bResult = MLScreensGetWatchHistoryList(&WatchHistoryList);

	if (bResult)
	{
		for (uint32 i = 0; i < WatchHistoryList.count; ++i)
		{
			MLScreensRemoveWatchHistoryEntry(WatchHistoryList.entries[i].id);
		}

		MLScreensReleaseWatchHistoryList(&WatchHistoryList);
	}
#endif //WITH_MLSDK

	return bResult;
}

bool UScreensComponent::GetScreensTransforms(TArray<FScreenTransform>& ScreensTransforms)
{
	bool bResult = false;
#if WITH_MLSDK
	ScreensTransforms.Empty();

	const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
	if (!AppFramework.IsInitialized())
	{
		return false;
	}
	float WorldToMetersScale = AppFramework.GetWorldToMetersScale();

	MLScreensScreenInfoList ScreensInfoList;
	bResult = MLScreensGetScreenInfoList(&ScreensInfoList);
	if (bResult)
	{
		FTransform PoseTransform = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this);
		for (uint32 i = 0; i < ScreensInfoList.count; ++i)
		{
			MLScreensScreenInfo& Entry = ScreensInfoList.entries[i];
			FScreenTransform ScreenTransform;

			FTransform screen = FTransform(MagicLeap::ToFQuat(Entry.transform.rotation), MagicLeap::ToFVector(Entry.transform.position, WorldToMetersScale), FVector(1.0f, 1.0f, 1.0f));
			if (!screen.GetRotation().IsNormalized())
			{
				FQuat rotation = screen.GetRotation();
				rotation.Normalize();
				screen.SetRotation(rotation);
			}
			screen.AddToTranslation(PoseTransform.GetLocation());
			screen.ConcatenateRotation(PoseTransform.Rotator().Quaternion());

			ScreenTransform.ScreenPosition = screen.GetLocation();
			ScreenTransform.ScreenOrientation = screen.Rotator();
			ScreenTransform.ScreenDimensions = MagicLeap::ToFVector(Entry.scale, WorldToMetersScale);
			ScreenTransform.ScreenDimensions.X = FMath::Abs<float>(ScreenTransform.ScreenDimensions.X);
			ScreenTransform.ScreenDimensions.Y = FMath::Abs<float>(ScreenTransform.ScreenDimensions.Y);
			ScreenTransform.ScreenDimensions.Z = FMath::Abs<float>(ScreenTransform.ScreenDimensions.Z);

			ScreensTransforms.Add(ScreenTransform);
		}
		MLScreensReleaseScreenInfoList(&ScreensInfoList);
	}
#endif //WITH_MLSDK
	return bResult;
}