// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DebugViewModeMaterialProxy.cpp : Contains definitions the debug view mode material shaders.
=============================================================================*/

#include "DebugViewModeMaterialProxy.h"
#include "DebugViewModeInterface.h"
#include "SceneInterface.h"
#include "EngineModule.h"
#include "RendererInterface.h"

#if WITH_EDITORONLY_DATA

FDebugViewModeMaterialProxy::FDebugViewModeMaterialProxy(
	UMaterialInterface* InMaterialInterface, 
	EMaterialQualityLevel::Type QualityLevel, 
	ERHIFeatureLevel::Type FeatureLevel, 
	bool InSynchronousCompilation, 
	EDebugViewShaderMode InDebugViewMode
)
	: FMaterial()
	, MaterialInterface(InMaterialInterface)
	, Material(nullptr)
	, Usage(EMaterialShaderMapUsage::DebugViewMode)
	, DebugViewMode(InDebugViewMode)
	, PixelShaderName(nullptr)
	, CachedMaterialUsage(0)
	, bValid(true)
	, bIsDefaultMaterial(InMaterialInterface->GetMaterial()->IsDefaultMaterial())
	, bSynchronousCompilation(InSynchronousCompilation)
{
	SetQualityLevelProperties(QualityLevel, false, FeatureLevel);
	Material = InMaterialInterface->GetMaterial();
	MaterialInterface->AppendReferencedTextures(ReferencedTextures);

	FMaterialResource* Resource = InMaterialInterface->GetMaterialResource(FeatureLevel);
	
	const FDebugViewModeInterface* DebugViewModeInterface = FDebugViewModeInterface::GetInterface(InDebugViewMode);
	if (DebugViewModeInterface)
	{
		PixelShaderName = DebugViewModeInterface->PixelShaderName;

		if (!DebugViewModeInterface->bNeedsOnlyLocalVertexFactor)
		{
			// Cache material usage.
			bIsUsedWithSkeletalMesh = Resource->IsUsedWithSkeletalMesh();
			bIsUsedWithLandscape = Resource->IsUsedWithLandscape();
			bIsUsedWithParticleSystem = Resource->IsUsedWithParticleSystem();
			bIsUsedWithParticleSprites = Resource->IsUsedWithParticleSprites();
			bIsUsedWithBeamTrails = Resource->IsUsedWithBeamTrails();
			bIsUsedWithMeshParticles = Resource->IsUsedWithMeshParticles();
			bIsUsedWithNiagaraSprites = Resource->IsUsedWithNiagaraSprites();
			bIsUsedWithNiagaraRibbons = Resource->IsUsedWithNiagaraRibbons();
			bIsUsedWithNiagaraMeshParticles = Resource->IsUsedWithNiagaraMeshParticles();
			bIsUsedWithMorphTargets = Resource->IsUsedWithMorphTargets();
			bIsUsedWithSplineMeshes = Resource->IsUsedWithSplineMeshes();
			bIsUsedWithInstancedStaticMeshes = Resource->IsUsedWithInstancedStaticMeshes();
			bIsUsedWithAPEXCloth = Resource->IsUsedWithAPEXCloth();
		}
	}

	FMaterialShaderMapId ResourceId;
	Resource->GetShaderMapId(GMaxRHIShaderPlatform, ResourceId);

	{
		TArray<FShaderType*> ShaderTypes;
		TArray<FVertexFactoryType*> VFTypes;
		TArray<const FShaderPipelineType*> ShaderPipelineTypes;
		GetDependentShaderAndVFTypes(GMaxRHIShaderPlatform, ShaderTypes, ShaderPipelineTypes, VFTypes);

		// Overwrite the shader map Id's dependencies with ones that came from the FMaterial actually being compiled (this)
		// This is necessary as we change FMaterial attributes like GetShadingModel(), which factor into the ShouldCache functions that determine dependent shader types
		ResourceId.SetShaderDependencies(ShaderTypes, ShaderPipelineTypes, VFTypes, GMaxRHIShaderPlatform);
	}

	ResourceId.Usage = Usage;

	CacheShaders(ResourceId, GMaxRHIShaderPlatform, true);
}

bool FDebugViewModeMaterialProxy::RequiresSynchronousCompilation() const
{ 
	return bSynchronousCompilation;
}

const FMaterial& FDebugViewModeMaterialProxy::GetMaterialWithFallback(ERHIFeatureLevel::Type FeatureLevel, const FMaterialRenderProxy*& OutFallbackMaterialRenderProxy) const
{
	if (GetRenderingThreadShaderMap())
	{
		return *this;
	}
	else
	{
		OutFallbackMaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
		return OutFallbackMaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, OutFallbackMaterialRenderProxy);
	}
}

bool FDebugViewModeMaterialProxy::GetVectorValue(const FMaterialParameterInfo& ParameterInfo, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	return MaterialInterface->GetRenderProxy()->GetVectorValue(ParameterInfo, OutValue, Context);
}

bool FDebugViewModeMaterialProxy::GetScalarValue(const FMaterialParameterInfo& ParameterInfo, float* OutValue, const FMaterialRenderContext& Context) const
{
	return MaterialInterface->GetRenderProxy()->GetScalarValue(ParameterInfo, OutValue, Context);
}

bool FDebugViewModeMaterialProxy::GetTextureValue(const FMaterialParameterInfo& ParameterInfo,const UTexture** OutValue, const FMaterialRenderContext& Context) const
{
	return MaterialInterface->GetRenderProxy()->GetTextureValue(ParameterInfo,OutValue,Context);
}

EMaterialDomain FDebugViewModeMaterialProxy::GetMaterialDomain() const
{
	return Material ? (EMaterialDomain)(Material->MaterialDomain) : MD_Surface;
}

bool FDebugViewModeMaterialProxy::IsTwoSided() const
{ 
	return MaterialInterface && MaterialInterface->IsTwoSided();
}

bool FDebugViewModeMaterialProxy::IsDitheredLODTransition() const
{ 
	return MaterialInterface && MaterialInterface->IsDitheredLODTransition();
}

bool FDebugViewModeMaterialProxy::IsLightFunction() const
{
	return Material && Material->MaterialDomain == MD_LightFunction;
}

bool FDebugViewModeMaterialProxy::IsDeferredDecal() const
{
	return	Material && Material->MaterialDomain == MD_DeferredDecal;
}

bool FDebugViewModeMaterialProxy::IsSpecialEngineMaterial() const
{
	return Material && Material->bUsedAsSpecialEngineMaterial;
}

bool FDebugViewModeMaterialProxy::IsWireframe() const
{
	return Material && Material->Wireframe;
}

bool FDebugViewModeMaterialProxy::IsMasked() const
{ 
	return Material && Material->IsMasked(); 
}

enum EBlendMode FDebugViewModeMaterialProxy::GetBlendMode() const
{ 
	return MaterialInterface ? MaterialInterface->GetBlendMode() : BLEND_Opaque;
}

enum EMaterialShadingModel FDebugViewModeMaterialProxy::GetShadingModel() const
{ 
	return Material ? Material->GetShadingModel() : MSM_Unlit;
}

float FDebugViewModeMaterialProxy::GetOpacityMaskClipValue() const
{ 
	return Material ? Material->GetOpacityMaskClipValue() : .5f;
}

bool FDebugViewModeMaterialProxy::GetCastDynamicShadowAsMasked() const
{
	return Material ? Material->GetCastShadowAsMasked() : false;
}

void FDebugViewModeMaterialProxy::GatherCustomOutputExpressions(TArray<class UMaterialExpressionCustomOutput*>& OutCustomOutputs) const
{
	if (Material)
	{
		Material->GetAllCustomOutputExpressions(OutCustomOutputs);
	}
}

void FDebugViewModeMaterialProxy::GatherExpressionsForCustomInterpolators(TArray<class UMaterialExpression*>& OutExpressions) const
{
	if (Material)
	{
		Material->GetAllExpressionsForCustomInterpolators(OutExpressions);
	}
}

#endif // WITH_EDITORONLY_DATA
