// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IEyeTrackerModule.h"
#include "EyeTrackerTypes.h"
#include "IEyeTracker.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/HUD.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MagicLeapEyeTrackerTypes.h"
#include "IMagicLeapModule.h"
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

USTRUCT(BlueprintType)
struct FMagicLeapFixationComfort
{
	GENERATED_BODY()

public:
	/**
	  Is the user's fixation point too close for sustained use. This value is true if the user is
      focused on a point that is within 37 cm of the eyeball centers. 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye Fixation Comfort")
	bool FixationDepthIsUncomfortable;

	/** True if the user has fixated on a point closer than 37 cm for longer than 10 seconds within the last minute. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye Fixation Comfort")
	bool FixationDepthViolationHasOccurred;

	/**
	  Number of seconds remaining that the user may be fixated at an uncomfortable depth. If this
	  persists for too long, the system may take action to move the fixation point further away.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye Fixation Comfort")
	float RemainingTimeAtUncomfortableDepth;
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
	void Destroy();
	virtual void SetEyeTrackedPlayer(APlayerController* PlayerController) override;
	virtual bool GetEyeTrackerGazeData(FEyeTrackerGazeData& OutGazeData) const override;
	virtual bool GetEyeTrackerStereoGazeData(FEyeTrackerStereoGazeData& OutGazeData) const override;
	virtual EEyeTrackerStatus GetEyeTrackerStatus() const override;
	virtual bool IsStereoGazeDataAvailable() const override;

	bool IsEyeTrackerCalibrated() const;
	bool GetEyeBlinkState(FMagicLeapEyeBlinkState& BlinkState) const;
	bool GetFixationComfort(FMagicLeapFixationComfort& FixationComfort) const;

	EMagicLeapEyeTrackingCalibrationStatus GetCalibrationStatus() const;

	inline FMagicLeapVREyeTracker* GetVREyeTracker() const
	{
		return VREyeTracker;
	}

private:
	FMagicLeapVREyeTracker* VREyeTracker;
};

class FMagicLeapEyeTrackerModule : public IMagicLeapEyeTrackerModule, public IMagicLeapModule
{
	/************************************************************************/
	/* IInputDeviceModule                                                   */
	/************************************************************************/
public:
	FMagicLeapEyeTrackerModule();
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual void Disable() override;
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
	  False if the calibration status is none, otherwise returns true, even with a bad calibration.
	  If not, user should be advised to run the Eye Calibrator app on the device.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DeprecatedFunction, DeprecationMessage = "Please use GetCalibrationStatus instead"), Category = "Eye Tracking|MagicLeap")
	static bool IsEyeTrackerCalibrated();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Eye Tracking|MagicLeap")
	static bool GetEyeBlinkState(FMagicLeapEyeBlinkState& BlinkState);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Eye Tracking|MagicLeap")
	static bool GetFixationComfort(FMagicLeapFixationComfort& FixationComfort);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EyeTracking|MagicLeap")
	static EMagicLeapEyeTrackingCalibrationStatus GetCalibrationStatus();
};
