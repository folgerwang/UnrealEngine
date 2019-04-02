// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CameraCaptureComponent.h"
#include "CameraCaptureRunnable.h"
#include "MagicLeapHMD.h"
#include "AppFramework.h"
#include "AppEventHandler.h"
#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Containers/Queue.h"
#include "Containers/Array.h"
#include "Lumin/LuminPlatformFile.h"
#if PLATFORM_LUMIN
#include "Lumin/LuminAffinity.h"
#endif // PLATFORM_LUMIN
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_camera.h>
#include <ml_camera_metadata.h>
#include <ml_camera_metadata_tags.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

DEFINE_LOG_CATEGORY(LogCameraCapture);

class FCameraCaptureImpl : public MagicLeap::IAppEventHandler
{
public:
	FCameraCaptureImpl()
	: CurrentCaptureTaskType(CaptureTaskType::None)
	{
#if PLATFORM_LUMIN
		checkf(GEngine, TEXT("[FCameraCaptureImpl::FCameraCaptureImpl()] GEngine is null!"));
		FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFramework();
		checkf(AppFramework.IsInitialized(), TEXT("[FCameraCaptureImpl::FCameraCaptureImpl()] AppFramework not yet initialized!"));
		CameraCaptureRunnable = AppFramework.GetCameraCaptureRunnable();
#endif //PLATFORM_LUMIN
	}

	virtual ~FCameraCaptureImpl()
	{
#if PLATFORM_LUMIN
		// Calling this explicitly to make the chain of destruction more obvious,
		// ie a potential call to FCameraCaptureRunnable's destructor right here.
		// On the lumin platform this call will take place inside the destruction
		// worker thread.
		CameraCaptureRunnable.Reset();
		// need this call to prevent the runnable form persisting with a ref count of 1 (due to Appframework's shared pointer instance).
		if (GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
		{
			static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFramework().RefreshCameraCaptureRunnableReferences();
		}
#endif //PLATFORM_LUMIN
	}

	bool TryCaptureImageToFile()
	{
#if PLATFORM_LUMIN
		if (CurrentCaptureTaskType == CaptureTaskType::None)
		{
			CurrentCaptureTaskType = CaptureTaskType::ImageToFile;
			FCaptureMessage Msg;
			Msg.Type = CaptureMsgType::Request;
			Msg.CaptureType = CurrentCaptureTaskType;
			Msg.Requester = this;
			CameraCaptureRunnable->ProcessCaptureMessage(Msg);
			return true;
		}
#endif //PLATFORM_LUMIN
		return false;
	}

	bool TryCaptureImageToTexture()
	{
#if PLATFORM_LUMIN
		if (CurrentCaptureTaskType == CaptureTaskType::None)
		{
			CurrentCaptureTaskType = CaptureTaskType::ImageToTexture;
			FCaptureMessage Msg;
			Msg.Type = CaptureMsgType::Request;
			Msg.CaptureType = CurrentCaptureTaskType;
			Msg.Requester = this;
			CameraCaptureRunnable->ProcessCaptureMessage(Msg);
			return true;
		}
#endif //PLATFORM_LUMIN
		return false;
	}

	bool TryCaptureVideoToFile()
	{
#if PLATFORM_LUMIN
		if (CurrentCaptureTaskType == CaptureTaskType::None)
		{
			CurrentCaptureTaskType = CaptureTaskType::StartVideoToFile;
			FCaptureMessage Msg;
			Msg.Type = CaptureMsgType::Request;
			Msg.CaptureType = CurrentCaptureTaskType;
			Msg.Requester = this;
			CameraCaptureRunnable->ProcessCaptureMessage(Msg);
			return true;
		}
#endif //PLATFORM_LUMIN
		return false;
	}

	bool TryStopCaptureVideoToFile()
	{
#if PLATFORM_LUMIN
		if (CurrentCaptureTaskType == CaptureTaskType::StartVideoToFile)
		{
			CurrentCaptureTaskType = CaptureTaskType::StopVideoToFile;
			FCaptureMessage Msg;
			Msg.Type = CaptureMsgType::Request;
			Msg.CaptureType = CurrentCaptureTaskType;
			Msg.Requester = this;
			CameraCaptureRunnable->ProcessCaptureMessage(Msg);
			return true;
		}
#endif //PLATFORM_LUMIN
		return false;
	}

	bool TryGetResult(FCaptureMessage& OutMsg)
	{
#if PLATFORM_LUMIN
		if (CurrentCaptureTaskType != CaptureTaskType::None && CameraCaptureRunnable->OutgoingMessages.Peek(OutMsg))
		{
			if (OutMsg.Requester == this)
			{
				CameraCaptureRunnable->OutgoingMessages.Pop();
				return true;
			}
		}
#endif //PLATFORM_LUMIN
		return false;
	}

public:
	CaptureTaskType CurrentCaptureTaskType;
	TSharedPtr<FCameraCaptureRunnable, ESPMode::ThreadSafe> CameraCaptureRunnable;
};

UCameraCaptureComponent::UCameraCaptureComponent()
    : Impl(nullptr)
{
  PrimaryComponentTick.TickGroup = TG_PrePhysics;
  PrimaryComponentTick.bStartWithTickEnabled = true;
  PrimaryComponentTick.bCanEverTick = true;
}

UCameraCaptureComponent::~UCameraCaptureComponent()
{
#if PLATFORM_LUMIN
	if (Impl)
	{
		if (!Impl->AsyncDestroy())
	    {
			delete Impl;
    	}
		Impl = nullptr;
	}
#else
	delete Impl;
	Impl = nullptr;
#endif // PLATFORM_LUMIN	
}

void UCameraCaptureComponent::BeginPlay()
{
  Super::BeginPlay();
  Impl = new FCameraCaptureImpl;
}

void UCameraCaptureComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
#if WITH_MLSDK
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  FCaptureMessage Msg;
  if (Impl->TryGetResult(Msg))
  {
    if (Msg.Type == CaptureMsgType::Request)
    {
      Log("Unexpected CaptureMsgType::Request received from worker thread!");
    }
    else if (Msg.Type == CaptureMsgType::Log)
    {
      Log(Msg.Log);
    }
    else if (Msg.Type == CaptureMsgType::Response)
    {
      switch (Msg.CaptureType)
      {
	  case CaptureTaskType::ImageToFile:
	  {
		  CaptureImgToFileResult.ExecuteIfBound(Msg.Success, Msg.FilePath);
		  Impl->CurrentCaptureTaskType = CaptureTaskType::None;
	  }
	  break;

	  case CaptureTaskType::ImageToTexture:
	  {
		  CaptureImgToTextureResult.ExecuteIfBound(Msg.Success, Msg.Texture);
		  Impl->CurrentCaptureTaskType = CaptureTaskType::None;
	  }
	  break;

	  case CaptureTaskType::StartVideoToFile:
	  {
		  StartRecordingResult.ExecuteIfBound(Msg.Success);
		  // do not reset Impl->CurrentCaptureTaskType if start recording is successful
		  // as this constitutes an ongoing capture state
		  if (!Msg.Success)
		  {
			  Impl->CurrentCaptureTaskType = CaptureTaskType::None;
		  }
	  }
	  break;

	  case CaptureTaskType::StopVideoToFile:
	  {
		  StopRecordingResult.ExecuteIfBound(Msg.Success, Msg.FilePath);
		  Impl->CurrentCaptureTaskType = CaptureTaskType::None;
	  }
      break;
      }
    }
  }
#endif //WITH_MLSDK
}

bool UCameraCaptureComponent::CaptureImageToFileAsync(const FCameraCaptureImgToFile& ResultDelegate)
{
  CaptureImgToFileResult = ResultDelegate;

  if (Impl->TryCaptureImageToFile())
  {
    return true;
  }

  Log("Camera capture already in progress!");
  return false;
}

bool UCameraCaptureComponent::CaptureImageToTextureAsync(const FCameraCaptureImgToTexture& ResultDelegate)
{
  CaptureImgToTextureResult = ResultDelegate;

  if (Impl->TryCaptureImageToTexture())
  {
    return true;
  }

  Log("Camera capture already in progress!");
  return false;
}

bool UCameraCaptureComponent::StartRecordingVideoAsync(const FCameraCaptureStartRecording& ResultDelegate)
{
  StartRecordingResult = ResultDelegate;

  if (Impl->TryCaptureVideoToFile())
  {
    return true;
  }

  Log("Camera capture already in progress!");
  return false;
}

bool UCameraCaptureComponent::StopRecordingVideoAsync(const FCameraCaptureStopRecording& ResultDelegate)
{
	StopRecordingResult = ResultDelegate;

	if (Impl->TryStopCaptureVideoToFile())
	{
		return true;
	}

	Log("Camera capture already in progress!");
	return false;
}

bool UCameraCaptureComponent::IsCapturing() const
{
	return Impl && Impl->CurrentCaptureTaskType != CaptureTaskType::None;
}

int64 UCameraCaptureComponent::GetPreviewHandle()
{
#if PLATFORM_LUMIN
	return FCameraCaptureRunnable::PreviewHandle.GetValue();
#else
	return -1;
#endif //PLATFORM_LUMIN
}

void UCameraCaptureComponent::Log(const FString& LogMessage)
{
  UE_LOG(LogCameraCapture, Log, TEXT("%s"), *LogMessage);
  CaptureLogMessage.Broadcast(FString::Printf(TEXT("<br>%s"), *LogMessage));
}