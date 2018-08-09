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
	, RetryConnectWaitTime(0.5f)
#if WITH_MLSDK
	, CameraOutput(nullptr)
#endif // WITH_MLSDK
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
			if (bCameraConnected)
			{
				MLResult Result = MLCameraDisconnect();
				UE_CLOG(Result != MLResult_Ok, LogCameraCaptureRunnable, Error, TEXT("MLCameraDisconnect failed with error %s!"), UTF8_TO_TCHAR(MLGetResultString(Result)));
				bCameraConnected = false;
			}

			Semaphore->Wait();
		}

		if (IncomingMessages.Dequeue(CurrentMessage))
		{
			while (!bCameraConnected)
			{
				if (GetPrivilegeStatus(MLPrivilegeID_CameraCapture) == MagicLeap::EPrivilegeState::Granted)
				{
					MLResult Result = MLCameraConnect();
					if (Result == MLResult_Ok)
					{
						bCameraConnected = true;
						MLCameraDeviceStatusCallbacks deviceStatusCallbacks;
						FMemory::Memset(&deviceStatusCallbacks, 0, sizeof(MLCameraDeviceStatusCallbacks));
						deviceStatusCallbacks.on_preview_buffer_available = OnPreviewBufferAvailable;
						Result = MLCameraSetDeviceStatusCallbacks(&deviceStatusCallbacks, nullptr);
						if (Result != MLResult_Ok)
						{
							Log(FString::Printf(TEXT("MLCameraSetDeviceStatusCallbacks failed with error %s!"), UTF8_TO_TCHAR(MLGetResultString(Result))));
						}
					}
					else
					{
						bCameraConnected = false;
						Log(FString::Printf(TEXT("MLCameraConnect failed with error %s!  Retrying in %.2f seconds"), UTF8_TO_TCHAR(MLGetResultString(Result)), RetryConnectWaitTime));
						IncomingMessages.Empty();
						FPlatformProcess::Sleep(RetryConnectWaitTime);
					}
				}
				else
				{
					Log(TEXT("Cannot connect to camera due to lack of privilege!"));
					break;
				}
			}

			BeginCapture();
			Semaphore->Wait();
		}
	}

	if (bCameraConnected)
	{
		MLResult Result = MLCameraDisconnect();
		UE_CLOG(Result != MLResult_Ok, LogCameraCaptureRunnable, Error, TEXT("MLCameraDisconnect failed with error %s!"), UTF8_TO_TCHAR(MLGetResultString(Result)));
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

void FCameraCaptureRunnable::BeginCapture()
{
#if WITH_MLSDK
	MLHandle Handle = ML_INVALID_HANDLE;
	MLResult Result = MLCameraPrepareCapture(CurrentMessage.CaptureType, &Handle);
	if (Result != MLResult_Ok)
	{
		Log(FString::Printf(TEXT("MLCameraPrepareCapture failed with error %s!  Camera capture aborted!"), UTF8_TO_TCHAR(MLGetResultString(Result))));
		EndCapture(false);
		return;
	}

	switch (CurrentMessage.CaptureType)
	{
	case MLCameraCaptureType_Image:
	{
		Log(TEXT("Beginning capture image to file."));

#if PLATFORM_LUMIN
		IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
		// This module is only for Lumin so this is fine for now.
		FLuminPlatformFile* LuminPlatformFile = static_cast<FLuminPlatformFile*>(&PlatformFile);
		UniqueFileName = LuminPlatformFile->ConvertToLuminPath(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("Img_"), *ImgExtension), true);
#endif
		Result = MLCameraCaptureImage(TCHAR_TO_UTF8(*UniqueFileName));
		if (Result != MLResult_Ok)
		{
			Log(FString::Printf(TEXT("MLCameraCaptureImage failed with error %s!  Camera capture aborted!"), UTF8_TO_TCHAR(MLGetResultString(Result))));
			EndCapture(false);
			return;
		}

		EndCapture(true);
	}
	break;

	case MLCameraCaptureType_ImageRaw:
	{
		CameraOutput = nullptr;

		Log(TEXT("Beginning capture image to texture."));
		Result = MLCameraCaptureImageRaw();
		if (Result != MLResult_Ok)
		{
			Log(FString::Printf(TEXT("MLCameraCaptureImageRaw failed with error %s!  Camera capture aborted!"), UTF8_TO_TCHAR(MLGetResultString(Result))));
			EndCapture(false);
			return;
		}

		Result = MLCameraGetImageStream(&CameraOutput);
		if (Result != MLResult_Ok)
		{
			Log(FString::Printf(TEXT("MLCameraGetImageStream failed with error %s!  Camera capture aborted!"), UTF8_TO_TCHAR(MLGetResultString(Result))));
			EndCapture(false);
			return;
		}

		if (CameraOutput->plane_count == 0)
		{
			Log(TEXT("Invalid plane_count!  Camera capture aborted!"));
			EndCapture(false);
			return;
		}

		EndCapture(true);
	}
	break;

	case MLCameraCaptureType_Video:
	{
		Log(TEXT("Beginning capture video to file."));
		if (GetPrivilegeStatus(MLPrivilegeID_AudioRecognizer) == MagicLeap::EPrivilegeState::Granted &&
			GetPrivilegeStatus(MLPrivilegeID_AudioCaptureMic) == MagicLeap::EPrivilegeState::Granted &&
			GetPrivilegeStatus(MLPrivilegeID_VoiceInput) == MagicLeap::EPrivilegeState::Granted)
		{
#if PLATFORM_LUMIN
			IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
			// This module is only for Lumin so this is fine for now.
			FLuminPlatformFile* LuminPlatformFile = static_cast<FLuminPlatformFile*>(&PlatformFile);
			UniqueFileName = LuminPlatformFile->ConvertToLuminPath(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("Vid_"), *VidExtension), true);
#endif
			Result = MLCameraCaptureVideoStart(TCHAR_TO_UTF8(*UniqueFileName));
			if (Result != MLResult_Ok)
			{
				Log(FString::Printf(TEXT("MLCameraCaptureVideoStart failed with error %s!  Video capture aborted!"), UTF8_TO_TCHAR(MLGetResultString(Result))));
				EndCapture(false);
				return;
			}

			FPlatformProcess::Sleep(CurrentMessage.Duration);
			EndCapture(true);
		}
		else
		{
			Log(TEXT("Cannot capture video due to lack of privilege!"));
			EndCapture(false);
		}

	}
	break;
	}
#endif //WITH_MLSDK
}

void FCameraCaptureRunnable::EndCapture(bool InSuccess)
{
#if WITH_MLSDK
	CurrentMessage.Type = CaptureMsgType::Response;
	CurrentMessage.Success = InSuccess;
	CurrentMessage.FilePath = UniqueFileName;

	if (!InSuccess)
	{
		uint32 DeviceStatus = 0;
		MLResult Result = MLCameraGetDeviceStatus(&DeviceStatus);
		if (Result == MLResult_Ok)
		{
			if ((MLCameraDeviceStatusFlag_Available & DeviceStatus) != 0)
			{
				Log(TEXT("Device status = MLCameraDeviceStatusFlag_Available"));
			}
			else if ((MLCameraDeviceStatusFlag_Opened & DeviceStatus) != 0)
			{
				Log(TEXT("Device status = MLCameraDeviceStatusFlag_Opened"));
			}
			else if ((MLCameraDeviceStatusFlag_Disconnected  & DeviceStatus) != 0)
			{
				Log(TEXT("Device status = MLCameraDeviceStatusFlag_Disconnected "));
			}
			else if ((MLCameraDeviceStatusFlag_Error & DeviceStatus) != 0)
			{
				Log(TEXT("Device status = MLCameraDeviceStatusFlag_Error"));
				MLCameraError CameraError;
				if (MLCameraGetErrorCode(&CameraError) == MLResult_Ok)
				{
					switch (CameraError)
					{
					case MLCameraError_None: Log(TEXT("Error = MLCameraError_None")); break;
					case MLCameraError_Invalid: Log(TEXT("Error = MLCameraError_Invalid")); break;
					case MLCameraError_Disabled: Log(TEXT("Error = MLCameraError_Disabled")); break;
					case MLCameraError_DeviceFailed: Log(TEXT("Error = MLCameraError_DeviceFailed")); break;
					case MLCameraError_ServiceFailed: Log(TEXT("Error = MLCameraError_ServiceFailed")); break;
					case MLCameraError_CaptureFailed: Log(TEXT("Error = MLCameraError_CaptureFailed")); break;
					case MLCameraError_Unknown: Log(TEXT("Error = MLCameraError_Unknown")); break;
					}
				}
				else
				{
					Log(TEXT("MLCameraGetErrorCode failed!"));
				}
			}
		}
		else
		{
			Log(FString::Printf(TEXT("MLCameraGetDeviceStatus failed with error %s!"), UTF8_TO_TCHAR(MLGetResultString(Result))));
		}
	}

	if (InSuccess)
	{
		switch (CurrentMessage.CaptureType)
		{
		case MLCameraCaptureType_Image:
		{
			Log(FString::Printf(TEXT("Captured image to %s"), *UniqueFileName));
		}
		break;

		case MLCameraCaptureType_ImageRaw:
		{
			MLCameraPlaneInfo& ImageInfo = CameraOutput->planes[0];
			if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(ImageInfo.data, ImageInfo.size))
			{
				const TArray<uint8>* RawData = NULL;
				if (ImageWrapper->GetRaw(ImageWrapper->GetFormat(), 8, RawData))
				{
					Log(FString::Printf(TEXT("ImageWrapper width=%d height=%d size=%d"), ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), RawData->Num()));
					UTexture2D* CaptureTexture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), EPixelFormat::PF_B8G8R8A8);
					CaptureTexture->AddToRoot();
					FTexture2DMipMap& Mip = CaptureTexture->PlatformData->Mips[0];
					void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
					FMemory::Memcpy(Data, RawData->GetData(), Mip.BulkData.GetBulkDataSize());
					Mip.BulkData.Unlock();
					CaptureTexture->UpdateResource();
					CurrentMessage.Texture = CaptureTexture;
				}
			}
		}
		break;

		case MLCameraCaptureType_Video:
		{
			MLResult Result = MLCameraCaptureVideoStop();
			if (Result != MLResult_Ok)
			{
				Log(FString::Printf(TEXT("MLCameraCaptureVideoStop failed with error %s!"), UTF8_TO_TCHAR(MLGetResultString(Result))));
			}
			else
			{
				Log(FString::Printf(TEXT("Captured video to %s"), *UniqueFileName));
			}
		}
		break;
		}
	}

	// clear any left over events from this capture session
	IncomingMessages.Empty();
	// signal the update thread to handle the capture result
	OutgoingMessages.Enqueue(CurrentMessage);
#endif //WITH_MLSDK
}

void FCameraCaptureRunnable::Log(const FString& Info)
{
	FCaptureMessage Msg;
	Msg.Type = CaptureMsgType::Log;
	Msg.Log = Info;
	Msg.Requester = CurrentMessage.Requester;
	OutgoingMessages.Enqueue(Msg);
}

void FCameraCaptureRunnable::ProcessCaptureMessage(const FCaptureMessage& InMsg)
{
	IncomingMessages.Enqueue(InMsg);
	// wake up the worker to process the event
	Semaphore->Trigger();
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

#if WITH_MLSDK
void FCameraCaptureRunnable::OnPreviewBufferAvailable(MLHandle Output, void *Data)
{
	(void)Data;
	FCameraCaptureRunnable::PreviewHandle.Set(static_cast<int64>(Output));
}
#endif //WITH_MLSDK