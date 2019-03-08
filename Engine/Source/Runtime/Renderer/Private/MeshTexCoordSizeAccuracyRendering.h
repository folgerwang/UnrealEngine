// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MeshTexCoordSizeAccuracyRendering.h: Declarations used for the viewmode.
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

/**
* Pixel shader that renders the accuracy of the texel factor.
*/
class FMeshTexCoordSizeAccuracyPS : public FDebugViewModePS
{
	DECLARE_SHADER_TYPE(FMeshTexCoordSizeAccuracyPS,MeshMaterial);

public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		// See FDebugViewModeMaterialProxy::GetFriendlyName()
		return AllowDebugViewShaderMode(DVSM_MeshUVDensityAccuracy, Platform, GetMaxSupportedFeatureLevel(Platform)) && Material->GetFriendlyName().Contains(TEXT("MeshTexCoordSizeAccuracy"));
	}

	FMeshTexCoordSizeAccuracyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FDebugViewModePS(Initializer)
	{
		CPUTexelFactorParameter.Bind(Initializer.ParameterMap,TEXT("CPUTexelFactor"));
		PrimitiveAlphaParameter.Bind(Initializer.ParameterMap, TEXT("PrimitiveAlpha"));
		TexCoordAnalysisIndexParameter.Bind(Initializer.ParameterMap, TEXT("TexCoordAnalysisIndex"));
	}

	FMeshTexCoordSizeAccuracyPS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << CPUTexelFactorParameter;
		Ar << PrimitiveAlphaParameter;
		Ar << TexCoordAnalysisIndexParameter;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("UNDEFINED_ACCURACY"), UndefinedStreamingAccuracyIntensity);
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

	FShaderParameter CPUTexelFactorParameter;
	FShaderParameter PrimitiveAlphaParameter;
	FShaderParameter TexCoordAnalysisIndexParameter;
};

class FMeshTexCoordSizeAccuracyInterface : public FDebugViewModeInterface
{
public:

	FMeshTexCoordSizeAccuracyInterface() : FDebugViewModeInterface(TEXT("MeshTexCoordSizeAccuracy"), false, false, false) {}
	virtual FDebugViewModePS* GetPixelShader(const FMaterial* InMaterial, FVertexFactoryType* VertexFactoryType) const override { return InMaterial->GetShader<FMeshTexCoordSizeAccuracyPS>(VertexFactoryType); }
};

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
