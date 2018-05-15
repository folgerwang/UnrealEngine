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

#include "CoreMinimal.h"
#include "RendererInterface.h"

#include "MagicLeapMath.h"
#include "MagicLeapUtils.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_api.h>

#if PLATFORM_LUMIN
#include <vulkan.h>
#include <ml_graphics.h>
#else
#include <ml_graphics.h>
#endif // PLATFORM_LUMIN

#include <ml_snapshot.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

class FMagicLeapHMD;
class FViewport;

static const uint8 kNumEyes = 2;

struct FTrackingFrame
{
public:
	uint64 FrameNumber; // current frame number
	bool HasHeadTrackingPosition;
	FTransform RawPose;
	float HFov;
	float VFov;
	float WorldToMetersScale;
	float FocusDistance;
	float NearClippingPlane;
	float FarClippingPlane;
	float RecommendedFarClippingPlane;
#if WITH_MLSDK
	MLSnapshot* Snapshot;
#endif //WITH_MLSDK
	bool bBeginFrameSucceeded;

#if WITH_MLSDK
	MLHandle Handle;
	MLCoordinateFrameUID FrameId;
	MLGraphicsClipExtentsInfoArray UpdateInfoArray; // update information for the frame
	MLGraphicsVirtualCameraInfoArray RenderInfoArray; // render information for the frame
#endif //WITH_MLSDK

	// [0.0f, 1.0f]
	float ScreenPercentage;

	FTrackingFrame()
		: FrameNumber(0)
		, HasHeadTrackingPosition(false)
		, RawPose(FTransform::Identity)
		, HFov(0.0f)
		, VFov(0.0f)
		, WorldToMetersScale(100.0f)
		, FocusDistance(1.0f)
		, NearClippingPlane(GNearClippingPlane)
		, FarClippingPlane(1000.0f) // 10m
		, RecommendedFarClippingPlane(FarClippingPlane)
#if WITH_MLSDK
		, Snapshot(nullptr)
#endif //WITH_MLSDK
		, bBeginFrameSucceeded(false)
#if WITH_MLSDK
		, Handle(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
		, ScreenPercentage(1.0f)
	{
#if WITH_MLSDK
		FrameId.data[0] = 0;
		FrameId.data[1] = 0;

		MagicLeap::ResetClipExtentsInfoArray(UpdateInfoArray);
		MagicLeap::ResetVirtualCameraInfoArray(RenderInfoArray);
#endif //WITH_MLSDK
	}
};

class FMagicLeapCustomPresent : public FRHICustomPresent
{
public:
	FMagicLeapCustomPresent(FMagicLeapHMD* plugin) :
		FRHICustomPresent(),
		Plugin(plugin),
		bNeedReinitRendererAPI(true),
		bNotifyLifecycleOfFirstPresent(true),
		bCustomPresentIsSet(false)
	{}

	virtual void BeginRendering() = 0;
	virtual void FinishRendering() = 0;
	virtual bool NeedsNativePresent() override;

	virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI) = 0;
	virtual void UpdateViewport_RenderThread() = 0;
	virtual void Reset() = 0;
	virtual void Shutdown() = 0;

	virtual void SetNeedReinitRendererAPI() { bNeedReinitRendererAPI = true; }

protected:
	FMagicLeapHMD* Plugin;
	bool bNeedReinitRendererAPI;
	bool bNotifyLifecycleOfFirstPresent;
	bool bCustomPresentIsSet;
};

#if PLATFORM_WINDOWS
class FMagicLeapCustomPresentD3D11 : public FMagicLeapCustomPresent
{
public:
	FMagicLeapCustomPresentD3D11(FMagicLeapHMD* plugin);

	virtual void OnBackBufferResize() override;
	virtual bool Present(int& SyncInterval) override;

	virtual void BeginRendering() override;
	virtual void FinishRendering() override;
	virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI) override;
	virtual void UpdateViewport_RenderThread() override;
	virtual void Reset() override;
	virtual void Shutdown() override;

protected:
	uint32_t RenderTargetTexture = 0;
};
#endif // PLATFORM_WINDOWS

#if PLATFORM_MAC
class FMagicLeapCustomPresentMetal : public FMagicLeapCustomPresent
{
public:
	FMagicLeapCustomPresentMetal(FMagicLeapHMD* plugin);

	virtual void OnBackBufferResize() override;
	virtual bool Present(int& SyncInterval) override;

	virtual void BeginRendering() override;
	virtual void FinishRendering() override;
	virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI) override;
	virtual void UpdateViewport_RenderThread() override;
	virtual void Reset() override;
	virtual void Shutdown() override;

protected:
	uint32_t RenderTargetTexture = 0;
};
#endif // PLATFORM_MAC

#if PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_LUMIN
class FMagicLeapCustomPresentOpenGL : public FMagicLeapCustomPresent
{
public:
	FMagicLeapCustomPresentOpenGL(FMagicLeapHMD* plugin);

	virtual void OnBackBufferResize() override;
	virtual bool Present(int& SyncInterval) override;

	virtual void BeginRendering() override;
	virtual void FinishRendering() override;
	virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI) override;
	virtual void UpdateViewport_RenderThread() override;
	virtual void Reset() override;
	virtual void Shutdown() override;

protected:
	uint32_t RenderTargetTexture = 0;
	uint32_t Framebuffers[2];
	bool bFramebuffersValid;
};
#endif // PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_LUMIN

#if PLATFORM_LUMIN
class FMagicLeapCustomPresentVulkan : public FMagicLeapCustomPresent
{
public:
	FMagicLeapCustomPresentVulkan(FMagicLeapHMD* plugin);

	virtual void OnBackBufferResize() override;
	virtual bool Present(int& SyncInterval) override;

	virtual void BeginRendering() override;
	virtual void FinishRendering() override;
	virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI) override;
	virtual void UpdateViewport_RenderThread() override;
	virtual void Reset() override;
	virtual void Shutdown() override;

protected:
	VkImage RenderTargetTexture = VK_NULL_HANDLE;
	VkDeviceMemory RenderTargetTextureAllocation = VK_NULL_HANDLE;
	VkImage RenderTargetTextureSRGB = VK_NULL_HANDLE;
	VkImage LastAliasedRenderTarget = VK_NULL_HANDLE;
};
#endif // PLATFORM_LUMIN


void CaptureCallStack(FString& OutCallstack);
