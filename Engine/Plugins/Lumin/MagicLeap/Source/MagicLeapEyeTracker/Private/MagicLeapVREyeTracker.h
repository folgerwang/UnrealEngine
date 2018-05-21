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

