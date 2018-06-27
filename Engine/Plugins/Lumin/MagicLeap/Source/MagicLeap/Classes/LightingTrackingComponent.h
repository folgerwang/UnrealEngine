// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "LightingTrackingComponent.generated.h"

/**
The LightingTrackingComponent wraps the Magic Leap lighting tracking API.
This api provides lumosity data from the camera that can be used to shade objects in a more realistic
manner (via the post processor).
*/
UCLASS(ClassGroup = MagicLeap, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class MAGICLEAP_API ULightingTrackingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULightingTrackingComponent();

	/** Intializes the lighting tracking api. If a post processing component cannot be found in the scene, one will be created. */
	void BeginPlay() override;
	/** Cleans up the lighting tracking api. */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Polls for data from the camera array and processes it based on the active modes (UseGlobalAmbience, UseColorTemp, ...). */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** Set to true if you want the global ambience value from the cameras to be used in post processing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightingTracking|MagicLeap")
		bool UseGlobalAmbience;
	/** Set to true if you want the color temperature value from the cameras to be used in post processing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightingTracking|MagicLeap")
		bool UseColorTemp;
	/** Set to true if you want the ambient cube map to be dynamically updated from the cameras' data. */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightingTracking|MagicLeap")
	//bool UseDynamicAmbientCubeMap;

private:
	class LightingTrackingImpl* Impl;
};

DECLARE_LOG_CATEGORY_EXTERN(LogLightingTracking, Display, All);
