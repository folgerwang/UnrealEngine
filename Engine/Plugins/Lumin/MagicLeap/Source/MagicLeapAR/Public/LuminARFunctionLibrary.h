// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LatentActionManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "LuminARTypes.h"
#include "LuminARSessionConfig.h"
#include "LuminARFunctionLibrary.generated.h"

/** A function library that provides static/Blueprint functions associated with LuminAR session.*/
UCLASS()
class MAGICLEAPAR_API ULuminARSessionFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//-----------------Lifecycle---------------------

	/**
	 * Starts a new LuminAR tracking session LuminAR specific configuration.
	 * If the session already started and the config isn't the same, it will stop the previous session and start a new session with the new config.
	 * Note that this is a latent action, you can query the session start result by querying GetLuminARSessionStatus() after the latent action finished.
	 *
	 * @param Configuration				The LuminARSession configuration to start the session.
	 */
	UFUNCTION(BlueprintCallable, Category = "LuminAR|Session", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", Keywords = "luminar session start config"))
	static void StartLuminARSession(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, ULuminARSessionConfig* Configuration);
};

/** A function library that provides static/Blueprint functions associated with most recent LuminAR tracking frame.*/
UCLASS()
class MAGICLEAPAR_API ULuminARFrameFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Returns the current ARCore session status.
	 *
	 * @return	A EARSessionStatus enum that describes the session status.
	 */
	UFUNCTION(BlueprintPure, Category = "LuminAR|MotionTracking", meta = (Keywords = "luminar session"))
	static ELuminARTrackingState GetTrackingState();

	///**
	// * Gets the latest tracking pose in Unreal world space of the ARCore device.
	// *
	// * Note that ARCore motion tracking has already integrated with HMD and the motion controller interface.
	// * Use this function only if you need to implement your own tracking component.
	// *
	// * @param OutPose		The latest device pose.
	// * @return				True if the pose is updated successfully for this frame.
	// */
	//UFUNCTION(BlueprintPure, Category = "LuminAR|MotionTracking", meta = (Keywords = "luminar pose transform"))
	//static void GetPose(FTransform& OutPose);

	/**
	 * Traces a ray from the user's device in the direction of the given location in the camera
	 * view. Intersections with detected scene geometry are returned, sorted by distance from the
	 * device; the nearest intersection is returned first.
	 *
	 * @param WorldContextObject	The world context.
	 * @param ScreenPosition		The position on the screen to cast the ray from.
	 * @param ARObjectType			A set of ELuminARLineTraceChannel indicate which type of line trace it should perform.
	 * @param OutHitResults			The list of hit results sorted by distance.
	 * @return						True if there is a hit detected.
	 */
	UFUNCTION(BlueprintCallable, Category = "LuminAR|LineTrace", meta = (WorldContext = "WorldContextObject", Keywords = "luminar raycast hit"))
	static bool LuminARLineTrace(UObject* WorldContextObject, const FVector2D& ScreenPosition, TSet<ELuminARLineTraceChannel> TraceChannels, TArray<FARTraceResult>& OutHitResults);


	/**
	 * Gets the latest light estimation.
	 *
	 * @param OutLightEstimate		The struct that describes the latest light estimation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LuminAR|LightEstimation", meta = (Keywords = "luminar light ambient"))
	static void GetLightEstimation(FLuminARLightEstimate& OutLightEstimate);
};
