// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapCameraRunnable.h"
#include "MagicLeapCameraPlugin.h"
#include "Engine/Texture2D.h"
#include "Lumin/LuminPlatformFile.h"
#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_api.h>
#include <ml_camera_metadata.h>
#include <ml_camera_metadata_tags.h>
#include <ml_media_error.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

FThreadSafeCounter64 FCameraRunnable::PreviewHandle = 0;

FCameraRunnable::FCameraRunnable()
#if WITH_MLSDK
	: FMagicLeapRunnable({ MLPrivilegeID_CameraCapture, MLPrivilegeID_AudioRecognizer, MLPrivilegeID_AudioCaptureMic, MLPrivilegeID_VoiceInput }, TEXT("FCameraRunnable"))
#else
	: FMagicLeapRunnable(TEXT("FCameraRunnable"))
#endif //WITH_MLSDK
	, bCameraConnected(false)
	, bWasConnectedOnPause(false)
	, ImgExtension(".jpeg")
	, VidExtension(".mp4")
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
}

void FCameraRunnable::Exit()
{
#if WITH_MLSDK
	TryDisconnect();
#endif
}

void FCameraRunnable::PushNewCaptureTask(FCameraTask::EType InTaskType)
{
#if WITH_MLSDK
	if (!bCameraConnected && InTaskType != FCameraTask::EType::Connect)
	{
		FCameraTask ConnectTask;
		ConnectTask.CaptureType = FCameraTask::EType::Connect;
		PushNewTask(ConnectTask);
	}
	FCameraTask CaptureImgToFileTask;
	CaptureImgToFileTask.CaptureType = InTaskType;
	PushNewTask(CaptureImgToFileTask);
#endif // WITH_MLSDK
}

bool FCameraRunnable::IsConnected() const
{
	return bCameraConnected;
}

void FCameraRunnable::Pause()
{
#if WITH_MLSDK
	bWasConnectedOnPause = bCameraConnected;
	// Cancel the current video recording (if one is active).
	if (CurrentTask.CaptureType == FCameraTask::EType::StartVideoToFile)
	{
		StopRecordingVideo();
		CurrentTask.bSuccess = false;
		PushCompletedTask(CurrentTask);
	}
	// Cancel any incoming tasks.
	CancelIncomingTasks();
	// Disconnect camera if connected
	TryDisconnect();
#endif // WITH_MLSDK
}

void FCameraRunnable::Resume()
{
	if (bWasConnectedOnPause)
	{
		FCameraTask ConnectTask;
		ConnectTask.CaptureType = FCameraTask::EType::Connect;
		PushNewTask(ConnectTask);
	}
}

bool FCameraRunnable::ProcessCurrentTask()
{
	bool bSuccess = false;

#if WITH_MLSDK
	switch (CurrentTask.CaptureType)
	{
	case FCameraTask::EType::None: bSuccess = false; checkf(false, TEXT("Invalid camera task encountered!")); break;
	case FCameraTask::EType::Connect: bSuccess = bCameraConnected = TryConnect(); break;
	case FCameraTask::EType::Disconnect: bSuccess = TryDisconnect(); break;
	case FCameraTask::EType::ImageToFile: bSuccess = CaptureImageToFile(); break;
	case FCameraTask::EType::ImageToTexture: bSuccess = CaptureImageToTexture(); break;
	case FCameraTask::EType::StartVideoToFile: bSuccess = StartRecordingVideo(); break;
	case FCameraTask::EType::StopVideoToFile: bSuccess = StopRecordingVideo(); break;
	}
#endif

	return bSuccess;
}

#if WITH_MLSDK
void FCameraRunnable::OnPreviewBufferAvailable(MLHandle Output, void *Data)
{
	(void)Data;
	PreviewHandle.Set(static_cast<int64>(Output));
}

bool FCameraRunnable::TryConnect()
{
	//IncomingTasks.Peek(CurrentTask); // this is purely so that any log messages go to the correct requester
	if (AppEventHandler.GetPrivilegeStatus(MLPrivilegeID_CameraCapture) != MagicLeap::EPrivilegeState::Granted)
	{
		Log(TEXT("Cannot connect to camera due to lack of privilege!"));
		return false;
	}
	else
	{
		if (bPaused) return false;

		MLResult Result = MLCameraConnect();

		if (bPaused) return false;

		if (Result != MLResult_Ok)
		{
			Log(FString::Printf(TEXT("MLCameraConnect failed with error %s!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result))));
			CancelIncomingTasks();
			return false;
		}
		else
		{
			FMemory::Memset(&DeviceStatusCallbacks, 0, sizeof(MLCameraDeviceStatusCallbacks));
			DeviceStatusCallbacks.on_preview_buffer_available = OnPreviewBufferAvailable;
			Result = MLCameraSetDeviceStatusCallbacks(&DeviceStatusCallbacks, nullptr);
			if (Result != MLResult_Ok)
			{
				Log(FString::Printf(TEXT("MLCameraSetDeviceStatusCallbacks failed with error %s!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result))));
			}
		}
	}

	return true;
}

bool FCameraRunnable::TryDisconnect()
{
	if (bCameraConnected)
	{
		MLResult Result = MLCameraDisconnect();
		if (Result != MLResult_Ok)
		{
			Log(FString::Printf(TEXT("MLCameraDisconnect failed with error %s!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result))));
		}
		else
		{
			bCameraConnected = false;
		}
	}

	return bCameraConnected == false;
}

bool FCameraRunnable::CaptureImageToFile()
{
	if (bPaused) return false;

	Log(TEXT("Beginning capture image to file."));
	MLHandle Handle = ML_INVALID_HANDLE;
	MLResult Result = MLCameraPrepareCapture(MLCameraCaptureType_Image, &Handle);
	if (Result != MLResult_Ok)
	{
		Log(FString::Printf(TEXT("MLCameraPrepareCapture failed with error %s!  Camera capture aborted!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result))));
		return false;
	}

	if (bPaused) return false;

#if PLATFORM_LUMIN
	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
	// This module is only for Lumin so this is fine for now.
	FLuminPlatformFile* LuminPlatformFile = static_cast<FLuminPlatformFile*>(&PlatformFile);
	UniqueFileName = LuminPlatformFile->ConvertToLuminPath(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("Img_"), *ImgExtension), true);
#endif
	Result = MLCameraCaptureImage(TCHAR_TO_UTF8(*UniqueFileName));
	if (Result != MLResult_Ok)
	{
		Log(FString::Printf(TEXT("MLCameraCaptureImage failed with error %s!  Camera capture aborted!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result))));
		return false;
	}

	Log(FString::Printf(TEXT("Captured image to %s"), *UniqueFileName));
	CurrentTask.FilePath = UniqueFileName;
	return true;
}

bool FCameraRunnable::CaptureImageToTexture()
{
	if (bPaused) return false;

	Log(TEXT("Beginning capture image to texture."));
	MLCameraOutput* CameraOutput = nullptr;
	MLHandle Handle = ML_INVALID_HANDLE;
	MLResult Result = MLCameraPrepareCapture(MLCameraCaptureType_ImageRaw, &Handle);
	if (Result != MLResult_Ok)
	{
		Log(FString::Printf(TEXT("MLCameraPrepareCapture failed with error %s!  Camera capture aborted!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result))));
		return false;
	}

	if (bPaused) return false;

	Result = MLCameraCaptureImageRaw();
	if (Result != MLResult_Ok)
	{
		Log(FString::Printf(TEXT("MLCameraCaptureImageRaw failed with error %s!  Camera capture aborted!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result))));
		return false;
	}

	if (bPaused) return false;

	Result = MLCameraGetImageStream(&CameraOutput);
	if (Result != MLResult_Ok)
	{
		Log(FString::Printf(TEXT("MLCameraGetImageStream failed with error %s!  Camera capture aborted!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result))));
		return false;
	}

	if (bPaused) return false;

	if (CameraOutput->plane_count == 0)
	{
		Log(TEXT("Invalid plane_count!  Camera capture aborted!"));
		return false;
	}

	MLCameraPlaneInfo& ImageInfo = CameraOutput->planes[0];
	if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(ImageInfo.data, ImageInfo.size))
	{
		const TArray<uint8>* RawData = NULL;
		if (ImageWrapper->GetRaw(ImageWrapper->GetFormat(), 8, RawData))
		{
			Log(FString::Printf(TEXT("ImageWrapper width=%d height=%d size=%d"), ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), RawData->Num()));
			UTexture2D* CaptureTexture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), EPixelFormat::PF_R8G8B8A8);
			CaptureTexture->AddToRoot();
			FTexture2DMipMap& Mip = CaptureTexture->PlatformData->Mips[0];
			void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(Data, RawData->GetData(), Mip.BulkData.GetBulkDataSize());
			Mip.BulkData.Unlock();
			CaptureTexture->UpdateResource();
			CurrentTask.Texture = CaptureTexture;
		}
	}

	return true;
}

bool FCameraRunnable::StartRecordingVideo()
{
	if (bPaused) return false;

	Log(TEXT("Beginning capture video to file."));
	MLHandle Handle = ML_INVALID_HANDLE;
	MLResult Result = MLCameraPrepareCapture(MLCameraCaptureType_Video, &Handle);
	if (Result != MLResult_Ok)
	{
		Log(FString::Printf(TEXT("MLCameraPrepareCapture failed with error %s!  Camera capture aborted!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result))));
		return false;
	}

	if (bPaused) return false;

	if (AppEventHandler.GetPrivilegeStatus(MLPrivilegeID_AudioRecognizer) != MagicLeap::EPrivilegeState::Granted)
	{
		Log(TEXT("Cannot capture video due to lack of privilege!"));
		return false;
	}

	if (bPaused) return false;

	if (AppEventHandler.GetPrivilegeStatus(MLPrivilegeID_AudioCaptureMic) != MagicLeap::EPrivilegeState::Granted)
	{
		Log(TEXT("Cannot capture video due to lack of privilege!"));
		return false;
	}

	if (bPaused) return false;

	if (AppEventHandler.GetPrivilegeStatus(MLPrivilegeID_VoiceInput) != MagicLeap::EPrivilegeState::Granted)
	{
		Log(TEXT("Cannot capture video due to lack of privilege!"));
		return false;
	}

	if (bPaused) return false;

#if PLATFORM_LUMIN
	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
	// This module is only for Lumin so this is fine for now.
	FLuminPlatformFile* LuminPlatformFile = static_cast<FLuminPlatformFile*>(&PlatformFile);
	UniqueFileName = LuminPlatformFile->ConvertToLuminPath(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("Vid_"), *VidExtension), true);
#endif
	Result = MLCameraCaptureVideoStart(TCHAR_TO_UTF8(*UniqueFileName));
	if (Result != MLResult_Ok)
	{
		Log(FString::Printf(TEXT("MLCameraCaptureVideoStart failed with error %s!  Video capture aborted!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result))));
		return false;
	}

	return true;
}

bool FCameraRunnable::StopRecordingVideo()
{
	MLResult Result = MLCameraCaptureVideoStop();
	if (Result != MLResult_Ok)
	{
		Log(FString::Printf(TEXT("MLCameraCaptureVideoStop failed with error %s!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result))));
		return false;
	}
	else
	{
		Log(FString::Printf(TEXT("Captured video to %s"), *UniqueFileName));
	}

	CurrentTask.FilePath = UniqueFileName;
	return true;
}

void FCameraRunnable::Log(const FString& Info)
{
	FCameraTask LogTask;
	LogTask.CaptureType = FCameraTask::EType::Log;
	LogTask.Log = Info;
	PushCompletedTask(LogTask);
	UE_LOG(LogMagicLeapCamera, Log, TEXT("%s"), *Info);
}
#endif //WITH_MLSDK
