// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DebugViewModeInterface.cpp: Contains definitions for rendering debug viewmodes.
=============================================================================*/

#include "DebugViewModeInterface.h"
#include "Materials/Material.h"
#include "RHIStaticStates.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

FDebugViewModeInterface* FDebugViewModeInterface::Singletons[DVSM_MAX] = {}; // Init to null.

void FDebugViewModeInterface::SetDrawRenderState(EBlendMode BlendMode, FRenderState& DrawRenderState) const
{
	if (IsTranslucentBlendMode(BlendMode))
	{
		// Otherwise, force translucent blend mode (shaders will use an hardcoded alpha).
		DrawRenderState.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
		DrawRenderState.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
	}
	else
	{
		DrawRenderState.BlendState = TStaticBlendState<>::GetRHI();
		// If not selected, use depth equal to make alpha test stand out (goes with EarlyZPassMode = DDM_AllOpaque) 
		DrawRenderState.DepthStencilState = TStaticDepthStencilState<false, CF_Equal>::GetRHI();
	}
}

void FDebugViewModeInterface::SetInterface(EDebugViewShaderMode InDebugViewMode, FDebugViewModeInterface* Interface)
{
	if ((uint32)InDebugViewMode < DVSM_MAX)
	{
		ensure(!Singletons[InDebugViewMode]);
		Singletons[InDebugViewMode] = Interface;
	}
}

bool FDebugViewModeInterface::AllowFallbackToDefaultMaterial(const FMaterial* InMaterial)
{
	check(InMaterial);

	// Check for anything that could change the shape from the default material.
	return !InMaterial->HasVertexPositionOffsetConnected() && 
		!InMaterial->HasPixelDepthOffsetConnected() &&
		InMaterial->GetTessellationMode() == MTM_NoTessellation; 
}


#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

