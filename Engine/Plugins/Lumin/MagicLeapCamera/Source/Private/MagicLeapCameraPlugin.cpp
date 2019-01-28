// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapCameraPlugin.h"
#include "Async/Async.h"

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_camera.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

//PRAGMA_DISABLE_OPTIMIZATION

DEFINE_LOG_CATEGORY(LogMagicLeapCamera);

FMagicLeapCameraPlugin::FMagicLeapCameraPlugin()
: UserCount(0)
, Runnable(nullptr)
, CurrentTaskType(FCameraTask::EType::None)
, PrevTaskType(FCameraTask::EType::None)
{
}

void FMagicLeapCameraPlugin::StartupModule()
{
	IMagicLeapCameraPlugin::StartupModule();
	APISetup.Startup();
#if WITH_MLSDK
	APISetup.LoadDLL(TEXT("ml_camera"));
#endif // WITH_MLSDK
	Runnable = new FCameraRunnable();
	TickDelegate = FTickerDelegate::CreateRaw(this, &FMagicLeapCameraPlugin::Tick);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate);
}

void FMagicLeapCameraPlugin::ShutdownModule()
{
	FCameraRunnable* InRunnable = Runnable;
	Runnable = nullptr;
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [InRunnable]()
	{
		delete InRunnable;
	});
	APISetup.Shutdown();
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	IModuleInterface::ShutdownModule();
}

bool FMagicLeapCameraPlugin::Tick(float DeltaTime)
{
	FCameraTask CompletedTask;
	if (Runnable->TryGetCompletedTask(CompletedTask))
	{
		switch (CompletedTask.CaptureType)
		{
		case FCameraTask::EType::Connect:
		{
			if (OnCameraConnect.IsBound())
			{
				OnCameraConnect.ExecuteIfBound(CompletedTask.bSuccess);
				// Only clear CurrentTaskType if connect was manually called
				// (otherwise it was auto-called from within the runnable and the actual
				// current task is still in progress).
				CurrentTaskType = FCameraTask::EType::None;
			}
		}
		break;

		case FCameraTask::EType::Disconnect:
		{
			OnCameraDisconnect.ExecuteIfBound(CompletedTask.bSuccess);
			CurrentTaskType = FCameraTask::EType::None;
		}
		break;

		case FCameraTask::EType::ImageToFile:
		{
			OnCaptureImgToFile.Broadcast(CompletedTask.bSuccess, CompletedTask.FilePath);
			CurrentTaskType = FCameraTask::EType::None;
		}
		break;

		case FCameraTask::EType::ImageToTexture:
		{
			OnCaptureImgToTexture.Broadcast(CompletedTask.bSuccess, CompletedTask.Texture);
			CurrentTaskType = FCameraTask::EType::None;
		}
		break;

		case FCameraTask::EType::StartVideoToFile:
		{
			OnStartRecording.Broadcast(CompletedTask.bSuccess);
			// do not reset CurrentTaskType if start recording is successful
			// as this constitutes an ongoing capture state
			if (!CompletedTask.bSuccess)
			{
				CurrentTaskType = FCameraTask::EType::None;
			}
		}
		break;

		case FCameraTask::EType::StopVideoToFile:
		{
			OnStopRecording.Broadcast(CompletedTask.bSuccess, CompletedTask.FilePath);
			CurrentTaskType = FCameraTask::EType::None;
		}
		break;

		case FCameraTask::EType::Log:
		{
			UE_LOG(LogMagicLeapCamera, Log, TEXT("%s"), *CompletedTask.Log);
			OnLogMessage.Broadcast(FString::Printf(TEXT("<br>%s"), *CompletedTask.Log));
		}
		break;
		}
	}

	return true;
}

bool FMagicLeapCameraPlugin::CameraConnect(const FCameraConnect& ResultDelegate)
{
	if (TryPushNewCaptureTask(FCameraTask::EType::Connect))
	{
		OnCameraConnect = ResultDelegate;
		return true;
	}

	return false;
}

bool FMagicLeapCameraPlugin::CameraDisconnect(const FCameraDisconnect& ResultDelegate)
{
	if (UserCount <= 0 && TryPushNewCaptureTask(FCameraTask::EType::Disconnect))
	{
		OnCameraDisconnect = ResultDelegate;
		return true;
	}

	return false;
}

int64 FMagicLeapCameraPlugin::GetPreviewHandle() const
{
	return FCameraRunnable::PreviewHandle.GetValue();
}

void FMagicLeapCameraPlugin::IncUserCount()
{
	++UserCount;
}

void FMagicLeapCameraPlugin::DecUserCount()
{
	--UserCount;
	if (UserCount <= 0)
	{
		UserCount = 0;
		TryPushNewCaptureTask(FCameraTask::EType::Disconnect);
	}
}

bool FMagicLeapCameraPlugin::CaptureImageToFileAsync(const FCameraCaptureImgToFileMulti& ResultDelegate)
{
	if (TryPushNewCaptureTask(FCameraTask::EType::ImageToFile))
	{
		OnCaptureImgToFile = ResultDelegate;
		return true;
	}

	return false;
}

bool FMagicLeapCameraPlugin::CaptureImageToTextureAsync(const FCameraCaptureImgToTextureMulti& ResultDelegate)
{
	if (TryPushNewCaptureTask(FCameraTask::EType::ImageToTexture))
	{
		OnCaptureImgToTexture = ResultDelegate;
		return true;
	}

	return false;
}

bool FMagicLeapCameraPlugin::StartRecordingAsync(const FCameraStartRecordingMulti& ResultDelegate)
{
	if (TryPushNewCaptureTask(FCameraTask::EType::StartVideoToFile))
	{
		OnStartRecording = ResultDelegate;
		return true;
	}

	return false;
}

bool FMagicLeapCameraPlugin::StopRecordingAsync(const FCameraStopRecordingMulti& ResultDelegate)
{
	if (TryPushNewCaptureTask(FCameraTask::EType::StopVideoToFile))
	{
		OnStopRecording = ResultDelegate;
		return true;
	}

	return false;
}

bool FMagicLeapCameraPlugin::SetLogDelegate(const FCameraLogMessageMulti& LogDelegate)
{
	OnLogMessage = LogDelegate;
	return true;
}

bool FMagicLeapCameraPlugin::IsCapturing() const
{
	return CurrentTaskType != FCameraTask::EType::None;
}

bool FMagicLeapCameraPlugin::TryPushNewCaptureTask(FCameraTask::EType InTaskType)
{
	bool bCanPushTask = false;

	switch (InTaskType)
	{
	case FCameraTask::EType::None:
	{
		bCanPushTask = false;
	}
	break;

	case FCameraTask::EType::Connect:
	{
		bCanPushTask = CurrentTaskType == FCameraTask::EType::None || CurrentTaskType == FCameraTask::EType::Disconnect;
	}
	break;

	case FCameraTask::EType::Disconnect:
	{
		bCanPushTask = CurrentTaskType == FCameraTask::EType::None || CurrentTaskType == FCameraTask::EType::Connect;
	}
	break;

	case FCameraTask::EType::ImageToFile:
	{
		bCanPushTask = CurrentTaskType == FCameraTask::EType::None;
	}
	break;

	case FCameraTask::EType::ImageToTexture:
	{
		bCanPushTask = CurrentTaskType == FCameraTask::EType::None;
	}
	break;

	case FCameraTask::EType::StartVideoToFile:
	{
		bCanPushTask = CurrentTaskType == FCameraTask::EType::None && PrevTaskType != FCameraTask::EType::StartVideoToFile;
	}
	break;

	case FCameraTask::EType::StopVideoToFile:
	{
		bCanPushTask = PrevTaskType != FCameraTask::EType::StopVideoToFile &&
			(CurrentTaskType == FCameraTask::EType::None || CurrentTaskType == FCameraTask::EType::StartVideoToFile);
	}
	break;

	case FCameraTask::EType::Log:
	{
		bCanPushTask = true;
	}
	break;
	}

	if (bCanPushTask)
	{
		if (InTaskType != FCameraTask::EType::Log)
		{
			PrevTaskType = CurrentTaskType;
			CurrentTaskType = InTaskType;
		}

		Runnable->PushNewCaptureTask(InTaskType);
		return true;
	}

	return false;
}

IMPLEMENT_MODULE(FMagicLeapCameraPlugin, MagicLeapCamera);
