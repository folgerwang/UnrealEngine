// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OculusFunctionLibrary.h"
#include "OculusHMDPrivate.h"
#include "OculusHMD.h"

//-------------------------------------------------------------------------------------------------
// UOculusFunctionLibrary
//-------------------------------------------------------------------------------------------------

UOculusFunctionLibrary::UOculusFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

OculusHMD::FOculusHMD* UOculusFunctionLibrary::GetOculusHMD()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		if (GEngine->XRSystem->GetSystemName() == OculusHMD::FOculusHMD::OculusSystemName)
		{
			return static_cast<OculusHMD::FOculusHMD*>(GEngine->XRSystem.Get());
		}
	}
#endif
	return nullptr;
}

void UOculusFunctionLibrary::GetPose(FRotator& DeviceRotation, FVector& DevicePosition, FVector& NeckPosition, bool bUseOrienationForPlayerCamera, bool bUsePositionForPlayerCamera, const FVector PositionScale)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD && OculusHMD->IsHeadTrackingAllowed())
	{
		FQuat HeadOrientation = FQuat::Identity;
		FVector HeadPosition = FVector::ZeroVector;

		OculusHMD->GetCurrentPose(OculusHMD->HMDDeviceId, HeadOrientation, HeadPosition);

		DeviceRotation = HeadOrientation.Rotator();
		DevicePosition = HeadPosition;
		NeckPosition = OculusHMD->GetNeckPosition(HeadOrientation, HeadPosition);
	}
	else
#endif // #if OCULUS_HMD_SUPPORTED_PLATFORMS
	{
		DeviceRotation = FRotator::ZeroRotator;
		DevicePosition = FVector::ZeroVector;
		NeckPosition = FVector::ZeroVector;
	}
}

void UOculusFunctionLibrary::SetBaseRotationAndBaseOffsetInMeters(FRotator Rotation, FVector BaseOffsetInMeters, EOrientPositionSelector::Type Options)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		if ((Options == EOrientPositionSelector::Orientation) || (Options == EOrientPositionSelector::OrientationAndPosition))
		{
			OculusHMD->SetBaseRotation(Rotation);
		}
		if ((Options == EOrientPositionSelector::Position) || (Options == EOrientPositionSelector::OrientationAndPosition))
		{
			OculusHMD->SetBaseOffsetInMeters(BaseOffsetInMeters);
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::GetBaseRotationAndBaseOffsetInMeters(FRotator& OutRotation, FVector& OutBaseOffsetInMeters)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OutRotation = OculusHMD->GetBaseRotation();
		OutBaseOffsetInMeters = OculusHMD->GetBaseOffsetInMeters();
	}
	else
	{
		OutRotation = FRotator::ZeroRotator;
		OutBaseOffsetInMeters = FVector::ZeroVector;
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::GetRawSensorData(FVector& AngularAcceleration, FVector& LinearAcceleration, FVector& AngularVelocity, FVector& LinearVelocity, float& TimeInSeconds, ETrackedDeviceType DeviceType)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr && OculusHMD->IsHMDActive())
	{
		ovrpPoseStatef state;
		if (OVRP_SUCCESS(ovrp_GetNodePoseState3(ovrpStep_Render, OVRP_CURRENT_FRAMEINDEX, OculusHMD::ToOvrpNode(DeviceType), &state)))
		{
			AngularAcceleration = OculusHMD::ToFVector(state.AngularAcceleration);
			LinearAcceleration = OculusHMD::ToFVector(state.Acceleration);
			AngularVelocity = OculusHMD::ToFVector(state.AngularVelocity);
			LinearVelocity = OculusHMD::ToFVector(state.Velocity);
			TimeInSeconds = state.Time;
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

bool UOculusFunctionLibrary::IsDeviceTracked(ETrackedDeviceType DeviceType)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr && OculusHMD->IsHMDActive())
	{
		ovrpBool Present;
		if (OVRP_SUCCESS(ovrp_GetNodePresent2(OculusHMD::ToOvrpNode(DeviceType), &Present)))
		{
			return Present != ovrpBool_False;
		}
		else
		{
			return false;
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
	return false;
}

void UOculusFunctionLibrary::SetCPUAndGPULevels(int CPULevel, int GPULevel)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr && OculusHMD->IsHMDActive())
	{
		ovrp_SetSystemCpuLevel2(CPULevel);
		ovrp_SetSystemGpuLevel2(GPULevel);
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::SetReorientHMDOnControllerRecenter(bool recenterMode)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr && OculusHMD->IsHMDActive())
	{
		ovrpBool ovrpBoolRecenter = recenterMode ? ovrpBool_True : ovrpBool_False;
		ovrp_SetReorientHMDOnControllerRecenter(ovrpBoolRecenter);
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

bool UOculusFunctionLibrary::GetUserProfile(FHmdUserProfile& Profile)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD::FOculusHMD::UserProfile Data;
		if (OculusHMD->GetUserProfile(Data))
		{
			Profile.Name = "";
			Profile.Gender = "Unknown";
			Profile.PlayerHeight = 0.0f;
			Profile.EyeHeight = Data.EyeHeight;
			Profile.IPD = Data.IPD;
			Profile.NeckToEyeDistance = FVector2D(Data.EyeDepth, 0.0f);
			return true;
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
	return false;
}

void UOculusFunctionLibrary::SetBaseRotationAndPositionOffset(FRotator BaseRot, FVector PosOffset, EOrientPositionSelector::Type Options)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		if (Options == EOrientPositionSelector::Orientation || Options == EOrientPositionSelector::OrientationAndPosition)
		{
			OculusHMD->SetBaseRotation(BaseRot);
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::GetBaseRotationAndPositionOffset(FRotator& OutRot, FVector& OutPosOffset)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OutRot = OculusHMD->GetBaseRotation();
		OutPosOffset = FVector::ZeroVector;
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::AddLoadingSplashScreen(class UTexture2D* Texture, FVector TranslationInMeters, FRotator Rotation, FVector2D SizeInMeters, FRotator DeltaRotation, bool bClearBeforeAdd)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD::FSplash* Splash = OculusHMD->GetSplash();
		if (Splash)
		{
			if (bClearBeforeAdd)
			{
				Splash->ClearSplashes();
			}
			Splash->SetLoadingIconMode(false);

			FOculusSplashDesc Desc;
			Desc.LoadingTexture = Texture;
			Desc.QuadSizeInMeters = SizeInMeters;
			Desc.TransformInMeters = FTransform(Rotation, TranslationInMeters);
			Desc.DeltaRotation = FQuat(DeltaRotation);
			Splash->AddSplash(Desc);
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::ClearLoadingSplashScreens()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD::FSplash* Splash = OculusHMD->GetSplash();
		if (Splash)
		{
			Splash->ClearSplashes();
			Splash->SetLoadingIconMode(false);
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::ShowLoadingSplashScreen()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr && OculusHMD->IsStereoEnabledOnNextFrame())
	{
		OculusHMD::FSplash* Splash = OculusHMD->GetSplash();
		if (Splash)
		{
			Splash->SetLoadingIconMode(false);
			Splash->Show();
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::HideLoadingSplashScreen(bool bClear)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD::FSplash* Splash = OculusHMD->GetSplash();
		if (Splash)
		{
			Splash->Hide();
			if (bClear)
			{
				Splash->ClearSplashes();
			}
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::EnableAutoLoadingSplashScreen(bool bAutoShowEnabled)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD::FSplash* Splash = OculusHMD->GetSplash();
		if (Splash)
		{
			Splash->SetAutoShow(bAutoShowEnabled);
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

bool UOculusFunctionLibrary::IsAutoLoadingSplashScreenEnabled()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD::FSplash* Splash = OculusHMD->GetSplash();
		if (Splash)
		{
			return Splash->IsAutoShow();
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
	return false;
}

void UOculusFunctionLibrary::ShowLoadingIcon(class UTexture2D* Texture)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr && OculusHMD->IsStereoEnabledOnNextFrame())
	{
		OculusHMD::FSplash* Splash = OculusHMD->GetSplash();
		if (Splash)
		{
			Splash->ClearSplashes();
			FOculusSplashDesc Desc;
			Desc.LoadingTexture = Texture;
			Splash->AddSplash(Desc);
			Splash->SetLoadingIconMode(true);
			Splash->Show();
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::HideLoadingIcon()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD::FSplash* Splash = OculusHMD->GetSplash();
		if (Splash)
		{
			Splash->Hide();
			Splash->ClearSplashes();
			Splash->SetLoadingIconMode(false);
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

bool UOculusFunctionLibrary::IsLoadingIconEnabled()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD::FSplash* Splash = OculusHMD->GetSplash();
		if (Splash)
		{
			return Splash->IsLoadingIconMode();
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
	return false;
}


void UOculusFunctionLibrary::SetLoadingSplashParams(FString TexturePath, FVector DistanceInMeters, FVector2D SizeInMeters, FVector RotationAxis, float RotationDeltaInDeg)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD::FSplash* Splash = OculusHMD->GetSplash();
		if (Splash)
		{
			Splash->ClearSplashes();
			Splash->SetLoadingIconMode(false);
			FOculusSplashDesc Desc;
			Desc.TexturePath = TexturePath;
			Desc.QuadSizeInMeters = SizeInMeters;
			Desc.TransformInMeters = FTransform(DistanceInMeters);
			Desc.DeltaRotation = FQuat(RotationAxis, FMath::DegreesToRadians(RotationDeltaInDeg));
			Splash->AddSplash(Desc);
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::GetLoadingSplashParams(FString& TexturePath, FVector& DistanceInMeters, FVector2D& SizeInMeters, FVector& RotationAxis, float& RotationDeltaInDeg)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD::FSplash* Splash = OculusHMD->GetSplash();
		if (Splash)
		{
			FOculusSplashDesc Desc;
			if (Splash->GetSplash(0, Desc))
			{
				if (Desc.LoadingTexture && Desc.LoadingTexture->IsValidLowLevel())
				{
					TexturePath = Desc.LoadingTexture->GetPathName();
				}
				else
				{
					TexturePath = Desc.TexturePath.ToString();
				}
				DistanceInMeters = Desc.TransformInMeters.GetTranslation();
				SizeInMeters	 = Desc.QuadSizeInMeters;

				const FQuat rotation(Desc.DeltaRotation);
				rotation.ToAxisAndAngle(RotationAxis, RotationDeltaInDeg);
			}
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

bool UOculusFunctionLibrary::HasInputFocus()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	const OculusHMD::FOculusHMD* const OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr && OculusHMD->IsHMDActive())
	{
		ovrpBool HasFocus;
		if (OVRP_SUCCESS(ovrp_GetAppHasInputFocus(&HasFocus)))
		{
			return HasFocus != ovrpBool_False;
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
	return false;
}

bool UOculusFunctionLibrary::HasSystemOverlayPresent()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	const OculusHMD::FOculusHMD* const OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr && OculusHMD->IsHMDActive())
	{
		ovrpBool HasFocus;
		if (OVRP_SUCCESS(ovrp_GetAppHasInputFocus(&HasFocus)))
		{
			return HasFocus == ovrpBool_False;
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
	return false;
}

void UOculusFunctionLibrary::GetGPUUtilization(bool& IsGPUAvailable, float& GPUUtilization)
{
	IsGPUAvailable = false;
	GPUUtilization = 0.0f;

#if OCULUS_HMD_SUPPORTED_PLATFORMS
	const OculusHMD::FOculusHMD* const OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		ovrpBool GPUAvailable;
		if (OVRP_SUCCESS(ovrp_GetGPUUtilSupported(&GPUAvailable)))
		{
			IsGPUAvailable = (GPUAvailable != ovrpBool_False);
			ovrp_GetGPUUtilLevel(&GPUUtilization);
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

float UOculusFunctionLibrary::GetGPUFrameTime()
{
	float frameTime = 0;
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	const OculusHMD::FOculusHMD* const OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		if (OVRP_SUCCESS(ovrp_GetGPUFrameTime(&frameTime)))
		{
			return frameTime;
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
	return 0.0f;
}

void UOculusFunctionLibrary::SetTiledMultiresLevel(ETiledMultiResLevel level)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD->SetTiledMultiResLevel(level);
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}

ETiledMultiResLevel UOculusFunctionLibrary::GetTiledMultiresLevel()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		ovrpTiledMultiResLevel Lvl;
		if (OVRP_SUCCESS(ovrp_GetTiledMultiResLevel(&Lvl)))
		{
			return (ETiledMultiResLevel)Lvl;
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
	return ETiledMultiResLevel::ETiledMultiResLevel_Off;
}

FString UOculusFunctionLibrary::GetDeviceName()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		const char* NameString;
		if (OVRP_SUCCESS(ovrp_GetSystemProductName2(&NameString)) && NameString)
		{
			return FString(NameString);
		}
	}
#endif
	return FString();
}

TArray<float> UOculusFunctionLibrary::GetAvailableDisplayFrequencies()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		int NumberOfFrequencies;
		if (OVRP_SUCCESS(ovrp_GetSystemDisplayAvailableFrequencies(NULL, &NumberOfFrequencies)))
		{
			TArray<float> freqArray;
			freqArray.SetNum(NumberOfFrequencies);
			ovrp_GetSystemDisplayAvailableFrequencies(freqArray.GetData(), &NumberOfFrequencies);
			return freqArray;
		}
	}
#endif
	return TArray<float>();
}

float UOculusFunctionLibrary::GetCurrentDisplayFrequency()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		float Frequency;
		if (OVRP_SUCCESS(ovrp_GetSystemDisplayFrequency2(&Frequency)))
		{
			return Frequency;
		}
	}
#endif
	return 0.0f;
}

void UOculusFunctionLibrary::SetDisplayFrequency(float RequestedFrequency)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		ovrp_SetSystemDisplayFrequency(RequestedFrequency);
	}
#endif
}

void UOculusFunctionLibrary::EnablePositionTracking(bool bPositionTracking)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		ovrp_SetTrackingPositionEnabled2(bPositionTracking);
	}
#endif
}


void UOculusFunctionLibrary::EnableOrientationTracking(bool bOrientationTracking)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		ovrp_SetTrackingOrientationEnabled2(bOrientationTracking);
	}
#endif
}


class IStereoLayers* UOculusFunctionLibrary::GetStereoLayers()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		return OculusHMD;
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
	return nullptr;
}

/** Helper that converts EBoundaryType to ovrpBoundaryType */
#if OCULUS_HMD_SUPPORTED_PLATFORMS
static ovrpBoundaryType ToOvrpBoundaryType(EBoundaryType Source)
{
	switch (Source)
	{
	case EBoundaryType::Boundary_PlayArea:
		return ovrpBoundary_PlayArea;

	case EBoundaryType::Boundary_Outer:
	default:
		return ovrpBoundary_Outer;
	}
}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS

bool UOculusFunctionLibrary::IsGuardianDisplayed()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		ovrpBool boundaryVisible;
		return OVRP_SUCCESS(ovrp_GetBoundaryVisible2(&boundaryVisible)) && boundaryVisible;
	}
#endif
	return false;
}

TArray<FVector> UOculusFunctionLibrary::GetGuardianPoints(EBoundaryType BoundaryType)
{
	TArray<FVector> BoundaryPointList;
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		ovrpBoundaryType obt = ToOvrpBoundaryType(BoundaryType);
		int NumPoints = 0;

		if (OVRP_SUCCESS(ovrp_GetBoundaryGeometry3(obt, NULL, &NumPoints)))
		{
			//allocate points
			const int BufferSize = NumPoints;
			ovrpVector3f* BoundaryPoints = new ovrpVector3f[BufferSize];

			if (OVRP_SUCCESS(ovrp_GetBoundaryGeometry3(obt, BoundaryPoints, &NumPoints)))
			{
				NumPoints = FMath::Min(BufferSize, NumPoints);
				check(NumPoints <= BufferSize); // For static analyzer
				BoundaryPointList.Reserve(NumPoints);

				for (int i = 0; i < NumPoints; i++)
				{
					FVector point = OculusHMD->ScaleAndMovePointWithPlayer(BoundaryPoints[i]);
					BoundaryPointList.Add(point);
				}
			}

			delete[] BoundaryPoints;
		}
	}
#endif
	return BoundaryPointList;
}

FVector UOculusFunctionLibrary::GetGuardianDimensions(EBoundaryType BoundaryType)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		ovrpBoundaryType obt = ToOvrpBoundaryType(BoundaryType);
		ovrpVector3f Dimensions;

		if (OVRP_FAILURE(ovrp_GetBoundaryDimensions2(obt, &Dimensions)))
			return FVector::ZeroVector;

		Dimensions.z *= -1.0;
		return OculusHMD->ConvertVector_M2U(Dimensions);
	}
#endif
	return FVector::ZeroVector;
}

FTransform UOculusFunctionLibrary::GetPlayAreaTransform()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		int NumPoints = 4;
		ovrpVector3f BoundaryPoints[4];

		if (OVRP_SUCCESS(ovrp_GetBoundaryGeometry3(ovrpBoundary_PlayArea, BoundaryPoints, &NumPoints)))
		{	
			FVector ConvertedPoints[4];

			for (int i = 0; i < NumPoints; i++)
			{
				ConvertedPoints[i] = OculusHMD->ScaleAndMovePointWithPlayer(BoundaryPoints[i]);
			}

			float metersScale = OculusHMD->GetWorldToMetersScale();

			FVector Edge = ConvertedPoints[1] - ConvertedPoints[0];
			float Angle = FMath::Acos((Edge).GetSafeNormal() | FVector::RightVector);
			FQuat Rotation(FVector::UpVector, Edge.X < 0 ? Angle : -Angle);
			
			FVector Position = (ConvertedPoints[0] + ConvertedPoints[1] + ConvertedPoints[2] + ConvertedPoints[3]) / 4;
			FVector Scale(FVector::Distance(ConvertedPoints[3], ConvertedPoints[0]) / metersScale, FVector::Distance(ConvertedPoints[1], ConvertedPoints[0]) / metersScale, 1.0);

			return FTransform(Rotation, Position, Scale);
		}
	}
#endif
	return FTransform();
}

FGuardianTestResult UOculusFunctionLibrary::GetPointGuardianIntersection(const FVector Point, EBoundaryType BoundaryType)
{
	FGuardianTestResult InteractionInfo;
	memset(&InteractionInfo, 0, sizeof(FGuardianTestResult));

#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		ovrpVector3f OvrpPoint = OculusHMD->WorldLocationToOculusPoint(Point);
		ovrpBoundaryType OvrpBoundaryType = ToOvrpBoundaryType(BoundaryType);
		ovrpBoundaryTestResult InteractionResult;

		if (OVRP_SUCCESS(ovrp_TestBoundaryPoint2(OvrpPoint, OvrpBoundaryType, &InteractionResult)))
		{
			InteractionInfo.IsTriggering = (InteractionResult.IsTriggering != 0);
			InteractionInfo.ClosestDistance = OculusHMD->ConvertFloat_M2U(InteractionResult.ClosestDistance);
			InteractionInfo.ClosestPoint = OculusHMD->ScaleAndMovePointWithPlayer(InteractionResult.ClosestPoint);
			InteractionInfo.ClosestPointNormal = OculusHMD->ConvertVector_M2U(InteractionResult.ClosestPointNormal);
			InteractionInfo.DeviceType = ETrackedDeviceType::None;
		}
	}
#endif

	return InteractionInfo;
}

FGuardianTestResult UOculusFunctionLibrary::GetNodeGuardianIntersection(ETrackedDeviceType DeviceType, EBoundaryType BoundaryType)
{
	FGuardianTestResult InteractionInfo;
	memset(&InteractionInfo, 0, sizeof(FGuardianTestResult));

#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		ovrpNode OvrpNode = OculusHMD::ToOvrpNode(DeviceType);
		ovrpBoundaryType OvrpBoundaryType = ToOvrpBoundaryType(BoundaryType);
		ovrpBoundaryTestResult TestResult;

		if (OVRP_SUCCESS(ovrp_TestBoundaryNode2(OvrpNode, ovrpBoundary_PlayArea, &TestResult)) && TestResult.IsTriggering)
		{
			InteractionInfo.IsTriggering = true;
			InteractionInfo.DeviceType = OculusHMD::ToETrackedDeviceType(OvrpNode);
			InteractionInfo.ClosestDistance = OculusHMD->ConvertFloat_M2U(TestResult.ClosestDistance);
			InteractionInfo.ClosestPoint = OculusHMD->ScaleAndMovePointWithPlayer(TestResult.ClosestPoint);
			InteractionInfo.ClosestPointNormal = OculusHMD->ConvertVector_M2U(TestResult.ClosestPointNormal);
		}
	}
#endif

	return InteractionInfo;
}

void UOculusFunctionLibrary::SetGuardianVisibility(bool GuardianVisible)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OculusHMD::FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		ovrp_SetBoundaryVisible2(GuardianVisible);
	}
#endif
}
