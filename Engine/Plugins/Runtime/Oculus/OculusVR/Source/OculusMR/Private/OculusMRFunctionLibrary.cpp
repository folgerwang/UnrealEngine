// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OculusMRFunctionLibrary.h"
#include "OculusMRPrivate.h"
#include "OculusMRModule.h"
#include "OculusMR_CastingCameraActor.h"
#include "OculusMR_State.h"
#include "OculusHMD.h"
#include "OculusHMDPrivate.h"
#include "IHeadMountedDisplay.h"

#include "GameFramework/PlayerController.h"

//-------------------------------------------------------------------------------------------------
// UOculusFunctionLibrary
//-------------------------------------------------------------------------------------------------

UOculusMRFunctionLibrary::UOculusMRFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UOculusMRFunctionLibrary::GetAllTrackedCamera(TArray<FTrackedCamera>& TrackedCameras, bool bCalibratedOnly)
{
	TrackedCameras.Empty();

	if (!FOculusMRModule::IsAvailable() || !FOculusMRModule::Get().IsInitialized() )
	{
		UE_LOG(LogMR, Error, TEXT("OculusMR not available"));
		return;
	}

	if (ovrp_GetInitialized() == ovrpBool_False)
	{
		UE_LOG(LogMR, Error, TEXT("OVRPlugin not initialized"));
		return;
	}

	if (OVRP_FAILURE(ovrp_UpdateExternalCamera()))
	{
		UE_LOG(LogMR, Error, TEXT("ovrp_UpdateExternalCamera failure"));
		return;
	}

	int cameraCount = 0;
	if (OVRP_FAILURE(ovrp_GetExternalCameraCount(&cameraCount)))
	{
		UE_LOG(LogMR, Log, TEXT("ovrp_GetExternalCameraCount failure"));
		return;
	}

	for (int i = 0; i < cameraCount; ++i)
	{
		char cameraName[OVRP_EXTERNAL_CAMERA_NAME_SIZE];
		ovrpCameraIntrinsics cameraIntrinsics;
		ovrpCameraExtrinsics cameraExtrinsics;
		ovrp_GetExternalCameraName(i, cameraName);
		ovrp_GetExternalCameraIntrinsics(i, &cameraIntrinsics);
		ovrp_GetExternalCameraExtrinsics(i, &cameraExtrinsics);
		if ((bCalibratedOnly == false || cameraExtrinsics.CameraStatus == ovrpCameraStatus_Calibrated) && cameraIntrinsics.IsValid && cameraExtrinsics.IsValid)
		{
			FTrackedCamera camera;
			camera.Index = i;
			camera.Name = cameraName;
			camera.FieldOfView = FMath::RadiansToDegrees(FMath::Atan(cameraIntrinsics.FOVPort.LeftTan) + FMath::Atan(cameraIntrinsics.FOVPort.RightTan));
			camera.SizeX = cameraIntrinsics.ImageSensorPixelResolution.w;
			camera.SizeY = cameraIntrinsics.ImageSensorPixelResolution.h;
			camera.AttachedTrackedDevice = OculusHMD::ToETrackedDeviceType(cameraExtrinsics.AttachedToNode);
			OculusHMD::FPose Pose;
			GetOculusHMD()->ConvertPose(cameraExtrinsics.RelativePose, Pose);
			camera.CalibratedRotation = Pose.Orientation.Rotator();
			camera.CalibratedOffset = Pose.Position;
			camera.UserRotation = FRotator::ZeroRotator;
			camera.UserOffset = FVector::ZeroVector;
			TrackedCameras.Add(camera);
		}
	}
}

OculusHMD::FOculusHMD* UOculusMRFunctionLibrary::GetOculusHMD()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		static const FName OculusSystemName(TEXT("OculusHMD"));
		if (GEngine->XRSystem->GetSystemName() == OculusSystemName)
		{
			return static_cast<OculusHMD::FOculusHMD*>(GEngine->XRSystem.Get());
		}
	}
#endif
	return nullptr;
}

bool UOculusMRFunctionLibrary::GetTrackingReferenceLocationAndRotationInWorldSpace(USceneComponent* TrackingReferenceComponent, FVector& TRLocation, FRotator& TRRotation)
{
	if (!TrackingReferenceComponent)
	{
		APlayerController* PlayerController = GWorld->GetFirstPlayerController();
		if (!PlayerController)
		{
			return false;
		}
		APawn* Pawn = PlayerController->GetPawn();
		if (!Pawn)
		{
			return false;
		}
		TRLocation = Pawn->GetActorLocation();
		TRRotation = Pawn->GetActorRotation();
		return true;
	}
	else
	{
		TRLocation = TrackingReferenceComponent->GetComponentLocation();
		TRRotation = TrackingReferenceComponent->GetComponentRotation();
		return true;
	}
}

UOculusMR_Settings* UOculusMRFunctionLibrary::GetOculusMRSettings()
{
	return FOculusMRModule::Get().GetMRSettings();
}

USceneComponent* UOculusMRFunctionLibrary::GetTrackingReferenceComponent()
{
	return FOculusMRModule::Get().GetMRState()->TrackingReferenceComponent;
}

void UOculusMRFunctionLibrary::SetTrackingReferenceComponent(USceneComponent* Component)
{
	FOculusMRModule::Get().GetMRState()->TrackingReferenceComponent = Component;
}