// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AppleARKitAvailability.h"
#include "ARTextures.h"
#if PLATFORM_MAC || PLATFORM_IOS
	#import <CoreVideo/CoreVideo.h>
	#import <AVFoundation/AVFoundation.h>
#endif
#include "AppleImageUtilsTypes.h"
#include "AppleARKitTextures.generated.h"

UCLASS(BlueprintType)
class APPLEARKIT_API UAppleARKitTextureCameraImage :
	public UARTextureCameraImage,
	public IAppleImageInterface
{
	GENERATED_UCLASS_BODY()

public:
	// UTexture interface implementation
	virtual void BeginDestroy() override;
	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() const override { return MCT_TextureExternal; }
	virtual float GetSurfaceWidth() const override { return Size.X; }
	virtual float GetSurfaceHeight() const override { return Size.Y; }
	virtual FGuid GetExternalTextureGuid() const override { return ExternalTextureGuid; }
	// End UTexture interface

#if SUPPORTS_ARKIT_1_0
	/** Sets any initialization data */
	virtual void Init(float InTimestamp, CVPixelBufferRef InCameraImage);
#endif

	virtual EAppleTextureType GetTextureType() const override { return EAppleTextureType::PixelBuffer; }

#if PLATFORM_MAC || PLATFORM_IOS
	/** Returns the cached camera image. You must retain this if you hold onto it */
	CVPixelBufferRef GetCameraImage() { return CameraImage; }

	// IAppleImageInterface interface implementation
	virtual CVPixelBufferRef GetPixelBuffer() const override { return CameraImage; }
	// End IAppleImageInterface interface
#endif

private:
#if PLATFORM_MAC || PLATFORM_IOS
	/** The Apple specific representation of the ar camera image */
	CVPixelBufferRef CameraImage;
#endif
};

UCLASS(BlueprintType)
class APPLEARKIT_API UAppleARKitTextureCameraDepth :
	public UARTextureCameraDepth
{
	GENERATED_UCLASS_BODY()

public:
	// UTexture interface implementation
	virtual void BeginDestroy() override;
	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() const override { return MCT_TextureExternal; }
	virtual float GetSurfaceWidth() const override { return Size.X; }
	virtual float GetSurfaceHeight() const override { return Size.Y; }
	virtual FGuid GetExternalTextureGuid() const override { return ExternalTextureGuid; }
	// End UTexture interface

#if SUPPORTS_ARKIT_1_0
	/** Sets any initialization data */
	virtual void Init(float InTimestamp, AVDepthData* InCameraDepth);
#endif

#if PLATFORM_MAC || PLATFORM_IOS
	/** Returns the cached camera depth. You must retain this if you hold onto it */
	AVDepthData* GetCameraDepth() { return CameraDepth; }
#endif

private:
#if PLATFORM_MAC || PLATFORM_IOS
	/** The Apple specific representation of the ar depth image */
	AVDepthData* CameraDepth;
#endif
};

UCLASS(BlueprintType)
class APPLEARKIT_API UAppleARKitEnvironmentCaptureProbeTexture :
	public UAREnvironmentCaptureProbeTexture,
	public IAppleImageInterface
{
	GENERATED_UCLASS_BODY()
	
public:
	
	// UTexture interface implementation
	virtual void BeginDestroy() override;
	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() const override { return MCT_TextureExternal; }
	virtual float GetSurfaceWidth() const override { return Size.X; }
	virtual float GetSurfaceHeight() const override { return Size.Y; }
	virtual FGuid GetExternalTextureGuid() const override { return ExternalTextureGuid; }
	// End UTexture interface
	
	virtual EAppleTextureType GetTextureType() const override { return EAppleTextureType::MetalTexture; }

#if PLATFORM_MAC || PLATFORM_IOS
	/** Sets any initialization data */
	virtual void Init(float InTimestamp, id<MTLTexture> InEnvironmentTexture);

	// IAppleImageInterface interface implementation
	virtual id<MTLTexture> GetMetalTexture() const override { return MetalTexture; }
	// End IAppleImageInterface interface

private:
	/** The Apple specific representation of the ar environment texture */
	id<MTLTexture> MetalTexture;
#endif
};


