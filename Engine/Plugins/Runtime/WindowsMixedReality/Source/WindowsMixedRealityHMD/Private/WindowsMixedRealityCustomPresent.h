// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "WindowsMixedRealityPrecompiled.h"
#include "SceneRendering.h"

#include "D3D11RHIPrivate.h"
#include "D3D11Util.h"

#include "MixedRealityInterop.h"

namespace WindowsMixedReality
{
	class FWindowsMixedRealityCustomPresent : public FRHICustomPresent
	{
	public:
		FWindowsMixedRealityCustomPresent(MixedRealityInterop* _hmd, ID3D11Device* device)
			: FRHICustomPresent()
			, hmd(_hmd)
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
			if (hmd == nullptr ||
				D3D11Context == nullptr ||
				ViewportTexture == nullptr)
			{
				return false;
			}

			return hmd->Present(D3D11Context, ViewportTexture);
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
		MixedRealityInterop* hmd = nullptr;

		ID3D11DeviceContext* D3D11Context = nullptr;
		ID3D11Texture2D* ViewportTexture = nullptr;
	};
}