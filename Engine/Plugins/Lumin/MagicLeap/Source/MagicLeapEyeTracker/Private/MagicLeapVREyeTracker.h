// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IEyeTrackerModule.h"
#include "EyeTrackerTypes.h"
#include "IEyeTracker.h"
#include "IMagicLeapVREyeTracker.h"
#include "MagicLeapEyeTrackerTypes.h"
#include "Containers/Ticker.h"
#include "GameFramework/HUD.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_api.h>
#include <ml_eye_tracking.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

class FMagicLeapVREyeTracker : public IMagicLeapVREyeTracker, public FTickerObjectBase
{
public:
	FMagicLeapVREyeTracker();
	virtual ~FMagicLeapVREyeTracker();

	void SetDefaultDataValues();

	void SetActivePlayerController(APlayerController* NewActivePlayerController);
	APlayerController* GetActivePlayerController() const { return ActivePlayerController.Get(); }

	virtual bool Tick(float DeltaTime) override;

	void DrawDebug(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);

	bool IsEyeTrackerCalibrated() const;

public:
	virtual const FMagicLeapVREyeTrackingData& GetVREyeTrackingData() override;

	virtual EMagicLeapEyeTrackingStatus GetEyeTrackingStatus() override;

private:
	TWeakObjectPtr<APlayerController> ActivePlayerController;
	EMagicLeapEyeTrackingStatus EyeTrackingStatus;

	FMagicLeapVREyeTrackingData UnfilteredEyeTrackingData;

	
	bool bReadyToInit;
	bool bInitialized;
	bool bIsCalibrated;

#if WITH_MLSDK
	MLHandle EyeTrackingHandle;
	MLEyeTrackingStaticData EyeTrackingStaticData;
#endif //WITH_MLSDK
};

