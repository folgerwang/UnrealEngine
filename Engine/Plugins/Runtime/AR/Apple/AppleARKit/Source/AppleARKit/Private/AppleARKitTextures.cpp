// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AppleARKitTextures.h"
#include "AppleARKitModule.h"
#include "ExternalTexture.h"
#include "RenderingThread.h"
#include "Containers/DynamicRHIResourceArray.h"

#if PLATFORM_MAC || PLATFORM_IOS
	#import <Metal/Metal.h>
#endif

UAppleARKitTextureCameraImage::UAppleARKitTextureCameraImage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if PLATFORM_MAC || PLATFORM_IOS
	, CameraImage(nullptr)
#endif
{
	ExternalTextureGuid = FGuid::NewGuid();
}

FTextureResource* UAppleARKitTextureCameraImage::CreateResource()
{
	// @todo joeg -- hook this up for rendering
	return nullptr;
}

void UAppleARKitTextureCameraImage::BeginDestroy()
{
#if PLATFORM_MAC || PLATFORM_IOS
	if (CameraImage != nullptr)
	{
		CFRelease(CameraImage);
		CameraImage = nullptr;
	}
#endif
	Super::BeginDestroy();
}

#if SUPPORTS_ARKIT_1_0

void UAppleARKitTextureCameraImage::Init(float InTimestamp, CVPixelBufferRef InCameraImage)
{
	// Handle the case where this UObject is being reused
	if (CameraImage != nullptr)
	{
		CFRelease(CameraImage);
		CameraImage = nullptr;
	}

	if (InCameraImage != nullptr)
	{
		Timestamp = InTimestamp;
		CameraImage = InCameraImage;
		CFRetain(CameraImage);
		Size.X = CVPixelBufferGetWidth(CameraImage);
		Size.Y = CVPixelBufferGetHeight(CameraImage);
	}
	//@todo joeg - Update the render resources
}

#endif

UAppleARKitTextureCameraDepth::UAppleARKitTextureCameraDepth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if PLATFORM_MAC || PLATFORM_IOS
	, CameraDepth(nullptr)
#endif
{
	ExternalTextureGuid = FGuid::NewGuid();
}

FTextureResource* UAppleARKitTextureCameraDepth::CreateResource()
{
	// @todo joeg -- hook this up for rendering
	return nullptr;
}

void UAppleARKitTextureCameraDepth::BeginDestroy()
{
#if PLATFORM_MAC || PLATFORM_IOS
	if (CameraDepth != nullptr)
	{
		CFRelease(CameraDepth);
		CameraDepth = nullptr;
	}
#endif
	Super::BeginDestroy();
}

#if SUPPORTS_ARKIT_1_0

void UAppleARKitTextureCameraDepth::Init(float InTimestamp, AVDepthData* InCameraDepth)
{
// @todo joeg -- finish this
	Timestamp = InTimestamp;
}

#endif

UAppleARKitEnvironmentCaptureProbeTexture::UAppleARKitEnvironmentCaptureProbeTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if PLATFORM_MAC || PLATFORM_IOS
	, MetalTexture(nullptr)
#endif
{
	ExternalTextureGuid = FGuid::NewGuid();
	SRGB = false;
}

#if PLATFORM_MAC || PLATFORM_IOS
void UAppleARKitEnvironmentCaptureProbeTexture::Init(float InTimestamp, id<MTLTexture> InEnvironmentTexture)
{
	if (Resource == nullptr)
	{
		UpdateResource();
	}
	
	// Do nothing if the textures are the same
	// They will change as the data comes in but the textures themselves may stay the same between updates
	if (MetalTexture == InEnvironmentTexture)
	{
		return;
	}
	
	// Handle the case where this UObject is being reused
	if (MetalTexture != nullptr)
	{
		CFRelease(MetalTexture);
		MetalTexture = nullptr;
	}
	
	if (InEnvironmentTexture != nullptr)
	{
		Timestamp = InTimestamp;
		MetalTexture = InEnvironmentTexture;
		CFRetain(MetalTexture);
		Size.X = MetalTexture.width;
		Size.Y = MetalTexture.height;
	}
	// Force an update to our external texture on the render thread
	if (Resource != nullptr)
	{
		ENQUEUE_RENDER_COMMAND(UpdateEnvironmentCapture)(
			[InResource = Resource](FRHICommandListImmediate& RHICmdList)
			{
				InResource->InitRHI();
			});
	}
}

/**
 * Passes a metaltexture through to the RHI to wrap in an RHI texture without traversing system memory.
 */
class FAppleARKitMetalTextureResourceWrapper :
	public FResourceBulkDataInterface
{
public:
	FAppleARKitMetalTextureResourceWrapper(id<MTLTexture> InImageBuffer)
		: ImageBuffer(InImageBuffer)
	{
		check(ImageBuffer);
		CFRetain(ImageBuffer);
	}
	
	virtual ~FAppleARKitMetalTextureResourceWrapper()
	{
		CFRelease(ImageBuffer);
		ImageBuffer = nullptr;
	}

	/**
	 * @return ptr to the resource memory which has been preallocated
	 */
	virtual const void* GetResourceBulkData() const override
	{
		return ImageBuffer;
	}
	
	/**
	 * @return size of resource memory
	 */
	virtual uint32 GetResourceBulkDataSize() const override
	{
		return 0;
	}
	
	/**
	 * @return the type of bulk data for special handling
	 */
	virtual EBulkDataType GetResourceType() const override
	{
		return EBulkDataType::MediaTexture;
	}
	
	/**
	 * Free memory after it has been used to initialize RHI resource
	 */
	virtual void Discard() override
	{
		delete this;
	}
	
	id<MTLTexture> ImageBuffer;
};

class FARMetalResource :
	public FTextureResource
{
public:
	FARMetalResource(UAppleARKitEnvironmentCaptureProbeTexture* InOwner)
		: Owner(InOwner)
	{
		bGreyScaleFormat = false;
		bSRGB = true;
	}
	
	virtual ~FARMetalResource()
	{
	}
	
	/**
	 * Called when the resource is initialized. This is only called by the rendering thread.
	 */
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;

		id<MTLTexture> MetalTexture = Owner->GetMetalTexture();
		if (MetalTexture != nullptr)
		{
			Size.X = Size.Y = Owner->Size.X;

			const uint32 CreateFlags = TexCreate_SRGB;
			EnvCubemapTextureRHIRef = RHICreateTextureCube(Size.X, PF_B8G8R8A8, 1, CreateFlags, CreateInfo);

			/**
			 * To map their texture faces into our space we need:
			 *	 +X	to +Y	Down Mirrored
			 *	 -X to -Y	Up Mirrored
			 *	 +Y to +Z	Left Mirrored
			 *	 -Y to -Z	Left Mirrored
			 *	 +Z to -X	Left Mirrored
			 *	 -Z to +X	Right Mirrored
			 */
			CopyCubeFace(MetalTexture, EnvCubemapTextureRHIRef, kCGImagePropertyOrientationDownMirrored, 0, 2);
			CopyCubeFace(MetalTexture, EnvCubemapTextureRHIRef, kCGImagePropertyOrientationUpMirrored, 1, 3);
			CopyCubeFace(MetalTexture, EnvCubemapTextureRHIRef, kCGImagePropertyOrientationLeftMirrored, 2, 4);
			CopyCubeFace(MetalTexture, EnvCubemapTextureRHIRef, kCGImagePropertyOrientationLeftMirrored, 3, 5);
			CopyCubeFace(MetalTexture, EnvCubemapTextureRHIRef, kCGImagePropertyOrientationLeftMirrored, 4, 1);
			CopyCubeFace(MetalTexture, EnvCubemapTextureRHIRef, kCGImagePropertyOrientationRightMirrored, 5, 0);
		}
		else
		{
			Size.X = Size.Y = 1;
			// Start with a 1x1 texture
			EnvCubemapTextureRHIRef = RHICreateTextureCube(Size.X, PF_B8G8R8A8, 1, 0, CreateInfo);
		}


		TextureRHI = EnvCubemapTextureRHIRef;
		TextureRHI->SetName(Owner->GetFName());
		RHIBindDebugLabelName(TextureRHI, *Owner->GetName());
		RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, TextureRHI);

		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
	}

	void CopyCubeFace(id<MTLTexture> MetalTexture, FTextureCubeRHIRef Cubemap, uint32 Rotation, int32 MetalCubeIndex, int32 OurCubeIndex)
	{
		// Rotate the image we need to get a view into the face as a new slice
		id<MTLTexture> CubeFaceMetalTexture = [MetalTexture newTextureViewWithPixelFormat: MTLPixelFormatBGRA8Unorm textureType: MTLTextureType2D levels: NSMakeRange(0, 1) slices: NSMakeRange(MetalCubeIndex, 1)];
		CIImage* CubefaceImage = [[CIImage alloc] initWithMTLTexture: CubeFaceMetalTexture options: nil];
		CIImage* RotatedCubefaceImage = [CubefaceImage imageByApplyingOrientation: Rotation];
		CIImage* ImageTransform = nullptr;
		if (Rotation != kCGImagePropertyOrientationUp)
		{
			ImageTransform = RotatedCubefaceImage;
		}
		else
		{
			// We don't need to rotate it so just use a copy instead
			ImageTransform = CubefaceImage;
		}

		// Make a new view into our texture and directly render to that to avoid the CPU copy
		id<MTLTexture> UnderlyingMetalTexture = (id<MTLTexture>)Cubemap->GetNativeResource();
		id<MTLTexture> OurCubeFaceMetalTexture = [UnderlyingMetalTexture newTextureViewWithPixelFormat: MTLPixelFormatBGRA8Unorm textureType: MTLTextureType2D levels: NSMakeRange(0, 1) slices: NSMakeRange(OurCubeIndex, 1)];

		CIContext* Context = [CIContext context];

		[Context render: RotatedCubefaceImage toMTLTexture: OurCubeFaceMetalTexture commandBuffer: nil bounds: CubefaceImage.extent colorSpace: CubefaceImage.colorSpace];

		[CubefaceImage release];
		[CubeFaceMetalTexture release];
		[OurCubeFaceMetalTexture release];
	}
	
	virtual void ReleaseRHI() override
	{
		RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, FTextureRHIParamRef());
		EnvCubemapTextureRHIRef.SafeRelease();
		FTextureResource::ReleaseRHI();
		FExternalTextureRegistry::Get().UnregisterExternalTexture(Owner->ExternalTextureGuid);
	}
	
	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const override
	{
		return Size.X;
	}
	
	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const override
	{
		return Size.Y;
	}
	
private:
	FIntPoint Size;
	
	FTextureCubeRHIRef EnvCubemapTextureRHIRef;
	
	const UAppleARKitEnvironmentCaptureProbeTexture* Owner;
};

#endif

FTextureResource* UAppleARKitEnvironmentCaptureProbeTexture::CreateResource()
{
#if PLATFORM_MAC || PLATFORM_IOS
	return new FARMetalResource(this);
#endif
	return nullptr;
}

void UAppleARKitEnvironmentCaptureProbeTexture::BeginDestroy()
{
#if PLATFORM_MAC || PLATFORM_IOS
	if (MetalTexture != nullptr)
	{
		CFRelease(MetalTexture);
		MetalTexture = nullptr;
	}
#endif
	Super::BeginDestroy();
}
