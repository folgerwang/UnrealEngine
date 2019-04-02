// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_TextureSetProxy.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD_CustomPresent.h"
#include "XRThreadUtils.h"


namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FTextureSetProxy
//-------------------------------------------------------------------------------------------------

FTextureSetProxy::FTextureSetProxy(FTextureRHIParamRef InRHITexture, const TArray<FTextureRHIRef>& InRHITextureSwapChain) :
	RHITexture(InRHITexture), RHITextureSwapChain(InRHITextureSwapChain), SwapChainIndex_RHIThread(0)
{
}


FTextureSetProxy::~FTextureSetProxy()
{
	check(InRenderThread() || InRHIThread());
	ExecuteOnRHIThread([this]()
	{
		ReleaseResources_RHIThread();
	});
}


void FTextureSetProxy::GenerateMips_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	CheckInRenderThread();

	if (RHITexture->GetNumMips() > 1 && RHITexture->GetTextureCube() == nullptr)
	{
#if PLATFORM_WINDOWS
		RHICmdList.GenerateMips(RHITexture);
#endif
	}
}


void FTextureSetProxy::IncrementSwapChainIndex_RHIThread(FCustomPresent* CustomPresent)
{
	CheckInRHIThread();

	SwapChainIndex_RHIThread = (SwapChainIndex_RHIThread + 1) % GetSwapChainLength();
	CustomPresent->AliasTextureResources_RHIThread(RHITexture, RHITextureSwapChain[SwapChainIndex_RHIThread]);
}


void FTextureSetProxy::ReleaseResources_RHIThread()
{
	CheckInRHIThread();

	RHITexture = nullptr;
	RHITextureSwapChain.Empty();
}


} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
