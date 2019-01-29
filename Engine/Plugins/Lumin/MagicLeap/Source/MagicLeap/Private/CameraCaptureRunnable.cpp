// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#include "CameraCaptureRunnable.h"
#include "Engine/Engine.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"
#include "AppFramework.h"
#include "Lumin/LuminPlatformFile.h"
#if PLATFORM_LUMIN
#include "Lumin/LuminAffinity.h"
#endif // PLATFORM_LUMIN
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_media_error.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

DEFINE_LOG_CATEGORY(LogCameraCaptureRunnable);

#if WITH_MLSDK
FThreadSafeCounter64 FCameraCaptureRunnable::PreviewHandle = ML_INVALID_HANDLE;
#endif //WITH_MLSDK

FCameraCaptureRunnable::FCameraCaptureRunnable()
#if WITH_MLSDK
	: MagicLeap::IAppEventHandler({ MLPrivilegeID_CameraCapture, MLPrivilegeID_AudioRecognizer, MLPrivilegeID_AudioCaptureMic, MLPrivilegeID_VoiceInput })
#else
	: MagicLeap::IAppEventHandler()
#endif //WITH_MLSDK
	, Thread(nullptr)
	, StopTaskCounter(0)
	, Semaphore(nullptr)
	, bCameraConnected(false)
	, ImgExtension(".jpeg")
	, VidExtension(".mp4")
	, bPaused(false)
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
	Semaphore = FGenericPlatformProcess::GetSynchEventFromPool(false);
#if PLATFORM_LUMIN
	Thread = FRunnableThread::Create(this, TEXT("FCameraCaptureWorker"), 0, TPri_BelowNormal, FLuminAffinity::GetPoolThreadMask());
#else
	Thread = FRunnableThread::Create(this, TEXT("FCameraCaptureWorker"), 0, TPri_BelowNormal);
#endif // PLATFORM_LUMIN
}

FCameraCaptureRunnable::~FCameraCaptureRunnable()
{
	Stop();
}

uint32 FCameraCaptureRunnable::Run()
{
#if WITH_MLSDK
	while (StopTaskCounter.GetValue() == 0)
	{
		if (bPaused)
		{
			Pause();
		}
		else if (!IncomingMessages.IsEmpty())
		{
			if (!bCameraConnected)
			{
				bCameraConnected = TryConnect();
			}
			else
			{
				DoCaptureTask();
			}
		}
		else
		{
			Semaphore->Wait();
		}
	}

	if (bCameraConnected)
	{
		MLResult Result = MLCameraDisconnect();
		UE_CLOG(Result != MLResult_Ok, LogCameraCaptureRunnable, Error, TEXT("MLCameraDisconnect failed with error %s!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	}
#endif //WITH_MLSDK
	return 0;
}

void FCameraCaptureRunnable::Stop()
{
	StopTaskCounter.Increment();

	if (Semaphore)
	{
		Semaphore->Trigger();
		Thread->WaitForCompletion();
		FGenericPlatformProcess::ReturnSynchEventToPool(Semaphore);
		Semaphore = nullptr;
	}

	delete Thread;
	Thread = nullptr;
}

void FCameraCaptureRunnable::OnAppPause()
{
	IAppEventHandler::OnAppPause();
	bPaused = true;
	Semaphore->Trigger();
}

void FCameraCaptureRunnable::OnAppResume()
{
	IAppEventHandler::OnAppResume();
	bPaused = false;
	Semaphore->Trigger();
}

void FCameraCaptureRunnable::OnAppShutDown()
{
	Stop();
}

void FCameraCaptureRunnable::ProcessCaptureMessage(const FCaptureMessage& InMsg)
{
	IncomingMessages.Enqueue(InMsg);
	// wake up the worker to process the event
	Semaphore->Trigger();
}

#if WITH_MLSDK
void FCameraCaptureRunnable::OnPreviewBufferAvailable(MLHandle Output, void *Data)
{
	(void)Data;
	FCameraCaptureRunnable::PreviewHandle.Set(static_cast<int64>(Output));
}
#endif //WITH_MLSDK

bool FCameraCaptureRunnable::TryConnect()
{
#if WITH_MLSDK
	IncomingMessages.Peek(CurrentTask); // this is purely so that any log messages go to the correct requester
	if (GetPrivilegeStatus(MLPrivilegeID_CameraCapture) != MagicLeap::EPrivilegeState::Granted)
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
#endif

	return true;
}

bool FCameraCaptureRunnable::DoCaptureTask()
{
	bool bSuccess = false;
	IncomingMessages.Dequeue(CurrentTask);

#if WITH_MLSDK
	switch (CurrentTask.CaptureType)
	{
	case CaptureTaskType::ImageToFile: bSuccess = CaptureImageToFile(); break;
	case CaptureTaskType::ImageToTexture: bSuccess = CaptureImageToTexture(); break;
	case CaptureTaskType::StartVideoToFile: bSuccess = StartRecordingVideo(); break;
	case CaptureTaskType::StopVideoToFile: bSuccess = StopRecordingVideo(); break;
	}
#endif //WITH_MLSDK

	if (bPaused) return false;

	FCaptureMessage Response = CurrentTask;
	Response.Type = CaptureMsgType::Response;
	Response.Success = bSuccess;
	OutgoingMessages.Enqueue(Response);

	return bSuccess;
}

bool FCameraCaptureRunnable::CaptureImageToFile()
{
#if WITH_MLSDK
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
#endif
	return true;
}

bool FCameraCaptureRunnable::CaptureImageToTexture()
{
#if WITH_MLSDK
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

#endif
	return true;
}

bool FCameraCaptureRunnable::StartRecordingVideo()
{
#if WITH_MLSDK
	Log(TEXT("Beginning capture video to file."));
	MLHandle Handle = ML_INVALID_HANDLE;
	MLResult Result = MLCameraPrepareCapture(MLCameraCaptureType_Video, &Handle);
	if (Result != MLResult_Ok)
	{
		Log(FString::Printf(TEXT("MLCameraPrepareCapture failed with error %s!  Camera capture aborted!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result))));
		return false;
	}

	if (bPaused) return false;

	if (GetPrivilegeStatus(MLPrivilegeID_AudioRecognizer) != MagicLeap::EPrivilegeState::Granted)
	{
		Log(TEXT("Cannot capture video due to lack of privilege!"));
		return false;
	}

	if (bPaused) return false;

	if (GetPrivilegeStatus(MLPrivilegeID_AudioCaptureMic) != MagicLeap::EPrivilegeState::Granted)
	{
		Log(TEXT("Cannot capture video due to lack of privilege!"));
		return false;
	}

	if (bPaused) return false;

	if (GetPrivilegeStatus(MLPrivilegeID_VoiceInput) != MagicLeap::EPrivilegeState::Granted)
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
#endif

	return true;
}

bool FCameraCaptureRunnable::StopRecordingVideo()
{
#if WITH_MLSDK
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
#endif
	return true;
}

void FCameraCaptureRunnable::Log(const FString& Info)
{
	FCaptureMessage Msg;
	Msg.Type = CaptureMsgType::Log;
	Msg.Log = Info;
	Msg.Requester = CurrentTask.Requester;
	OutgoingMessages.Enqueue(Msg);
	UE_LOG(LogCameraCaptureRunnable, Log, TEXT("%s"), *Info);
}

void FCameraCaptureRunnable::Pause()
{
#if WITH_MLSDK
	// Cancel the current video recording (if one is active).
	if (CurrentTask.CaptureType == CaptureTaskType::StartVideoToFile)
	{
		StopRecordingVideo();
		FCaptureMessage Response = CurrentTask;
		Response.Type = CaptureMsgType::Response;
		Response.Success = false;
		OutgoingMessages.Enqueue(Response);
	}
	// Cancel any incoming tasks.
	CancelIncomingTasks();
	// Disconnect camera if connected
	if (bCameraConnected)
	{
		MLResult Result = MLCameraDisconnect();
		UE_CLOG(Result != MLResult_Ok, LogCameraCaptureRunnable, Error, TEXT("MLCameraDisconnect failed with error %s!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		bCameraConnected = false;
	}
	// Wait for signal from resume call.
	Semaphore->Wait();
#endif
}

void FCameraCaptureRunnable::CancelIncomingTasks()
{
	FCaptureMessage CancelledTask;
	while (IncomingMessages.Dequeue(CancelledTask))
	{
		CancelledTask.Type = CaptureMsgType::Response;
		CancelledTask.Success = false;
		OutgoingMessages.Enqueue(CancelledTask);
	}
}