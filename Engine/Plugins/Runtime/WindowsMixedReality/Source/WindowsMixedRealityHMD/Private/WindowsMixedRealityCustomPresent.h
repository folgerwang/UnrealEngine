// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "WindowsMixedRealityPrecompiled.h"
#include "SceneRendering.h"

#include "D3D11RHIPrivate.h"
#include "D3D11Util.h"

#if WITH_WINDOWS_MIXED_REALITY
#include "Windows/AllowWindowsPlatformTypes.h"
#include "MixedRealityInterop.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace WindowsMixedReality
{
	class FWindowsMixedRealityCustomPresent : public FRHICustomPresent
	{
	public:
		FWindowsMixedRealityCustomPresent(
#if WITH_WINDOWS_MIXED_REALITY
			MixedRealityInterop* _hmd, 
#endif
			ID3D11Device* device)
			: FRHICustomPresent()
#if WITH_WINDOWS_MIXED_REALITY
			, hmd(_hmd)
#endif
		{
			// Get the D3D11 context.
			device->GetImmediateContext(&D3D11Context);
		}

		// Inherited via FRHICustomPresent
		virtual void OnBackBufferResize() override { }
		virtual bool NeedsNativePresent() override
		{
			return true;
		}
		virtual bool Present(int32 & InOutSyncInterval) override
		{
#if WITH_WINDOWS_MIXED_REALITY
			if (hmd == nullptr ||
				D3D11Context == nullptr ||
				ViewportTexture == nullptr)
			{
				return false;
			}

			return hmd->Present(D3D11Context, ViewportTexture);
#else
			return false;
#endif
		}

		void UpdateViewport(
			const FViewport& InViewport,
			class FRHIViewport* InViewportRHI)
		{
			if (InViewportRHI == nullptr)
			{
				return;
			}

			if (InViewportRHI->GetCustomPresent() != this)
			{
				InViewportRHI->SetCustomPresent(this);
			}

			const FTexture2DRHIRef& RT = InViewport.GetRenderTargetTexture();
			if (!IsValidRef(RT))
			{
				return;
			}

			ViewportTexture = (ID3D11Texture2D*)RT->GetNativeResource();
		}


	private:
#if WITH_WINDOWS_MIXED_REALITY
		MixedRealityInterop* hmd = nullptr;
#endif

		ID3D11DeviceContext* D3D11Context = nullptr;
		ID3D11Texture2D* ViewportTexture = nullptr;
	};
}