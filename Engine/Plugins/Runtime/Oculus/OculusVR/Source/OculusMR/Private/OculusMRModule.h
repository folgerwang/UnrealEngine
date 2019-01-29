// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IOculusMRModule.h"

#define LOCTEXT_NAMESPACE "OculusMR"

enum class EOculusMR_CompositionMethod : uint8;
enum class EOculusMR_CameraDeviceEnum : uint8;
enum class EOculusMR_DepthQuality : uint8;

class UOculusMR_Settings;
class AOculusMR_CastingCameraActor;
class UOculusMR_State;

//-------------------------------------------------------------------------------------------------
// FOculusInputModule
//-------------------------------------------------------------------------------------------------

class FOculusMRModule : public IOculusMRModule
{
public:
	FOculusMRModule();
	~FOculusMRModule();

	static inline FOculusMRModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FOculusMRModule >("OculusMR");
	}

	// IOculusMRModule
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool IsInitialized() { return bInitialized; }

	UOculusMR_Settings* GetMRSettings();
	UOculusMR_State* GetMRState();

private:
	bool bInitialized;
	UOculusMR_Settings* MRSettings;
	UOculusMR_State* MRState;
	AOculusMR_CastingCameraActor* MRActor;
	UWorld* CurrentWorld;

	FDelegateHandle WorldAddedEventBinding;
	FDelegateHandle WorldDestroyedEventBinding;
	FDelegateHandle WorldLoadEventBinding;

	/** Initialize the tracked physical camera */
	void SetupExternalCamera();
	/** Close the tracked physical camera */
	void CloseExternalCamera();
	/** Set up the needed settings and actors for MRC in-game */
	void SetupInGameCapture(UWorld* World);
	/** Reset all the MRC settings and state to the config and default */
	void ResetSettingsAndState();

	/** Handle changes on specific settings */
	void OnCompositionMethodChanged(EOculusMR_CompositionMethod OldVal, EOculusMR_CompositionMethod NewVal);
	void OnCapturingCameraChanged(EOculusMR_CameraDeviceEnum OldVal, EOculusMR_CameraDeviceEnum NewVal);
	void OnIsCastingChanged(bool OldVal, bool NewVal);
	void OnUseDynamicLightingChanged(bool OldVal, bool NewVal);
	void OnDepthQualityChanged(EOculusMR_DepthQuality OldVal, EOculusMR_DepthQuality NewVal);
	void OnTrackedCameraIndexChanged(int OldVal, int NewVal);

	void OnWorldCreated(UWorld* NewWorld);
	void OnWorldDestroyed(UWorld* NewWorld);
#if WITH_EDITOR
	FDelegateHandle PieBeginEventBinding;
	FDelegateHandle PieStartedEventBinding;
	FDelegateHandle PieEndedEventBinding;

	void OnPieBegin(bool bIsSimulating);
	void OnPieStarted(bool bIsSimulating);
	void OnPieEnded(bool bIsSimulating);
#endif
};


#undef LOCTEXT_NAMESPACE
