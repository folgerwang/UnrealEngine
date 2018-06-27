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
#include "IMagicLeapPlugin.h"
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
	AddToHistory,
	UpdateEntry,
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
		case ScreensTaskType::AddToHistory: AddToHistory();  break;
		case ScreensTaskType::UpdateEntry: UpdateWatchHistoryEntry(); break;
		}
	}

	void AddToHistory()
	{
#if WITH_MLSDK
		check(CurrentMessage.WatchHistory.Num() != 0);
		FScreensWatchHistoryEntry WatchHistoryEntry = CurrentMessage.WatchHistory[0];
		FScreensMessage Msg;
		Msg.Type = ScreensMsgType::Response;
		Msg.TaskType = ScreensTaskType::AddToHistory;

		MLScreensWatchHistoryEntry Entry;
		Entry.id = WatchHistoryEntry.ID.ID;
		Entry.title = TCHAR_TO_UTF8(*WatchHistoryEntry.Title);
		Entry.subtitle = TCHAR_TO_UTF8(*WatchHistoryEntry.Subtitle);
		Entry.playback_position_ms = static_cast<uint32>(WatchHistoryEntry.PlaybackPosition.GetTotalMilliseconds());
		Entry.playback_duration_ms = static_cast<uint32>(WatchHistoryEntry.PlaybackDuration.GetTotalMilliseconds());
		Entry.custom_data = TCHAR_TO_UTF8(*WatchHistoryEntry.CustomData);
		MLImage MLThumbnail;

		if (!WatchHistoryEntry.Thumbnail ||
			!IsSupportedFormat(WatchHistoryEntry.Thumbnail->GetPixelFormat()) ||
			!UTexture2DToMLImage(*WatchHistoryEntry.Thumbnail, MLThumbnail))
		{
			MLThumbnail = DefaultThumbnail;
		}

		MLResult Result = MLScreensInsertWatchHistoryEntry(&Entry, &MLThumbnail);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogScreens, Error, TEXT("MLScreensInsertWatchHistoryEntry failed with error %d!"), Result);
			Msg.WatchHistory.Add(WatchHistoryEntry);
		}
		else
		{
			FScreensWatchHistoryEntry NewEntry;
			MLWatchHistoryEntryToUnreal(Entry, NewEntry);
			Msg.WatchHistory.Add(NewEntry);
			Msg.bSuccess = true;
		}

		OutgoingMessages.Enqueue(Msg);
#endif // WITH_MLSDK
	}

	void UpdateWatchHistoryEntry()
	{
#if WITH_MLSDK
		check(CurrentMessage.WatchHistory.Num() != 0);
		FScreensWatchHistoryEntry WatchHistoryEntry = CurrentMessage.WatchHistory[0];
		FScreensMessage Msg;
		Msg.Type = ScreensMsgType::Response;
		Msg.TaskType = ScreensTaskType::UpdateEntry;
		MLScreensWatchHistoryEntry Entry;

		Entry.id = WatchHistoryEntry.ID.ID;
		Entry.title = TCHAR_TO_UTF8(*WatchHistoryEntry.Title);
		Entry.subtitle = TCHAR_TO_UTF8(*WatchHistoryEntry.Subtitle);
		Entry.playback_position_ms = static_cast<uint32>(WatchHistoryEntry.PlaybackPosition.GetTotalMilliseconds());
		Entry.playback_duration_ms = static_cast<uint32>(WatchHistoryEntry.PlaybackDuration.GetTotalMilliseconds());
		Entry.custom_data = TCHAR_TO_UTF8(*WatchHistoryEntry.CustomData);
		MLImage MLThumbnail;

		if (!WatchHistoryEntry.Thumbnail ||
			!IsSupportedFormat(WatchHistoryEntry.Thumbnail->GetPixelFormat()) ||
			!UTexture2DToMLImage(*WatchHistoryEntry.Thumbnail, MLThumbnail))
		{
			MLThumbnail = DefaultThumbnail;
		}

		MLResult Result = MLScreensUpdateWatchHistoryEntry(&Entry, &MLThumbnail);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogScreens, Error, TEXT("MLScreensUpdateWatchHistoryEntry failed with error %d!"), Result);
			Msg.WatchHistory.Add(WatchHistoryEntry);
		}
		else
		{
			FScreensWatchHistoryEntry UpdatedEntry;
			MLWatchHistoryEntryToUnreal(Entry, UpdatedEntry);
			Msg.WatchHistory.Add(UpdatedEntry);
			Msg.bSuccess = true;
		}

		OutgoingMessages.Enqueue(Msg);
#endif // WITH_MLSDK
	}

	void GetWatchHistory()
	{
#if WITH_MLSDK
		FScreensMessage Msg;
		Msg.Type = ScreensMsgType::Response;
		Msg.TaskType = ScreensTaskType::GetHistory;
		MLScreensWatchHistoryList WatchHistoryList;

		MLResult Result = MLScreensGetWatchHistoryList(&WatchHistoryList);
		if (Result == MLResult_Ok)
		{
			for (uint32 i = 0; i < WatchHistoryList.count; ++i)
			{
				FScreensWatchHistoryEntry WatchHistoryEntry;
				MLWatchHistoryEntryToUnreal(WatchHistoryList.entries[i], WatchHistoryEntry);
				Msg.WatchHistory.Add(WatchHistoryEntry);
			}
			Result = MLScreensReleaseWatchHistoryList(&WatchHistoryList);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogScreens, Error, TEXT("MLScreensReleaseWatchHistoryList failed with error %d!"), Result);
			}
			Msg.bSuccess = true;
		}
		else
		{
			UE_LOG(LogScreens, Error, TEXT("MLScreensGetWatchHistoryList failed with error %d!"), Result);
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
		MLResult Result = MLScreensGetWatchHistoryThumbnail(InEntry.id, &MLThumbnail);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogScreens, Error, TEXT("MLScreensGetWatchHistoryThumbnail failed for screen ID %u with error %d!"), (uint32)InEntry.id, Result);
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
			case ScreensTaskType::AddToHistory:
			{
				if (Msg.WatchHistory.Num() > 0)
				{
					FScreensWatchHistoryEntry Entry = Msg.WatchHistory[0];
					AddToWatchHistoryResult.Broadcast(Entry, Msg.bSuccess);
				}
				else
				{
					UE_LOG(LogScreens, Error, TEXT("Unexpected empty watch history in an AddToHistory response from the worker thread"));
				}
			}
			break;
			case ScreensTaskType::UpdateEntry:
			{
				if (Msg.WatchHistory.Num() > 0)
				{
					FScreensWatchHistoryEntry Entry = Msg.WatchHistory[0];
					UpdateWatchHistoryEntryResult.Broadcast(Entry, Msg.bSuccess);
				}
				else
				{
					UE_LOG(LogScreens, Error, TEXT("Unexpected empty watch history in an UpdateEntry response from the worker thread"));
				}
			}
			break;
			}
		}
	}
}

void UScreensComponent::GetWatchHistoryAsync()
{
	FScreensMessage Msg;
	Msg.Type = ScreensMsgType::Request;
	Msg.TaskType = ScreensTaskType::GetHistory;
	Impl->ProcessMessage(Msg);
}

void UScreensComponent::AddWatchHistoryEntryAsync(const FScreensWatchHistoryEntry WatchHistoryEntry)
{
	FScreensMessage Msg;
	Msg.Type = ScreensMsgType::Request;
	Msg.TaskType = ScreensTaskType::AddToHistory;
	Msg.WatchHistory.Add(WatchHistoryEntry);
	Impl->ProcessMessage(Msg);
}

void UScreensComponent::UpdateWatchHistoryEntryAsync(const FScreensWatchHistoryEntry WatchHistoryEntry)
{
	FScreensMessage Msg;
	Msg.Type = ScreensMsgType::Request;
	Msg.TaskType = ScreensTaskType::UpdateEntry;
	Msg.WatchHistory.Add(WatchHistoryEntry);
	Impl->ProcessMessage(Msg);
}

bool UScreensComponent::RemoveWatchHistoryEntry(const FScreenID& ID)
{
#if WITH_MLSDK
	MLResult Result = MLScreensRemoveWatchHistoryEntry(ID.ID);
	UE_CLOG(Result != MLResult_Ok, LogScreens, Error, TEXT("MLScreensRemoveWatchHistoryEntry failed with error %d for entry with id %d!"), Result, ID.ID);
	return Result == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UScreensComponent::ClearWatchHistory()
{
	bool bSuccess = false;
#if WITH_MLSDK
	MLScreensWatchHistoryList WatchHistoryList;
	MLResult Result = MLScreensGetWatchHistoryList(&WatchHistoryList);
	bSuccess = true;

	if (Result == MLResult_Ok)
	{
		for (uint32 i = 0; i < WatchHistoryList.count; ++i)
		{
			Result = MLScreensRemoveWatchHistoryEntry(WatchHistoryList.entries[i].id);
			if (Result != MLResult_Ok)
			{
				bSuccess = false;
				UE_LOG(LogScreens, Error, TEXT("MLScreensRemoveWatchHistoryEntry failed with error %d for entry %d!"), Result, WatchHistoryList.entries[i].id);
			}
		}

		Result = MLScreensReleaseWatchHistoryList(&WatchHistoryList);
		if (Result != MLResult_Ok)
		{
			bSuccess = false;
			UE_LOG(LogScreens, Error, TEXT("MLScreensReleaseWatchHistoryList failed with error %d!"), Result);
		}
	}
	else
	{
		bSuccess = false;
		UE_LOG(LogScreens, Error, TEXT("MLScreensGetWatchHistoryList failed with error %d!"), Result);
	}
#endif //WITH_MLSDK

	return bSuccess;
}

bool UScreensComponent::GetScreensTransforms(TArray<FScreenTransform>& ScreensTransforms)
{
	bool bResult = false;
#if WITH_MLSDK
	ScreensTransforms.Empty();

	if (!IMagicLeapPlugin::Get().IsMagicLeapHMDValid())
	{
		return false;
	}

	const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
	if (!AppFramework.IsInitialized())
	{
		return false;
	}
	float WorldToMetersScale = AppFramework.GetWorldToMetersScale();

	MLScreensScreenInfoList ScreensInfoList;
	MLResult Result = MLScreensGetScreenInfoList(&ScreensInfoList);
	bResult = (Result == MLResult_Ok);
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
			ScreenTransform.ScreenDimensions = MagicLeap::ToFVector(Entry.dimensions, WorldToMetersScale);
			ScreenTransform.ScreenDimensions.X = FMath::Abs<float>(ScreenTransform.ScreenDimensions.X);
			ScreenTransform.ScreenDimensions.Y = FMath::Abs<float>(ScreenTransform.ScreenDimensions.Y);
			ScreenTransform.ScreenDimensions.Z = FMath::Abs<float>(ScreenTransform.ScreenDimensions.Z);

			ScreensTransforms.Add(ScreenTransform);
		}
		Result = MLScreensReleaseScreenInfoList(&ScreensInfoList);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogScreens, Error, TEXT("MLScreensReleaseScreenInfoList failed with error %d!"), Result);
		}
	}
	else
	{
		UE_LOG(LogScreens, Error, TEXT("MLScreensGetScreenInfoList failed with error %d!"), Result);
	}	
#endif //WITH_MLSDK
	return bResult;
}