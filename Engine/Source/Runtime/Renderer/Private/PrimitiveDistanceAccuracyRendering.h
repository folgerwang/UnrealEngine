// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PrimitiveDistanceAccuracyRendering.h: Declarations used for the viewmode.
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
* Pixel shader that renders texture streamer wanted mips accuracy.
*/
class FPrimitiveDistanceAccuracyPS : public FDebugViewModePS
{
	DECLARE_SHADER_TYPE(FPrimitiveDistanceAccuracyPS,MeshMaterial);

public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		// See FDebugViewModeMaterialProxy::GetFriendlyName()
		return AllowDebugViewShaderMode(DVSM_PrimitiveDistanceAccuracy, Platform, GetMaxSupportedFeatureLevel(Platform)) && Material->GetFriendlyName().Contains(TEXT("PrimitiveDistanceAccuracy"));
	}

	FPrimitiveDistanceAccuracyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FDebugViewModePS(Initializer)
	{
		CPULogDistanceParameter.Bind(Initializer.ParameterMap, TEXT("CPULogDistance"));
		PrimitiveAlphaParameter.Bind(Initializer.ParameterMap, TEXT("PrimitiveAlpha"));
	}

	FPrimitiveDistanceAccuracyPS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << CPULogDistanceParameter;
		Ar << PrimitiveAlphaParameter;
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

	FShaderParameter CPULogDistanceParameter;
	FShaderParameter PrimitiveAlphaParameter;
};

class FPrimitiveDistanceAccuracyInterface : public FDebugViewModeInterface
{
public:

	FPrimitiveDistanceAccuracyInterface() : FDebugViewModeInterface(TEXT("PrimitiveDistanceAccuracy"), false, false, false) {}
	virtual FDebugViewModePS* GetPixelShader(const FMaterial* InMaterial, FVertexFactoryType* VertexFactoryType) const override { return InMaterial->GetShader<FPrimitiveDistanceAccuracyPS>(VertexFactoryType); }
};

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
