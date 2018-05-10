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
#include "Modules/ModuleManager.h"
#include "GameFramework/HUD.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MagicLeapEyeTrackerTypes.h"
#include "MagicLeapEyeTrackerModule.generated.h"

class FMagicLeapVREyeTracker;

USTRUCT(BlueprintType)
struct FMagicLeapEyeBlinkState
{
	GENERATED_BODY()

public:
	/** True if eyes are inside a blink. When not wearing the device, values can be arbitrary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye Blink State")
	bool LeftEyeBlinked;

	/** True if eyes are inside a blink. When not wearing the device, values can be arbitrary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye Blink State")
	bool RightEyeBlinked;
};

/**
* The public interface of the Magic Leap Eye Tracking Module.
*/
class IMagicLeapEyeTrackerModule : public IEyeTrackerModule
{
public:
	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though. Your module might have been
	* unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline IMagicLeapEyeTrackerModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IMagicLeapEyeTrackerModule>("MagicLeapEyeTracker");
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if
	* IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("MagicLeapEyeTracker");
	}

	virtual FString GetModuleKeyName() const override
	{
		return TEXT("MagicLeapEyeTracker");
	};
};


class FMagicLeapEyeTracker : public IEyeTracker
{
public:
	FMagicLeapEyeTracker();
	virtual ~FMagicLeapEyeTracker();

	
	/************************************************************************/
	/* IEyeTracker                                                          */
	/************************************************************************/
	virtual void SetEyeTrackedPlayer(APlayerController* PlayerController) override;
	virtual bool GetEyeTrackerGazeData(FEyeTrackerGazeData& OutGazeData) const override;
	virtual bool GetEyeTrackerStereoGazeData(FEyeTrackerStereoGazeData& OutGazeData) const override;
	virtual EEyeTrackerStatus GetEyeTrackerStatus() const override;
	virtual bool IsStereoGazeDataAvailable() const override;

	bool IsEyeTrackerCalibrated() const;
	bool GetEyeBlinkState(FMagicLeapEyeBlinkState& BlinkState) const;

private:
	friend class FMagicLeapEyeTrackerModule;

	FMagicLeapVREyeTracker* VREyeTracker;
};

class FMagicLeapEyeTrackerModule : public IMagicLeapEyeTrackerModule
{
	/************************************************************************/
	/* IInputDeviceModule                                                   */
	/************************************************************************/
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual TSharedPtr< class IEyeTracker, ESPMode::ThreadSafe > CreateEyeTracker() override;

	/************************************************************************/
	/* IEyeTrackerModule													*/
	/************************************************************************/

	/** Note: returns true if ANY Magic Leap eye tracker is connected (VR or Desktop) */
	virtual bool IsEyeTrackerConnected() const override;

	/************************************************************************/
	/* IMagicLeapCoreModule														*/
	/************************************************************************/

private:
	TSharedPtr<FMagicLeapEyeTracker, ESPMode::ThreadSafe> MagicLeapEyeTracker;
	FDelegateHandle OnDrawDebugHandle;
	FDelegateHandle OnPreLoadMapHandle;

	void OnDrawDebug(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);
};

UCLASS(ClassGroup = MagicLeap)
class MAGICLEAPEYETRACKER_API UMagicLeapEyeTrackerFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 
	  True if the calibration step was completed for this user.
	  If not, user should be advised to run the Eye Calibrator app on the device.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Eye Tracking|MagicLeap")
	static bool IsEyeTrackerCalibrated();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Eye Tracking|MagicLeap")
	static bool GetEyeBlinkState(FMagicLeapEyeBlinkState& BlinkState);
};
