// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneCaptureComponent2D.h"
#include "InputCoreTypes.h" // for EControllerHand
#include "Math/Color.h" // for FLinearColor
#include "MixedRealityCaptureDevice.h"
#include "Delegates/Delegate.h"
#include "Templates/SubclassOf.h"
#include "MixedRealityConfigurationSaveGame.h"
#include "MixedRealityLensDistortion.h"
#include "MixedRealityCaptureComponent.generated.h"

class UMediaPlayer;
class UMaterial;
class UMixedRealityBillboard;
class UChildActorComponent;
class AMixedRealityProjectionActor;
class UMotionControllerComponent;
class AStaticMeshActor;
class USceneCaptureComponent2D;
class UMixedRealityGarbageMatteCaptureComponent;
class UTextureRenderTarget2D;
class FMRLatencyViewExtension;
class UMixedRealityCalibrationData;
class AMixedRealityGarbageMatteActor;

DECLARE_LOG_CATEGORY_EXTERN(LogMixedReality, Log, All);

/**
 *	
 */
UCLASS(ClassGroup = Rendering, editinlinenew, Blueprintable, BlueprintType, config = Engine, meta = (BlueprintSpawnableComponent))
class MIXEDREALITYFRAMEWORK_API UMixedRealityCaptureComponent : public USceneCaptureComponent2D
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=VideoCapture)
	UMediaPlayer* MediaSource;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetVidProjectionMat, Category=Composition)
	UMaterialInterface* VideoProcessingMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetChromaSettings, Category=Composition)
	FChromaKeyParams ChromaKeySettings;

	UPROPERTY(BlueprintReadWrite, BlueprintSetter=SetCaptureDevice, Category=Composition)
	FMRCaptureDeviceIndex CaptureFeedRef;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetLensDistortionParameters, Category=VideoCapture)
	FMRLensDistortion LensDistortionParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetLensDistortionCropping, Category = VideoCapture)
	float LensDistortionCropping;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Tracking)
	FName TrackingSourceName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Composition)
	UTextureRenderTarget2D* GarbageMatteCaptureTextureTarget;

	/** Millisecond delay to apply to motion controller components when rendering to the capture view (to better align with latent camera feeds) */
	UPROPERTY(BlueprintReadWrite, Config, BlueprintSetter=SetTrackingDelay, Category=Composition, meta=(ClampMin="0", UIMin="0"))
	int32 TrackingLatency;

	/** Determines if this component should attempt to load the default MR calibration file on initialization */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category=Calibration)
	bool bAutoLoadConfiguration;

	/** Depth offset (in UE units) for the card that the camera feed is projected onto. By default the card is aligned with the HMD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, BlueprintSetter=SetProjectionDepthOffset, Category=Composition)
	float ProjectionDepthOffset;

	/** Enabled by default, the projection plane tracks with the HMD to simulate the depth of the player. Disable to keep the projection plane from moving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, BlueprintSetter=SetEnableProjectionDepthTracking, Category=Tracking)
	bool bProjectionDepthTracking;

public:
	//~ UObject interface

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

public:
	//~ UActorComponent interface

	virtual void OnRegister() override;
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;
	virtual void InitializeComponent() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

public:
	//~ USceneComponent interface

#if WITH_EDITOR
	virtual bool GetEditorPreviewInfo(float DeltaTime, FMinimalViewInfo& ViewOut) override;
#endif 

public:
	//~ USceneCaptureComponent interface

	virtual const AActor* GetViewOwner() const override;
	virtual void UpdateSceneCaptureContents(FSceneInterface* Scene) override;

public:
	//~ Blueprint API	

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Calibration", meta = (DisplayName = "SaveAsDefaultConfiguration"))
	bool SaveAsDefaultConfiguration_K2(); // non-const to make it an exec node
	bool SaveAsDefaultConfiguration() const;

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Calibration", meta = (DisplayName="SaveConfiguration"))
	bool SaveConfiguration_K2(const FString& SlotName, int32 UserIndex); // non-const to make it an exec node
	bool SaveConfiguration(const FString& SlotName, int32 UserIndex) const;

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Calibration")
	bool LoadDefaultConfiguration();

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Calibration")
	bool LoadConfiguration(const FString& SlotName, int32 UserIndex);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MixedReality|Calibration")
	UMixedRealityCalibrationData* ConstructCalibrationData() const;

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Calibration")
	void FillOutCalibrationData(UMixedRealityCalibrationData* Dst) const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MixedReality|Calibration")
	void ApplyCalibrationData(UMixedRealityCalibrationData* ConfigData);

	/**
	 * Set an external garbage matte actor to be used instead of the mixed reality component's
	 * normal configuration save game based actor.  This is used during garbage matte setup to
	 * preview the garbage mask in realtime.
	*/
	UFUNCTION(BlueprintCallable, Category = "MixedReality|GarbageMatting")
	bool SetGarbageMatteActor(AMixedRealityGarbageMatteActor* Actor);

	UFUNCTION(BlueprintSetter)
	void SetVidProjectionMat(UMaterialInterface* NewMaterial);

	UFUNCTION(BlueprintSetter)
	void SetChromaSettings(const FChromaKeyParams& NewChromaSettings);

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Tracking")
	void SetDeviceAttachment(FName SourceName);

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Tracking")
	void DetatchFromDevice();

	UFUNCTION(BlueprintSetter)
	void SetCaptureDevice(const FMRCaptureDeviceIndex& FeedRef);

	UFUNCTION(BlueprintSetter)
	void SetLensDistortionParameters(const FMRLensDistortion& ModelRef);

	UFUNCTION(BlueprintSetter)
	void SetLensDistortionCropping(float Alpha);

	UFUNCTION(BlueprintSetter)
	void SetTrackingDelay(int32 DelayMS);

	UFUNCTION(BlueprintSetter)
	void SetProjectionDepthOffset(float DepthOffset);

	UFUNCTION(BlueprintPure, Category = "MixedReality|Projection", meta=(DisplayName="GetProjectionActor"))
	AActor* GetProjectionActor_K2() const;
	AMixedRealityProjectionActor* GetProjectionActor() const;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMRCaptureFeedOpenedDelegate, const FMRCaptureDeviceIndex&, FeedRef);
	UPROPERTY(BlueprintAssignable, Category = VideoCapture)
	FMRCaptureFeedOpenedDelegate OnCaptureSourceOpened;

	/** 
	 * Enabled by default, the projection plane tracks with the HMD to simulate 
	 * the depth of the player. Disable to keep the projection plane from moving.
	 */
	UFUNCTION(BlueprintSetter)
	void SetEnableProjectionDepthTracking(bool bEnable = true);

public:
	/**
	 *
	 */
	void RefreshCameraFeed();

	/**
	 *
	 */
	void RefreshDevicePairing();


private:
	UFUNCTION() // needs to be a UFunction for binding purposes
	void OnVideoFeedOpened(const FMRCaptureDeviceIndex& FeedRef);

	void  RefreshProjectionDimensions();
	float GetDesiredAspectRatio() const;

	void UpdateUVLookupTexture();

	void ApplyUVTextureToMaterial(class UMaterialInstanceDynamic* MID) const;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	UStaticMesh* ProxyMesh;

	UPROPERTY(Transient)
	UStaticMeshComponent* ProxyMeshComponent;
#endif

	UPROPERTY(Transient)
	UChildActorComponent* ProjectionActor;

	UPROPERTY(Transient)
	UMotionControllerComponent* PairedTracker;

	UPROPERTY(Transient)
	UMixedRealityGarbageMatteCaptureComponent* GarbageMatteCaptureComponent;

	UPROPERTY(Transient)
	UTexture2D* UndistortionUVMap;

	TSharedPtr<FMRLatencyViewExtension, ESPMode::ThreadSafe> ViewExtension;
};
