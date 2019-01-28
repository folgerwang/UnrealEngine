// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MagicLeapRunnable.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_camera.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

struct FCameraTask : public FMagicLeapTask
{
	enum class EType : uint32
	{
		None,
		Connect,
		Disconnect,
		ImageToFile,
		ImageToTexture,
		StartVideoToFile,
		StopVideoToFile,
		Log,
	};

	EType CaptureType;
	FString FilePath;
	FString Log;
	class UTexture2D* Texture;

	FCameraTask()
		: CaptureType(EType::None)
		, Texture(nullptr)
	{
	}
};

class FCameraRunnable : public FMagicLeapRunnable<FCameraTask>
{
public:
	FCameraRunnable();
	void Exit() override;
	void PushNewCaptureTask(FCameraTask::EType InTaskType);
	bool IsConnected() const;

	static FThreadSafeCounter64 PreviewHandle;

protected:
	void Pause() override;
	void Resume() override;

private:
	bool ProcessCurrentTask() override;
#if WITH_MLSDK
	static void OnPreviewBufferAvailable(MLHandle Output, void *Data);
	bool TryConnect();
	bool TryDisconnect();
	bool CaptureImageToFile();
	bool CaptureImageToTexture();
	bool StartRecordingVideo();
	bool StopRecordingVideo();
	void Log(const FString& InLogMsg);

	MLCameraDeviceStatusCallbacks DeviceStatusCallbacks;
#endif //WITH_MLSDK
	FCameraTask::EType CurrentTaskType;
	FThreadSafeBool bCameraConnected;
	bool bWasConnectedOnPause;
	const FString ImgExtension;
	const FString VidExtension;
	FString UniqueFileName;
	TSharedPtr<IImageWrapper> ImageWrapper;
};
