// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
