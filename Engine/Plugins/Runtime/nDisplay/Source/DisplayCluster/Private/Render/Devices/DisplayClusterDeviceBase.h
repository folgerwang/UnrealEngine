// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RHI.h"
#include "RHIResources.h"
#include "StereoRendering.h"
#include "StereoRenderTargetManager.h"

#include "Render/IDisplayClusterStereoRendering.h"
#include "Render/IDisplayClusterProjectionScreenDataProvider.h"
#include "Render/Devices/DisplayClusterRenderViewport.h"
#include "Render/Devices/DisplayClusterViewportArea.h"


/**
 * Abstract render device
 */
class FDisplayClusterDeviceBase
	: public IStereoRendering
	, public IStereoRenderTargetManager
	, public IDisplayClusterStereoRendering
	, public FRHICustomPresent
{
public:
	FDisplayClusterDeviceBase() = delete;
	FDisplayClusterDeviceBase(uint32 ViewsPerViewport);
	virtual ~FDisplayClusterDeviceBase();

public:
	virtual bool Initialize();

public:
	inline uint32 GetSwapInt() const
	{ return SwapInterval; }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterStereoDevice
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void AddViewport(const FString& ViewportId, IDisplayClusterProjectionScreenDataProvider* DataProvider) override;
	virtual void RemoveViewport(const FString& ViewportId) override;
	virtual void RemoveAllViewports() override;
	virtual void SetDesktopStereoParams(float FOV) override;
	virtual void SetDesktopStereoParams(const FVector2D& screenSize, const FIntPoint& screenRes, float screenDist) override;
	virtual void SetInterpupillaryDistance(float dist) override;
	virtual float GetInterpupillaryDistance() const override;
	virtual void SetEyesSwap(bool swap) override;
	virtual bool GetEyesSwap() const override;
	virtual bool ToggleEyesSwap() override;
	virtual void SetSwapSyncPolicy(EDisplayClusterSwapSyncPolicy policy) override;
	virtual EDisplayClusterSwapSyncPolicy GetSwapSyncPolicy() const override;
	virtual void GetCullingDistance(float& NearDistance, float& FarDistance) const override;
	virtual void SetCullingDistance(float NearDistance, float FarDistance) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IStereoRendering
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool IsStereoEnabled() const override;
	virtual bool IsStereoEnabledOnNextFrame() const override;
	virtual bool EnableStereo(bool stereo = true) override;
	virtual void AdjustViewRect(enum EStereoscopicPass StereoPassType, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual void CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) override;
	virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const override;
	virtual void InitCanvasFromView(class FSceneView* InView, class UCanvas* Canvas) override;
	
	virtual IStereoRenderTargetManager* GetRenderTargetManager() override
	{ return this; }

	virtual int32 GetDesiredNumberOfViews(bool bStereoRequested) const override
	{
		return RenderViewports.Num() * ViewsAmountPerViewport;
	}

	virtual EStereoscopicPass GetViewPassForIndex(bool bStereoRequested, uint32 ViewIndex) const override;
	virtual uint32 GetViewIndexForPass(EStereoscopicPass StereoPassType) const override;

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IStereoRenderTargetManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	/**
	* Whether a separate render target should be used or not.
	* In case the stereo rendering implementation does not require special handling of separate render targets
	* at all, it can leave out implementing this interface completely and simply let the default implementation
	* of IStereoRendering::GetRenderTargetManager() return nullptr.
	*/
	virtual bool ShouldUseSeparateRenderTarget() const override
	{ return false; }

	/**
	* Updates viewport for direct rendering of distortion. Should be called on a game thread.
	*
	* @param bUseSeparateRenderTarget	Set to true if a separate render target will be used. Can potentiallt be true even if ShouldUseSeparateRenderTarget() returned false earlier.
	* @param Viewport					The Viewport instance calling this method.
	* @param ViewportWidget			(optional) The Viewport widget containing the view. Can be used to access SWindow object.
	*/
	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget = nullptr) override;

	/**
	* Calculates dimensions of the render target texture for direct rendering of distortion.
	*/
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;

	/**
	* Returns true, if render target texture must be re-calculated.
	*/
	virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) override
	{ return false; }

	/**
	* Returns true, if render target texture must be re-calculated.
	*/
	virtual bool NeedReAllocateDepthTexture(const TRefCountPtr<struct IPooledRenderTarget>& DepthTarget) override
	{ return false; }

	/**
	* Returns number of required buffered frames.
	*/
	virtual uint32 GetNumberOfBufferedFrames() const override
	{ return 1; }

	/**
	* Allocates a render target texture.
	* The default implementation always return false to indicate that the default texture allocation should be used instead.
	*
	* @param Index			(in) index of the buffer, changing from 0 to GetNumberOfBufferedFrames()
	* @return				true, if texture was allocated; false, if the default texture allocation should be used.
	*/
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override
	{ return false; }

	/**
	* Allocates a depth texture.
	*
	* @param Index			(in) index of the buffer, changing from 0 to GetNumberOfBufferedFrames()
	* @return				true, if texture was allocated; false, if the default texture allocation should be used.
	*/
	virtual bool AllocateDepthTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) { return false; }

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// FRHICustomPresent
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void OnBackBufferResize() override;
	// Called from render thread to see if a native present will be requested for this frame.
	// @return	true if native Present will be requested for this frame; false otherwise.  Must
	// match value subsequently returned by Present for this frame.
	virtual bool NeedsNativePresent() override
	{ return true; }

	virtual bool Present(int32& InOutSyncInterval) override;

protected:
	enum EDisplayClusterEyeType
	{
		StereoLeft  = 0,
		Mono        = 1,
		StereoRight = 2,
		COUNT
	};

protected:
	// Retrieves the projections screen data for current frame
	void UpdateProjectionDataForThisFrame();

	// Custom projection screen geometry (hw - half-width, hh - half-height of projection screen)
	// Left bottom corner (from camera point view)
	virtual FVector GetProjectionScreenGeometryLBC(const float& hw, const float& hh) const
	{ return FVector(0.f, -hw, -hh);}
	
	// Right bottom corner (from camera point view)
	virtual FVector GetProjectionScreenGeometryRBC(const float& hw, const float& hh) const
	{ return FVector(0.f, hw, -hh);}

	// Left top corner (from camera point view)
	virtual FVector GetProjectionScreenGeometryLTC(const float& hw, const float& hh) const
	{ return FVector(0.f, -hw, hh);}

	// Encodes view index to EStereoscopicPass (the value may be out of EStereoscopicPass bounds)
	EStereoscopicPass EncodeStereoscopicPass(int ViewIndex) const;
	// Decodes normal EStereoscopicPass from encoded EStereoscopicPass
	EStereoscopicPass DecodeStereoscopicPass(const enum EStereoscopicPass StereoPassType) const;
	// Decodes viewport index from encoded EStereoscopicPass
	int DecodeViewportIndex(const enum EStereoscopicPass StereoPassType) const;
	// Decodes eye type from encoded EStereoscopicPass
	EDisplayClusterEyeType DecodeEyeType(const enum EStereoscopicPass StereoPassType) const;

protected:
	// Implements buffer swap synchronization mechanism depending on selected sync policy
	virtual void WaitForBufferSwapSync(int32& InOutSyncInterval);

	virtual void PerformSynchronizationPolicyNone(int32& InOutSyncInterval);
	virtual void PerformSynchronizationPolicySoft(int32& InOutSyncInterval);
	virtual void PerformSynchronizationPolicyNvSwapLock(int32& InOutSyncInterval);
	void exec_BarrierWait();

protected:
	// Data access synchronization
	mutable FCriticalSection InternalsSyncScope;

	// Viewport and back buffer size
	FIntPoint BackBuffSize = { 0, 0 };
	FIntPoint ViewportSize = { 0, 0 };

	// Stereo parameters
	float EyeDist      = 0.064f; // meters
	bool  bEyeSwap     = false;

	// Current world scale
	float CurrentWorldToMeters = 100.f;

	// Viewports
	TArray<FDisplayClusterRenderViewport> RenderViewports;

	// Views per viewport (render passes)
	uint32 ViewsAmountPerViewport = 0;

	// Clipping plane
	float NearClipPlane = GNearClippingPlane;
	float FarClipPlane = 2000000.f;

	uint32 SwapInterval = 1;

	// Swap sync policy
	EDisplayClusterSwapSyncPolicy SwapSyncPolicy = EDisplayClusterSwapSyncPolicy::None;

protected:
	FViewport* MainViewport = nullptr;
};
