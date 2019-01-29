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

#include "IMagicLeapPlugin.h"
#include "AppEventHandler.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_image_tracking.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

struct FTrackerMessage
{
	enum TaskType
	{
		None,
		Pause,
		Resume,
		UpdateSettings,
		TryCreateTarget,
		TargetCreateFailed,
		TargetCreateSucceeded,
	};

	TaskType TaskType;
	class FImageTrackerImpl* Requester;
#if WITH_MLSDK
	MLHandle PrevTarget;
	MLHandle Target;
	MLImageTrackerTargetStaticData Data;
	MLImageTrackerTargetSettings TargetSettings;
#endif //WITH_MLSDK
	FString TargetName;
	class UTexture2D* TargetImageTexture;
	bool bEnable;
	uint32 MaxTargets;

	FTrackerMessage()
	: TaskType(TaskType::None)
	, Requester(nullptr)
#if WITH_MLSDK
	, PrevTarget(ML_INVALID_HANDLE)
	, Target(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
	, TargetImageTexture(nullptr)
	, bEnable(true)
	, MaxTargets(1)
	{}
};

class FImageTrackerRunnable : public FRunnable, public MagicLeap::IAppEventHandler
{
public:
	FImageTrackerRunnable();
	virtual ~FImageTrackerRunnable();
	virtual uint32 Run() override;

#if WITH_MLSDK
	MLHandle GetHandle() const;
#endif //WITH_MLSDK
	void SetEnabled(bool bEnabled);
	bool GetEnabled();
	void SetMaxSimultaneousTargets(int32 MaxTargets);
	int32 GetMaxSimultaneousTargets();

	TQueue<FTrackerMessage, EQueueMode::Spsc> IncomingMessages;
	TQueue<FTrackerMessage, EQueueMode::Spsc> OutgoingMessages;

private:
	void OnAppPause() override;
	void OnAppResume() override;
	void TryPause();
	void TryResume();
	void OnAppShutDown() override;	
	void SetTarget();
	void UpdateTrackerSettings();

#if WITH_MLSDK
	MLHandle ImageTracker;
	MLImageTrackerSettings Settings;
#endif //WITH_MLSDK
	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;
	FCriticalSection SettingsMutex;
	FTrackerMessage CurrentMessage;
	const float RetryCreateTrackerWaitTime;
};