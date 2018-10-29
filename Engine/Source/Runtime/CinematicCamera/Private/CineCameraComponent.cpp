// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CineCameraComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Materials/Material.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"

#define LOCTEXT_NAMESPACE "CineCameraComponent"


//////////////////////////////////////////////////////////////////////////
// UCameraComponent

/// @cond DOXYGEN_WARNINGS

UCineCameraComponent::UCineCameraComponent()
{
	// Super 35mm 4 Perf
	// These will be overridden if valid default presets are specified in ini
	FilmbackSettings.SensorWidth = 24.89f;
	FilmbackSettings.SensorHeight = 18.67;
	LensSettings.MinFocalLength = 50.f;
	LensSettings.MaxFocalLength = 50.f;
	LensSettings.MinFStop = 2.f;
	LensSettings.MaxFStop = 2.f;
	LensSettings.MinimumFocusDistance = 15.f;
	LensSettings.DiaphragmBladeCount = FPostProcessSettings::kDefaultDepthOfFieldBladeCount;

#if WITH_EDITORONLY_DATA
	bTickInEditor = true;
#endif
	
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;

	bConstrainAspectRatio = true;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Default to CircleDOF, but allow the user to customize it
	PostProcessSettings.DepthOfFieldMethod = DOFM_CircleDOF;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	RecalcDerivedData();

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		// overrides CameraComponent's camera mesh
		static ConstructorHelpers::FObjectFinder<UStaticMesh> EditorCameraMesh(TEXT("/Engine/EditorMeshes/Camera/SM_CineCam.SM_CineCam"));
		CameraMesh = EditorCameraMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/ArtTools/RenderToTexture/Meshes/S_1_Unit_Plane.S_1_Unit_Plane"));
	FocusPlaneVisualizationMesh = PlaneMesh.Object;

	static ConstructorHelpers::FObjectFinder<UMaterial> PlaneMat(TEXT("/Engine/EngineDebugMaterials/M_SimpleTranslucent.M_SimpleTranslucent"));
	FocusPlaneVisualizationMaterial = PlaneMat.Object;
#endif
}

void UCineCameraComponent::PostInitProperties()
{
	Super::PostInitProperties();

	// default filmback
	SetFilmbackPresetByName(DefaultFilmbackPresetName);
	SetLensPresetByName(DefaultLensPresetName);

	// other lens defaults
	CurrentAperture = DefaultLensFStop;
	CurrentFocalLength = DefaultLensFocalLength;

	RecalcDerivedData();
}

void UCineCameraComponent::PostLoad()
{
	RecalcDerivedData();
	bResetInterpolation = true;
	Super::PostLoad();
}

static const FColor DebugFocusPointSolidColor(102, 26, 204, 153);		// purple
static const FColor DebugFocusPointOutlineColor = FColor::Black;

void UCineCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
#if WITH_EDITORONLY_DATA
	// make sure drawing is set up
	if (FocusSettings.bDrawDebugFocusPlane)
	{
		if (DebugFocusPlaneComponent == nullptr)
		{
			CreateDebugFocusPlane();
		}

		UpdateDebugFocusPlane();
	}
	else
	{
		if (DebugFocusPlaneComponent != nullptr)
		{
			DestroyDebugFocusPlane();
		}
	}
#endif

#if ENABLE_DRAW_DEBUG
	if (FocusSettings.TrackingFocusSettings.bDrawDebugTrackingFocusPoint)
	{
		AActor const* const TrackedActor = FocusSettings.TrackingFocusSettings.ActorToTrack;

		FVector FocusPoint;
		if (TrackedActor)
		{
			FTransform const BaseTransform = TrackedActor->GetActorTransform();
			FocusPoint = BaseTransform.TransformPosition(FocusSettings.TrackingFocusSettings.RelativeOffset);
		}
		else
		{
			FocusPoint = FocusSettings.TrackingFocusSettings.RelativeOffset;
		}

		::DrawDebugSolidBox(GetWorld(), FocusPoint, FVector(12.f), DebugFocusPointSolidColor);
		::DrawDebugBox(GetWorld(), FocusPoint, FVector(12.f), DebugFocusPointOutlineColor);
	}
#endif // ENABLE_DRAW_DEBUG

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

#if WITH_EDITORONLY_DATA

void UCineCameraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	RecalcDerivedData();

	// handle debug focus plane
	if (FocusSettings.bDrawDebugFocusPlane && (DebugFocusPlaneComponent == nullptr))
	{
		CreateDebugFocusPlane();
	}
	else if ((FocusSettings.bDrawDebugFocusPlane == false) && (DebugFocusPlaneComponent != nullptr))
	{
		DestroyDebugFocusPlane();
	}

	// set focus plane color in case that's what changed
	if (DebugFocusPlaneMID)
	{
		DebugFocusPlaneMID->SetVectorParameterValue(FName(TEXT("Color")), FocusSettings.DebugFocusPlaneColor.ReinterpretAsLinear());
	}

	// reset interpolation if the user changes anything
	bResetInterpolation = true;

	UpdateDebugFocusPlane();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UCineCameraComponent::ResetProxyMeshTransform()
{
	if (ProxyMeshComponent)
	{
		// CineCam mesh is offset 90deg yaw
		ProxyMeshComponent->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
		ProxyMeshComponent->SetRelativeLocation(FVector(-46.f, 0, -24.f));
	}
}

#endif	// WITH_EDITORONLY_DATA

float UCineCameraComponent::GetHorizontalFieldOfView() const
{
	return (CurrentFocalLength > 0.f)
		? FMath::RadiansToDegrees(2.f * FMath::Atan(FilmbackSettings.SensorWidth / (2.f * CurrentFocalLength)))
		: 0.f;
}

float UCineCameraComponent::GetVerticalFieldOfView() const
{
	return (CurrentFocalLength > 0.f)
		? FMath::RadiansToDegrees(2.f * FMath::Atan(FilmbackSettings.SensorHeight / (2.f * CurrentFocalLength)))
		: 0.f;
}

FString UCineCameraComponent::GetFilmbackPresetName() const
{
	TArray<FNamedFilmbackPreset> const& Presets = UCineCameraComponent::GetFilmbackPresets();
	int32 const NumPresets = Presets.Num();
	for (int32 PresetIdx = 0; PresetIdx < NumPresets; ++PresetIdx)
	{
		FNamedFilmbackPreset const& P = Presets[PresetIdx];
		if (P.FilmbackSettings == FilmbackSettings)
		{
			return P.Name;
		}
	}

	return FString();
}

void UCineCameraComponent::SetFilmbackPresetByName(const FString& InPresetName)
{
	TArray<FNamedFilmbackPreset> const& Presets = UCineCameraComponent::GetFilmbackPresets();
	int32 const NumPresets = Presets.Num();
	for (int32 PresetIdx = 0; PresetIdx < NumPresets; ++PresetIdx)
	{
		FNamedFilmbackPreset const& P = Presets[PresetIdx];
		if (P.Name == InPresetName)
		{
			FilmbackSettings = P.FilmbackSettings;
			break;
		}
	}
}

FString UCineCameraComponent::GetLensPresetName() const
{
	TArray<FNamedLensPreset> const& Presets = UCineCameraComponent::GetLensPresets();
	int32 const NumPresets = Presets.Num();
	for (int32 PresetIdx = 0; PresetIdx < NumPresets; ++PresetIdx)
	{
		FNamedLensPreset const& P = Presets[PresetIdx];
		if (P.LensSettings == LensSettings)
		{
			return P.Name;
		}
	}

	return FString();
}

void UCineCameraComponent::SetLensPresetByName(const FString& InPresetName)
{
	TArray<FNamedLensPreset> const& Presets = UCineCameraComponent::GetLensPresets();
	int32 const NumPresets = Presets.Num();
	for (int32 PresetIdx = 0; PresetIdx < NumPresets; ++PresetIdx)
	{
		FNamedLensPreset const& P = Presets[PresetIdx];
		if (P.Name == InPresetName)
		{
			LensSettings = P.LensSettings;
			break;
		}
	}
}

float UCineCameraComponent::GetWorldToMetersScale() const
{
	UWorld const* const World = GetWorld();
	AWorldSettings const* const WorldSettings = World ? World->GetWorldSettings() : nullptr;
	return WorldSettings ? WorldSettings->WorldToMeters : 100.f;
}

// static
TArray<FNamedFilmbackPreset> const& UCineCameraComponent::GetFilmbackPresets()
{
	return GetDefault<UCineCameraComponent>()->FilmbackPresets;
}

// static
TArray<FNamedLensPreset> const& UCineCameraComponent::GetLensPresets()
{
	return GetDefault<UCineCameraComponent>()->LensPresets;
}

void UCineCameraComponent::RecalcDerivedData()
{
	// respect physical limits of the (simulated) hardware
	CurrentFocalLength = FMath::Clamp(CurrentFocalLength, LensSettings.MinFocalLength, LensSettings.MaxFocalLength);
	CurrentAperture = FMath::Clamp(CurrentAperture, LensSettings.MinFStop, LensSettings.MaxFStop);

	float const MinFocusDistInWorldUnits = LensSettings.MinimumFocusDistance * (GetWorldToMetersScale() / 1000.f);	// convert mm to uu
	FocusSettings.ManualFocusDistance = FMath::Max(FocusSettings.ManualFocusDistance, MinFocusDistInWorldUnits);

	FieldOfView = GetHorizontalFieldOfView();
	FilmbackSettings.SensorAspectRatio = (FilmbackSettings.SensorHeight > 0.f) ? (FilmbackSettings.SensorWidth / FilmbackSettings.SensorHeight) : 0.f;
	AspectRatio = FilmbackSettings.SensorAspectRatio;

#if WITH_EDITORONLY_DATA
	CurrentHorizontalFOV = FieldOfView;			// informational variable only, for editor users
#endif
}

/// @endcond

float UCineCameraComponent::GetDesiredFocusDistance(const FVector& InLocation) const
{
	float DesiredFocusDistance = 0.f;

	// get focus distance
	switch (FocusSettings.FocusMethod)
	{
	case ECameraFocusMethod::Manual:
		DesiredFocusDistance = FocusSettings.ManualFocusDistance;
		break;

	case ECameraFocusMethod::Tracking:
		{
			AActor const* const TrackedActor = FocusSettings.TrackingFocusSettings.ActorToTrack;

			FVector FocusPoint;
			if (TrackedActor)
			{
				FTransform const BaseTransform = TrackedActor->GetActorTransform();
				FocusPoint = BaseTransform.TransformPosition(FocusSettings.TrackingFocusSettings.RelativeOffset);
			}
			else
			{
				FocusPoint = FocusSettings.TrackingFocusSettings.RelativeOffset;
			}

			DesiredFocusDistance = (FocusPoint - InLocation).Size();
		}
		break;
	}
	
	// add in the adjustment offset
	DesiredFocusDistance += FocusSettings.FocusOffset;

	return DesiredFocusDistance;
}

void UCineCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	RecalcDerivedData();

	Super::GetCameraView(DeltaTime, DesiredView);

	UpdateCameraLens(DeltaTime, DesiredView);

	bResetInterpolation = false;
}

#if WITH_EDITOR
FText UCineCameraComponent::GetFilmbackText() const
{
	const float SensorWidth = FilmbackSettings.SensorWidth;
	const float SensorHeight = FilmbackSettings.SensorHeight;

	// Search presets for one that matches
	const FNamedFilmbackPreset* Preset = UCineCameraComponent::GetFilmbackPresets().FindByPredicate([&](const FNamedFilmbackPreset& InPreset) {
		return InPreset.FilmbackSettings.SensorWidth == SensorWidth && InPreset.FilmbackSettings.SensorHeight == SensorHeight;
	});

	if (Preset)
	{
		return FText::FromString(Preset->Name);
	}
	else
	{
		FNumberFormattingOptions Opts = FNumberFormattingOptions().SetMaximumFractionalDigits(1);
		return FText::Format(
			LOCTEXT("CustomFilmbackFormat", "Custom ({0}mm x {1}mm)"),
			FText::AsNumber(SensorWidth, &Opts),
			FText::AsNumber(SensorHeight, &Opts)
		);
	}
}
#endif

#if WITH_EDITORONLY_DATA
void UCineCameraComponent::UpdateDebugFocusPlane()
{
	if (FocusPlaneVisualizationMesh && DebugFocusPlaneComponent)
	{
		FVector const CamLocation = GetComponentTransform().GetLocation();
		FVector const CamDir = GetComponentTransform().GetRotation().Vector();

		UWorld const* const World = GetWorld();
		float const FocusDistance = (World && World->IsGameWorld()) ? CurrentFocusDistance : GetDesiredFocusDistance(CamLocation);		// in editor, use desired focus distance directly, no interp
		FVector const FocusPoint = GetComponentTransform().GetLocation() + CamDir * FocusDistance;

		DebugFocusPlaneComponent->SetWorldLocation(FocusPoint);
	}
}
#endif

void UCineCameraComponent::UpdateCameraLens(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	if (FocusSettings.FocusMethod == ECameraFocusMethod::None)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		DesiredView.PostProcessSettings.bOverride_DepthOfFieldMethod = false;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		DesiredView.PostProcessSettings.bOverride_DepthOfFieldFstop = false;
		DesiredView.PostProcessSettings.bOverride_DepthOfFieldMinFstop = false;
		DesiredView.PostProcessSettings.bOverride_DepthOfFieldBladeCount = false;
		DesiredView.PostProcessSettings.bOverride_DepthOfFieldFocalDistance = false;
		DesiredView.PostProcessSettings.bOverride_DepthOfFieldSensorWidth = false;
	}
	else
	{
		// Update focus/DoF
		DesiredView.PostProcessBlendWeight = 1.f;
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		DesiredView.PostProcessSettings.bOverride_DepthOfFieldMethod = true;
		DesiredView.PostProcessSettings.DepthOfFieldMethod = PostProcessSettings.DepthOfFieldMethod;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		DesiredView.PostProcessSettings.bOverride_DepthOfFieldFstop = true;
		DesiredView.PostProcessSettings.DepthOfFieldFstop = CurrentAperture;

		DesiredView.PostProcessSettings.bOverride_DepthOfFieldMinFstop = true;
		DesiredView.PostProcessSettings.DepthOfFieldMinFstop = LensSettings.MinFStop;

		DesiredView.PostProcessSettings.bOverride_DepthOfFieldBladeCount = true;
		DesiredView.PostProcessSettings.DepthOfFieldBladeCount = LensSettings.DiaphragmBladeCount;

		CurrentFocusDistance = GetDesiredFocusDistance(DesiredView.Location);

		// clamp to min focus distance
		float const MinFocusDistInWorldUnits = LensSettings.MinimumFocusDistance * (GetWorldToMetersScale() / 1000.f);	// convert mm to uu
		CurrentFocusDistance = FMath::Max(CurrentFocusDistance, MinFocusDistInWorldUnits);

		// smoothing, if desired
		if (FocusSettings.bSmoothFocusChanges)
		{
			if (bResetInterpolation == false)
			{
				CurrentFocusDistance = FMath::FInterpTo(LastFocusDistance, CurrentFocusDistance, DeltaTime, FocusSettings.FocusSmoothingInterpSpeed);
			}
		}
		LastFocusDistance = CurrentFocusDistance;

		DesiredView.PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
		DesiredView.PostProcessSettings.DepthOfFieldFocalDistance = CurrentFocusDistance;

		DesiredView.PostProcessSettings.bOverride_DepthOfFieldSensorWidth = true;
		DesiredView.PostProcessSettings.DepthOfFieldSensorWidth = FilmbackSettings.SensorWidth;
	}
}

void UCineCameraComponent::NotifyCameraCut()
{
	Super::NotifyCameraCut();

	// reset any interpolations
	bResetInterpolation = true;
}

#if WITH_EDITORONLY_DATA
void UCineCameraComponent::CreateDebugFocusPlane()
{
	if (AActor* const MyOwner = GetOwner())
	{
		if (DebugFocusPlaneComponent == nullptr)
		{
			DebugFocusPlaneComponent = NewObject<UStaticMeshComponent>(MyOwner, NAME_None, RF_Transactional | RF_TextExportTransient);
			DebugFocusPlaneComponent->SetupAttachment(this);
			DebugFocusPlaneComponent->SetIsVisualizationComponent(true);
			DebugFocusPlaneComponent->SetStaticMesh(FocusPlaneVisualizationMesh);
			DebugFocusPlaneComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			DebugFocusPlaneComponent->bHiddenInGame = false;
			DebugFocusPlaneComponent->CastShadow = false;
			DebugFocusPlaneComponent->PostPhysicsComponentTick.bCanEverTick = false;
			DebugFocusPlaneComponent->CreationMethod = CreationMethod;
			DebugFocusPlaneComponent->bSelectable = false;

			DebugFocusPlaneComponent->RelativeScale3D = FVector(10000.f, 10000.f, 1.f);
			DebugFocusPlaneComponent->RelativeRotation = FRotator(90.f, 0.f, 0.f);

			DebugFocusPlaneComponent->RegisterComponentWithWorld(GetWorld());

			DebugFocusPlaneMID = DebugFocusPlaneComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(0, FocusPlaneVisualizationMaterial);
			if (DebugFocusPlaneMID)
			{
				DebugFocusPlaneMID->SetVectorParameterValue(FName(TEXT("Color")), FocusSettings.DebugFocusPlaneColor.ReinterpretAsLinear());
			}
		}
	}
}

void UCineCameraComponent::DestroyDebugFocusPlane()
{
	if (DebugFocusPlaneComponent)
	{
		DebugFocusPlaneComponent->SetVisibility(false);
		DebugFocusPlaneComponent = nullptr;

		DebugFocusPlaneMID = nullptr;
	}
}
#endif

void UCineCameraComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	ResetProxyMeshTransform();
#endif
}

#if WITH_EDITOR
void UCineCameraComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	if (DebugFocusPlaneComponent)
	{
		DebugFocusPlaneComponent->DestroyComponent();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
