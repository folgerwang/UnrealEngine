// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RHI.h"
#include "RHIResources.h"
#include "StereoRendering.h"
#include "StereoRenderTargetManager.h"

#include "Render/IDisplayClusterStereoDevice.h"
#include "Render/Devices/DisplayClusterViewportArea.h"


/**
 * Abstract render device
 */
class FDisplayClusterDeviceBase
	: public  IStereoRendering
	, public  IStereoRenderTargetManager
	, public  IDisplayClusterStereoDevice
	, public  FRHICustomPresent
{
public:
	FDisplayClusterDeviceBase();
	virtual ~FDisplayClusterDeviceBase();

public:
	virtual bool Initialize();

protected:

	inline uint32 GetSwapInt() const
	{ return SwapInterval; }

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IStereoRendering
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool IsStereoEnabled() const override;
	virtual bool IsStereoEnabledOnNextFrame() const override;
	virtual bool EnableStereo(bool stereo = true) override;
	virtual void AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual void CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) override;
	virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const override;
	virtual void InitCanvasFromView(class FSceneView* InView, class UCanvas* Canvas) override;
	virtual IStereoRenderTargetManager* GetRenderTargetManager() override
	{ return this; }

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

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterStereoDevice
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void SetViewportArea(const FIntPoint& loc, const FIntPoint& size) override;
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

protected:
	// Implements buffer swap synchronization mechanism
	virtual void WaitForBufferSwapSync(int32& InOutSyncInterval);
	// Retrieves the projections screen data for current frame
	void UpdateProjectionScreenDataForThisFrame();

	// Custom projection screen geometry (hw - half-width, hh - half-height of projection screen)
	// Left bottom corner (from camera point view)
	virtual FVector GetProjectionScreenGeometryLBC(const enum EStereoscopicPass StereoPassType, const float& hw, const float& hh) const
	{ return FVector(0.f, -hw, -hh);}
	
	// Right bottom corner (from camera point view)
	virtual FVector GetProjectionScreenGeometryRBC(const enum EStereoscopicPass StereoPassType, const float& hw, const float& hh) const
	{ return FVector(0.f, hw, -hh);}

	// Left top corner (from camera point view)
	virtual FVector GetProjectionScreenGeometryLTC(const enum EStereoscopicPass StereoPassType, const float& hw, const float& hh) const
	{ return FVector(0.f, -hw, hh);}

protected:
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
	FVector  EyeLoc[2] = { FVector::ZeroVector, FVector::ZeroVector };
	FRotator EyeRot[2] = { FRotator::ZeroRotator, FRotator::ZeroRotator };

	// Current world scale
	float CurrentWorldToMeters = 100.f;

	// Viewport area settings
	FDisplayClusterViewportArea ViewportArea;

	// Clipping plane
	float NearClipPlane = GNearClippingPlane;
	float FarClipPlane = 2000000.f;

	// Projection screen data
	FVector   ProjectionScreenLoc;
	FRotator  ProjectionScreenRot;
	FVector2D ProjectionScreenSize;

	uint32 SwapInterval = 1;

	// Swap sync policy
	EDisplayClusterSwapSyncPolicy SwapSyncPolicy = EDisplayClusterSwapSyncPolicy::None;

protected:
	FViewport* CurrentViewport;
};
