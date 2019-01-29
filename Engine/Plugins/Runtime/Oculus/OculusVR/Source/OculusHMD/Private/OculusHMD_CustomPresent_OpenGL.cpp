// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_CustomPresent.h"
#include "OculusHMDPrivateRHI.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
#include "OculusHMD.h"

#if PLATFORM_WINDOWS
#ifndef WINDOWS_PLATFORM_TYPES_GUARD
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
#endif

#if PLATFORM_ANDROID
#include "Android/AndroidOpenGL.h"
#endif

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FCustomPresentGL
//-------------------------------------------------------------------------------------------------

class FOpenGLCustomPresent : public FCustomPresent
{
public:
	FOpenGLCustomPresent(FOculusHMD* InOculusHMD, bool srgbSupport);

	// Implementation of FCustomPresent, called by Plugin itself
	virtual int GetLayerFlags() const override;
	virtual FTextureRHIRef CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, uint32 InTexCreateFlags) override;
	virtual void AliasTextureResources_RHIThread(FTextureRHIParamRef DestTexture, FTextureRHIParamRef SrcTexture) override;
	virtual void SubmitGPUFrameTime(float GPUFrameTime) override;
};


FOpenGLCustomPresent::FOpenGLCustomPresent(FOculusHMD* InOculusHMD, bool srgbSupport) :
	FCustomPresent(InOculusHMD, ovrpRenderAPI_OpenGL, PF_R8G8B8A8, srgbSupport, false)
{
}


int FOpenGLCustomPresent::GetLayerFlags() const
{
#if PLATFORM_ANDROID
	return ovrpLayerFlag_TextureOriginAtBottomLeft;
#else
	return 0;
#endif
}


FTextureRHIRef FOpenGLCustomPresent::CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, uint32 InTexCreateFlags)
{
	CheckInRenderThread();

	FOpenGLDynamicRHI* DynamicRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);

	switch (InResourceType)
	{
	case RRT_Texture2D:
		return DynamicRHI->RHICreateTexture2DFromResource(InFormat, InSizeX, InSizeY, InNumMips, InNumSamples, InNumSamplesTileMem, InBinding, (GLuint) InTexture, InTexCreateFlags).GetReference();

	case RRT_Texture2DArray:
		return DynamicRHI->RHICreateTexture2DArrayFromResource(InFormat, InSizeX, InSizeY, 2, InNumMips, InNumSamples, InNumSamplesTileMem, InBinding, (GLuint) InTexture, InTexCreateFlags).GetReference();

	case RRT_TextureCube:
		return DynamicRHI->RHICreateTextureCubeFromResource(InFormat, InSizeX, false, 1, InNumMips, InNumSamples, InNumSamplesTileMem, InBinding, (GLuint) InTexture, InTexCreateFlags).GetReference();

	default:
		return nullptr;
	}
}


void FOpenGLCustomPresent::AliasTextureResources_RHIThread(FTextureRHIParamRef DestTexture, FTextureRHIParamRef SrcTexture)
{
	CheckInRHIThread();

	FOpenGLDynamicRHI* DynamicRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);
	DynamicRHI->RHIAliasTextureResources(DestTexture, SrcTexture);
}

void FOpenGLCustomPresent::SubmitGPUFrameTime(float GPUFrameTime)
{
	FOpenGLDynamicRHI* DynamicRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);
	DynamicRHI->GetGPUProfilingData().ExternalGPUTime = GPUFrameTime * 1000;
}


//-------------------------------------------------------------------------------------------------
// APIs
//-------------------------------------------------------------------------------------------------

FCustomPresent* CreateCustomPresent_OpenGL(FOculusHMD* InOculusHMD)
{
#if PLATFORM_ANDROID
	const bool sRGBSupport = FAndroidOpenGL::SupportsFramebufferSRGBEnable();
#else
	const bool sRGBSupport = true;
#endif
	return new FOpenGLCustomPresent(InOculusHMD, sRGBSupport);
}


} // namespace OculusHMD

#if PLATFORM_WINDOWS
#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
