// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DecalRenderingShared.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Materials/Material.h"
#include "RenderUtils.h"
#include "HAL/IConsoleManager.h"

// Actual values are used in the shader so do not change
enum EDecalRenderStage
{
	// for DBuffer decals (get proper baked lighting)
	DRS_BeforeBasePass = 0,
	// for volumetrics to update the depth buffer
	DRS_AfterBasePass = 1,
	// for normal decals not modifying the depth buffer
	DRS_BeforeLighting = 2,
	// for rendering decals on mobile
	DRS_Mobile = 3,
	// for rendering ambient occlusion decals
	DRS_AmbientOcclusion = 4,

	// For DBuffer decals that have emissive component.
	// All regular attributes are rendered before base pass.
	// Emissive is rendered after base pass, using additive blend.
	DRS_Emissive = 5,

	// later we could add "after lighting" and multiply
};

enum EDecalRasterizerState
{
	DRS_Undefined,
	DRS_CCW,
	DRS_CW,
};

/**
 * Shared decal functionality for deferred and forward shading
 */
struct FDecalRenderingCommon
{
	enum ERenderTargetMode
	{
		RTM_Unknown = -1,
		RTM_SceneColorAndGBufferWithNormal,
		RTM_SceneColorAndGBufferNoNormal,
		RTM_SceneColorAndGBufferDepthWriteWithNormal,
		RTM_SceneColorAndGBufferDepthWriteNoNormal,
		RTM_DBuffer, 
		RTM_GBufferNormal,
		RTM_SceneColor,
		RTM_AmbientOcclusion,
	};
	
	static EDecalBlendMode ComputeFinalDecalBlendMode(EShaderPlatform Platform, EDecalBlendMode DecalBlendMode, bool bUseNormal)
	{
		const bool bShouldConvertToDBuffer = !IsUsingGBuffers(Platform) && !IsSimpleForwardShadingEnabled(Platform) && IsUsingDBuffers(Platform);

		if (bShouldConvertToDBuffer)
		{
			switch (DecalBlendMode)
			{
			case DBM_AlphaComposite:
				DecalBlendMode = DBM_DBuffer_AlphaComposite;
				break;
			case DBM_Stain: // Stain mode can't be automatically converted. It is approximated as regular translucent.
			case DBM_Translucent:
				DecalBlendMode = DBM_DBuffer_ColorNormalRoughness;
				break;
			case DBM_Normal:
				DecalBlendMode = DBM_DBuffer_Normal;
				break;
			case DBM_Emissive:
				DecalBlendMode = DBM_DBuffer_Emissive;
				break;
			case DBM_DBuffer_ColorNormalRoughness:
			case DBM_DBuffer_Color:
			case DBM_DBuffer_ColorNormal:
			case DBM_DBuffer_ColorRoughness:
			case DBM_DBuffer_Normal:
			case DBM_DBuffer_NormalRoughness:
			case DBM_DBuffer_Roughness:
			case DBM_DBuffer_Emissive:
			case DBM_DBuffer_AlphaComposite:
			case DBM_DBuffer_EmissiveAlphaComposite:
			case DBM_Volumetric_DistanceFunction:
			case DBM_AmbientOcclusion:
				// No conversion needed
				break;
			default:
				check(0); // We must explicitly handle all decal blend modes here
			}
		}

		if (!bUseNormal)
		{
			if(DecalBlendMode == DBM_DBuffer_ColorNormalRoughness)
			{
				DecalBlendMode = DBM_DBuffer_ColorRoughness;
			}
			else if(DecalBlendMode == DBM_DBuffer_NormalRoughness)
			{
				DecalBlendMode = DBM_DBuffer_Roughness;
			}
		}
		
		return DecalBlendMode;
	}

	static EDecalBlendMode ComputeFinalDecalBlendMode(EShaderPlatform Platform, const FMaterial* Material)
	{
		return ComputeFinalDecalBlendMode(Platform,
			(EDecalBlendMode)Material->GetDecalBlendMode(),
			Material->HasNormalConnected());
	}

	static ERenderTargetMode ComputeRenderTargetMode(EShaderPlatform Platform, EDecalBlendMode DecalBlendMode, bool bHasNormal)
	{
		if (IsMobilePlatform(Platform))
		{
			return RTM_SceneColor;
		}
	
		// Can't modify GBuffers when forward shading, just modify scene color
		if (IsAnyForwardShadingEnabled(Platform)
			&& (DecalBlendMode == DBM_Translucent
				|| DecalBlendMode == DBM_Stain
				|| DecalBlendMode == DBM_Normal))
		{
			return RTM_SceneColor;
		}

		switch(DecalBlendMode)
		{
			case DBM_Translucent:
			case DBM_Stain:
				return bHasNormal ? RTM_SceneColorAndGBufferWithNormal : RTM_SceneColorAndGBufferNoNormal;

			case DBM_Normal:
				return RTM_GBufferNormal;

			case DBM_Emissive:
			case DBM_DBuffer_Emissive:
			case DBM_DBuffer_EmissiveAlphaComposite:
				return RTM_SceneColor;

			case DBM_AlphaComposite:
				return RTM_SceneColorAndGBufferNoNormal;

			case DBM_DBuffer_AlphaComposite:
			case DBM_DBuffer_ColorNormalRoughness:
			case DBM_DBuffer_Color:
			case DBM_DBuffer_ColorNormal:
			case DBM_DBuffer_ColorRoughness:
			case DBM_DBuffer_Normal:
			case DBM_DBuffer_NormalRoughness:
			case DBM_DBuffer_Roughness:
				// can be optimized using less MRT when possible
				return RTM_DBuffer;

			case DBM_Volumetric_DistanceFunction:
				return bHasNormal ? RTM_SceneColorAndGBufferDepthWriteWithNormal : RTM_SceneColorAndGBufferDepthWriteNoNormal;

			case DBM_AmbientOcclusion:
				return RTM_AmbientOcclusion;
		}

		// add the missing decal blend mode to the switch
		check(0);
		return RTM_Unknown;
	}
		
	static EDecalRenderStage ComputeRenderStage(EShaderPlatform Platform, EDecalBlendMode DecalBlendMode)
	{
		if (IsMobilePlatform(Platform))
		{
			return DRS_Mobile;
		}
		
		switch(DecalBlendMode)
		{
			case DBM_DBuffer_ColorNormalRoughness:
			case DBM_DBuffer_Color:
			case DBM_DBuffer_ColorNormal:
			case DBM_DBuffer_ColorRoughness:
			case DBM_DBuffer_Normal:
			case DBM_DBuffer_NormalRoughness:
			case DBM_DBuffer_Roughness:
			case DBM_DBuffer_AlphaComposite:
				return DRS_BeforeBasePass;

			case DBM_DBuffer_Emissive:
			case DBM_DBuffer_EmissiveAlphaComposite:
				return DRS_Emissive;

			case DBM_Translucent:
			case DBM_Stain:
			case DBM_Normal:
			case DBM_Emissive:
			case DBM_AlphaComposite:
				return DRS_BeforeLighting;
		
			case DBM_Volumetric_DistanceFunction:
				return DRS_AfterBasePass;

			case DBM_AmbientOcclusion:
				return DRS_AmbientOcclusion;

			default:
				check(0);
		}
	
		return DRS_BeforeBasePass;
	}

	static EDecalBlendMode ComputeDecalBlendModeForRenderStage(EDecalBlendMode DecalBlendMode, EDecalRenderStage DecalRenderStage)
	{
		if (DecalRenderStage == DRS_Emissive)
		{
			DecalBlendMode = (DecalBlendMode == DBM_DBuffer_AlphaComposite)
				? DBM_DBuffer_EmissiveAlphaComposite
				: DBM_DBuffer_Emissive;
		}

		return DecalBlendMode;
	}

	// @return DECAL_RENDERTARGET_COUNT for the shader
	static uint32 ComputeRenderTargetCount(EShaderPlatform Platform, ERenderTargetMode RenderTargetMode)
	{
		// has to be SceneColor on mobile 
		check(!IsMobilePlatform(Platform) || RenderTargetMode == RTM_SceneColor);

		switch(RenderTargetMode)
		{
			case RTM_SceneColorAndGBufferWithNormal:				return 4;
			case RTM_SceneColorAndGBufferNoNormal:					return 4;
			case RTM_SceneColorAndGBufferDepthWriteWithNormal:		return 5;
			case RTM_SceneColorAndGBufferDepthWriteNoNormal:		return 5;
			case RTM_DBuffer:										return IsUsingPerPixelDBufferMask(Platform) ? 4 : 3;
			case RTM_GBufferNormal:									return 1;
			case RTM_SceneColor:									return 1;
			case RTM_AmbientOcclusion:								return 1;
		}

		return 0;
	}


	static EDecalRasterizerState ComputeDecalRasterizerState(bool bInsideDecal, bool bIsInverted, bool ViewReverseCulling)
	{
		bool bClockwise = bInsideDecal;

		if (ViewReverseCulling)
		{
			bClockwise = !bClockwise;
		}

		if (bIsInverted)
		{
			bClockwise = !bClockwise;
		}
		return bClockwise ? DRS_CW : DRS_CCW;
	}

	static bool IsCompatibleWithRenderStage(EDecalRenderStage CurrentRenderStage,
		EDecalRenderStage DecalRenderStage,
		EDecalBlendMode DecalBlendMode,
		const FMaterial* DecalMaterial)
	{
		if (CurrentRenderStage == DecalRenderStage)
		{
			return true;
		}
		else if (CurrentRenderStage == DRS_Emissive)
		{
			// Any DBuffer decals that have emissive component should be rendered in DRS_BeforeBasePass and in DRS_Emissive.
			const bool bIsDBuffer = IsDBufferDecalBlendMode(DecalBlendMode);
			const bool bHasEmissiveColor = DecalMaterial->HasEmissiveColorConnected();
			return bIsDBuffer && bHasEmissiveColor;
		}
		else
		{
			return false;
		}
	}
};
