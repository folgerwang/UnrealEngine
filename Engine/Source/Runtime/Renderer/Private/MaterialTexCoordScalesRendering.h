// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MaterialTexCoordScalesRendering.h: Declarations used for the viewmode.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "Engine/TextureStreamingTypes.h"
#include "MeshMaterialShader.h"
#include "DebugViewModeRendering.h"
#include "DebugViewModeInterface.h"

class FPrimitiveSceneProxy;
struct FMeshBatchElement;
struct FMeshDrawingRenderState;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

/**
* Pixel shader that renders texcoord scales.
* The shader is only compiled with the local vertex factory to prevent multiple compilation.
* Nothing from the factory is actually used, but the shader must still derive from FMeshMaterialShader.
*/
class FMaterialTexCoordScalePS : public FDebugViewModePS
{
	DECLARE_SHADER_TYPE(FMaterialTexCoordScalePS,MeshMaterial);

public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		// See FDebugViewModeMaterialProxy::GetFriendlyName()
		return AllowDebugViewShaderMode(DVSM_OutputMaterialTextureScales, Platform, GetMaxSupportedFeatureLevel(Platform)) && Material->GetFriendlyName().Contains(TEXT("MaterialTexCoordScale"));
	}

	FMaterialTexCoordScalePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FDebugViewModePS(Initializer)
	{
		AnalysisParamsParameter.Bind(Initializer.ParameterMap,TEXT("AnalysisParams"));
		OneOverCPUTexCoordScalesParameter.Bind(Initializer.ParameterMap,TEXT("OneOverCPUTexCoordScales"));
		TexCoordIndicesParameter.Bind(Initializer.ParameterMap, TEXT("TexCoordIndices"));
		PrimitiveAlphaParameter.Bind(Initializer.ParameterMap, TEXT("PrimitiveAlpha"));
	}

	FMaterialTexCoordScalePS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << AnalysisParamsParameter;
		Ar << OneOverCPUTexCoordScalesParameter;
		Ar << TexCoordIndicesParameter;
		Ar << PrimitiveAlphaParameter;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("UNDEFINED_ACCURACY"), UndefinedStreamingAccuracyIntensity);
		OutEnvironment.SetDefine(TEXT("MAX_NUM_TEX_COORD"), (uint32)TEXSTREAM_MAX_NUM_UVCHANNELS);
		OutEnvironment.SetDefine(TEXT("INITIAL_GPU_SCALE"), (uint32)TEXSTREAM_INITIAL_GPU_SCALE);
		OutEnvironment.SetDefine(TEXT("TILE_RESOLUTION"), (uint32)TEXSTREAM_TILE_RESOLUTION);
		OutEnvironment.SetDefine(TEXT("MAX_NUM_TEXTURE_REGISTER"), (uint32)TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL);
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
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

	FShaderParameter AnalysisParamsParameter;
	FShaderParameter OneOverCPUTexCoordScalesParameter;
	FShaderParameter TexCoordIndicesParameter;
	FShaderParameter PrimitiveAlphaParameter;
};

class FMaterialTexCoordScaleAccuracyInterface : public FDebugViewModeInterface
{
public:

	FMaterialTexCoordScaleAccuracyInterface() : FDebugViewModeInterface(TEXT("MaterialTexCoordScale"), false, true, false) {}
	virtual FDebugViewModePS* GetPixelShader(const FMaterial* InMaterial, FVertexFactoryType* VertexFactoryType) const override 
	{ 
		return InMaterial->GetShader<FMaterialTexCoordScalePS>(VertexFactoryType);
	}
};

class FOutputMaterialTexCoordScaleInterface : public FDebugViewModeInterface
{
public:

	FOutputMaterialTexCoordScaleInterface() : FDebugViewModeInterface(TEXT("MaterialTexCoordScale"), true, true, false) {}
	virtual FDebugViewModePS* GetPixelShader(const FMaterial* InMaterial, FVertexFactoryType* VertexFactoryType) const override 
	{ 
		return InMaterial->GetShader<FMaterialTexCoordScalePS>(VertexFactoryType); 
	}
	virtual void SetDrawRenderState(EBlendMode BlendMode, FRenderState& DrawRenderState) const override;
};

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
