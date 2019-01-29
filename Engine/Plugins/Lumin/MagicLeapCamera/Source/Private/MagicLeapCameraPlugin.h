// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMagicLeapPlugin.h"
#include "IMagicLeapCameraPlugin.h"
#include "MagicLeapCameraRunnable.h"
#include "MagicLeapCameraTypes.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMagicLeapCamera, Verbose, All);

class FMagicLeapCameraPlugin : public IMagicLeapCameraPlugin
{
public:
	FMagicLeapCameraPlugin();
	void StartupModule() override;
	void ShutdownModule() override;
	bool Tick(float DeltaTime) override;
	bool CameraConnect(const FCameraConnect& ResultDelegate) override;
	bool CameraDisconnect(const FCameraDisconnect& ResultDelegate) override;
	int64 GetPreviewHandle() const override;

	void IncUserCount();
	void DecUserCount();
	bool CaptureImageToFileAsync(const FCameraCaptureImgToFileMulti& ResultDelegate);
	bool CaptureImageToTextureAsync(const FCameraCaptureImgToTextureMulti& ResultDelegate);
	bool StartRecordingAsync(const FCameraStartRecordingMulti& ResultDelegate);
	bool StopRecordingAsync(const FCameraStopRecordingMulti& ResultDelegate);
	bool SetLogDelegate(const FCameraLogMessageMulti& LogDelegate);
	bool IsCapturing() const;

private:
	bool TryPushNewCaptureTask(FCameraTask::EType InTaskType);

	FMagicLeapAPISetup APISetup;
	FTickerDelegate TickDelegate;
	FDelegateHandle TickDelegateHandle;
	uint32 UserCount;
	FCameraRunnable* Runnable;
	FCameraTask::EType CurrentTaskType;
	FCameraTask::EType PrevTaskType;
	FCameraConnect OnCameraConnect;
	FCameraDisconnect OnCameraDisconnect;
	FCameraCaptureImgToFileMulti OnCaptureImgToFile;
	FCameraCaptureImgToTextureMulti OnCaptureImgToTexture;
	FCameraStartRecordingMulti OnStartRecording;
	FCameraStopRecordingMulti OnStopRecording;
	FCameraLogMessageMulti OnLogMessage;
};

#define GET_MAGIC_LEAP_CAMERA_PLUGIN() static_cast<FMagicLeapCameraPlugin*>(FModuleManager::Get().GetModule("MagicLeapCamera"))
