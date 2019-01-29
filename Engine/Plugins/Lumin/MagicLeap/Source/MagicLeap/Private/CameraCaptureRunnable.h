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

#pragma once

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/ThreadSafeCounter64.h"
#include "Containers/Queue.h"
#include "Containers/Array.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "AppEventHandler.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END
#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_api.h>
#include <ml_camera.h>
#include <ml_camera_metadata.h>
#include <ml_camera_metadata_tags.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

enum class CaptureMsgType : uint32
{
	Invalid,
	Request,
	Response,
	Log
};

enum class CaptureTaskType : uint32
{
	None,
	ImageToFile,
	ImageToTexture,
	StartVideoToFile,
	StopVideoToFile,
};

struct FCaptureMessage
{
	CaptureMsgType Type;
	CaptureTaskType CaptureType;
	FString Log;
	FString FilePath;
	bool Success;
	class UTexture2D* Texture;
	float Duration;
	class FCameraCaptureImpl* Requester;

	FCaptureMessage()
	{
		Clear();
	}

	void Clear()
	{
		Type = CaptureMsgType::Invalid;
		CaptureType = CaptureTaskType::None;
		Log = "";
		Success = false;
		Texture = nullptr;
		Duration = 0.0f;
		Requester = nullptr;
	}
};

class FCameraCaptureRunnable : public FRunnable, public MagicLeap::IAppEventHandler
{
public:
	FCameraCaptureRunnable();
	virtual ~FCameraCaptureRunnable();

	uint32 Run() override;
	void Stop() override;
	void OnAppPause() override;
	void OnAppResume() override;
	void OnAppShutDown() override;
	void ProcessCaptureMessage(const FCaptureMessage& InMsg);

	/** Internal thread this instance is running on */
	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;
	TQueue<FCaptureMessage, EQueueMode::Spsc> IncomingMessages;
	TQueue<FCaptureMessage, EQueueMode::Spsc> OutgoingMessages;
	FEvent* Semaphore;
	bool bCameraConnected;

#if WITH_MLSDK
	MLCameraDeviceStatusCallbacks DeviceStatusCallbacks;
#endif //WITH_MLSDK

	const FString ImgExtension;
	const FString VidExtension;
	FString UniqueFileName;
	TSharedPtr<IImageWrapper> ImageWrapper;
	FThreadSafeBool bPaused;
	FCaptureMessage CurrentTask;
	static FThreadSafeCounter64 PreviewHandle;

private:
#if WITH_MLSDK
	static void OnPreviewBufferAvailable(MLHandle Output, void *Data);
#endif //WITH_MLSDK

	bool TryConnect();
	bool DoCaptureTask();
	bool CaptureImageToFile();
	bool CaptureImageToTexture();
	bool StartRecordingVideo();
	bool StopRecordingVideo();
	void Pause();
	void Log(const FString& Info);
	void CancelIncomingTasks();
};

DECLARE_LOG_CATEGORY_EXTERN(LogCameraCaptureRunnable, Verbose, All);