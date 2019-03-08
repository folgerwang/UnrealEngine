// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ShaderComplexityRendering.h: Declarations used for the shader complexity viewmode.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "DebugViewModeRendering.h"
#include "DebugViewModeInterface.h"

class FPrimitiveSceneProxy;
struct FMeshBatchElement;
struct FMeshDrawingRenderState;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

template <bool bQuadComplexity>
class TComplexityAccumulatePS : public FDebugViewModePS
{
	DECLARE_SHADER_TYPE(TComplexityAccumulatePS,MeshMaterial);
public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		// See FDebugViewModeMaterialProxy::GetFriendlyName()
		return AllowDebugViewShaderMode(bQuadComplexity ? DVSM_QuadComplexity : DVSM_ShaderComplexity, Platform, GetMaxSupportedFeatureLevel(Platform)) && Material->GetFriendlyName().Contains(TEXT("ComplexityAccumulate"));
	}

	TComplexityAccumulatePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FDebugViewModePS(Initializer)
	{
		NormalizedComplexity.Bind(Initializer.ParameterMap,TEXT("NormalizedComplexity"));
		ShowQuadOverdraw.Bind(Initializer.ParameterMap,TEXT("bShowQuadOverdraw"));
		QuadBufferUAV.Bind(Initializer.ParameterMap,TEXT("RWQuadBuffer"));
	}

	TComplexityAccumulatePS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << NormalizedComplexity;
		Ar << ShowQuadOverdraw;
		Ar << QuadBufferUAV;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("OUTPUT_QUAD_OVERDRAW"), AllowDebugViewShaderMode(DVSM_QuadComplexity, Platform, GetMaxSupportedFeatureLevel(Platform)));
	}

	virtual void GetDebugViewModeShaderBindings(
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT Material,
		EDebugViewShaderMode DebugViewMode,
		const FVector& ViewOrigin,
		int32 VisualizeLODIndex,
		int32 VisualizeElementIndex,
		int32 NumVSInstructions,
		int32 NumPSInstructions,
		int32 ViewModeParam,
		FName ViewModeParamName,
		FMeshDrawSingleShaderBindings& ShaderBindings
	) const override;

private:

	FShaderParameter NormalizedComplexity;
	FShaderParameter ShowQuadOverdraw;
	FShaderResourceParameter QuadBufferUAV;
};

class FComplexityAccumulateInterface : public FDebugViewModeInterface
{
	const bool bShowShaderComplexity;
	const bool bShowQuadComplexity;

public:

	FComplexityAccumulateInterface(bool InShowShaderComplexity, bool InShowQuadComplexity) 
		: FDebugViewModeInterface(TEXT("ComplexityAccumulate"), false, false, true)
		, bShowShaderComplexity(InShowShaderComplexity)
		, bShowQuadComplexity(InShowQuadComplexity)
	{}

	virtual FDebugViewModePS* GetPixelShader(const FMaterial* InMaterial, FVertexFactoryType* VertexFactoryType) const override;
	virtual void SetDrawRenderState(EBlendMode BlendMode, FRenderState& DrawRenderState) const;
};

#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
