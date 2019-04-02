// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapCustomPresent.h"
#include "MagicLeapHMD.h"
#include "RenderingThread.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_lifecycle.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

#if PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_LUMIN
#include "OpenGLDrvPrivate.h"
#include "MagicLeapHelperOpenGL.h"
#endif // PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_LUMIN

#include "MagicLeapGraphics.h"

#if PLATFORM_WINDOWS || PLATFORM_LUMIN
#include "VulkanRHIPrivate.h"
#include "XRThreadUtils.h"
#include "MagicLeapHelperVulkan.h"
#endif // PLATFORM_WINDOWS || PLATFORM_LUMIN

bool FMagicLeapCustomPresent::NeedsNativePresent()
{
	return (Plugin->GetWindowMirrorMode() > 0);
}

#if PLATFORM_WINDOWS

FMagicLeapCustomPresentD3D11::FMagicLeapCustomPresentD3D11(FMagicLeapHMD* plugin)
	: FMagicLeapCustomPresent(plugin)
	, RenderTargetTexture(0)
{
}

void FMagicLeapCustomPresentD3D11::BeginRendering()
{
	check(IsInRenderingThread());
}

void FMagicLeapCustomPresentD3D11::FinishRendering()
{
	check(IsInRenderingThread());
}

void FMagicLeapCustomPresentD3D11::Reset()
{
	if (IsInGameThread())
	{
		// Wait for all resources to be released
		FlushRenderingCommands();
	}
}

void FMagicLeapCustomPresentD3D11::Shutdown()
{
	Reset();
}

void FMagicLeapCustomPresentD3D11::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	// TODO Assign RenderTargetTexture here.
	InViewportRHI->SetCustomPresent(this);
}

void FMagicLeapCustomPresentD3D11::UpdateViewport_RenderThread()
{
}

void FMagicLeapCustomPresentD3D11::OnBackBufferResize()
{
}

bool FMagicLeapCustomPresentD3D11::Present(int32& SyncInterval)
{
	check(IsInRenderingThread());

	// turn off VSync for the 'normal Present'.
	SyncInterval = 0;
	bool bHostPresent = Plugin->GetWindowMirrorMode() > 0;

	FinishRendering();
	return bHostPresent;
}

#endif // PLATFORM_WINDOWS

#if PLATFORM_MAC

FMagicLeapCustomPresentMetal::FMagicLeapCustomPresentMetal(FMagicLeapHMD* plugin)
	: FMagicLeapCustomPresent(plugin)
	, RenderTargetTexture(0)
{
}

void FMagicLeapCustomPresentMetal::BeginRendering()
{
	check(IsInRenderingThread() || IsInRHIThread());
}

void FMagicLeapCustomPresentMetal::FinishRendering()
{
	check(IsInRenderingThread() || IsInRHIThread());
}

void FMagicLeapCustomPresentMetal::Reset()
{
	if (IsInGameThread())
	{
		// Wait for all resources to be released
		FlushRenderingCommands();
	}
}

void FMagicLeapCustomPresentMetal::Shutdown()
{
	Reset();
}

void FMagicLeapCustomPresentMetal::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	// TODO Assign RenderTargetTexture here.
	InViewportRHI->SetCustomPresent(this);
}

void FMagicLeapCustomPresentMetal::UpdateViewport_RenderThread()
{
}

void FMagicLeapCustomPresentMetal::OnBackBufferResize()
{
}

bool FMagicLeapCustomPresentMetal::Present(int32& SyncInterval)
{
	check(IsInRenderingThread() || IsInRHIThread());

	// turn off VSync for the 'normal Present'.
	SyncInterval = 0;
	bool bHostPresent = Plugin->GetWindowMirrorMode() > 0;

	FinishRendering();
	return bHostPresent;
}

#endif // PLATFORM_MAC

#if PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_LUMIN

#define BEGIN_END_FRAME_BALANCE_HACK 0

#if BEGIN_END_FRAME_BALANCE_HACK
static int BalanceCounter = 0;
static MLHandle BalanceHandles[4] = { 0,0,0,0 };
static MLHandle BalancePrevFrameHandle = 0;
#endif //BEGIN_END_FRAME_BALANCE_HACK

FMagicLeapCustomPresentOpenGL::FMagicLeapCustomPresentOpenGL(FMagicLeapHMD* plugin)
	: FMagicLeapCustomPresent(plugin)
	, RenderTargetTexture(0)
	, bFramebuffersValid(false)
{
}

void FMagicLeapCustomPresentOpenGL::BeginRendering()
{
#if WITH_MLSDK
	check(IsInRenderingThread());

	FTrackingFrame& frame = Plugin->GetCurrentFrameMutable();
	if (bCustomPresentIsSet)
	{
		// TODO [Blake] : Need to see if we can use this newer matrix and override the view
		// projection matrix (since they query GetStereoProjectionMatrix on the main thread)
		MLGraphicsFrameParams camera_params;
		MLResult Result = MLGraphicsInitFrameParams(&camera_params);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsInitFrameParams failed with status %d"), Result);
		}
		camera_params.projection_type = MLGraphicsProjectionType_ReversedInfiniteZ;
		camera_params.surface_scale = frame.PixelDensity;
		camera_params.protected_surface = false;
		GConfig->GetBool(TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings"), TEXT("bProtectedContent"), camera_params.protected_surface, GEngineIni);

		// The near clipping plane is expected in meters despite what is documented in the header.
		camera_params.near_clip = GNearClippingPlane / frame.WorldToMetersScale;
		camera_params.far_clip = frame.FarClippingPlane / frame.WorldToMetersScale;
		// Only focus distance equaling 1 engine unit seems to work on board without wearable and on desktop.
#if PLATFORM_LUMIN
		camera_params.focus_distance = frame.FocusDistance / frame.WorldToMetersScale;
#else
		camera_params.focus_distance = 1.0f;
#endif

#if BEGIN_END_FRAME_BALANCE_HACK
		if (BalanceCounter != 0)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("Begin / End frame calls out of balance!"));
			MLResult Result = MLGraphicsSignalSyncObjectGL(Plugin->GraphicsClient, BalanceHandles[0]);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsSignalSyncObjectGL for BalanceHandles[0] failed with status %d"), Result);
			}

			Result = MLGraphicsSignalSyncObjectGL(Plugin->GraphicsClient, BalanceHandles[1]);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsSignalSyncObjectGL for BalanceHandles[1] failed with status %d"), Result);
			}

			Result = MLGraphicsEndFrame(Plugin->GraphicsClient, BalancePrevFrameHandle);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsEndFrame failed with status %d"), Result);
			}
			--BalanceCounter;
		}
#endif

		Result = MLGraphicsBeginFrame(Plugin->GraphicsClient, &camera_params, &frame.Handle, &frame.RenderInfoArray);
		frame.bBeginFrameSucceeded = (Result == MLResult_Ok);
		if (frame.bBeginFrameSucceeded)
		{
#if BEGIN_END_FRAME_BALANCE_HACK
			++BalanceCounter;
			BalancePrevFrameHandle = frame.Handle;
			BalanceHandles[0] = frame.RenderInfoArray.virtual_cameras[0].sync_object;
			BalanceHandles[1] = frame.RenderInfoArray.virtual_cameras[1].sync_object;
#endif //BEGIN_END_FRAME_BALANCE_HACK

			/* Convert eye extents from Graphics Projection Model to Unreal Projection Model */
			// Unreal expects the projection matrix to be in centimeters and uses it for various purposes
			// such as bounding volume calculations for lights in the shadow algorithm.
			// We're overwriting the near value to match the units of unreal here instead of using the units of the SDK
			for (uint32_t eye = 0; eye < frame.RenderInfoArray.num_virtual_cameras; ++eye)
			{
				frame.RenderInfoArray.virtual_cameras[eye].projection.matrix_colmajor[10] = 0.0f; // Model change hack
				frame.RenderInfoArray.virtual_cameras[eye].projection.matrix_colmajor[11] = -1.0f; // Model change hack
				frame.RenderInfoArray.virtual_cameras[eye].projection.matrix_colmajor[14] = GNearClippingPlane; // Model change hack
			}
		}
		else
		{
			if (Result != MLResult_Timeout)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsBeginFrame failed with status %d"), Result);
			}
			// TODO: See if this is only needed for ZI.
			frame.Handle = ML_INVALID_HANDLE;
			MagicLeap::ResetVirtualCameraInfoArray(frame.RenderInfoArray);
		}
	}
#endif //WITH_MLSDK
}

void FMagicLeapCustomPresentOpenGL::FinishRendering()
{
#if WITH_MLSDK
	check(IsInRenderingThread());

	if (Plugin->IsDeviceInitialized() && Plugin->GetCurrentFrame().bBeginFrameSucceeded)
	{
		if (bNotifyLifecycleOfFirstPresent)
		{
			// Lifecycle tells the system's loading indicator to stop.
			// App's rendering takes over.
			MLResult Result = MLLifecycleSetReadyIndication();
			bNotifyLifecycleOfFirstPresent = (Result != MLResult_Ok);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Error (%d) sending app ready indication to lifecycle."), Result);
			}
			else
			{
				// [temporary] used for KPI tracking.
				UE_LOG(LogMagicLeap, Display, TEXT("Presenting first render from app."));
			}
		}

		// TODO [Blake] : Hack since we cannot yet specify a handle per view in the view family
		const MLGraphicsVirtualCameraInfoArray& vp_array = Plugin->GetCurrentFrame().RenderInfoArray;
		const uint32 vp_width = static_cast<uint32>(vp_array.viewport.w);
		const uint32 vp_height = static_cast<uint32>(vp_array.viewport.h);

		if (!bFramebuffersValid)
		{
			glGenFramebuffers(2, Framebuffers);
			bFramebuffersValid = true;
		}

		GLint CurrentFB = 0;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &CurrentFB);

		GLint FramebufferSRGB = 0;
		glGetIntegerv(GL_FRAMEBUFFER_SRGB, &FramebufferSRGB);
		if (FramebufferSRGB)
		{
			glDisable(GL_FRAMEBUFFER_SRGB);
		}

		//check(vp_array.num_virtual_cameras >= 2); // We assume at least one virtual camera per eye

		const FIntPoint& IdealRenderTargetSize = Plugin->GetHMDDevice()->GetIdealRenderTargetSize();
		const int32 SizeX = FMath::CeilToInt(IdealRenderTargetSize.X * Plugin->GetCurrentFrame().PixelDensity);
		const int32 SizeY = FMath::CeilToInt(IdealRenderTargetSize.Y * Plugin->GetCurrentFrame().PixelDensity);

		// this texture contains both eye renders
		glBindFramebuffer(GL_FRAMEBUFFER, Framebuffers[0]);
		FOpenGL::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, RenderTargetTexture, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, Framebuffers[1]);
		FOpenGL::FramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, vp_array.color_id, 0, 0);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, Framebuffers[0]);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, Framebuffers[1]);

		const bool bShouldFlipVertically = !IsES2Platform(GMaxRHIShaderPlatform);

		bShouldFlipVertically ?
			FOpenGL::BlitFramebuffer(0, 0, SizeX / 2, SizeY, 0, vp_height, vp_width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST) :
			FOpenGL::BlitFramebuffer(0, 0, SizeX / 2, SizeY, 0, 0, vp_width, vp_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		MLResult Result = MLGraphicsSignalSyncObjectGL(Plugin->GraphicsClient, vp_array.virtual_cameras[0].sync_object);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsSignalSyncObjectGL for eye 0 failed with status %d"), Result);
		}

		FOpenGL::FramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, vp_array.color_id, 0, 1);

		bShouldFlipVertically ?
			FOpenGL::BlitFramebuffer(SizeX / 2, 0, SizeX, SizeY, 0, vp_height, vp_width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST) :
			FOpenGL::BlitFramebuffer(SizeX / 2, 0, SizeX, SizeY, 0, 0, vp_width, vp_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		Result = MLGraphicsSignalSyncObjectGL(Plugin->GraphicsClient, vp_array.virtual_cameras[1].sync_object);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsSignalSyncObjectGL for eye 1 failed with status %d"), Result);
		}

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CurrentFB);
		if (FramebufferSRGB)
		{
			glEnable(GL_FRAMEBUFFER_SRGB);
		}

		static_assert(ARRAY_COUNT(vp_array.virtual_cameras) == 2, "The MLSDK has updated the size of the virtual_cameras array.");
#if 0 // Enable this in case the MLSDK increases the size of the virtual_cameras array past 2
		for (uint32 i = 2; i < vp_array.num_virtual_cameras; ++i)
		{
			Result = MLGraphicsSignalSyncObjectGL(Plugin->GraphicsClient, vp_array.virtual_cameras[i].sync_object);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsSignalSyncObjectGL for eye %d failed with status %d"), i, Result);
			}
		}
#endif

#if BEGIN_END_FRAME_BALANCE_HACK
		--BalanceCounter;
#endif //BEGIN_END_FRAME_BALANCE_HACK

		Result = MLGraphicsEndFrame(Plugin->GraphicsClient, Plugin->GetCurrentFrame().Handle);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsEndFrame failed with status %d"), Result);
		}
	}
	Plugin->InitializeOldFrameFromRenderFrame();
#endif //WITH_MLSDK
}

void FMagicLeapCustomPresentOpenGL::Reset()
{
	if (IsInGameThread())
	{
		// Wait for all resources to be released
		FlushRenderingCommands();
	}
	else if (IsInRenderingThread() && bFramebuffersValid)
	{
		glDeleteFramebuffers(2, Framebuffers);
		bFramebuffersValid = false;
	}
}

void FMagicLeapCustomPresentOpenGL::Shutdown()
{
	Reset();
}

void FMagicLeapCustomPresentOpenGL::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	RenderTargetTexture = *(reinterpret_cast<uint32_t*>(RT->GetNativeResource()));
	InViewportRHI->SetCustomPresent(this);
	
	FMagicLeapCustomPresentOpenGL* CustomPresent = this;
	ENQUEUE_RENDER_COMMAND(UpdateViewport_RT)(
		[CustomPresent](FRHICommandListImmediate& RHICmdList)
		{
			CustomPresent->UpdateViewport_RenderThread();
		}
	);
}

void FMagicLeapCustomPresentOpenGL::UpdateViewport_RenderThread()
{
	check(IsInRenderingThread());
	bCustomPresentIsSet = true;
}

void FMagicLeapCustomPresentOpenGL::OnBackBufferResize()
{
}

bool FMagicLeapCustomPresentOpenGL::Present(int& SyncInterval)
{
	check(IsInRenderingThread());

	// turn off VSync for the 'normal Present'.
	SyncInterval = 0;
	// We don't do any mirroring on Lumin as we render direct to the device only.
#if PLATFORM_LUMIN
	bool bHostPresent = false;
#else
	bool bHostPresent = Plugin->GetWindowMirrorMode() > 0;
#endif

	FinishRendering();

	bCustomPresentIsSet = false;

	return bHostPresent;
}

#endif  // PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_LUMIN

#if PLATFORM_WINDOWS || PLATFORM_LUMIN

FMagicLeapCustomPresentVulkan::FMagicLeapCustomPresentVulkan(FMagicLeapHMD* plugin)
	: FMagicLeapCustomPresent(plugin)
	, RenderTargetTexture(VK_NULL_HANDLE)
	, RenderTargetTextureAllocation(VK_NULL_HANDLE)
	, RenderTargetTextureAllocationOffset(0)
	, RenderTargetTextureSRGB(VK_NULL_HANDLE)
	, LastAliasedRenderTarget(VK_NULL_HANDLE)
{
}

void FMagicLeapCustomPresentVulkan::BeginRendering()
{
#if WITH_MLSDK
	check(IsInRenderingThread());

	ExecuteOnRHIThread([this]()
	{
		// Always use RHITrackingFrame here, which is then copied to the RenderTrackingFrame. 
		FTrackingFrame& RHIframe = Plugin->RHITrackingFrame;
		if (bCustomPresentIsSet)
		{
			// TODO [Blake] : Need to see if we can use this newer matrix and override the view
			// projection matrix (since they query GetStereoProjectionMatrix on the main thread)
			MLGraphicsFrameParams camera_params;
			MLResult InitResult = MLGraphicsInitFrameParams(&camera_params);
			if (InitResult != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsInitFrameParams failed with status %d"), InitResult);
			}
			camera_params.projection_type = MLGraphicsProjectionType_UnsignedZ;
			camera_params.surface_scale = RHIframe.PixelDensity;
			camera_params.protected_surface = false;
			GConfig->GetBool(TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings"), TEXT("bProtectedContent"), camera_params.protected_surface, GEngineIni);

			// The near clipping plane is expected in meters despite what is documented in the header.
			camera_params.near_clip = GNearClippingPlane / RHIframe.WorldToMetersScale;
			camera_params.far_clip = RHIframe.FarClippingPlane / RHIframe.WorldToMetersScale;

			// The focus distance is expected in meters despite what is documented in the header.
			// Only focus distance equaling 1 engine unit seems to work on board without wearable and on desktop.
#if PLATFORM_LUMIN
			camera_params.focus_distance = RHIframe.FocusDistance / RHIframe.WorldToMetersScale;
#else
			camera_params.focus_distance = 1.0f;
#endif

#if BEGIN_END_FRAME_BALANCE_HACK
			if (BalanceCounter != 0)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Begin / End frame calls out of balance!"));
				FMagicLeapHelperVulkan::SignalObjects((uint64)BalanceHandles[0], (uint64)BalanceHandles[1]);
				MLResult Result = MLGraphicsEndFrame(Plugin->GraphicsClient, BalancePrevFrameHandle);
				if (Result != MLResult_Ok)
				{
					UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsEndFrame failed with status %d"), Result);
				}
				--BalanceCounter;
			}
#endif

			MLResult Result = MLGraphicsBeginFrame(Plugin->GraphicsClient, &camera_params, &RHIframe.Handle, &RHIframe.RenderInfoArray);
			RHIframe.bBeginFrameSucceeded = (Result == MLResult_Ok);
			if (RHIframe.bBeginFrameSucceeded)
			{
#if BEGIN_END_FRAME_BALANCE_HACK
				++BalanceCounter;
				BalancePrevFrameHandle = RHIframe.Handle;
				BalanceHandles[0] = RHIframe.RenderInfoArray.virtual_cameras[0].sync_object;
				BalanceHandles[1] = RHIframe.RenderInfoArray.virtual_cameras[1].sync_object;
#endif //BEGIN_END_FRAME_BALANCE_HACK

			  /* Convert eye extents from Graphics Projection Model to Unreal Projection Model */
			  // Unreal expects the projection matrix to be in centimeters and uses it for various purposes
			  // such as bounding volume calculations for lights in the shadow algorithm.
			  // We're overwriting the near value to match the units of unreal here instead of using the units of the SDK
				for (uint32_t eye = 0; eye < RHIframe.RenderInfoArray.num_virtual_cameras; ++eye)
				{
					RHIframe.RenderInfoArray.virtual_cameras[eye].projection.matrix_colmajor[10] = 0.0f; // Model change hack
					RHIframe.RenderInfoArray.virtual_cameras[eye].projection.matrix_colmajor[11] = -1.0f; // Model change hack
					RHIframe.RenderInfoArray.virtual_cameras[eye].projection.matrix_colmajor[14] = GNearClippingPlane; // Model change hack
				}
			}
			else
			{
				if (Result != MLResult_Timeout)
				{
				UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsBeginFrame failed with status %d"), Result);
				}
				// TODO: See if this is only needed for ZI.
				RHIframe.Handle = ML_INVALID_HANDLE;
				MagicLeap::ResetVirtualCameraInfoArray(RHIframe.RenderInfoArray);
			}
			Plugin->InitializeRenderFrameFromRHIFrame();
		}
	});
#endif // WITH_MLSDK
}

void FMagicLeapCustomPresentVulkan::FinishRendering()
{
#if WITH_MLSDK
	check(IsInRenderingThread() || IsInRHIThread());

	if (Plugin->IsDeviceInitialized() && Plugin->GetCurrentFrame().bBeginFrameSucceeded)
	{
		// TODO [Blake] : Hack since we cannot yet specify a handle per view in the view family
		const MLGraphicsVirtualCameraInfoArray& vp_array = Plugin->GetCurrentFrame().RenderInfoArray;
		const uint32 vp_width = static_cast<uint32>(vp_array.viewport.w);
		const uint32 vp_height = static_cast<uint32>(vp_array.viewport.h);

		bool bTestClear = false;
		if (bTestClear)
		{
			FMagicLeapHelperVulkan::TestClear((uint64)vp_array.color_id);
		}
		else
		{
			// Alias the render target with an srgb image description for proper color space output.
			if (RenderTargetTextureAllocation != VK_NULL_HANDLE && LastAliasedRenderTarget != RenderTargetTexture)
			{
				// SDKUNREAL-1135: ML remote image is corrupted on AMD hardware
				if (!IsRHIDeviceAMD())
				{
					// TODO: If RenderTargetTextureSRGB is non-null, we're leaking the previous handle here. Also leaking on shutdown.
					RenderTargetTextureSRGB = reinterpret_cast<void *>(FMagicLeapHelperVulkan::AliasImageSRGB((uint64)RenderTargetTextureAllocation, RenderTargetTextureAllocationOffset, vp_width * 2, vp_height));
					check(RenderTargetTextureSRGB != VK_NULL_HANDLE);
				}
				LastAliasedRenderTarget = RenderTargetTexture;
				UE_LOG(LogMagicLeap, Log, TEXT("Aliased render target for correct sRGB ouput."));
			}

			const VkImage FinalTarget = (RenderTargetTextureSRGB != VK_NULL_HANDLE) ? reinterpret_cast<const VkImage>(RenderTargetTextureSRGB) : reinterpret_cast<const VkImage>(RenderTargetTexture);
			FMagicLeapHelperVulkan::BlitImage((uint64)FinalTarget, 0, 0, 0, 0, vp_width, vp_height, 1, (uint64)vp_array.color_id, 0, 0, 0, 0, vp_width, vp_height, 1);
			FMagicLeapHelperVulkan::BlitImage((uint64)FinalTarget, 0, vp_width, 0, 0, vp_width, vp_height, 1, (uint64)vp_array.color_id, 1, 0, 0, 0, vp_width, vp_height, 1);
		}

		FMagicLeapHelperVulkan::SignalObjects((uint64)vp_array.virtual_cameras[0].sync_object, (uint64)vp_array.virtual_cameras[1].sync_object);

#if BEGIN_END_FRAME_BALANCE_HACK
		--BalanceCounter;
#endif //BEGIN_END_FRAME_BALANCE_HACK

		MLResult Result = MLGraphicsEndFrame(Plugin->GraphicsClient, Plugin->GetCurrentFrame().Handle);
		if (Result != MLResult_Ok)
		{
#if !WITH_EDITOR
			UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsEndFrame failed with status %d"), Result);
#endif
		}
	}
  
	Plugin->InitializeOldFrameFromRenderFrame();
#endif // WITH_MLSDK
}

void FMagicLeapCustomPresentVulkan::Reset()
{
	if (IsInGameThread())
	{
		// Wait for all resources to be released
		FlushRenderingCommands();
	}
}

void FMagicLeapCustomPresentVulkan::Shutdown()
{
	Reset();
}

void FMagicLeapCustomPresentVulkan::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	RenderTargetTexture = RT->GetNativeResource();
	RenderTargetTextureAllocation = reinterpret_cast<void*>(static_cast<FVulkanTexture2D*>(RT->GetTexture2D())->Surface.GetAllocationHandle());
	RenderTargetTextureAllocationOffset = static_cast<FVulkanTexture2D*>(RT->GetTexture2D())->Surface.GetAllocationOffset();

	InViewportRHI->SetCustomPresent(this);
	
	FMagicLeapCustomPresentVulkan* CustomPresent = this;
	ENQUEUE_RENDER_COMMAND(UpdateViewport_RT)(
		[CustomPresent](FRHICommandListImmediate& RHICmdList)
		{
			CustomPresent->UpdateViewport_RenderThread();
		}
	);
}

void FMagicLeapCustomPresentVulkan::UpdateViewport_RenderThread()
{
	check(IsInRenderingThread());

	ExecuteOnRHIThread_DoNotWait([this]()
	{
		bCustomPresentIsSet = true;
	});
}

void FMagicLeapCustomPresentVulkan::OnBackBufferResize()
{
}

bool FMagicLeapCustomPresentVulkan::Present(int& SyncInterval)
{
#if WITH_MLSDK
	check(IsInRenderingThread() || IsInRHIThread());

	if (bNotifyLifecycleOfFirstPresent)
	{
		MLResult Result = MLLifecycleSetReadyIndication();
		bNotifyLifecycleOfFirstPresent = (Result != MLResult_Ok);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLLifecycleSetReadyIndication failed with error %d."), Result);
		}
		else
		{
			// [temporary] used for KPI tracking.
			UE_LOG(LogMagicLeap, Display, TEXT("Presenting first render from app."));
		}
	}

	// turn off VSync for the 'normal Present'.
	SyncInterval = 0;
	// We don't do any mirroring on Lumin as we render direct to the device only.
#if PLATFORM_LUMIN || PLATFORM_LUMINGL4
	bool bHostPresent = false;
#else
	bool bHostPresent = Plugin->GetWindowMirrorMode() > 0;
#endif

	FinishRendering();

	bCustomPresentIsSet = false;

	return bHostPresent;
#else
	return false;
#endif // WITH_MLSDK
}

#endif // PLATFORM_WINDOWS || PLATFORM_LUMIN
