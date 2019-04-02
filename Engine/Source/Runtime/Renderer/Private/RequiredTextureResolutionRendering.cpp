// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
RequiredTextureResolutionRendering.cpp: Contains definitions for rendering the viewmode.
=============================================================================*/

#include "RequiredTextureResolutionRendering.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

IMPLEMENT_MATERIAL_SHADER_TYPE(,FRequiredTextureResolutionPS,TEXT("/Engine/Private/RequiredTextureResolutionPixelShader.usf"),TEXT("Main"),SF_Pixel);

void FRequiredTextureResolutionPS::GetDebugViewModeShaderBindings(
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
	int32 AnalysisIndex = INDEX_NONE;
	int32 TextureResolution = 64;
	FMaterialRenderContext MaterialContext(&MaterialRenderProxy, Material, nullptr);
	const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& ExpressionsByType = Material.GetUniform2DTextureExpressions();
	if (ViewModeParam != INDEX_NONE && ViewModeParamName == NAME_None) // If displaying texture per texture indices
	{
		AnalysisIndex = ViewModeParam;

		for (FMaterialUniformExpressionTexture* Expression : ExpressionsByType)
		{
			if (Expression && Expression->GetTextureIndex() == ViewModeParam)
			{
				const UTexture* Texture = nullptr;
				ESamplerSourceMode SourceMode;
				Expression->GetTextureValue(MaterialContext, Material, Texture, SourceMode);
				const UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
				if (Texture2D && Texture2D->Resource)
				{
					FTexture2DResource* Texture2DResource = (FTexture2DResource*)Texture2D->Resource;
					if (Texture2DResource->GetTexture2DRHI().IsValid())
					{
						TextureResolution = 1 << (Texture2DResource->GetTexture2DRHI()->GetNumMips() - 1);
					}
					break;
				}
			}
		}
	}
	else if (ViewModeParam != INDEX_NONE) // Otherwise show only texture matching the given name
	{
		AnalysisIndex = 1024; // Make sure not to find anything by default.
		for (FMaterialUniformExpressionTexture* Expression : ExpressionsByType)
		{
			if (Expression)
			{
				const UTexture* Texture = nullptr;
				ESamplerSourceMode SourceMode;
				Expression->GetTextureValue(MaterialContext, Material, Texture, SourceMode);
				if (Texture && Texture->GetFName() == ViewModeParamName)
				{
					const UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
					if (Texture2D && Texture2D->Resource)
					{
						FTexture2DResource* Texture2DResource = (FTexture2DResource*)Texture2D->Resource;
						if (Texture2DResource->GetTexture2DRHI().IsValid())
						{
							AnalysisIndex = Expression->GetTextureIndex();
							TextureResolution = 1 << (Texture2DResource->GetTexture2DRHI()->GetNumMips() - 1);
						}
						break;
					}
				}
			}
		}
	}

	ShaderBindings.Add(AnalysisParamsParameter, FIntPoint(AnalysisIndex, TextureResolution));
	ShaderBindings.Add(PrimitiveAlphaParameter, (!PrimitiveSceneProxy || PrimitiveSceneProxy->IsSelected()) ? 1.f : .2f);
}


#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
