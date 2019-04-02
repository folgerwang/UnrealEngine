// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PrimitiveDistanceAccuracyRendering.cpp: Contains definitions for rendering the viewmode.
=============================================================================*/

#include "PrimitiveDistanceAccuracyRendering.h"
#include "PrimitiveSceneProxy.h"
#include "EngineGlobals.h"
#include "MeshBatch.h"
#include "Engine/Engine.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

IMPLEMENT_MATERIAL_SHADER_TYPE(,FPrimitiveDistanceAccuracyPS,TEXT("/Engine/Private/PrimitiveDistanceAccuracyPixelShader.usf"),TEXT("Main"),SF_Pixel);

void FPrimitiveDistanceAccuracyPS::GetDebugViewModeShaderBindings(
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
) const
{
	float CPULogDistance = -1.f;
#if WITH_EDITORONLY_DATA
	float Distance = 0;
	if (PrimitiveSceneProxy && PrimitiveSceneProxy->GetPrimitiveDistance(VisualizeLODIndex, VisualizeElementIndex, ViewOrigin, Distance))
	{
		CPULogDistance =  FMath::Max<float>(0.f, FMath::Log2(FMath::Max<float>(1.f, Distance)));
	}
#endif
	// Because the streamer use FMath::FloorToFloat, here we need to use -1 to have a useful result.
	ShaderBindings.Add(CPULogDistanceParameter, CPULogDistance);
	ShaderBindings.Add(PrimitiveAlphaParameter,  (!PrimitiveSceneProxy || PrimitiveSceneProxy->IsSelected()) ? 1.f : .2f);
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
