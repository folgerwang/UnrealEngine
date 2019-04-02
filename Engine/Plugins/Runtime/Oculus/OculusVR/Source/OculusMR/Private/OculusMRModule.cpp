// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "OculusMRModule.h"

#include "Engine/Engine.h"
#include "ISpectatorScreenController.h"
#include "IXRTrackingSystem.h"
#include "SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "OculusHMDModule.h"
#include "OculusHMD.h"
#include "OculusMRFunctionLibrary.h"
#include "OculusMRPrivate.h"
#include "OculusMR_Settings.h"
#include "OculusMR_State.h"
#include "OculusMR_CastingCameraActor.h"

#if WITH_EDITOR
#include "Editor.h" // for FEditorDelegates::PostPIEStarted
#endif

#define LOCTEXT_NAMESPACE "OculusMR"

namespace
{
	ovrpCameraDevice ConvertCameraDevice(EOculusMR_CameraDeviceEnum device)
	{
		if (device == EOculusMR_CameraDeviceEnum::CD_WebCamera0)
			return ovrpCameraDevice_WebCamera0;
		else if (device == EOculusMR_CameraDeviceEnum::CD_WebCamera1)
			return ovrpCameraDevice_WebCamera1;
		else if (device == EOculusMR_CameraDeviceEnum::CD_ZEDCamera)
			return ovrpCameraDevice_ZEDStereoCamera;
		checkNoEntry();
		return ovrpCameraDevice_None;
	}

	ovrpCameraDeviceDepthQuality ConvertCameraDepthQuality(EOculusMR_DepthQuality depthQuality)
	{
		if (depthQuality == EOculusMR_DepthQuality::DQ_Low)
		{
			return ovrpCameraDeviceDepthQuality_Low;
		}
		else if (depthQuality == EOculusMR_DepthQuality::DQ_Medium)
		{
			return ovrpCameraDeviceDepthQuality_Medium;
		}
		else if (depthQuality == EOculusMR_DepthQuality::DQ_High)
		{
			return ovrpCameraDeviceDepthQuality_High;
		}
		checkNoEntry();
		return ovrpCameraDeviceDepthQuality_Medium;
	}
}

FOculusMRModule::FOculusMRModule()
	: bInitialized(false)
	, MRSettings(nullptr)
	, MRState(nullptr)
	, MRActor(nullptr)
	, CurrentWorld(nullptr)
{}

FOculusMRModule::~FOculusMRModule()
{}

void FOculusMRModule::StartupModule()
{
#if OCULUS_MR_SUPPORTED_PLATFORMS
	const TCHAR* CmdLine = FCommandLine::Get();
	const bool bAutoOpenFromParams = FParse::Param(CmdLine, TEXT("mixedreality"));

	if (bAutoOpenFromParams && FOculusHMDModule::Get().PreInit() && OVRP_SUCCESS(ovrp_InitializeMixedReality()))
	{
		bInitialized = true;

		MRSettings = NewObject<UOculusMR_Settings>();
		MRSettings->AddToRoot();
		MRState = NewObject<UOculusMR_State>();
		MRState->AddToRoot();

		// Always bind the event handlers in case devs call them without MRC on
		MRSettings->TrackedCameraIndexChangeDelegate.BindRaw(this, &FOculusMRModule::OnTrackedCameraIndexChanged);
		MRSettings->CompositionMethodChangeDelegate.BindRaw(this, &FOculusMRModule::OnCompositionMethodChanged);
		MRSettings->CapturingCameraChangeDelegate.BindRaw(this, &FOculusMRModule::OnCapturingCameraChanged);
		MRSettings->IsCastingChangeDelegate.BindRaw(this, &FOculusMRModule::OnIsCastingChanged);
		MRSettings->UseDynamicLightingChangeDelegate.BindRaw(this, &FOculusMRModule::OnUseDynamicLightingChanged);
		MRSettings->DepthQualityChangeDelegate.BindRaw(this, &FOculusMRModule::OnDepthQualityChanged);

		ResetSettingsAndState();

		// Add all the event delegates to handle game start, end and level change
		WorldAddedEventBinding = GEngine->OnWorldAdded().AddRaw(this, &FOculusMRModule::OnWorldCreated);
		WorldDestroyedEventBinding = GEngine->OnWorldDestroyed().AddRaw(this, &FOculusMRModule::OnWorldDestroyed);
		WorldLoadEventBinding = FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FOculusMRModule::OnWorldCreated);
#if WITH_EDITOR
		// Bind events on PIE start/end to open/close camera
		PieBeginEventBinding = FEditorDelegates::BeginPIE.AddRaw(this, &FOculusMRModule::OnPieBegin);
		PieStartedEventBinding = FEditorDelegates::PostPIEStarted.AddRaw(this, &FOculusMRModule::OnPieStarted);
		PieEndedEventBinding = FEditorDelegates::PrePIEEnded.AddRaw(this, &FOculusMRModule::OnPieEnded);
#else
		// Start casting and open camera with the module if it's the game
		MRSettings->SetIsCasting(true);
#endif
	}
#endif
}

void FOculusMRModule::ShutdownModule()
{
#if OCULUS_MR_SUPPORTED_PLATFORMS
	if (bInitialized)
	{
		if (GEngine)
		{
			GEngine->OnWorldAdded().Remove(WorldAddedEventBinding);
			GEngine->OnWorldDestroyed().Remove(WorldDestroyedEventBinding);
			FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(WorldLoadEventBinding);
#if WITH_EDITOR
			FEditorDelegates::PostPIEStarted.Remove(PieStartedEventBinding);
			FEditorDelegates::PrePIEEnded.Remove(PieEndedEventBinding);
#else
			// Stop casting and close camera with module if it's the game
			MRSettings->SetIsCasting(false);
#endif
		}
		ovrp_ShutdownMixedReality();

		MRSettings->RemoveFromRoot();
		MRState->RemoveFromRoot();
	}
#endif
}

UOculusMR_Settings* FOculusMRModule::GetMRSettings()
{
	return MRSettings;
}

UOculusMR_State* FOculusMRModule::GetMRState()
{
	return MRState;
}

void FOculusMRModule::OnWorldCreated(UWorld* NewWorld)
{
#if WITH_EDITORONLY_DATA
	const bool bIsGameInst = !IsRunningCommandlet() && NewWorld->IsGameWorld();
	if (bIsGameInst)
#endif 
	{
		CurrentWorld = NewWorld;
		SetupInGameCapture(NewWorld);
	}
}

void FOculusMRModule::OnWorldDestroyed(UWorld* NewWorld)
{
	CurrentWorld = nullptr;
}

void FOculusMRModule::SetupExternalCamera()
{
	using namespace OculusHMD;

	if (!MRSettings->GetIsCasting())
	{
		return;
	}

	// Always request the MRC actor to handle a camera state change on its end
	MRState->ChangeCameraStateRequested = true;

	if (MRSettings->CompositionMethod == EOculusMR_CompositionMethod::DirectComposition)
	{
		ovrpBool available = ovrpBool_False;
		if (MRSettings->CapturingCamera == EOculusMR_CameraDeviceEnum::CD_None)
		{
			MRState->CurrentCapturingCamera = ovrpCameraDevice_None;
			UE_LOG(LogMR, Error, TEXT("CapturingCamera is set to CD_None which is invalid. Please pick a valid camera for CapturingCamera. If you are not sure, try to set it to CD_WebCamera0 and use the first connected USB web camera"));
			return;
		}

		MRState->CurrentCapturingCamera = ConvertCameraDevice(MRSettings->CapturingCamera);
		if (OVRP_FAILURE(ovrp_IsCameraDeviceAvailable2(MRState->CurrentCapturingCamera, &available)) || !available)
		{
			MRState->CurrentCapturingCamera = ovrpCameraDevice_None;
			UE_LOG(LogMR, Error, TEXT("CapturingCamera not available"));
			return;
		}

		ovrpSizei Size;
		if (MRState->TrackedCamera.Index >= 0)
		{
			Size.w = MRState->TrackedCamera.SizeX;
			Size.h = MRState->TrackedCamera.SizeY;
			ovrp_SetCameraDevicePreferredColorFrameSize(MRState->CurrentCapturingCamera, Size);
		}
		else
		{
			Size.w = 1280;
			Size.h = 720;
			ovrp_SetCameraDevicePreferredColorFrameSize(MRState->CurrentCapturingCamera, Size);
		}

		if (MRSettings->bUseDynamicLighting)
		{
			ovrpBool supportDepth = ovrpBool_False;
			if (OVRP_SUCCESS(ovrp_DoesCameraDeviceSupportDepth(MRState->CurrentCapturingCamera, &supportDepth)) && supportDepth)
			{
				ovrp_SetCameraDeviceDepthSensingMode(MRState->CurrentCapturingCamera, ovrpCameraDeviceDepthSensingMode_Fill);
				ovrp_SetCameraDevicePreferredDepthQuality(MRState->CurrentCapturingCamera, ConvertCameraDepthQuality(MRSettings->DepthQuality));
			}
		}
		ovrpBool cameraOpen;
		if (OVRP_FAILURE(ovrp_HasCameraDeviceOpened2(MRState->CurrentCapturingCamera, &cameraOpen)) || (!cameraOpen && OVRP_FAILURE(ovrp_OpenCameraDevice(MRState->CurrentCapturingCamera))))
		{
			MRState->CurrentCapturingCamera = ovrpCameraDevice_None;
			UE_LOG(LogMR, Error, TEXT("Cannot open CapturingCamera"));
			return;
		}
	}
	else if (MRSettings->CompositionMethod == EOculusMR_CompositionMethod::ExternalComposition)
	{
		// Close the camera device for external composition since we don't need the actual camera feed
		if (MRState->CurrentCapturingCamera != ovrpCameraDevice_None)
		{
			ovrp_CloseCameraDevice(MRState->CurrentCapturingCamera);
		}
	}
}

void FOculusMRModule::CloseExternalCamera()
{
	if (MRState->CurrentCapturingCamera != ovrpCameraDevice_None)
	{
		ovrp_CloseCameraDevice(MRState->CurrentCapturingCamera);
		MRState->CurrentCapturingCamera = ovrpCameraDevice_None;
	}
}

void FOculusMRModule::SetupInGameCapture(UWorld* World)
{
	// Don't do anything if we don't have a UWorld or if we are not casting
	if (World == nullptr || !MRSettings->GetIsCasting())
	{
		return;
	}

	// Set the bind camera request to true
	MRState->BindToTrackedCameraIndexRequested = true;

	// Don't add another actor if there's already a MRC camera actor
	for (TActorIterator<AOculusMR_CastingCameraActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		if (!ActorIt->IsPendingKillOrUnreachable() && ActorIt->IsValidLowLevel())
		{
			MRActor = *ActorIt;
			return;
		}
	}

	// Spawn an MRC camera actor if one wasn't already there
	MRActor = World->SpawnActorDeferred<AOculusMR_CastingCameraActor>(AOculusMR_CastingCameraActor::StaticClass(), FTransform::Identity);
	MRActor->InitializeStates(MRSettings, MRState);
	UGameplayStatics::FinishSpawningActor(MRActor, FTransform::Identity);
}

void FOculusMRModule::ResetSettingsAndState()
{
	// Reset MR State
	MRState->TrackedCamera = FTrackedCamera();
	MRState->TrackingReferenceComponent = nullptr;
	MRState->CurrentCapturingCamera = ovrpCameraDevice_None;
	MRState->ChangeCameraStateRequested = false;
	MRState->BindToTrackedCameraIndexRequested = false;

	// Reset MR Settings
	const bool bAutoOpenInExternalComposition = FParse::Param(FCommandLine::Get(), TEXT("externalcomposition"));
	const bool bAutoOpenInDirectComposition = FParse::Param(FCommandLine::Get(), TEXT("directcomposition"));
	MRSettings->BindToTrackedCameraIndexIfAvailable(0);
	MRSettings->LoadFromIni();

	// Save right after load to write defaults to the config if they weren't already there
	MRSettings->SaveToIni();

	if (bAutoOpenInExternalComposition)
	{
		MRSettings->CompositionMethod = EOculusMR_CompositionMethod::ExternalComposition;
	}
	else if (bAutoOpenInDirectComposition)
	{
		MRSettings->CompositionMethod = EOculusMR_CompositionMethod::DirectComposition;
	}
}

void FOculusMRModule::OnTrackedCameraIndexChanged(int OldVal, int NewVal)
{
	if (OldVal == NewVal)
	{
		return;
	}
	MRState->BindToTrackedCameraIndexRequested = true;
}

void FOculusMRModule::OnCompositionMethodChanged(EOculusMR_CompositionMethod OldVal, EOculusMR_CompositionMethod NewVal)
{
	if (OldVal == NewVal)
	{
		return;
	}
	SetupExternalCamera();
}

void FOculusMRModule::OnCapturingCameraChanged(EOculusMR_CameraDeviceEnum OldVal, EOculusMR_CameraDeviceEnum NewVal)
{
	if (OldVal == NewVal)
	{
		return;
	}

	// Close the old camera device before switching
	if (OldVal != EOculusMR_CameraDeviceEnum::CD_None)
	{
		auto CameraDevice = ConvertCameraDevice(OldVal);
		ovrp_CloseCameraDevice(CameraDevice);
	}
	SetupExternalCamera();
}

void FOculusMRModule::OnIsCastingChanged(bool OldVal, bool NewVal)
{
	if (OldVal == NewVal)
	{
		return;
	}
	if (NewVal == true)
	{
		// Initialize everything again if we turn MRC on
		SetupExternalCamera();
		SetupInGameCapture(CurrentWorld);
	}
	else
	{
		// Destory actor and close the camera when we turn MRC off
		if (MRActor != nullptr && MRActor->GetWorld() != nullptr)
		{
			MRActor->Destroy();
			MRActor = nullptr;
		}
		CloseExternalCamera();
	}
}

void FOculusMRModule::OnUseDynamicLightingChanged(bool OldVal, bool NewVal)
{
	if (OldVal == NewVal)
	{
		return;
	}
	SetupExternalCamera();
}

void FOculusMRModule::OnDepthQualityChanged(EOculusMR_DepthQuality OldVal, EOculusMR_DepthQuality NewVal)
{
	if (OldVal == NewVal)
	{
		return;
	}
	SetupExternalCamera();
}

#if WITH_EDITOR
void FOculusMRModule::OnPieBegin(bool bIsSimulating)
{
	// Reset all the parameters and start casting when PIE starts but before the game is initialized
	if (!bIsSimulating)
	{

		ResetSettingsAndState();

		// Always start casting with PIE (since this can only be reached if the command line param is on)
		MRSettings->SetIsCasting(true);
	}
}

void FOculusMRModule::OnPieStarted(bool bIsSimulating)
{
	// Handle the PIE world as a normal game world
	UWorld* PieWorld = GEditor->GetPIEWorldContext()->World();
	if (!bIsSimulating && PieWorld)
	{
		OnWorldCreated(PieWorld);
	}
}

void FOculusMRModule::OnPieEnded(bool bIsSimulating)
{
	UWorld* PieWorld = GEditor->GetPIEWorldContext()->World();
	if (!bIsSimulating && PieWorld)
	{
		// Stop casting when PIE ends
		MRSettings->SetIsCasting(false);
		OnWorldDestroyed(PieWorld);
	}
}
#endif // WITH_EDITOR

IMPLEMENT_MODULE( FOculusMRModule, OculusMR )

#undef LOCTEXT_NAMESPACE
