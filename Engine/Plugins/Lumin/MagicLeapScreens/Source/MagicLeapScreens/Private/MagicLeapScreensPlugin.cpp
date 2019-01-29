// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapScreensPlugin.h"
#include "RenderUtils.h"
#include "IMagicLeapPlugin.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "MagicLeapScreensWorker.h"
#include "Misc/CoreDelegates.h"
#include "MagicLeapHMD.h"
 
#define MAX_TEXTURE_SIZE 450 * 450 * 4 // currently limited by binder implementation

DEFINE_LOG_CATEGORY_STATIC(LogScreensPlugin, Display, All);

#if WITH_MLSDK
MLImage FMagicLeapScreensPlugin::DefaultThumbnail = MLImage();
FCriticalSection FMagicLeapScreensPlugin::CriticalSection;
#endif // WITH_MLSDK
FScreensWorker *FMagicLeapScreensPlugin::Impl = new FScreensWorker();
TArray<uint8> FMagicLeapScreensPlugin::PixelDataMemPool = TArray<uint8>();

void FMagicLeapScreensPlugin::StartupModule()
{
	IModuleInterface::StartupModule();
	APISetup.Startup();
	APISetup.LoadDLL(TEXT("ml_screens"));

#if WITH_MLSDK
	DefaultThumbnail.width = 2;
	DefaultThumbnail.height = 2;
	DefaultThumbnail.image_type = MLImageType_RGBA32;
	DefaultThumbnail.alignment = 1;
	const SIZE_T DataSize = DefaultThumbnail.width * DefaultThumbnail.height * 4;
	DefaultThumbnail.data = new uint8[DataSize];
	FMemory::Memset(DefaultThumbnail.data, 255, DataSize);
#endif // WITH_MLSDK

	TickDelegate = FTickerDelegate::CreateRaw(this, &FMagicLeapScreensPlugin::Tick);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate);

	PixelDataMemPool.Reserve(MAX_TEXTURE_SIZE);
	bEngineLoopInitComplete = false;
	FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FMagicLeapScreensPlugin::OnEngineLoopInitComplete);
}

void FMagicLeapScreensPlugin::ShutdownModule()
{
	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
	APISetup.Shutdown();

#if PLATFORM_LUMIN
	if (Impl)
	{
		Impl->AsyncDestroy();
		Impl = nullptr;
	}
#else
	delete Impl;
	Impl = nullptr;
#endif

#if WITH_MLSDK
	delete[] DefaultThumbnail.data;
	DefaultThumbnail.data = nullptr;
	MLScreensReleaseWatchHistoryThumbnail(&FMagicLeapScreensPlugin::DefaultThumbnail);
#endif // WITH_MLSDK
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	IModuleInterface::ShutdownModule();
}

bool FMagicLeapScreensPlugin::IsEngineLoopInitComplete() const
{
	return bEngineLoopInitComplete;
}

void FMagicLeapScreensPlugin::OnEngineLoopInitComplete()
{
	if (Impl->Semaphore == nullptr)
	{
		Impl->EngineInited();
	}

	bEngineLoopInitComplete = true;
}

bool FMagicLeapScreensPlugin::IsSupportedFormat(EPixelFormat InPixelFormat)
{
	if (InPixelFormat == PF_B8G8R8A8 || InPixelFormat == PF_R8G8B8A8)
	{
		return true;
	}

	UE_LOG(LogScreensPlugin, Error, TEXT("Unsupported pixel format!"));

	return false;
}

#if WITH_MLSDK
UTexture2D* FMagicLeapScreensPlugin::MLImageToUTexture2D(const MLImage& Source)
{
	UTexture2D* Thumbnail = UTexture2D::CreateTransient(Source.width, Source.height, EPixelFormat::PF_R8G8B8A8);
	FTexture2DMipMap& Mip = Thumbnail->PlatformData->Mips[0];
	void* PixelData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	const uint32 PixelDataSize = Mip.BulkData.GetBulkDataSize();
	FMemory::Memcpy(PixelData, Source.data, PixelDataSize);
	UE_LOG(LogScreensPlugin, Log, TEXT("MLImageToUTexture2D width = %u height = %u size = %u"), Source.width, Source.height, PixelDataSize);
	Thumbnail->SRGB = true;
	Mip.BulkData.Unlock();
	Thumbnail->UpdateResource();

	return Thumbnail;
}

void FMagicLeapScreensPlugin::MLWatchHistoryEntryToUnreal(const MLScreensWatchHistoryEntry& InEntry, FScreensWatchHistoryEntry& OutEntry)
{
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
		UE_LOG(LogScreensPlugin, Log, TEXT("MLScreensGetWatchHistoryThumbnail failed for screen ID %u with error %s!"), (uint32)InEntry.id, UTF8_TO_TCHAR(MLScreensGetResultString(Result)));
		OutEntry.Thumbnail = MLImageToUTexture2D(FMagicLeapScreensPlugin::DefaultThumbnail);
	}
	else
	{
		OutEntry.Thumbnail = MLImageToUTexture2D(MLThumbnail);
		// Only release when default thumbnail is not used, default thumbnail should only be released when plugin shuts down
		Result = MLScreensReleaseWatchHistoryThumbnail(&MLThumbnail);
		UE_CLOG(Result != MLResult_Ok, LogScreensPlugin, Error, TEXT("MLScreensReleaseWatchHistoryThumbnail failed for with error %s!"), UTF8_TO_TCHAR(MLScreensGetResultString(Result)));
	}
}

bool FMagicLeapScreensPlugin::UTexture2DToMLImage(const UTexture2D& Source, MLImage& Target)
{
	bool bSuccess = false;
	FTexture2DMipMap& Mip = Source.PlatformData->Mips[0];
	void* PixelData = Mip.BulkData.Lock(LOCK_READ_ONLY);
	const int32 size = Mip.BulkData.GetBulkDataSize();

	if ((size <= MAX_TEXTURE_SIZE) && (size != 0) && (PixelData != nullptr))
	{
		UE_LOG(LogScreensPlugin, Log, TEXT("UTexture2DToMLImage width = %d height = %d size = %d"), Mip.SizeX, Mip.SizeY, size);
		Target.width = Mip.SizeX;
		Target.height = Mip.SizeY;
		Target.image_type = MLImageType_RGBA32;
		Target.alignment = 1;
		Target.data = PixelDataMemPool.GetData();
		FMemory::Memcpy(Target.data, PixelData, size);

		if (Source.GetPixelFormat() == EPixelFormat::PF_B8G8R8A8)
		{
			check(((size % 4) == 0) && (size != 0));

			for (int32 i = 0; i < size - 4; i += 4)
			{
				Swap<uint8>(Target.data[i], Target.data[i + 2]);
			}
		}

		bSuccess = true;
	}
	else
	{
		UE_CLOG(size > MAX_TEXTURE_SIZE, LogScreensPlugin, Error, TEXT("Texture size (%d) exceeds max texture size (%d)"), size, MAX_TEXTURE_SIZE);
		UE_CLOG(size == 0, LogScreensPlugin, Error, TEXT("Texture size is zero"));
	}

	Mip.BulkData.Unlock();

	return bSuccess;
}

bool FMagicLeapScreensPlugin::ShouldUseDefaultThumbnail(const FScreensWatchHistoryEntry& Entry,  MLImage& MLImage)
{
	return ((Entry.Thumbnail == nullptr) ||
		(FMagicLeapScreensPlugin::IsSupportedFormat(Entry.Thumbnail->GetPixelFormat()) == false) ||
		(FMagicLeapScreensPlugin::UTexture2DToMLImage(*Entry.Thumbnail, MLImage) == false)) ? true : false;
}
#endif // WITH_MLSDK

bool FMagicLeapScreensPlugin::RemoveWatchHistoryEntry(const FScreenID& ID)
{
#if WITH_MLSDK
	MLResult Result = MLResult_UnspecifiedFailure;
	{
		FScopeLock Lock(&FMagicLeapScreensPlugin::CriticalSection);
		Result = MLScreensRemoveWatchHistoryEntry(ID.ID);
		UE_CLOG(Result != MLResult_Ok, LogScreensPlugin, Error, TEXT("MLScreensRemoveWatchHistoryEntry failed with error %d for entry with id %d!"), Result, ID.ID);
	}
	return Result == MLResult_Ok;
#else
	return false;
#endif // WITH_MLSDK
}

void FMagicLeapScreensPlugin::GetWatchHistoryEntriesAsync(const TOptional<FScreensHistoryRequestResultDelegate>& OptionalResultDelegate)
{
	FScreensMessage Msg;
	Msg.Type = EScreensMsgType::Request;
	Msg.TaskType = EScreensTaskType::GetHistory;
	if (OptionalResultDelegate.IsSet())
	{
		Msg.HistoryDelegate = OptionalResultDelegate.GetValue();
	}
	FMagicLeapScreensPlugin::Impl->ProcessMessage(Msg);
}

void FMagicLeapScreensPlugin::AddToWatchHistoryAsync(const FScreensWatchHistoryEntry& NewEntry, const TOptional<FScreensEntryRequestResultDelegate>& OptionalResultDelegate)
{
	FScreensMessage Msg;
	Msg.Type = EScreensMsgType::Request;
	Msg.TaskType = EScreensTaskType::AddToHistory;
	if (OptionalResultDelegate.IsSet())
	{
		Msg.EntryDelegate = OptionalResultDelegate.GetValue();
	}
	Msg.WatchHistory.Add(NewEntry);
	FMagicLeapScreensPlugin::Impl->ProcessMessage(Msg);
}

void FMagicLeapScreensPlugin::UpdateWatchHistoryEntryAsync(const FScreensWatchHistoryEntry& UpdateEntry, const TOptional<FScreensEntryRequestResultDelegate>& OptionalResultDelegate)
{
	FScreensMessage Msg;
	Msg.Type = EScreensMsgType::Request;
	Msg.TaskType = EScreensTaskType::UpdateEntry;
	if (OptionalResultDelegate.IsSet())
	{
		Msg.EntryDelegate = OptionalResultDelegate.GetValue();
	}
	Msg.WatchHistory.Add(UpdateEntry);
	FMagicLeapScreensPlugin::Impl->ProcessMessage(Msg);
}


FScreensMessage FMagicLeapScreensPlugin::GetWatchHistoryEntries()
{
	FScreensMessage Msg;
	Msg.Type = EScreensMsgType::Response;
	Msg.TaskType = EScreensTaskType::GetHistory;
#if WITH_MLSDK
	{
		FScopeLock Lock(&FMagicLeapScreensPlugin::CriticalSection);
		MLScreensWatchHistoryList WatchHistoryList;
		MLResult Result = MLScreensGetWatchHistoryList(&WatchHistoryList);
		if (Result == MLResult_Ok)
		{
			Msg.WatchHistory.Reserve(WatchHistoryList.count);
			for (uint32 i = 0; i < WatchHistoryList.count; ++i)
			{
				FScreensWatchHistoryEntry WatchHistoryEntry;
				FMagicLeapScreensPlugin::MLWatchHistoryEntryToUnreal(WatchHistoryList.entries[i], WatchHistoryEntry);
				Msg.WatchHistory.Add(WatchHistoryEntry);
			}
			Result = MLScreensReleaseWatchHistoryList(&WatchHistoryList);
			UE_CLOG(Result != MLResult_Ok, LogScreensPlugin, Error, TEXT("MLScreensReleaseWatchHistoryList failed with error %s!"), UTF8_TO_TCHAR(MLScreensGetResultString(Result)));
			Msg.bSuccess = true;
		}
		else
		{
			Msg.bSuccess = false;
			UE_LOG(LogScreensPlugin, Error, TEXT("MLScreensGetWatchHistoryList failed with error %s!"), UTF8_TO_TCHAR(MLScreensGetResultString(Result)));
		}
	}
#endif // WITH_MLSDK
	return Msg;
}

bool FMagicLeapScreensPlugin::ClearWatchHistory()
{
#if WITH_MLSDK
	{
		FScopeLock Lock(&FMagicLeapScreensPlugin::CriticalSection);
		MLScreensWatchHistoryList WatchHistoryList;
		MLResult Result = MLScreensGetWatchHistoryList(&WatchHistoryList);
		if (Result == MLResult_Ok)
		{
			for (uint32 i = 0; i < WatchHistoryList.count; ++i)
			{
				Result = MLScreensRemoveWatchHistoryEntry(WatchHistoryList.entries[i].id);
				if (Result != MLResult_Ok)
				{
					UE_LOG(LogScreensPlugin, Error, TEXT("MLScreensRemoveWatchHistoryEntry failed with error %s for entry %d!"), UTF8_TO_TCHAR(MLScreensGetResultString(Result)), WatchHistoryList.entries[i].id);
					return false;
				}
			}

			Result = MLScreensReleaseWatchHistoryList(&WatchHistoryList);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogScreensPlugin, Error, TEXT("MLScreensReleaseWatchHistoryList failed with error %s!"), UTF8_TO_TCHAR(MLScreensGetResultString(Result)));
				return false;
			}
		}
		else
		{
			UE_LOG(LogScreensPlugin, Error, TEXT("MLScreensGetWatchHistoryList failed with error %s!"), UTF8_TO_TCHAR(MLScreensGetResultString(Result)));
			return false;
		}
	}
#endif //WITH_MLSDK
	return true;
}

FScreensMessage FMagicLeapScreensPlugin::AddToWatchHistory(const FScreensWatchHistoryEntry& WatchHistoryEntry)
{
	FScreensMessage Msg;
	Msg.Type = EScreensMsgType::Response;
	Msg.TaskType = EScreensTaskType::AddToHistory;

#if WITH_MLSDK
	{
		FScopeLock Lock(&FMagicLeapScreensPlugin::CriticalSection);
		MLScreensWatchHistoryEntry Entry;
		Entry.title = TCHAR_TO_UTF8(*WatchHistoryEntry.Title);
		Entry.subtitle = TCHAR_TO_UTF8(*WatchHistoryEntry.Subtitle);
		Entry.playback_position_ms = static_cast<uint32>(WatchHistoryEntry.PlaybackPosition.GetTotalMilliseconds());
		Entry.playback_duration_ms = static_cast<uint32>(WatchHistoryEntry.PlaybackDuration.GetTotalMilliseconds());
		Entry.custom_data = TCHAR_TO_UTF8(*WatchHistoryEntry.CustomData);
		MLImage MLThumbnail;

		if (FMagicLeapScreensPlugin::ShouldUseDefaultThumbnail(WatchHistoryEntry, MLThumbnail))
		{
			MLThumbnail = FMagicLeapScreensPlugin::DefaultThumbnail;
		}

		MLResult Result = MLScreensInsertWatchHistoryEntry(&Entry, &MLThumbnail);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogScreensPlugin, Error, TEXT("MLScreensInsertWatchHistoryEntry failed with error %s!"), UTF8_TO_TCHAR(MLScreensGetResultString(Result)));
			Msg.WatchHistory.Add(WatchHistoryEntry);
			Msg.bSuccess = false;
		}
		else
		{
			FScreensWatchHistoryEntry NewEntry;
			FMagicLeapScreensPlugin::MLWatchHistoryEntryToUnreal(Entry, NewEntry);
			Msg.WatchHistory.Add(NewEntry);
			Msg.bSuccess = true;
		}
	}
#endif // WITH_MLSDK
	return Msg;
}

FScreensMessage FMagicLeapScreensPlugin::UpdateWatchHistoryEntry(const FScreensWatchHistoryEntry& WatchHistoryEntry)
{
	FScreensMessage Msg;
	Msg.Type = EScreensMsgType::Response;
	Msg.TaskType = EScreensTaskType::UpdateEntry;

#if WITH_MLSDK
	{
		FScopeLock Lock(&FMagicLeapScreensPlugin::CriticalSection);
		MLScreensWatchHistoryEntry Entry;
		Entry.id = WatchHistoryEntry.ID.ID;
		Entry.title = TCHAR_TO_UTF8(*WatchHistoryEntry.Title);
		Entry.subtitle = TCHAR_TO_UTF8(*WatchHistoryEntry.Subtitle);
		Entry.playback_position_ms = static_cast<uint32>(WatchHistoryEntry.PlaybackPosition.GetTotalMilliseconds());
		Entry.playback_duration_ms = static_cast<uint32>(WatchHistoryEntry.PlaybackDuration.GetTotalMilliseconds());
		Entry.custom_data = TCHAR_TO_UTF8(*WatchHistoryEntry.CustomData);
		MLImage MLThumbnail;

		if (FMagicLeapScreensPlugin::ShouldUseDefaultThumbnail(WatchHistoryEntry, MLThumbnail))
		{
			MLThumbnail = FMagicLeapScreensPlugin::DefaultThumbnail;
		}

		MLResult Result = MLScreensUpdateWatchHistoryEntry(&Entry, &MLThumbnail);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogScreensPlugin, Error, TEXT("MLScreensUpdateWatchHistoryEntry failed with error %s!"), UTF8_TO_TCHAR(MLScreensGetResultString(Result)));
			Msg.WatchHistory.Add(WatchHistoryEntry);
			Msg.bSuccess = false;
		}
		else
		{
			FScreensWatchHistoryEntry UpdatedEntry;
			MLWatchHistoryEntryToUnreal(Entry, UpdatedEntry);
			Msg.WatchHistory.Add(UpdatedEntry);
			Msg.bSuccess = true;
		}
	}
#endif // WITH_MLSDK
	return Msg;
}

bool FMagicLeapScreensPlugin::Tick(float DeltaTime)
{
	if (!FMagicLeapScreensPlugin::Impl->OutgoingMessages.IsEmpty())
	{
		FScreensMessage Msg;
		FMagicLeapScreensPlugin::Impl->OutgoingMessages.Dequeue(Msg);

		if (Msg.Type == EScreensMsgType::Request)
		{
			UE_LOG(LogScreensPlugin, Error, TEXT("Unexpected EScreensMsgType::Request received from worker thread!"));
		}
		else if (Msg.Type == EScreensMsgType::Response)
		{
			switch (Msg.TaskType)
			{
			case EScreensTaskType::None: break;
			case EScreensTaskType::GetHistory:
			{
				Msg.HistoryDelegate.ExecuteIfBound(Msg.bSuccess, Msg.WatchHistory);
			}
			break;
			case EScreensTaskType::AddToHistory:
			{
				Msg.EntryDelegate.ExecuteIfBound(Msg.bSuccess, Msg.WatchHistory[0]);
			}
			break;
			case EScreensTaskType::UpdateEntry:
			{
				Msg.EntryDelegate.ExecuteIfBound(Msg.bSuccess, Msg.WatchHistory[0]);
			}
			break;
			}
		}
	}
	return true;
}

bool FMagicLeapScreensPlugin::GetScreensTransforms(TArray<FScreenTransform>& ScreensTransforms)
{
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

	MLScreensScreenInfoListEx ScreensInfoList;
	MLScreensScreenInfoListExInit(&ScreensInfoList);
	MLResult Result = MLScreensGetScreenInfoListEx(&ScreensInfoList);
	if (Result == MLResult_Ok)
	{
		// TODO: Param to GetTrackingToWorldTransform is unused (?)
		FTransform PoseTransform = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(nullptr);
		for (uint32 i = 0; i < ScreensInfoList.count; ++i)
		{
			MLScreensScreenInfoEx& Entry = ScreensInfoList.entries[i];
			// TODO: Remove this once Screens team fixes setting dimensions when returning the ScreenInfoEx list
			MLVec3f Scale = MagicLeap::ScaleFromMLMatrix(Entry.transform);
			Entry.dimensions.x = 0.874f * Scale.x;
			Entry.dimensions.y = 0.611f * Scale.y;
			Entry.dimensions.z = 0.5f * Scale.z;
			FScreenTransform ScreenTransform;

			FTransform EntryTransform = FTransform(FQuat(FVector(0.0, 0.0, 1.0), PI) * MagicLeap::ToFQuat(Entry.transform), MagicLeap::ToFVector(Entry.transform, WorldToMetersScale), FVector(1.0, 1.0, 1.0));
			if (EntryTransform.ContainsNaN())
			{
				UE_LOG(LogScreensPlugin, Error, TEXT("Screens info entry %d transform contains NaN."), Entry.screen_id);
				continue;
			}
			if (!EntryTransform.GetRotation().IsNormalized())
			{
				FQuat rotation = EntryTransform.GetRotation();
				rotation.Normalize();
				EntryTransform.SetRotation(rotation);
			}
			EntryTransform.AddToTranslation(PoseTransform.GetLocation());
			EntryTransform.ConcatenateRotation(PoseTransform.Rotator().Quaternion());

			ScreenTransform.ScreenPosition = EntryTransform.GetLocation();
			ScreenTransform.ScreenOrientation = EntryTransform.Rotator();
			ScreenTransform.ScreenDimensions = MagicLeap::ToFVector(Entry.dimensions, WorldToMetersScale);
			ScreenTransform.ScreenDimensions.X = FMath::Abs<float>(ScreenTransform.ScreenDimensions.X);
			ScreenTransform.ScreenDimensions.Y = FMath::Abs<float>(ScreenTransform.ScreenDimensions.Y);
			ScreenTransform.ScreenDimensions.Z = FMath::Abs<float>(ScreenTransform.ScreenDimensions.Z);

			ScreensTransforms.Add(ScreenTransform);
		}
		Result = MLScreensReleaseScreenInfoListEx(&ScreensInfoList);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogScreensPlugin, Error, TEXT("MLScreensReleaseScreenInfoListEx failed with error %s!"), UTF8_TO_TCHAR(MLScreensGetResultString(Result)));
			return false;
		}
	}
	else
	{
		UE_LOG(LogScreensPlugin, Error, TEXT("MLScreensGetScreenInfoListEx failed with error %s!"), UTF8_TO_TCHAR(MLScreensGetResultString(Result)));
		return false;
	}
#endif // WITH_MLSDK
	return true;
}

IMPLEMENT_MODULE(FMagicLeapScreensPlugin, MagicLeapScreens);