// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MixedRealityCaptureComponent.h"
#include "MotionControllerComponent.h"
#include "MixedRealityBillboard.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MediaPlayer.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Pawn.h" // for GetWorld() 
#include "Engine/World.h" // for GetPlayerControllerIterator()
#include "GameFramework/PlayerController.h" // for GetPawn()
#include "MixedRealityUtilLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MixedRealityConfigurationSaveGame.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MediaCaptureSupport.h"
#include "MixedRealityGarbageMatteCaptureComponent.h"
#include "Engine/StaticMesh.h"
#include "MRLatencyViewExtension.h"
#include "Misc/ConfigCacheIni.h"
#include "MixedRealitySettings.h"

#if WITH_EDITORONLY_DATA
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#endif

DEFINE_LOG_CATEGORY(LogMixedReality);

/* MRCaptureComponent_Impl
 *****************************************************************************/

namespace MRCaptureComponent_Impl
{
	static UMixedRealityGarbageMatteCaptureComponent* CreateGarbageMatteComponent(UMixedRealityCaptureComponent* Outer);
}

//------------------------------------------------------------------------------
static UMixedRealityGarbageMatteCaptureComponent* MRCaptureComponent_Impl::CreateGarbageMatteComponent(UMixedRealityCaptureComponent* Outer)
{
	ensureMsgf(Outer->IsActive(), TEXT("Spawning garbage mattes for a MR capture that isn't active."));

	UMixedRealityGarbageMatteCaptureComponent* NewGarbageMatteComp = NewObject<UMixedRealityGarbageMatteCaptureComponent>(Outer, TEXT("MR_GarbageMatteCapture"), RF_Transient | RF_TextExportTransient);
	NewGarbageMatteComp->CaptureSortPriority = Outer->CaptureSortPriority + 1;
	NewGarbageMatteComp->SetupAttachment(Outer);
	NewGarbageMatteComp->RegisterComponent();

	NewGarbageMatteComp->SetTrackingOrigin(Outer->GetAttachParent());
	
	return NewGarbageMatteComp;
}

/* UMixedRealityCaptureComponent
 *****************************************************************************/

//------------------------------------------------------------------------------
UMixedRealityCaptureComponent::UMixedRealityCaptureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAutoLoadConfiguration(true)
	, bProjectionDepthTracking(true)
{
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UMediaPlayer> DefaultMediaSource;
		ConstructorHelpers::FObjectFinder<UMaterial>    DefaultVideoProcessingMaterial;
		ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> DefaultRenderTarget;
#if WITH_EDITORONLY_DATA
		ConstructorHelpers::FObjectFinder<UStaticMesh>  EditorCameraMesh;
#endif

		FConstructorStatics()
			: DefaultMediaSource(TEXT("/MixedRealityFramework/MRCameraSource"))
			, DefaultVideoProcessingMaterial(TEXT("/MixedRealityFramework/M_MRCamSrcProcessing"))
			, DefaultRenderTarget(TEXT("/MixedRealityFramework/T_MRRenderTarget"))
#if WITH_EDITORONLY_DATA
			, EditorCameraMesh(TEXT("/Engine/EditorMeshes/MatineeCam_SM"))
#endif
		{}
	};
	static FConstructorStatics ConstructorStatics;

	MediaSource = ConstructorStatics.DefaultMediaSource.Object;
	VideoProcessingMaterial = ConstructorStatics.DefaultVideoProcessingMaterial.Object;
	TextureTarget = ConstructorStatics.DefaultRenderTarget.Object;

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		ProxyMesh = ConstructorStatics.EditorCameraMesh.Object;
	}
#endif

	// The default camera-processing (chroma keying) materials assume we're rendering with post-processing (they invert tonemapping, etc.).
	// Also, the spectator screen's back buffer expects the texture data to be in sRGB space (a conversion that happens in post-processing).
	// @TODO: Are we sure the resulting texture is in sRGB space? Unsure. We need to 100% confirm this
	CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	// For some reason, eye adaption isn't working with scene captures, which can result in a scene that is darker/lighter than expected.
	// So, for the time being, use the mobile exposure method.
	PostProcessSettings.bOverride_AutoExposureMethod = true;
	PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Basic;

	// ensure InitializeComponent() gets called
	bWantsInitializeComponent = true;
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
#if WITH_EDITORONLY_DATA
	UMixedRealityCaptureComponent* This = CastChecked<UMixedRealityCaptureComponent>(InThis);
	Collector.AddReferencedObject(This->ProxyMeshComponent);
#endif

	Super::AddReferencedObjects(InThis, Collector);
}
 
//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	AActor* MyOwner = GetOwner();
	if (MyOwner)
	{
		if (ProxyMeshComponent == nullptr)
		{
			ProxyMeshComponent = NewObject<UStaticMeshComponent>(MyOwner, NAME_None, RF_Transactional | RF_TextExportTransient);
			ProxyMeshComponent->SetupAttachment(this);
			ProxyMeshComponent->bIsEditorOnly = true;
			ProxyMeshComponent->SetStaticMesh(ProxyMesh);
			ProxyMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			ProxyMeshComponent->bHiddenInGame = true;
			ProxyMeshComponent->CastShadow    = false;
			ProxyMeshComponent->PostPhysicsComponentTick.bCanEverTick = false;
			ProxyMeshComponent->CreationMethod = CreationMethod;
			ProxyMeshComponent->RegisterComponent();
		}
	}
#endif
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	if (bIsActive)
	{
		RefreshDevicePairing();

		if (!ProjectionActor)
		{
			ProjectionActor = NewObject<UChildActorComponent>(this, TEXT("MR_ProjectionPlane"), RF_Transient | RF_TextExportTransient);
			ProjectionActor->SetChildActorClass(AMixedRealityProjectionActor::StaticClass());
			ProjectionActor->SetupAttachment(this);

			ProjectionActor->RegisterComponent();

			AMixedRealityProjectionActor* ProjectionActorObj = CastChecked<AMixedRealityProjectionActor>(ProjectionActor->GetChildActor());
			ProjectionActorObj->SetProjectionMaterial(VideoProcessingMaterial);
			ProjectionActorObj->SetProjectionAspectRatio(GetDesiredAspectRatio());

			if (ensure(ProjectionActorObj->ProjectionComponent))
			{
				ProjectionActorObj->ProjectionComponent->DepthOffset = ProjectionDepthOffset;
				ProjectionActorObj->ProjectionComponent->EnableHMDDepthTracking(bProjectionDepthTracking);
			}
		}

		RefreshCameraFeed();
	}	
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::Deactivate()
{
	Super::Deactivate();

	if (!bIsActive)
	{
		if (MediaSource)
		{
			MediaSource->Close();
		}

		// the GarbageMatte component's lifetime is governed by ApplyCalibrationData

		if (ProjectionActor)
		{
			ProjectionActor->DestroyComponent();
			ProjectionActor = nullptr;
		}

		if (PairedTracker)
		{
			PairedTracker->DestroyComponent(/*bPromoteChildren =*/true);
			PairedTracker = nullptr;
		}
	}
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (!VideoProcessingMaterial->IsA<UMaterialInstanceDynamic>())
	{
		SetVidProjectionMat(UMaterialInstanceDynamic::Create(VideoProcessingMaterial, this));
	}

	const UWorld* MyWorld = GetWorld();
	if (MyWorld && MyWorld->IsGameWorld() && bAutoLoadConfiguration)
	{
		LoadDefaultConfiguration();
	}

	float CalibratedFOVOverride = FOVAngle;
	if (GConfig->GetFloat(TEXT("/Script/MixedRealityFramework.MixedRealityFrameworkSettings"), TEXT("CalibratedFOVOverride"), CalibratedFOVOverride, GEngineIni))
	{
		FOVAngle = GetDefault<UMixedRealityFrameworkSettings>()->CalibratedFOVOverride;
	}

	RefreshCameraFeed();
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
#if WITH_EDITORONLY_DATA
	if (ProxyMeshComponent)
	{
		const FTransform WorldXform = GetComponentToWorld();
		ProxyMeshComponent->SetWorldTransform(WorldXform);
	}
#endif

	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
#if WITH_EDITORONLY_DATA
	if (ProxyMeshComponent)
	{
		ProxyMeshComponent->DestroyComponent();
	}
#endif 

	if (ProjectionActor)
	{
		ProjectionActor->DestroyComponent();
	}

	if (PairedTracker)
	{
		PairedTracker->DestroyComponent();
	}

	if (GarbageMatteCaptureComponent)
	{
		GarbageMatteCaptureComponent->ShowOnlyActors.Empty();
		GarbageMatteCaptureComponent->DestroyComponent();
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

//------------------------------------------------------------------------------
#if WITH_EDITOR
bool UMixedRealityCaptureComponent::GetEditorPreviewInfo(float /*DeltaTime*/, FMinimalViewInfo& ViewOut)
{
	ViewOut.Location = GetComponentLocation();
	ViewOut.Rotation = GetComponentRotation();

	ViewOut.FOV = FOVAngle;

	ViewOut.AspectRatio = GetDesiredAspectRatio();
	ViewOut.bConstrainAspectRatio = true;

	// see default in FSceneViewInitOptions
	ViewOut.bUseFieldOfViewForLOD = true;
	
 	ViewOut.ProjectionMode = ProjectionType;
	ViewOut.OrthoWidth = OrthoWidth;

	// see BuildProjectionMatrix() in SceneCaptureRendering.cpp
	ViewOut.OrthoNearClipPlane = 0.0f;
	ViewOut.OrthoFarClipPlane  = WORLD_MAX / 8.0f;;

	ViewOut.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.0f)
	{
		ViewOut.PostProcessSettings = PostProcessSettings;
	}

	return true;
}
#endif	// WITH_EDITOR

//------------------------------------------------------------------------------
const AActor* UMixedRealityCaptureComponent::GetViewOwner() const
{
	return GetProjectionActor();
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::UpdateSceneCaptureContents(FSceneInterface* Scene)
{ 
	if (!ViewExtension.IsValid())
	{
		ViewExtension = FSceneViewExtensions::NewExtension<FMRLatencyViewExtension>(this);
		FMotionDelayService::RegisterDelayClient(ViewExtension.ToSharedRef());
	}
	const bool bPreCommandQueued = ViewExtension->SetupPreCapture(Scene);

	Super::UpdateSceneCaptureContents(Scene);

	if (bPreCommandQueued)
	{
		ViewExtension->SetupPostCapture(Scene);
	}
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::RefreshCameraFeed()
{
	const UWorld* MyWorld = GetWorld();
	if (CaptureFeedRef.DeviceURL.IsEmpty() && bIsActive && HasBeenInitialized() && MyWorld && MyWorld->IsGameWorld())
	{
		TArray<FMediaCaptureDeviceInfo> CaptureDevices;
		MediaCaptureSupport::EnumerateVideoCaptureDevices(CaptureDevices);

		if (CaptureDevices.Num() > 0)
		{
			FMRCaptureFeedDelegate::FDelegate OnOpenCallback;
			OnOpenCallback.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UMixedRealityCaptureComponent, OnVideoFeedOpened));

			UAsyncTask_OpenMRCaptureDevice::OpenMRCaptureDevice(CaptureDevices[0], MediaSource, OnOpenCallback);
		}
	}
	else
	{
		SetCaptureDevice(CaptureFeedRef);
	}
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::RefreshDevicePairing()
{
	AActor* MyOwner = GetOwner();
	if (MyOwner && MyOwner->GetWorld() && MyOwner->GetWorld()->IsGameWorld())
	{
		if (!TrackingSourceName.IsNone())
		{
			USceneComponent* Parent = GetAttachParent();
			UMotionControllerComponent* PreDefinedTracker = Cast<UMotionControllerComponent>(Parent);
			const bool bNeedsInternalController = (!PreDefinedTracker || PreDefinedTracker->MotionSource != TrackingSourceName);

			if (bNeedsInternalController)
			{
				if (!PairedTracker)
				{
					PairedTracker = NewObject<UMotionControllerComponent>(this, TEXT("MR_MotionController"), RF_Transient | RF_TextExportTransient);
					
					USceneComponent* HMDRoot = UMixedRealityUtilLibrary::FindAssociatedHMDRoot(MyOwner);
					if (HMDRoot && HMDRoot->GetOwner() == MyOwner)
					{
						PairedTracker->SetupAttachment(HMDRoot);
					}
					else if (Parent)
					{
						PairedTracker->SetupAttachment(Parent, GetAttachSocketName());
					}
					else
					{
						MyOwner->SetRootComponent(PairedTracker);
					}

					PairedTracker->RegisterComponent();
					// if this is registered during initialization, then it will fail to auto-activate and won't track; so force it on here
					PairedTracker->Activate(/*bReset =*/false);

					FAttachmentTransformRules ReattachRules(EAttachmentRule::KeepRelative, /*bWeldSimulatedBodies =*/false);
					AttachToComponent(PairedTracker, ReattachRules);
				}

				PairedTracker->MotionSource = TrackingSourceName;
			}			
		}
		else if (PairedTracker)
		{
			DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			if (USceneComponent* NewParent = PairedTracker->GetAttachParent())
			{
				AttachToComponent(NewParent, FAttachmentTransformRules::KeepRelativeTransform);
			}
			
			if (MyOwner->GetRootComponent() == PairedTracker)
			{
				MyOwner->SetRootComponent(this);
			}

			PairedTracker->DestroyComponent();
			PairedTracker = nullptr;
		}
	}
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::SetVidProjectionMat(UMaterialInterface* NewMaterial)
{
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(NewMaterial);
	if (MID != nullptr)
	{
		ChromaKeySettings.ApplyToMaterial(MID);
		ApplyUVTextureToMaterial(MID);
	}
	// else, should we convert it to be a MID?

	VideoProcessingMaterial = NewMaterial;
	if (AMixedRealityProjectionActor* ProjectionTarget = GetProjectionActor())
	{
		ProjectionTarget->SetProjectionMaterial(NewMaterial);
	}
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::SetChromaSettings(const FChromaKeyParams& NewChromaSettings)
{
	NewChromaSettings.ApplyToMaterial(Cast<UMaterialInstanceDynamic>(VideoProcessingMaterial));
	ChromaKeySettings = NewChromaSettings;
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::SetDeviceAttachment(FName SourceName)
{
	TrackingSourceName = SourceName;
	RefreshDevicePairing();
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::DetatchFromDevice()
{
	TrackingSourceName = NAME_None;
	RefreshDevicePairing();
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::SetCaptureDevice(const FMRCaptureDeviceIndex& FeedRef)
{
	const UWorld* MyWorld = GetWorld();
	if (HasBeenInitialized() && bIsActive && MyWorld && MyWorld->IsGameWorld())
	{
		if (MediaSource)
		{
			if (!FeedRef.IsSet(MediaSource))
			{
				FMRCaptureFeedDelegate::FDelegate OnOpenCallback;
				OnOpenCallback.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UMixedRealityCaptureComponent, OnVideoFeedOpened));

				UAsyncTask_OpenMRCaptureFeed::OpenMRCaptureFeed(FeedRef, MediaSource, OnOpenCallback);
			}
			else
			{
				CaptureFeedRef = FeedRef;
				RefreshProjectionDimensions();
			}
		}
	}
	else
	{
		CaptureFeedRef = FeedRef;
	}
}

void UMixedRealityCaptureComponent::SetLensDistortionParameters(const FMRLensDistortion & ModelRef)
{
	if (ModelRef != LensDistortionParameters)
	{
		LensDistortionParameters = ModelRef;
		UpdateUVLookupTexture();
	}
}

void UMixedRealityCaptureComponent::SetLensDistortionCropping(float Alpha)
{
	if (LensDistortionCropping != Alpha)
	{
		LensDistortionCropping = Alpha;
		UpdateUVLookupTexture();
	}
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::SetTrackingDelay(int32 DelayMS)
{
	TrackingLatency = FMath::Max(DelayMS, 0);
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::SetProjectionDepthOffset(float DepthOffset)
{
	ProjectionDepthOffset = DepthOffset;

	AMixedRealityProjectionActor* ProjActor = GetProjectionActor();
	if (ProjActor && ProjActor->ProjectionComponent)
	{
		ProjActor->ProjectionComponent->DepthOffset = ProjectionDepthOffset;
	}
}

//------------------------------------------------------------------------------
AActor* UMixedRealityCaptureComponent::GetProjectionActor_K2() const
{
	return GetProjectionActor();
}

//------------------------------------------------------------------------------
AMixedRealityProjectionActor* UMixedRealityCaptureComponent::GetProjectionActor() const
{
	return ProjectionActor ? Cast<AMixedRealityProjectionActor>(ProjectionActor->GetChildActor()) : nullptr;
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::SetEnableProjectionDepthTracking(bool bEnable)
{
	bProjectionDepthTracking = bEnable;

	AMixedRealityProjectionActor* ProjActor = GetProjectionActor();
	if (ProjActor && ProjActor->ProjectionComponent)
	{
		ProjActor->ProjectionComponent->EnableHMDDepthTracking(bEnable);
	}
}

//------------------------------------------------------------------------------
float UMixedRealityCaptureComponent::GetDesiredAspectRatio() const
{
	float DesiredAspectRatio = 0.0f;

	if (MediaSource)
	{
		const int32 SelectedTrack = MediaSource->GetSelectedTrack(EMediaPlayerTrack::Video);
		DesiredAspectRatio = MediaSource->GetVideoTrackAspectRatio(SelectedTrack, MediaSource->GetTrackFormat(EMediaPlayerTrack::Video, SelectedTrack));
	}

	if (DesiredAspectRatio == 0.0f)
	{
		if (TextureTarget)
		{
			DesiredAspectRatio = TextureTarget->GetSurfaceWidth() / TextureTarget->GetSurfaceHeight();
		}
		else
		{
			DesiredAspectRatio = 16.f / 9.f;
		}
	}

	return DesiredAspectRatio;
}

void UMixedRealityCaptureComponent::UpdateUVLookupTexture()
{
	if (LensDistortionParameters.IsSet() && TextureTarget)
	{
		float OutVFOV, OutAspectRatio;
		UndistortionUVMap = LensDistortionParameters.CreateUndistortUVMap(FIntPoint(TextureTarget->SizeX, TextureTarget->SizeY), LensDistortionCropping, FOVAngle, OutVFOV, OutAspectRatio);
	}
	else
	{
		UndistortionUVMap = nullptr;
	}

	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(VideoProcessingMaterial);
	if (MID != nullptr)
	{
		ApplyUVTextureToMaterial(MID);
	}
}

void UMixedRealityCaptureComponent::ApplyUVTextureToMaterial(UMaterialInstanceDynamic* MID) const
{
	check(MID != nullptr);
	static FName EnableParamName("EnableMapping");
	MID->SetScalarParameterValue(EnableParamName, UndistortionUVMap ? 1.0f : 0.0f);
	static FName MapParamName("UVLookupTexture");
	MID->SetTextureParameterValue(MapParamName, UndistortionUVMap);
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::OnVideoFeedOpened(const FMRCaptureDeviceIndex& FeedRef)
{
	CaptureFeedRef = FeedRef;
	RefreshProjectionDimensions();

	OnCaptureSourceOpened.Broadcast(FeedRef);
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::RefreshProjectionDimensions()
{
	if (AMixedRealityProjectionActor* VidProjection = GetProjectionActor())
	{
		VidProjection->SetProjectionAspectRatio( GetDesiredAspectRatio() );
	}
}

//------------------------------------------------------------------------------
bool UMixedRealityCaptureComponent::SaveAsDefaultConfiguration_K2()
{
	return SaveAsDefaultConfiguration();
}

//------------------------------------------------------------------------------
bool UMixedRealityCaptureComponent::SaveAsDefaultConfiguration() const
{
	FString EmptySlotName;
	return SaveConfiguration(EmptySlotName, INDEX_NONE);
}

//------------------------------------------------------------------------------
bool UMixedRealityCaptureComponent::SaveConfiguration_K2(const FString& SlotName, int32 UserIndex)
{
	return SaveConfiguration(SlotName, UserIndex);
}

//------------------------------------------------------------------------------
bool UMixedRealityCaptureComponent::SaveConfiguration(const FString& SlotName, int32 UserIndex) const
{
	UMixedRealityCalibrationData* SaveGameInstance = ConstructCalibrationData();

	const UMixedRealityConfigurationSaveGame* DefaultSaveData = GetDefault<UMixedRealityConfigurationSaveGame>();
	check(DefaultSaveData);
	const FString& LocalSlotName  = SlotName.Len() > 0 ? SlotName  : DefaultSaveData->SaveSlotName;
	const uint32   LocalUserIndex = SlotName.Len() > 0 ? UserIndex : DefaultSaveData->UserIndex;

	const bool bSuccess = UGameplayStatics::SaveGameToSlot(SaveGameInstance, LocalSlotName, LocalUserIndex);
	if (bSuccess)
	{
		UE_LOG(LogMixedReality, Log, TEXT("UMixedRealityCaptureComponent::SaveConfiguration to slot %s user %i Succeeded."), *LocalSlotName, LocalUserIndex);
	} 
	else
	{
		UE_LOG(LogMixedReality, Warning, TEXT("UMixedRealityCaptureComponent::SaveConfiguration to slot %s user %i Failed!"), *LocalSlotName, LocalUserIndex);
	}
	return bSuccess;
}

//------------------------------------------------------------------------------
bool UMixedRealityCaptureComponent::LoadDefaultConfiguration()
{
	FString EmptySlotName;
	return LoadConfiguration(EmptySlotName, INDEX_NONE);
}

//------------------------------------------------------------------------------
bool UMixedRealityCaptureComponent::LoadConfiguration(const FString& SlotName, int32 UserIndex)
{
	const UMixedRealityConfigurationSaveGame* DefaultSaveData = GetDefault<UMixedRealityConfigurationSaveGame>();
	check(DefaultSaveData);
	const FString& LocalSlotName  = SlotName.Len() > 0 ? SlotName  : DefaultSaveData->SaveSlotName;
	const uint32   LocalUserIndex = SlotName.Len() > 0 ? UserIndex : DefaultSaveData->UserIndex;

	UMixedRealityCalibrationData* SaveGameInstance = Cast<UMixedRealityCalibrationData>(UGameplayStatics::LoadGameFromSlot(LocalSlotName, LocalUserIndex));
	if (SaveGameInstance == nullptr)
	{
		UE_LOG(LogMixedReality, Warning, TEXT("UMixedRealityCaptureComponent::LoadConfiguration from slot %s user %i Failed!"), *LocalSlotName, LocalUserIndex);
		return false;
	}

	ApplyCalibrationData(SaveGameInstance);

	UE_LOG(LogMixedReality, Log, TEXT("UMixedRealityCaptureComponent::LoadConfiguration from slot %s user %i Succeeded."), *LocalSlotName, LocalUserIndex);
	return true;
}

//------------------------------------------------------------------------------
UMixedRealityCalibrationData* UMixedRealityCaptureComponent::ConstructCalibrationData_Implementation() const
{
	UMixedRealityCalibrationData* ConfigData = NewObject<UMixedRealityCalibrationData>(GetTransientPackage());
	FillOutCalibrationData(ConfigData);

	return ConfigData;
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::FillOutCalibrationData(UMixedRealityCalibrationData* Dst) const
{
	if (Dst)
	{
		// view info
		{
			Dst->LensData.FOV = FOVAngle;
			Dst->LensData.DistortionParameters = LensDistortionParameters;
		}
		// alignment info
		{
			const FTransform RelativeXform = GetRelativeTransform();
			Dst->AlignmentData.CameraOrigin = RelativeXform.GetLocation();
			Dst->AlignmentData.Orientation = RelativeXform.GetRotation().Rotator();

			Dst->AlignmentData.TrackingAttachmentId = TrackingSourceName;
		}
		// compositing info
		{
			Dst->CompositingData.ChromaKeySettings = ChromaKeySettings;
			Dst->CompositingData.CaptureDeviceURL = CaptureFeedRef;
			Dst->CompositingData.DepthOffset = ProjectionDepthOffset;
			Dst->CompositingData.TrackingLatency = TrackingLatency;
		}
		// garbage matte
		{
			if (GarbageMatteCaptureComponent)
			{
				GarbageMatteCaptureComponent->GetGarbageMatteData(Dst->GarbageMatteSaveDatas);
			}
			else
			{
				Dst->GarbageMatteSaveDatas.Empty();
			}
		}
	}
}


//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::ApplyCalibrationData_Implementation(UMixedRealityCalibrationData* ConfigData)
{
	if (ConfigData)
	{
		// view data
		{
			FOVAngle = ConfigData->LensData.FOV;
			SetLensDistortionParameters(ConfigData->LensData.DistortionParameters);
		}
		// alignment data
		{
			SetDeviceAttachment(ConfigData->AlignmentData.TrackingAttachmentId);

			SetRelativeLocation(ConfigData->AlignmentData.CameraOrigin);
			SetRelativeRotation(ConfigData->AlignmentData.Orientation);
		}
		// compositing data
		{
			SetChromaSettings(ConfigData->CompositingData.ChromaKeySettings);
			SetCaptureDevice(ConfigData->CompositingData.CaptureDeviceURL);
			SetTrackingDelay(ConfigData->CompositingData.TrackingLatency);
			SetProjectionDepthOffset(ConfigData->CompositingData.DepthOffset);
		}
		// garbage matte
		{
			if (ConfigData->GarbageMatteSaveDatas.Num() > 0)
			{
				if (GarbageMatteCaptureComponent == nullptr)
				{
					GarbageMatteCaptureComponent = MRCaptureComponent_Impl::CreateGarbageMatteComponent(this);
				}
				GarbageMatteCaptureComponent->ApplyCalibrationData(ConfigData);
			}
			else if (GarbageMatteCaptureComponent)
			{
				GarbageMatteCaptureComponent->DestroyComponent();
				GarbageMatteCaptureComponent = nullptr;
			}
		}
	}	
}

//------------------------------------------------------------------------------
bool UMixedRealityCaptureComponent::SetGarbageMatteActor(AMixedRealityGarbageMatteActor* Actor)
{
	bool bSuccess = false;
	if (GarbageMatteCaptureComponent)
	{
		GarbageMatteCaptureComponent->SetGarbageMatteActor(Actor);
		bSuccess = true;
	}
	else if (bIsActive)
	{
		GarbageMatteCaptureComponent = MRCaptureComponent_Impl::CreateGarbageMatteComponent(this);
		GarbageMatteCaptureComponent->SetGarbageMatteActor(Actor);

		bSuccess = true;
	}

	return bSuccess;
}
