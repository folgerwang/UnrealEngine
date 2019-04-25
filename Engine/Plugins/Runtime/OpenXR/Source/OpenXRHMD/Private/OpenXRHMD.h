// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HeadMountedDisplayBase.h"
#include "XRTrackingSystemBase.h"
#include "XRRenderTargetManager.h"
#include "XRRenderBridge.h"
#include "SceneViewExtension.h"
#include "DefaultSpectatorScreenController.h"

#include <openxr/openxr.h>

class APlayerController;
class FSceneView;
class FSceneViewFamily;
class UCanvas;

/**
 * Simple Head Mounted Display
 */
class FOpenXRHMD : public FHeadMountedDisplayBase, public FXRRenderTargetManager, public FSceneViewExtensionBase
{
public:
	class FOpenXRSwapchain : public TSharedFromThis<FOpenXRSwapchain, ESPMode::ThreadSafe>
	{
	public:
		FOpenXRSwapchain(XrSwapchain InSwapchain, FTexture2DRHIParamRef InRHITexture, const TArray<FTexture2DRHIRef>& InRHITextureSwapChain);
		virtual ~FOpenXRSwapchain();

		FRHITexture* GetTexture() const { return RHITexture.GetReference(); }
		FRHITexture2D* GetTexture2D() const { return RHITexture->GetTexture2D(); }
		FRHITextureCube* GetTextureCube() const { return RHITexture->GetTextureCube(); }
		uint32 GetSwapchainLength() const { return (uint32)RHITextureSwapChain.Num(); }

		uint32 GetSwapchainIndex_RenderThread() { return SwapChainIndex_RenderThread; }
		void IncrementSwapChainIndex_RenderThread(XrDuration Timeout);
		void ReleaseSwapChainImage_RenderThread();
		
		XrSwapchain Handle;
	protected:
		void ReleaseResources_RenderThread();

		FTextureRHIRef RHITexture;
		TArray<FTexture2DRHIRef> RHITextureSwapChain;
		uint32 SwapChainIndex_RenderThread;
		bool IsAcquired;
	};

	class D3D11Bridge : public FXRRenderBridge
	{
	public:
		D3D11Bridge(FOpenXRHMD* HMD)
			: OpenXRHMD(HMD)
		{}

		/** FRHICustomPresent */
		virtual bool Present(int32& InOutSyncInterval);

	private:
		FOpenXRHMD* OpenXRHMD;
	};

	/** IXRTrackingSystem interface */
	virtual FName GetSystemName() const override
	{
		static FName DefaultName(TEXT("OpenXR"));
		return DefaultName;
	}

	virtual bool EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) override;

	virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
	virtual float GetInterpupillaryDistance() const override;
	virtual bool GetRelativeEyePose(int32 InDeviceId, EStereoscopicPass InEye, FQuat& OutOrientation, FVector& OutPosition) override;

	virtual void ResetOrientationAndPosition(float yaw = 0.f) override;
	virtual void ResetOrientation(float Yaw = 0.f) override;
	virtual void ResetPosition() override;

	virtual bool GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	virtual void SetBaseRotation(const FRotator& BaseRot) override;
	virtual FRotator GetBaseRotation() const override;

	virtual void SetBaseOrientation(const FQuat& BaseOrient) override;
	virtual FQuat GetBaseOrientation() const override;

	virtual void SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin) override
	{
		TrackingSpaceType = (NewOrigin == EHMDTrackingOrigin::Floor && StageSpace != XR_NULL_HANDLE) ? XR_REFERENCE_SPACE_TYPE_STAGE : XR_REFERENCE_SPACE_TYPE_LOCAL;
	}

	virtual EHMDTrackingOrigin::Type GetTrackingOrigin() override
	{
		return (TrackingSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE) ? EHMDTrackingOrigin::Floor : EHMDTrackingOrigin::Eye;
	}

	virtual class IHeadMountedDisplay* GetHMDDevice() override
	{ 
		return this;
	}

	virtual class TSharedPtr< class IStereoRendering, ESPMode::ThreadSafe > GetStereoRenderingDevice() override
	{
		return SharedThis(this);
	}
protected:
	/** FXRTrackingSystemBase protected interface */
	virtual float GetWorldToMetersScale() const override;

public:
	/** IHeadMountedDisplay interface */
	virtual bool IsHMDConnected() override { return true; }
	virtual bool DoesSupportPositionalTracking() const override { return true; }
	virtual bool IsHMDEnabled() const override;
	virtual void EnableHMD(bool allow = true) override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;
	virtual void GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const override;
	virtual bool IsChromaAbCorrectionEnabled() const override;
	virtual FIntPoint GetIdealRenderTargetSize() const override;
	virtual bool GetHMDDistortionEnabled(EShadingPath ShadingPath) const override { return false; }
	virtual FIntRect GetFullFlatEyeRect_RenderThread(FTexture2DRHIRef EyeTexture) const override;
	virtual void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef SrcTexture, FIntRect SrcRect, FTexture2DRHIParamRef DstTexture, FIntRect DstRect, bool bClearBlack, bool bNoAlpha) const override;
	virtual bool HasHiddenAreaMesh() const override { return false; };
	virtual void DrawHiddenAreaMesh_RenderThread(class FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;
	virtual void OnBeginRendering_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) override;
	virtual void OnBeginRendering_GameThread() override;
	virtual void OnLateUpdateApplied_RenderThread(const FTransform& NewRelativeTransform) override;
	virtual bool OnStartGameFrame(FWorldContext& WorldContext) override;

	/** IStereoRendering interface */
	virtual bool IsStereoEnabled() const override;
	virtual bool EnableStereo(bool stereo = true) override;
	virtual void AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual int32 GetDesiredNumberOfViews(bool bStereoRequested) const override;
	virtual EStereoscopicPass GetViewPassForIndex(bool bStereoRequested, uint32 ViewIndex) const override;
	virtual uint32 GetViewIndexForPass(EStereoscopicPass StereoPassType) const override;
	
	virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const override;
	virtual void GetEyeRenderParams_RenderThread(const struct FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override;
	virtual IStereoRenderTargetManager* GetRenderTargetManager() override;
	virtual void RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture, FVector2D WindowSize) const override;

	/** ISceneViewExtension interface */
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual bool IsActiveThisFrame(class FViewport* InViewport) const;

	/** IStereoRenderTargetManager */
	virtual bool ShouldUseSeparateRenderTarget() const override { return true; }
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override;
	virtual FXRRenderBridge* GetActiveRenderBridge_GameThread(bool bUseSeparateRenderTarget) override;

	/** IStereoRenderTargetManager */
	virtual void OnBeginPlay(FWorldContext& InWorldContext) override;
	virtual void OnEndPlay(FWorldContext& InWorldContext) override;

public:
	/** Constructor */
	FOpenXRHMD(const FAutoRegister&, XrInstance InInstance, XrSystemId InSystem);

	/** Destructor */
	virtual ~FOpenXRHMD();

	/** @return	True if the HMD was initialized OK */
	OPENXRHMD_API bool IsInitialized() const;
	OPENXRHMD_API bool IsRunning() const;
	void FinishRendering();

	OPENXRHMD_API int32 AddActionDevice(XrAction Action);

	FOpenXRSwapchain* GetSwapchain() { return Swapchain.Get(); }
	XrInstance GetInstance() { return Instance; }
	XrSystemId GetSystem() { return System; }
	XrSession GetSession() { return Session; }
	XrSpace GetTrackingSpace()
	{
		return (TrackingSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE) ? StageSpace : LocalSpace;
	}

private:
	bool					bIsRunning;

	FTransform				BaseTransform;
	XrInstance				Instance;
	XrSystemId				System;
	XrSession				Session;
	TArray<XrSpace>			DeviceSpaces;
	XrSpace					LocalSpace;
	XrSpace					StageSpace;
	XrSpace					TrackingSpaceRHI;
	XrReferenceSpaceType	TrackingSpaceType;

	XrFrameState			FrameState;
	XrFrameState			FrameStateRHI;
	XrViewState				ViewState;

	TArray<XrViewConfigurationView> Configs;
	TArray<XrView>			Views;
	TArray<XrCompositionLayerProjectionView> ViewsRHI;
#if 0
	TArray<FHMDViewMesh>	HiddenAreaMeshes;
#endif

	TRefCountPtr<FXRRenderBridge> RenderBridge;
	IRendererModule*		RendererModule;

	// TODO: Needs to be part of a custom present class
	TSharedPtr<FOpenXRSwapchain, ESPMode::ThreadSafe> Swapchain;
};
