// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UObject/ObjectMacros.h"

#include "OculusMR_Settings.generated.h"

UENUM(BlueprintType)
enum class EOculusMR_CameraDeviceEnum : uint8
{
	CD_None         UMETA(DisplayName = "None"),
	CD_WebCamera0   UMETA(DisplayName = "Web Camera 0"),
	CD_WebCamera1   UMETA(DisplayName = "Web Camera 1"),
	CD_ZEDCamera    UMETA(DisplayName = "ZED Camera"),
};

UENUM(BlueprintType)
enum class EOculusMR_ClippingReference : uint8
{
	CR_TrackingReference    UMETA(DisplayName = "Tracking Reference"),
	CR_Head                 UMETA(DisplayName = "Head"),
};

UENUM(BlueprintType)
enum class EOculusMR_VirtualGreenScreenType : uint8
{
	VGS_Off              UMETA(DisplayName = "Off"),
	VGS_OuterBoundary    UMETA(DisplayName = "Outer Boundary"),
	VGS_PlayArea         UMETA(DisplayName = "Play Area")
};

UENUM(BlueprintType)
enum class EOculusMR_PostProcessEffects : uint8
{
	PPE_Off             UMETA(DisplayName = "Off"),
	PPE_On				UMETA(DisplayName = "On"),
};

UENUM(BlueprintType)
enum class EOculusMR_DepthQuality : uint8
{
	DQ_Low              UMETA(DisplayName = "Low"),
	DQ_Medium           UMETA(DisplayName = "Medium"),
	DQ_High             UMETA(DisplayName = "High")
};

UENUM(BlueprintType)
enum class EOculusMR_CompositionMethod : uint8
{
	/* Generate both foreground and background views for compositing with 3rd-party software like OBS. */
	ExternalComposition		UMETA(DisplayName = "External Composition"),
	/* Composite the camera stream directly to the output with the proper depth.*/
	DirectComposition		UMETA(DisplayName = "Direct Composition")
};

UCLASS(ClassGroup = OculusMR, Blueprintable)
class UOculusMR_Settings : public UObject
{
	GENERATED_BODY()

public:
	UOculusMR_Settings(const FObjectInitializer& ObjectInitializer);

	/** Specify the distance to the camera which divide the background and foreground in MxR casting.
	  * Set it to CR_TrackingReference to use the distance to the Tracking Reference, which works better
	  * in the stationary experience. Set it to CR_Head would use the distance to the HMD, which works better
	  * in the room scale experience.
	  */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	EOculusMR_ClippingReference ClippingReference;

	/** The casting viewports would use the same resolution of the camera which used in the calibration process. */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	bool bUseTrackedCameraResolution;

	/** When bUseTrackedCameraResolution is false, the width of each casting viewport */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	int WidthPerView;

	/** When bUseTrackedCameraResolution is false, the height of each casting viewport */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	int HeightPerView;

	/** When CompositionMethod is External Composition, the latency of the casting output which could be adjusted to
	  * match the camera latency in the external composition application */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "0.1"))
	float CastingLatency;

	/** When CompositionMethod is Direct Composition, you could adjust this latency to delay the virtual
	* hand movement by a small amount of time to match the camera latency */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "0.5"))
	float HandPoseStateLatency;

	/** [Green-screen removal] Chroma Key Color. Apply when CompositionMethod is DirectComposition */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	FColor ChromaKeyColor;

	/** [Green-screen removal] Chroma Key Similarity. Apply when CompositionMethod is DirectComposition */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "1.0"))
	float ChromaKeySimilarity;

	/** [Green-screen removal] Chroma Key Smooth Range. Apply when CompositionMethod is DirectComposition */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "0.2"))
	float ChromaKeySmoothRange;

	/** [Green-screen removal] Chroma Key Spill Range. Apply when CompositionMethod is DirectComposition */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "0.2"))
	float ChromaKeySpillRange;

	/** The type of virtual green screen */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	EOculusMR_VirtualGreenScreenType VirtualGreenScreenType;

	/** Larger values make dynamic lighting effects smoother, but values that are too large make the lighting look flat. */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "16"))
	float DynamicLightingDepthSmoothFactor;

	/** Sets the maximum depth variation across edges (smaller values set smoother edges) */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "0.1"))
	float DynamicLightingDepthVariationClampingValue;

	/** Set the amount of post process effects in the MR view for external composition */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	EOculusMR_PostProcessEffects ExternalCompositionPostProcessEffects;


	/** ExternalComposition: The casting window includes the background and foreground view
	  * DirectComposition: The game scene would be composited with the camera frame directly
	  */
	UFUNCTION(BlueprintCallable, Category = OculusMR)
	EOculusMR_CompositionMethod GetCompositionMethod() { return CompositionMethod; }

	/** ExternalComposition: The casting window includes the background and foreground view
	  * DirectComposition: The game scene would be composited with the camera frame directly
	  */
	UFUNCTION(BlueprintCallable, Category = OculusMR)
	void SetCompositionMethod(EOculusMR_CompositionMethod val);

	/** When CompositionMethod is DirectComposition, the physical camera device which provide the frame */
	UFUNCTION(BlueprintCallable, Category = OculusMR)
	EOculusMR_CameraDeviceEnum GetCapturingCamera() { return CapturingCamera; }

	/** When CompositionMethod is DirectComposition, the physical camera device which provide the frame */
	UFUNCTION(BlueprintCallable, Category = OculusMR)
	void SetCapturingCamera(EOculusMR_CameraDeviceEnum val);

	/** Is MRC on and off */
	UFUNCTION(BlueprintCallable, Category = OculusMR)
	bool GetIsCasting() { return bIsCasting; }

	/** Turns MRC on and off */
	UFUNCTION(BlueprintCallable, Category = OculusMR)
	void SetIsCasting(bool val);

	/** Is using the in-game lights on the camera frame */
	UFUNCTION(BlueprintCallable, Category = OculusMR)
	bool GetUseDynamicLighting() { return bUseDynamicLighting; }

	/** Use the in-game lights on the camera frame */
	UFUNCTION(BlueprintCallable, Category = OculusMR)
	void SetUseDynamicLighting(bool val);

	/** The quality level of the depth sensor */
	UFUNCTION(BlueprintCallable, Category = OculusMR)
	EOculusMR_DepthQuality GetDepthQuality() { return DepthQuality; }

	/** The quality level of the depth sensor */
	UFUNCTION(BlueprintCallable, Category = OculusMR)
	void SetDepthQuality(EOculusMR_DepthQuality val);

	/** Bind the casting camera to the calibrated external camera.
	  * (Requires a calibrated external camera)
	  */
	UFUNCTION(BlueprintCallable, Category = OculusMR)
	void BindToTrackedCameraIndexIfAvailable(int InTrackedCameraIndex);

	UFUNCTION(BlueprintCallable, Category = OculusMR)
	int GetBindToTrackedCameraIndex() { return BindToTrackedCameraIndex; }

	/** Load settings from the config file */
	UFUNCTION(BlueprintCallable, Category = OculusMR)
	void LoadFromIni();

	/** Save settings to the config file */
	UFUNCTION(BlueprintCallable, Category = OculusMR)
	void SaveToIni() const;

private:
	/** Turns MRC on and off (does not get saved to or loaded from ini) */
	UPROPERTY()
	bool bIsCasting;

	/** ExternalComposition: The casting window includes the background and foreground view
	  * DirectComposition: The game scene would be composited with the camera frame directly
	  */
	UPROPERTY()
	EOculusMR_CompositionMethod CompositionMethod;

	/** When CompositionMethod is DirectComposition, the physical camera device which provide the frame */
	UPROPERTY()
	EOculusMR_CameraDeviceEnum CapturingCamera;

	/** Use the in-game lights on the camera frame */
	UPROPERTY()
	bool bUseDynamicLighting;

	/** The quality level of the depth sensor */
	UPROPERTY()
	EOculusMR_DepthQuality DepthQuality;

	/** Tracked camera that we want to bind the in-game MR camera to*/
	int BindToTrackedCameraIndex;

	DECLARE_DELEGATE_TwoParams(OnCompositionMethodChangeDelegate, EOculusMR_CompositionMethod, EOculusMR_CompositionMethod);
	DECLARE_DELEGATE_TwoParams(OnCapturingCameraChangeDelegate, EOculusMR_CameraDeviceEnum, EOculusMR_CameraDeviceEnum);
	DECLARE_DELEGATE_TwoParams(OnDepthQualityChangeDelegate, EOculusMR_DepthQuality, EOculusMR_DepthQuality);
	DECLARE_DELEGATE_TwoParams(OnBooleanSettingChangeDelegate, bool, bool);
	DECLARE_DELEGATE_TwoParams(OnIntegerSettingChangeDelegate, int, int);

	OnIntegerSettingChangeDelegate TrackedCameraIndexChangeDelegate;
	OnCompositionMethodChangeDelegate CompositionMethodChangeDelegate;
	OnCapturingCameraChangeDelegate CapturingCameraChangeDelegate;
	OnBooleanSettingChangeDelegate IsCastingChangeDelegate;
	OnBooleanSettingChangeDelegate UseDynamicLightingChangeDelegate;
	OnDepthQualityChangeDelegate DepthQualityChangeDelegate;

	// Give the OculusMR module access to the delegates so that 
	friend class FOculusMRModule;
};