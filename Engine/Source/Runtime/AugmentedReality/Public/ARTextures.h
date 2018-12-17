// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Texture.h"
#include "Engine/TextureCube.h"
#include "ARTextures.generated.h"

UENUM(BlueprintType, Category="AR AugmentedReality", meta=(Experimental))
enum class EARTextureType : uint8
{
	CameraImage,
	CameraDepth,
	EnvironmentCapture
};

/**
 * Base class for all AR texture types.
 * Derived from UTexture instead of UTexture2D because UTexture2D is all about streaming and source art
 */
UCLASS(Abstract, BlueprintType)
class AUGMENTEDREALITY_API UARTexture : public UTexture
{
	GENERATED_UCLASS_BODY()

public:
	/** The type of texture this is */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality", meta=(Experimental))
	EARTextureType TextureType;

	/** The timestamp this texture was captured at */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality", meta=(Experimental))
	float Timestamp;

	/** The guid of texture that gets registered as an external texture */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality", meta=(Experimental))
	FGuid ExternalTextureGuid;

	/** The width and height of the texture */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality", meta=(Experimental))
	FVector2D Size;
};

/**
 * Base class for all AR textures that represent the camera image data
 */
UCLASS(Abstract, BlueprintType)
class AUGMENTEDREALITY_API UARTextureCameraImage : public UARTexture
{
	GENERATED_UCLASS_BODY()

public:
};

UENUM(BlueprintType, Category="AR AugmentedReality", meta=(Experimental))
enum class EARDepthQuality : uint8
{
	Unkown,
	/** Not suitable to use as part of a rendering pass or for scene reconstruction */
	Low,
	/** Suitable for rendering against or for use in scene reconstruction */
	High
};

UENUM(BlueprintType, Category="AR AugmentedReality", meta=(Experimental))
enum class EARDepthAccuracy : uint8
{
	Unkown,
	/** Suitable for gross sorting of depths */
	Approximate,
	/** Accurate depth values that match the physical world */
	Accurate
};

/**
 * Base class for all AR textures that represent the camera depth data
 */
UCLASS(Abstract, BlueprintType)
class AUGMENTEDREALITY_API UARTextureCameraDepth : public UARTexture
{
	GENERATED_UCLASS_BODY()

public:
	/** The quality of the depth information captured this frame */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality", meta=(Experimental))
	EARDepthQuality DepthQuality;

	/** The accuracy of the depth information captured this frame */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality", meta=(Experimental))
	EARDepthAccuracy DepthAccuracy;

	/** Whether or not the depth information is temporally smoothed */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality", meta=(Experimental))
	bool bIsTemporallySmoothed;
};

/**
 * Base class for all AR textures that represent the environment for lighting and reflection
 */
UCLASS(Abstract, BlueprintType)
class AUGMENTEDREALITY_API UAREnvironmentCaptureProbeTexture : public UTextureCube
{
	GENERATED_UCLASS_BODY()
	
public:
	/** The type of texture this is */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality", meta=(Experimental))
	EARTextureType TextureType;
	
	/** The timestamp this texture was captured at */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality", meta=(Experimental))
	float Timestamp;
	
	/** The guid of texture that gets registered as an external texture */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality", meta=(Experimental))
	FGuid ExternalTextureGuid;
	
	/** The width and height of the texture */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality", meta=(Experimental))
	FVector2D Size;
};
