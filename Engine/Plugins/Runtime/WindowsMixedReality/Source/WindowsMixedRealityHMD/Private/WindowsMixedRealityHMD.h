// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "HeadMountedDisplayBase.h"
#include "XRTrackingSystemBase.h"
#include "SceneViewExtension.h"
#include "DefaultXRCamera.h"

#include "Windows/WindowsApplication.h"
#include "Framework/Application/SlateApplication.h"

#include "WindowsMixedRealityCustomPresent.h"

#include "XRRenderTargetManager.h"
#include "RendererInterface.h"

#if WITH_WINDOWS_MIXED_REALITY
#include "Windows/AllowWindowsPlatformTypes.h"
#include "MixedRealityInterop.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace WindowsMixedReality
{
	// Plugin for stereo rendering on Windows Mixed Reality devices.
	class FWindowsMixedRealityHMD
		: public FHeadMountedDisplayBase
		, public FXRRenderTargetManager
		, public FSceneViewExtensionBase
	{
	public:

		/** IXRTrackingSystem interface */
		virtual FName GetSystemName() const override
		{
			static FName DefaultName(TEXT("WindowsMixedRealityHMD"));
			return DefaultName;
		}

		virtual FString GetVersionString() const override;

		virtual void OnBeginPlay(FWorldContext& InWorldContext) override;
		virtual void OnEndPlay(FWorldContext& InWorldContext) override;
		virtual bool OnStartGameFrame(FWorldContext& WorldContext) override;
		virtual void SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin) override;
		virtual EHMDTrackingOrigin::Type GetTrackingOrigin() override;

		virtual bool EnumerateTrackedDevices(
			TArray<int32>& OutDevices,
			EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) override;

		virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
		virtual float GetInterpupillaryDistance() const override;

		virtual void ResetOrientationAndPosition(float yaw = 0.f) override;
		virtual void ResetOrientation(float yaw = 0.f) override { }
		virtual void ResetPosition() override { }

		virtual bool GetCurrentPose(
			int32 DeviceId,
			FQuat& CurrentOrientation,
			FVector& CurrentPosition) override;
		virtual bool GetRelativeEyePose(int32 DeviceId, EStereoscopicPass Eye, FQuat& OutOrientation, FVector& OutPosition) override;

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

	private:
		int gameWindowWidth = 1920;
		int gameWindowHeight = 1080;

	public:
		/** IHeadMountedDisplay interface */
		virtual bool IsHMDConnected() override;
		virtual bool IsHMDEnabled() const override;
		virtual EHMDWornState::Type GetHMDWornState() override;
		virtual void EnableHMD(bool allow = true) override { }
		virtual bool GetHMDMonitorInfo(MonitorInfo&) override { return true; }
		virtual void GetFieldOfView(
			float& OutHFOVInDegrees,
			float& OutVFOVInDegrees) const override { }
		virtual bool IsChromaAbCorrectionEnabled() const override { return false; }

		/** IStereoRendering interface */
		virtual bool IsStereoEnabled() const override;
		virtual bool EnableStereo(bool stereo = true) override;
		virtual void AdjustViewRect(
			EStereoscopicPass StereoPass,
			int32& X, int32& Y,
			uint32& SizeX, uint32& SizeY) const override;
		virtual void CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, FRotator& ViewRotation,
			const float MetersToWorld, FVector& ViewLocation) override;
		virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const override;
		virtual IStereoRenderTargetManager* GetRenderTargetManager() override { return this; }

		virtual bool HasHiddenAreaMesh() const override;
		virtual void DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

		virtual bool HasVisibleAreaMesh() const override;
		virtual void DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

		/** ISceneViewExtension interface */
		virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
		virtual void SetupView(
			FSceneViewFamily& InViewFamily,
			FSceneView& InView) override { }
		virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override { }
		virtual void PreRenderView_RenderThread(
			FRHICommandListImmediate& RHICmdList,
			FSceneView& InView) override { }
		virtual void PreRenderViewFamily_RenderThread(
			FRHICommandListImmediate& RHICmdList,
			FSceneViewFamily& InViewFamily) override;
		virtual bool IsActiveThisFrame(class FViewport* InViewport) const;

		void CreateHMDDepthTexture(FRHICommandListImmediate& RHICmdList);

	public:
#if WITH_WINDOWS_MIXED_REALITY
		FWindowsMixedRealityHMD(const FAutoRegister&, MixedRealityInterop* InHMD);
#endif
		virtual ~FWindowsMixedRealityHMD();
		bool IsInitialized() const;

		void InitializeHolographic();
		void ShutdownHolographic();

		bool IsCurrentlyImmersive();

	private:
		void StartCustomPresent();

		TRefCountPtr<ID3D11Device> InternalGetD3D11Device();
#if WITH_WINDOWS_MIXED_REALITY
		MixedRealityInterop* HMD = nullptr;
#endif

		bool bIsStereoEnabled = false;
		bool bIsStereoDesired = true;

		bool bRequestRestart = false;

		float ScreenScalePercentage = 1.0f;
		float CachedWorldToMetersScale = 100.0f;

		TRefCountPtr<ID3D11Device> D3D11Device = nullptr;

		FTexture2DRHIRef remappedDepthTexture = nullptr;
		ID3D11Texture2D* stereoDepthTexture = nullptr;
		const float farPlaneDistance = 100000.0f;

		// The back buffer for this frame
		FTexture2DRHIRef CurrentBackBuffer;
		void InitTrackingFrame();
		TRefCountPtr<FWindowsMixedRealityCustomPresent> mCustomPresent = nullptr;

		EHMDTrackingOrigin::Type HMDTrackingOrigin;
		FIntRect EyeRenderViewport;

		FQuat CurrOrientation = FQuat::Identity;
		FVector CurrPosition = FVector::ZeroVector;
		FQuat RotationL = FQuat::Identity;
		FQuat RotationR = FQuat::Identity;
		FVector PositionL = FVector::ZeroVector;
		FVector PositionR = FVector::ZeroVector;

		float ipd = 0;

		TArray<FHMDViewMesh> HiddenAreaMesh;
		TArray<FHMDViewMesh> VisibleAreaMesh;

		void StopCustomPresent();

		void SetupHolographicCamera();

		// Inherited via FXRRenderTargetManager
		virtual void GetEyeRenderParams_RenderThread(
			const struct FRenderingCompositePassContext& Context,
			FVector2D& EyeToSrcUVScaleValue,
			FVector2D& EyeToSrcUVOffsetValue) const override;
		virtual FIntPoint GetIdealRenderTargetSize() const override;
		virtual float GetPixelDenity() const override;
		virtual void SetPixelDensity(const float NewDensity) override;
		virtual void UpdateViewportRHIBridge(bool bUseSeparateRenderTarget, const class FViewport& Viewport, FRHIViewport* const ViewportRHI) override;
		virtual void RenderTexture_RenderThread
		(
			class FRHICommandListImmediate & RHICmdList,
			class FRHITexture2D * BackBuffer,
			class FRHITexture2D * SrcTexture,
			FVector2D WindowSize
		) const override;
		virtual bool AllocateRenderTargetTexture(
			uint32 index,
			uint32 sizeX, uint32 sizeY,
			uint8 format,
			uint32 numMips,
			uint32 flags,
			uint32 targetableTextureFlags,
			FTexture2DRHIRef& outTargetableTexture,
			FTexture2DRHIRef& outShaderResourceTexture,
			uint32 numSamples = 1) override;

		virtual bool GetHMDDistortionEnabled(EShadingPath ShadingPath) const override
		{
			return false;
		}

		virtual bool ShouldUseSeparateRenderTarget() const override
		{
			return IsStereoEnabled();
		}

	private:
		// Handle app suspend requests.
		FDelegateHandle PauseHandle;
		void AppServicePause();

		IRendererModule* RendererModule = nullptr;

		EHMDWornState::Type currentWornState = EHMDWornState::Type::Unknown;
		bool mouseLockedToCenter = true;

	public:
		// Spatial input
		bool IsAvailable();
		bool SupportsSpatialInput();
#if WITH_WINDOWS_MIXED_REALITY
		MixedRealityInterop::HMDTrackingStatus GetControllerTrackingStatus(MixedRealityInterop::HMDHand hand);
		bool GetControllerOrientationAndPosition(MixedRealityInterop::HMDHand hand, FRotator & OutOrientation, FVector & OutPosition);
		bool PollInput();

		MixedRealityInterop::HMDInputPressState GetPressState(
			MixedRealityInterop::HMDHand hand,
			MixedRealityInterop::HMDInputControllerButtons button);
		float GetAxisPosition(
			MixedRealityInterop::HMDHand hand,
			MixedRealityInterop::HMDInputControllerAxes axis);
		void SubmitHapticValue(
			MixedRealityInterop::HMDHand hand,
			float value);
#endif
		void LockMouseToCenter(bool locked)
		{
			mouseLockedToCenter = locked;
		}

	public:
		// Remoting
		void ConnectToRemoteHoloLens(const wchar_t* ip, unsigned int bitrate);
		void DisconnectFromRemoteHoloLens();
	};
}

