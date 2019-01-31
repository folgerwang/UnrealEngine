// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DebugViewModeInterface.h: Contains definitions for rendering debug viewmodes.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "DebugViewModeHelpers.h"
#include "Engine/EngineTypes.h"
#include "RHI.h"
#include "RHIResources.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

class FDebugViewModePS;
class FMaterial;
class FVertexFactoryType;
struct FMeshPassProcessorRenderState;

class ENGINE_API FDebugViewModeInterface
{
public:

	struct FRenderState
	{
		FRenderState()
			: BlendState(nullptr)
			, DepthStencilState(nullptr)
		{}
		FBlendStateRHIParamRef			BlendState;
		FDepthStencilStateRHIParamRef	DepthStencilState;
	};

	FDebugViewModeInterface(
		const TCHAR* InPixelShaderName,		// Shader class name implementing the PS
		bool InNeedsOnlyLocalVertexFactor,  // Whether this viewmode will only be used with the local vertex factory (for draw tiled mesh).
		bool InNeedsMaterialProperties,		// Whether the PS use any of the material properties (otherwise default material will be used, reducing shader compilation).
		bool InNeedsInstructionCount		// Whether FDebugViewModePS::GetDebugViewModeShaderBindings() will use the num of instructions.
	)
		: PixelShaderName(InPixelShaderName)
		, bNeedsOnlyLocalVertexFactor(InNeedsOnlyLocalVertexFactor)
		, bNeedsMaterialProperties(InNeedsMaterialProperties)
		, bNeedsInstructionCount(InNeedsInstructionCount)
	{}

	virtual ~FDebugViewModeInterface() {}

	virtual FDebugViewModePS* GetPixelShader(const FMaterial* InMaterial, FVertexFactoryType* VertexFactoryType) const = 0;
	virtual void SetDrawRenderState(EBlendMode BlendMode, FRenderState& DrawRenderState) const;

	/** The shader class name, used to filter out shaders that need to be compiled. */
	const TCHAR* PixelShaderName;

	/** Whether only tiled mesh draw will be required. */
	const bool bNeedsOnlyLocalVertexFactor;

	/** Whether the viewmode any of material properties (otherwise it can fallback to using the default material) */
	const bool bNeedsMaterialProperties;

	/** Whether FDebugViewModePS::GetDebugViewModeShaderBindings() will use the num of instructions. */
	const bool bNeedsInstructionCount;

	/** Return the interface object for the given viewmode. */
	static const FDebugViewModeInterface* GetInterface(EDebugViewShaderMode InDebugViewMode) 
	{
		return (uint32)InDebugViewMode < DVSM_MAX ? Singletons[InDebugViewMode] : nullptr;
	}

	/** Return the interface object for the given viewmode. */
	static void SetInterface(EDebugViewShaderMode InDebugViewMode, FDebugViewModeInterface* Interface);
	
	/** Whether this material can be substituted by the default material. */
	static bool AllowFallbackToDefaultMaterial(const FMaterial* InMaterial);

private:
	
	static FDebugViewModeInterface* Singletons[DVSM_MAX];
};

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
