// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "IHeadMountedDisplay.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "IOculusHMDModule.h"
#include "OculusFunctionLibrary.generated.h"

namespace OculusHMD
{
	class FOculusHMD;
}

/* Tracked device types corresponding to ovrTrackedDeviceType enum*/
UENUM(BlueprintType)
enum class ETrackedDeviceType : uint8
{
	None UMETA(DisplayName = "No Devices"),
	HMD	UMETA(DisplayName = "HMD"),
	LTouch	UMETA(DisplayName = "Left Hand"),
	RTouch	UMETA(DisplayName = "Right Hand"),
	Touch		UMETA(DisplayName = "All Hands"),
    DeviceObjectZero    UMETA(DisplayName = "DeviceObject Zero"),
	All	UMETA(DisplayName = "All Devices")
};

USTRUCT(BlueprintType, meta = (DisplayName = "HMD User Profile Data Field"))
struct FHmdUserProfileField
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString FieldName;

	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString FieldValue;

	FHmdUserProfileField() {}
	FHmdUserProfileField(const FString& Name, const FString& Value) :
		FieldName(Name), FieldValue(Value) {}
};

USTRUCT(BlueprintType, meta = (DisplayName = "HMD User Profile Data"))
struct FHmdUserProfile
{
	GENERATED_USTRUCT_BODY()

	/** Name of the user's profile. */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString Name;

	/** Gender of the user ("male", "female", etc). */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString Gender;

	/** Height of the player, in meters */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	float PlayerHeight;

	/** Height of the player, in meters */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	float EyeHeight;

	/** Interpupillary distance of the player, in meters */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	float IPD;

	/** Neck-to-eye distance, in meters. X - horizontal, Y - vertical. */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FVector2D NeckToEyeDistance;

	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	TArray<FHmdUserProfileField> ExtraFields;

	FHmdUserProfile() :
		PlayerHeight(0.f), EyeHeight(0.f), IPD(0.f), NeckToEyeDistance(FVector2D::ZeroVector) {}
};

UENUM(BlueprintType)
enum class ETiledMultiResLevel : uint8
{
	ETiledMultiResLevel_Off = 0,
	ETiledMultiResLevel_LMSLow,
	ETiledMultiResLevel_LMSMedium,
	ETiledMultiResLevel_LMSHigh
};

/* Guardian boundary types*/
UENUM(BlueprintType)
enum class EBoundaryType : uint8
{
	Boundary_Outer	UMETA(DisplayName = "Outer Boundary"),
	Boundary_PlayArea	UMETA(DisplayName = "Play Area"),
};

/*
* Information about relationships between a triggered boundary (EBoundaryType::Boundary_Outer or
* EBoundaryType::Boundary_PlayArea) and a device or point in the world.
* All dimensions, points, and vectors are returned in Unreal world coordinate space.
*/
USTRUCT(BlueprintType)
struct FGuardianTestResult
{
	GENERATED_BODY()

	/** Is there a triggering interaction between the device/point and specified boundary? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result")
	bool IsTriggering;

	/** Device type triggering boundary (ETrackedDeviceType::None if BoundaryTestResult corresponds to a point rather than a device) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result")
	ETrackedDeviceType DeviceType;

	/** Distance of device/point to surface of boundary specified by BoundaryType */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result")
	float ClosestDistance;

	/** Closest point on surface corresponding to specified boundary */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result")
	FVector ClosestPoint;

	/** Normal of closest point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result")
	FVector ClosestPointNormal;
};

UCLASS()
class OCULUSHMD_API UOculusFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	 * Grabs the current orientation and position for the HMD.  If positional tracking is not available, DevicePosition will be a zero vector
	 *
	 * @param DeviceRotation	(out) The device's current rotation
	 * @param DevicePosition	(out) The device's current position, in its own tracking space
	 * @param NeckPosition		(out) The estimated neck position, calculated using NeckToEye vector from User Profile. Same coordinate space as DevicePosition.
	 * @param bUseOrienationForPlayerCamera	(in) Should be set to 'true' if the orientation is going to be used to update orientation of the camera manually.
	 * @param bUsePositionForPlayerCamera	(in) Should be set to 'true' if the position is going to be used to update position of the camera manually.
	 * @param PositionScale		(in) The 3D scale that will be applied to position.
	 */
	UFUNCTION(BlueprintPure, Category="OculusLibrary")
	static void GetPose(FRotator& DeviceRotation, FVector& DevicePosition, FVector& NeckPosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false, const FVector PositionScale = FVector::ZeroVector);

	/**
	* Reports raw sensor data. If HMD doesn't support any of the parameters then it will be set to zero.
	*
	* @param AngularAcceleration	(out) Angular acceleration in radians per second per second.
	* @param LinearAcceleration		(out) Acceleration in meters per second per second.
	* @param AngularVelocity		(out) Angular velocity in radians per second.
	* @param LinearVelocity			(out) Velocity in meters per second.
	* @param TimeInSeconds			(out) Time when the reported IMU reading took place, in seconds.
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary")
	static void GetRawSensorData(FVector& AngularAcceleration, FVector& LinearAcceleration, FVector& AngularVelocity, FVector& LinearVelocity, float& TimeInSeconds, ETrackedDeviceType DeviceType = ETrackedDeviceType::HMD);

	/**
	* Returns if the device is currently tracked by the runtime or not.
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary")
	static bool IsDeviceTracked(ETrackedDeviceType DeviceType);

	/**
	* Returns if the device is currently tracked by the runtime or not.
	*/
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary")
	static void SetCPUAndGPULevels(int CPULevel, int GPULevel);

	/**
	* Sets the HMD recenter behavior to a mode that specifies HMD recentering behavior when a
	* controller recenter is performed. If the recenterMode specified is 1, the HMD will recenter
	* on controller recenter; if it's 0, only the controller will recenter. Returns false if not
	* supported.
	*/
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary")
	static void SetReorientHMDOnControllerRecenter(bool recenterMode);

	/**
	* Returns current user profile.
	*
	* @param Profile		(out) Structure to hold current user profile.
	* @return (boolean)	True, if user profile was acquired.
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary")
	static bool GetUserProfile(FHmdUserProfile& Profile);

	/**
	* Sets 'base rotation' - the rotation that will be subtracted from
	* the actual HMD orientation.
	* Sets base position offset (in meters). The base position offset is the distance from the physical (0, 0, 0) position
	* to current HMD position (bringing the (0, 0, 0) point to the current HMD position)
	* Note, this vector is set by ResetPosition call; use this method with care.
	* The axis of the vector are the same as in Unreal: X - forward, Y - right, Z - up.
	*
	* @param Rotation			(in) Rotator object with base rotation
	* @param BaseOffsetInMeters (in) the vector to be set as base offset, in meters.
	* @param Options			(in) specifies either position, orientation or both should be set.
	*/
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary")
	static void SetBaseRotationAndBaseOffsetInMeters(FRotator Rotation, FVector BaseOffsetInMeters, EOrientPositionSelector::Type Options);

	/**
	* Returns current base rotation and base offset.
	* The base offset is currently used base position offset, previously set by the
	* ResetPosition or SetBasePositionOffset calls. It represents a vector that translates the HMD's position
	* into (0,0,0) point, in meters.
	* The axis of the vector are the same as in Unreal: X - forward, Y - right, Z - up.
	*
	* @param OutRotation			(out) Rotator object with base rotation
	* @param OutBaseOffsetInMeters	(out) base position offset, vector, in meters.
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary")
	static void GetBaseRotationAndBaseOffsetInMeters(FRotator& OutRotation, FVector& OutBaseOffsetInMeters);

	/**
	 * Scales the HMD position that gets added to the virtual camera position.
	 *
	 * @param PosScale3D	(in) the scale to apply to the HMD position.
	 */
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "This feature is no longer supported."))
	static void SetPositionScale3D(FVector PosScale3D) { }

	/**
	 * Sets 'base rotation' - the rotation that will be subtracted from
	 * the actual HMD orientation.
	 * The position offset might be added to current HMD position,
	 * effectively moving the virtual camera by the specified offset. The addition
	 * occurs after the HMD orientation and position are applied.
	 *
	 * @param BaseRot			(in) Rotator object with base rotation
	 * @param PosOffset			(in) the vector to be added to HMD position.
	 * @param Options			(in) specifies either position, orientation or both should be set.
	 */
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "A hack, proper camera positioning should be used"))
	static void SetBaseRotationAndPositionOffset(FRotator BaseRot, FVector PosOffset, EOrientPositionSelector::Type Options);

	/**
	 * Returns current base rotation and position offset.
	 *
	 * @param OutRot			(out) Rotator object with base rotation
	 * @param OutPosOffset		(out) the vector with previously set position offset.
	 */
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "A hack, proper camera positioning should be used"))
	static void GetBaseRotationAndPositionOffset(FRotator& OutRot, FVector& OutPosOffset);

	/**
	 * Adds loading splash screen with parameters
	 *
	 * @param Texture			(in) A texture asset to be used for the splash. Gear VR uses it as a path for loading icon; all other params are currently ignored by Gear VR.
	 * @param TranslationInMeters (in) Initial translation of the center of the splash screen (in meters).
	 * @param Rotation			(in) Initial rotation of the splash screen, with the origin at the center of the splash screen.
	 * @param SizeInMeters		(in) Size, in meters, of the quad with the splash screen.
	 * @param DeltaRotation		(in) Incremental rotation, that is added each 2nd frame to the quad transform. The quad is rotated around the center of the quad.
	 * @param bClearBeforeAdd	(in) If true, clears splashes before adding a new one.
	 */
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary")
	static void AddLoadingSplashScreen(class UTexture2D* Texture, FVector TranslationInMeters, FRotator Rotation, FVector2D SizeInMeters = FVector2D(1.0f, 1.0f), FRotator DeltaRotation = FRotator::ZeroRotator, bool bClearBeforeAdd = false);

	/**
	 * Removes all the splash screens.
	 */
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary")
	static void ClearLoadingSplashScreens();

	/**
	 * Shows loading splash screen.
	 */
	UE_DEPRECATED(4.22, "UOculusFunctionLibrary::ShowLoadingSplashScreen has been deprecated and no longer functions as before.  Use the generic UStereoLayerFunctionLibrary::ShowSplashScreen instead")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "UOculusFunctionLibrary::ShowLoadingSplashScreen has been deprecated and no longer functions as before.  Use the generic UStereoLayerFunctionLibrary::ShowSplashScreen instead"))
	static void ShowLoadingSplashScreen();

	/**
	 * Hides loading splash screen.
	 *
	 * @param	bClear	(in) Clear all splash screens after hide.
	 */
	UE_DEPRECATED(4.22, "UOculusFunctionLibrary::HideLoadingSplashScreen has been deprecated and no longer functions as before.  Use the generic UStereoLayerFunctionLibrary::HideSplashScreen instead")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "UOculusFunctionLibrary::HideLoadingSplashScreen has been deprecated and no longer functions as before.  Use the generic UStereoLayerFunctionLibrary::HideSplashScreen instead"))
	static void HideLoadingSplashScreen(bool bClear = false);

	/**
	 * Enables/disables splash screen to be automatically shown when LoadMap is called.
	 *
	 * @param	bAutoShowEnabled	(in)	True, if automatic showing of splash screens is enabled when map is being loaded.
	 */
	UE_DEPRECATED(4.22, "UOculusFunctionLibrary::EnableAutoLoadingSplashScreen has been deprecated and no longer functions as before.  Use the generic UStereoLayerFunctionLibrary::EnableAutoLoadingSplashScreen instead")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "UOculusFunctionLibrary::EnableAutoLoadingSplashScreen has been deprecated and no longer functions as before.  Use the generic UStereoLayerFunctionLibrary::EnableAutoLoadingSplashScreen instead"))
	static void EnableAutoLoadingSplashScreen(bool bAutoShowEnabled);

	/**
	 * Returns true, if the splash screen is automatically shown when LoadMap is called.
	 */
	UE_DEPRECATED(4.22, "UOculusFunctionLibrary::IsAutoLoadingSplashScreenEnabled has been deprecated and no longer functions as before. Please use the generic UStereoLayerFunctionLibrary instead.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "UOculusFunctionLibrary::IsAutoLoadingSplashScreenEnabled has been deprecated and no longer functions as before. Please use the generic UStereoLayerFunctionLibrary instead."))
	static bool IsAutoLoadingSplashScreenEnabled();

	/**
	 * Sets a texture for loading icon mode and shows it. This call will clear all the splashes.
	 */
	UE_DEPRECATED(4.22, "UOculusFunctionLibrary::ShowLoadingIcon has been deprecated and no longer functions as before. Please use the generic UStereoLayerFunctionLibrary instead.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "UOculusFunctionLibrary::ShowLoadingIcon has been deprecated and no longer functions as before. Please use the generic UStereoLayerFunctionLibrary instead."))
	static void ShowLoadingIcon(class UTexture2D* Texture);

	/**
	 * Clears the loading icon. This call will clear all the splashes.
	 */
	UE_DEPRECATED(4.22, "UOculusFunctionLibrary::HideLoadingIcon has been deprecated and no longer functions as before. Please use the generic UStereoLayerFunctionLibrary instead.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "UOculusFunctionLibrary::HideLoadingIcon has been deprecated and no longer functions as before. Please use the generic UStereoLayerFunctionLibrary instead."))
	static void HideLoadingIcon();

	/**
	 * Returns true, if the splash screen is in loading icon mode.
	 */
	UE_DEPRECATED(4.22, "UOculusFunctionLibrary::IsLoadingIconEnabled has been deprecated and no longer functions as before. Please use the generic UStereoLayerFunctionLibrary instead.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "UOculusFunctionLibrary::IsLoadingIconEnabled has been deprecated and no longer functions as before. Please use the generic UStereoLayerFunctionLibrary instead."))
	static bool IsLoadingIconEnabled();

	/**
	 * Sets loading splash screen parameters.
	 *
	 * @param TexturePath		(in) A path to the texture asset to be used for the splash. Gear VR uses it as a path for loading icon; all other params are currently ignored by Gear VR.
	 * @param DistanceInMeters	(in) Distance, in meters, to the center of the splash screen.
	 * @param SizeInMeters		(in) Size, in meters, of the quad with the splash screen.
	 * @param RotationAxes		(in) A vector that specifies the axis of the splash screen rotation (if RotationDelta is specified).
	 * @param RotationDeltaInDeg (in) Rotation delta, in degrees, that is added each 2nd frame to the quad transform. The quad is rotated around the vector "RotationAxes".
	 */
	UE_DEPRECATED(4.22, "UOculusFunctionLibrary::SetLoadingSplashParams has been deprecated and no longer functions as before. Please use the generic UStereoLayerFunctionLibrary instead.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "UOculusFunctionLibrary::SetLoadingSplashParams has been deprecated and no longer functions as before. Please use the generic UStereoLayerFunctionLibrary instead."))
	static void SetLoadingSplashParams(FString TexturePath, FVector DistanceInMeters, FVector2D SizeInMeters, FVector RotationAxis, float RotationDeltaInDeg);

	/**
	 * Returns loading splash screen parameters.
	 *
	 * @param TexturePath		(out) A path to the texture asset to be used for the splash. Gear VR uses it as a path for loading icon; all other params are currently ignored by Gear VR.
	 * @param DistanceInMeters	(out) Distance, in meters, to the center of the splash screen.
	 * @param SizeInMeters		(out) Size, in meters, of the quad with the splash screen.
	 * @param RotationAxes		(out) A vector that specifies the axis of the splash screen rotation (if RotationDelta is specified).
	 * @param RotationDeltaInDeg (out) Rotation delta, in degrees, that is added each 2nd frame to the quad transform. The quad is rotated around the vector "RotationAxes".
	 */
	UE_DEPRECATED(4.22, "UOculusFunctionLibrary::GetLoadingSplashParams has been deprecated and no longer functions as before. Please use the generic UStereoLayerFunctionLibrary instead.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "UOculusFunctionLibrary::GetLoadingSplashParams has been deprecated and no longer functions as before. Please use the generic UStereoLayerFunctionLibrary instead."))
	static void GetLoadingSplashParams(FString& TexturePath, FVector& DistanceInMeters, FVector2D& SizeInMeters, FVector& RotationAxis, float& RotationDeltaInDeg);

	/**
	* Returns true, if the app has input focus.
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary")
	static bool HasInputFocus();

	/**
	* Returns true, if the system overlay is present.
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary")
	static bool HasSystemOverlayPresent();

	/**
	* Returns the GPU utilization availability and value
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary")
	static void GetGPUUtilization(bool& IsGPUAvailable, float& GPUUtilization);

	/**
	* Returns the GPU frame time on supported mobile platforms (Go for now)
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary")
	static float GetGPUFrameTime();

	/**
	* Returns the current multiresolution level
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary")
	static ETiledMultiResLevel GetTiledMultiresLevel();

	/**
	* Set the requested multiresolution level for the next frame
	*/
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary")
	static void SetTiledMultiresLevel(ETiledMultiResLevel level);

	/**
	* Returns the current device's name
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary")
	static FString GetDeviceName();

	/**
	* Returns the current available frequencies
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary")
	static TArray<float> GetAvailableDisplayFrequencies();

	/**
	* Returns the current display frequency
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary")
	static float GetCurrentDisplayFrequency();

	/**
	* Sets the requested display frequency
	*/
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary")
	static void SetDisplayFrequency(float RequestedFrequency);

	/**
	* Enables/disables positional tracking on devices that support it.
	*/
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary")
	static void EnablePositionTracking(bool bPositionTracking);

	/**
	* Enables/disables orientation tracking on devices that support it.
	*/
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary")
	static void EnableOrientationTracking(bool bOrientationTracking);

	/**
	* Enables/disables orientation tracking on devices that support it.
	*/
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary")
	static void SetColorScaleAndOffset(FLinearColor ColorScale, FLinearColor ColorOffset, bool bApplyToAllLayers = false);

	/**
	 * Returns IStereoLayers interface to work with overlays.
	 */
	static class IStereoLayers* GetStereoLayers();

	/* GUARDIAN API */
	/**
	* Returns true if the Guardian Outer Boundary is being displayed
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|Guardian")
	static bool IsGuardianDisplayed();

	/**
	* Returns the list of points in UE world space of the requested Boundary Type 
	* @param BoundaryType			(in) An enum representing the boundary type requested, either Outer Boundary (exact guardian bounds) or PlayArea (rectangle inside the Outer Boundary)
	* @param UsePawnSpace			(in) Boolean indicating to return the points in world space or pawn space
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|Guardian")
	static TArray<FVector> GetGuardianPoints(EBoundaryType BoundaryType, bool UsePawnSpace = false);

	/**
	* Returns the dimensions in UE world space of the requested Boundary Type
	* @param BoundaryType			(in) An enum representing the boundary type requested, either Outer Boundary (exact guardian bounds) or PlayArea (rectangle inside the Outer Boundary)
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|Guardian")
	static FVector GetGuardianDimensions(EBoundaryType BoundaryType);

	/**
	* Returns the transform of the play area rectangle, defining its position, rotation and scale to apply to a unit cube to match it with the play area.
	*/
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|Guardian")
	static FTransform GetPlayAreaTransform();

	/**
	* Get the intersection result between a UE4 coordinate and a guardian boundary
	* @param Point					(in) Point in UE space to test against guardian boundaries
	* @param BoundaryType			(in) An enum representing the boundary type requested, either Outer Boundary (exact guardian bounds) or PlayArea (rectangle inside the Outer Boundary)
	*/
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|Guardian")
	static FGuardianTestResult GetPointGuardianIntersection(const FVector Point, EBoundaryType BoundaryType);

	/**
	* Get the intersection result between a tracked device (HMD or controllers) and a guardian boundary
	* @param DeviceType             (in) Tracked Device type to test against guardian boundaries
	* @param BoundaryType			(in) An enum representing the boundary type requested, either Outer Boundary (exact guardian bounds) or PlayArea (rectangle inside the Outer Boundary)
	*/
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|Guardian")
	static FGuardianTestResult GetNodeGuardianIntersection(ETrackedDeviceType DeviceType, EBoundaryType BoundaryType);

	/**
	* Forces the runtime to render guardian at all times or not
	* @param GuardianVisible			(in) True will display guardian, False will hide it
	*/
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|Guardian")
	static void SetGuardianVisibility(bool GuardianVisible);

	/** When player triggers the Guardian boundary */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOculusGuardianTriggeredEvent, FGuardianTestResult);

	/** When player returns within outer bounds */
	DECLARE_MULTICAST_DELEGATE(FOculusGuardianReturnedEvent);

	/**
	* For outer boundary only. Devs can bind delegates via something like: BoundaryComponent->OnOuterBoundaryTriggered.AddDynamic(this, &UCameraActor::PauseGameForBoundarySystem) where
	* PauseGameForBoundarySystem() takes a TArray<FBoundaryTestResult> parameter.
	*/
	//UPROPERTY(BlueprintAssignable, Category = "Input|OculusLibrary|Guardian")
	//static FOculusGuardianTriggeredEvent OnGuardianTriggered;

	/** For outer boundary only. Devs can bind delegates via something like: BoundaryComponent->OnOuterBoundaryReturned.AddDynamic(this, &UCameraActor::ResumeGameForBoundarySystem) */
	//UPROPERTY(BlueprintAssignable, Category = "OculusLibrary|Guardian")
	//FOculusGuardianReturnedEvent OnGuardianReturned;

protected:
	static class OculusHMD::FOculusHMD* GetOculusHMD();
};
