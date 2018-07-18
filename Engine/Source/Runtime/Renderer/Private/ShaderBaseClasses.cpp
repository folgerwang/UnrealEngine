// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderBaseClasses.cpp: Shader base classes
=============================================================================*/

#include "ShaderBaseClasses.h"
#include "PostProcess/SceneRenderTargets.h"
#include "RendererModule.h"
#include "ScenePrivate.h"
#include "ParameterCollection.h"

/** If true, cached uniform expressions are allowed. */
int32 FMaterialShader::bAllowCachedUniformExpressions = true;

/** Console variable ref to toggle cached uniform expressions. */
FAutoConsoleVariableRef FMaterialShader::CVarAllowCachedUniformExpressions(
	TEXT("r.AllowCachedUniformExpressions"),
	bAllowCachedUniformExpressions,
	TEXT("Allow uniform expressions to be cached."),
	ECVF_RenderThreadSafe);

FName FMaterialShader::UniformBufferLayoutName(TEXT("Material"));

FMaterialShader::FMaterialShader(const FMaterialShaderType::CompiledShaderInitializerType& Initializer)
:	FShader(Initializer)
#if ALLOW_SHADERMAP_DEBUG_DATA
,	DebugUniformExpressionSet(Initializer.UniformExpressionSet)
,	DebugUniformExpressionUBLayout(FRHIUniformBufferLayout::Zero)
,	DebugDescription(Initializer.DebugDescription)
#endif
{
#if ALLOW_SHADERMAP_DEBUG_DATA
	check(!DebugDescription.IsEmpty());
	DebugUniformExpressionUBLayout.CopyFrom(Initializer.UniformExpressionSet.GetUniformBufferStruct().GetLayout());
#endif

	// Bind the material uniform buffer parameter.
	MaterialUniformBuffer.Bind(Initializer.ParameterMap,TEXT("Material"));

	for (int32 CollectionIndex = 0; CollectionIndex < Initializer.UniformExpressionSet.ParameterCollections.Num(); CollectionIndex++)
	{
		FShaderUniformBufferParameter CollectionParameter;
		CollectionParameter.Bind(Initializer.ParameterMap,*FString::Printf(TEXT("MaterialCollection%u"), CollectionIndex));
		ParameterCollectionUniformBuffers.Add(CollectionParameter);
	}

	SceneTextureParameters.Bind(Initializer);

	InstanceCount.Bind(Initializer.ParameterMap, TEXT("InstanceCount"));
	InstanceOffset.Bind(Initializer.ParameterMap, TEXT("InstanceOffset"));
	VertexOffset.Bind(Initializer.ParameterMap, TEXT("VertexOffset"));
}

FUniformBufferRHIParamRef FMaterialShader::GetParameterCollectionBuffer(const FGuid& Id, const FSceneInterface* SceneInterface) const
{
	const FScene* Scene = (const FScene*)SceneInterface;
	FUniformBufferRHIParamRef UniformBuffer = Scene ? Scene->GetParameterCollectionBuffer(Id) : FUniformBufferRHIParamRef();

	if (!UniformBuffer)
	{
		FMaterialParameterCollectionInstanceResource** CollectionResource = GDefaultMaterialParameterCollectionInstances.Find(Id);
		if (CollectionResource && *CollectionResource)
		{
			UniformBuffer = (*CollectionResource)->GetUniformBuffer();
		}
	}

	return UniformBuffer;
}

#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING || !WITH_EDITOR)
void FMaterialShader::VerifyExpressionAndShaderMaps(const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial& Material, const FUniformExpressionCache* UniformExpressionCache)
{
	// Validate that the shader is being used for a material that matches the uniform expression set the shader was compiled for.
	const FUniformExpressionSet& MaterialUniformExpressionSet = Material.GetRenderingThreadShaderMap()->GetUniformExpressionSet();
	bool bUniformExpressionSetMismatch = !DebugUniformExpressionSet.Matches(MaterialUniformExpressionSet)
		|| UniformExpressionCache->CachedUniformExpressionShaderMap != Material.GetRenderingThreadShaderMap();
	if (!bUniformExpressionSetMismatch)
	{
		auto DumpUB = [](const FRHIUniformBufferLayout& Layout)
		{
			FString DebugName = Layout.GetDebugName().GetPlainNameString();
			UE_LOG(LogShaders, Warning, TEXT("Layout %s, Hash %08x"), *DebugName, Layout.GetHash());
			FString ResourcesString;
			for (int32 Index = 0; Index < Layout.Resources.Num(); ++Index)
			{
				ResourcesString += FString::Printf(TEXT("%d "), Layout.Resources[Index]);
			}
			UE_LOG(LogShaders, Warning, TEXT("Layout CB Size %d %d Resources: %s"), Layout.ConstantBufferSize, Layout.Resources.Num(), *ResourcesString);
		};
		if (UniformExpressionCache->LocalUniformBuffer.IsValid())
		{
			if (UniformExpressionCache->LocalUniformBuffer.BypassUniform)
			{
				if (DebugUniformExpressionUBLayout.GetHash() != UniformExpressionCache->LocalUniformBuffer.BypassUniform->GetLayout().GetHash())
				{
					UE_LOG(LogShaders, Warning, TEXT("Material Expression UB mismatch!"));
					DumpUB(DebugUniformExpressionUBLayout);
					DumpUB(UniformExpressionCache->LocalUniformBuffer.BypassUniform->GetLayout());
					bUniformExpressionSetMismatch = true;
				}
			}
			else
			{
				if (DebugUniformExpressionUBLayout.GetHash() != UniformExpressionCache->LocalUniformBuffer.WorkArea->Layout->GetHash())
				{
					UE_LOG(LogShaders, Warning, TEXT("Material Expression UB mismatch!"));
					DumpUB(DebugUniformExpressionUBLayout);
					DumpUB(*UniformExpressionCache->LocalUniformBuffer.WorkArea->Layout);
					bUniformExpressionSetMismatch = true;
				}
			}
		}
		else
		{
			if (DebugUniformExpressionUBLayout.GetHash() != UniformExpressionCache->UniformBuffer->GetLayout().GetHash())
			{
				UE_LOG(LogShaders, Warning, TEXT("Material Expression UB mismatch!"));
				DumpUB(DebugUniformExpressionUBLayout);
				DumpUB(UniformExpressionCache->UniformBuffer->GetLayout());
				bUniformExpressionSetMismatch = true;
			}
		}
	}
	if (bUniformExpressionSetMismatch)
	{
		FString DebugDesc;
#if ALLOW_SHADERMAP_DEBUG_DATA
		DebugDesc = DebugDescription;
#endif
		UE_LOG(
			LogShaders,
			Fatal,
			TEXT("%s shader uniform expression set mismatch for material %s/%s.\n")
			TEXT("Shader compilation info:                %s\n")
			TEXT("Material render proxy compilation info: %s\n")
			TEXT("Shader uniform expression set:   %u vectors, %u scalars, %u 2D textures, %u cube textures, %u 3D textures, shader map %p\n")
			TEXT("Material uniform expression set: %u vectors, %u scalars, %u 2D textures, %u cube textures, %u 3D textures, shader map %p\n"),
			GetType()->GetName(),
			*MaterialRenderProxy->GetFriendlyName(),
			*Material.GetFriendlyName(),
			*DebugDesc,
			*Material.GetRenderingThreadShaderMap()->GetDebugDescription(),
			DebugUniformExpressionSet.NumVectorExpressions,
			DebugUniformExpressionSet.NumScalarExpressions,
			DebugUniformExpressionSet.Num2DTextureExpressions,
			DebugUniformExpressionSet.NumCubeTextureExpressions,
			DebugUniformExpressionSet.NumVolumeTextureExpressions,
			UniformExpressionCache->CachedUniformExpressionShaderMap,
			MaterialUniformExpressionSet.UniformVectorExpressions.Num(),
			MaterialUniformExpressionSet.UniformScalarExpressions.Num(),
			MaterialUniformExpressionSet.Uniform2DTextureExpressions.Num(),
			MaterialUniformExpressionSet.UniformCubeTextureExpressions.Num(),
			MaterialUniformExpressionSet.UniformVolumeTextureExpressions.Num(),
			Material.GetRenderingThreadShaderMap()
		);
	}
}
#endif

template<typename ShaderRHIParamRef>
void FMaterialShader::SetParametersInner(
	FRHICommandList& RHICmdList,
	const ShaderRHIParamRef ShaderRHI,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FMaterial& Material,
	const FSceneView& View)
{
	// If the material has cached uniform expressions for selection or hover
	// and that is being overridden by show flags in the editor, recache
	// expressions for this draw call.
	const bool bOverrideSelection =
		GIsEditor &&
		!View.Family->EngineShowFlags.Selection &&
		(MaterialRenderProxy->IsSelected() || MaterialRenderProxy->IsHovered());

	ERHIFeatureLevel::Type FeatureLevel = View.GetFeatureLevel();
	checkf(Material.GetRenderingThreadShaderMap(), TEXT("RenderingThreadShaderMap: %i"), Material.GetRenderingThreadShaderMap() ? 1 : 0);
	checkf(Material.GetRenderingThreadShaderMap()->IsValidForRendering(true) && Material.GetFeatureLevel() == FeatureLevel, TEXT("IsValid:%i, MaterialFeatureLevel:%i, FeatureLevel:%i"), Material.GetRenderingThreadShaderMap()->IsValidForRendering() ? 1 : 0, Material.GetFeatureLevel(), FeatureLevel);

	FUniformExpressionCache* UniformExpressionCache = &MaterialRenderProxy->UniformExpressionCache[FeatureLevel];
	bool bUniformExpressionCacheNeedsDelete = false;
	bool bForceExpressionEvaluation = false;

#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING || !WITH_EDITOR)
	if (!(!bAllowCachedUniformExpressions || !UniformExpressionCache->bUpToDate || bOverrideSelection))
	{
		// UE-46061 - Workaround for a rare crash with an outdated cached shader map
		if (UniformExpressionCache->CachedUniformExpressionShaderMap != Material.GetRenderingThreadShaderMap())
		{
			UMaterialInterface* MtlInterface = Material.GetMaterialInterface();
			UMaterialInterface* ProxyInterface = MaterialRenderProxy->GetMaterialInterface();

			ensureMsgf(false,
				TEXT("%s shader uniform expression set mismatched shader map for material %s/%s, forcing expression cache evaluation.\n")
				TEXT("Material:  %s\n")
				TEXT("Proxy:  %s\n"),
				GetType()->GetName(),
				*MaterialRenderProxy->GetFriendlyName(), *Material.GetFriendlyName(),
				MtlInterface ? *MtlInterface->GetFullName() : TEXT("nullptr"),
				ProxyInterface ? *ProxyInterface->GetFullName() : TEXT("nullptr"));
			bForceExpressionEvaluation = true;
		}
	}
#endif

	if (!bAllowCachedUniformExpressions || !UniformExpressionCache->bUpToDate || bOverrideSelection || bForceExpressionEvaluation)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, Material, &View);
		bUniformExpressionCacheNeedsDelete = true;
		UniformExpressionCache = new FUniformExpressionCache();
		MaterialRenderProxy->EvaluateUniformExpressions(*UniformExpressionCache, MaterialRenderContext, &RHICmdList);
		SetLocalUniformBufferParameter(RHICmdList, ShaderRHI, MaterialUniformBuffer, UniformExpressionCache->LocalUniformBuffer);
	}
	else
	{
		SetUniformBufferParameter(RHICmdList, ShaderRHI, MaterialUniformBuffer, UniformExpressionCache->UniformBuffer);
	}

#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING || !WITH_EDITOR)
	VerifyExpressionAndShaderMaps(MaterialRenderProxy, Material, UniformExpressionCache);
#endif

	{
		const TArray<FGuid>& ParameterCollections = UniformExpressionCache->ParameterCollections;
		const int32 ParameterCollectionsNum = ParameterCollections.Num();

		// For shipping and test builds the assert above will be compiled out, but we're trying to verify that this condition is never hit.
		if (ParameterCollectionUniformBuffers.Num() < ParameterCollectionsNum)
		{
			UE_LOG(LogRenderer, Warning,
				TEXT("ParameterCollectionUniformBuffers.Num() [%u] < ParameterCollectionsNum [%u], this would crash below on SetUniformBufferParameter.\n")
				TEXT("RenderProxy=%s Material=%s"),
				ParameterCollectionUniformBuffers.Num(),
				ParameterCollectionsNum,
				*MaterialRenderProxy->GetFriendlyName(),
				*Material.GetFriendlyName()
				);
		}

		check(ParameterCollectionUniformBuffers.Num() >= ParameterCollectionsNum);

		

		int32 NumToSet = FMath::Min(ParameterCollectionUniformBuffers.Num(), ParameterCollections.Num());

		// Find each referenced parameter collection's uniform buffer in the scene and set the parameter
		for (int32 CollectionIndex = 0; CollectionIndex < NumToSet; CollectionIndex++)
		{			
			FUniformBufferRHIParamRef UniformBuffer = GetParameterCollectionBuffer(ParameterCollections[CollectionIndex], View.Family->Scene);

			if (!UniformBuffer)
			{
				// Dump the currently registered parameter collections and the ID we failed to find.
				// In a cooked project these numbers are persistent so we can track back to the original
				// parameter collection that was being referenced and no longer exists
				FString InstancesString;
				TMap<FGuid, FMaterialParameterCollectionInstanceResource*>::TIterator Iter = GDefaultMaterialParameterCollectionInstances.CreateIterator();
				while (Iter)
				{
					FMaterialParameterCollectionInstanceResource* Instance = Iter.Value();
					InstancesString += FString::Printf(TEXT("\n0x%p: %s: %s"),
						Instance, Instance ? *Instance->GetOwnerName().ToString() : TEXT("None"), *Iter.Key().ToString());
					++Iter;
				}

				UE_LOG(LogRenderer, Fatal, TEXT("Failed to find parameter collection buffer with GUID '%s'.\n")
					TEXT("Currently %i listed default instances: %s"),
					*ParameterCollections[CollectionIndex].ToString(),
					GDefaultMaterialParameterCollectionInstances.Num(), *InstancesString);
			}

			SetUniformBufferParameter(RHICmdList, ShaderRHI, ParameterCollectionUniformBuffers[CollectionIndex], UniformBuffer);			
		}
	}

	if (bUniformExpressionCacheNeedsDelete)
	{
		delete UniformExpressionCache;
	}
}

template<typename ShaderRHIParamRef>
void FMaterialShader::SetParameters(
	FRHICommandList& RHICmdList,
	const ShaderRHIParamRef ShaderRHI,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FMaterial& Material,
	const FSceneView& View,
	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
	ESceneTextureSetupMode SceneTextureSetupMode)
{
	SetViewParameters(RHICmdList, ShaderRHI, View, ViewUniformBuffer);
	FMaterialShader::SetParametersInner(RHICmdList, ShaderRHI, MaterialRenderProxy, Material, View);

	SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, SceneTextureSetupMode);
}

// Doxygen struggles to parse these explicit specializations. Just ignore them for now.
#if !UE_BUILD_DOCS

#define IMPLEMENT_MATERIAL_SHADER_SetParametersInner( ShaderRHIParamRef ) \
	template RENDERER_API void FMaterialShader::SetParametersInner< ShaderRHIParamRef >( \
		FRHICommandList& RHICmdList,					\
		const ShaderRHIParamRef ShaderRHI,				\
		const FMaterialRenderProxy* MaterialRenderProxy,\
		const FMaterial& Material,						\
		const FSceneView& View							\
	);

IMPLEMENT_MATERIAL_SHADER_SetParametersInner( FVertexShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParametersInner( FHullShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParametersInner( FDomainShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParametersInner( FGeometryShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParametersInner( FPixelShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParametersInner( FComputeShaderRHIParamRef );

#endif

// Doxygen struggles to parse these explicit specializations. Just ignore them for now.
#if !UE_BUILD_DOCS

#define IMPLEMENT_MATERIAL_SHADER_SetParameters( ShaderRHIParamRef ) \
	template RENDERER_API void FMaterialShader::SetParameters< ShaderRHIParamRef >( \
		FRHICommandList& RHICmdList,					\
		const ShaderRHIParamRef ShaderRHI,				\
		const FMaterialRenderProxy* MaterialRenderProxy,\
		const FMaterial& Material,						\
		const FSceneView& View,							\
		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer, \
		ESceneTextureSetupMode SceneTextureSetupMode		\
	);

IMPLEMENT_MATERIAL_SHADER_SetParameters( FVertexShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParameters( FHullShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParameters( FDomainShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParameters( FGeometryShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParameters( FPixelShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParameters( FComputeShaderRHIParamRef );

#endif

bool FMaterialShader::Serialize(FArchive& Ar)
{
	const bool bShaderHasOutdatedParameters = FShader::Serialize(Ar);
	Ar << SceneTextureParameters;
	Ar << MaterialUniformBuffer;
	Ar << ParameterCollectionUniformBuffers;

#if !ALLOW_SHADERMAP_DEBUG_DATA
	FDebugUniformExpressionSet	DebugUniformExpressionSet;
	static FName DebugUniformExpressionUB(TEXT("DebugUniformExpressionUB"));
	FRHIUniformBufferLayout		DebugUniformExpressionUBLayout(DebugUniformExpressionUB);
	FString						DebugDescription;
#endif

	Ar << DebugUniformExpressionSet;
	if (Ar.IsLoading())
	{
		FName LayoutName;
		Ar << LayoutName;
		DebugUniformExpressionUBLayout = FRHIUniformBufferLayout(LayoutName);
		Ar << DebugUniformExpressionUBLayout.ConstantBufferSize;
		Ar << DebugUniformExpressionUBLayout.ResourceOffsets;
		Ar << DebugUniformExpressionUBLayout.Resources;
#if ALLOW_SHADERMAP_DEBUG_DATA
		DebugUniformExpressionUBLayout.ComputeHash();
#endif
	}
	else
	{
		FName LayoutName = DebugUniformExpressionUBLayout.GetDebugName();
		Ar << LayoutName;
		Ar << DebugUniformExpressionUBLayout.ConstantBufferSize;
		Ar << DebugUniformExpressionUBLayout.ResourceOffsets;
		Ar << DebugUniformExpressionUBLayout.Resources;
	}
	Ar << DebugDescription;

	Ar << InstanceCount;
	Ar << InstanceOffset;
	Ar << VertexOffset;

	return bShaderHasOutdatedParameters;
}

uint32 FMaterialShader::GetAllocatedSize() const
{
	return FShader::GetAllocatedSize()
		+ ParameterCollectionUniformBuffers.GetAllocatedSize()
#if ALLOW_SHADERMAP_DEBUG_DATA
		+ DebugDescription.GetAllocatedSize()
#endif
	;
}


template< typename ShaderRHIParamRef >
void FMeshMaterialShader::SetMesh(
	FRHICommandList& RHICmdList,
	const ShaderRHIParamRef ShaderRHI,
	const FVertexFactory* VertexFactory,
	const FSceneView& View,
	const FPrimitiveSceneProxy* Proxy,
	const FMeshBatchElement& BatchElement,
	const FDrawingPolicyRenderState& DrawRenderState,
	uint32 DataFlags )
{
	// Set the mesh for the vertex factory
	VertexFactoryParameters.SetMesh(RHICmdList, this,VertexFactory,View,BatchElement, DataFlags);
		
	if(IsValidRef(BatchElement.PrimitiveUniformBuffer))
	{
		SetUniformBufferParameter(RHICmdList, ShaderRHI,GetUniformBufferParameter<FPrimitiveUniformShaderParameters>(),BatchElement.PrimitiveUniformBuffer);
	}
	else
	{
		check(BatchElement.PrimitiveUniformBufferResource);
		SetUniformBufferParameter(RHICmdList, ShaderRHI,GetUniformBufferParameter<FPrimitiveUniformShaderParameters>(),*BatchElement.PrimitiveUniformBufferResource);
	}

	TShaderUniformBufferParameter<FDistanceCullFadeUniformShaderParameters> LODParameter = GetUniformBufferParameter<FDistanceCullFadeUniformShaderParameters>();
	if( LODParameter.IsBound() )
	{
		SetUniformBufferParameter(RHICmdList, ShaderRHI,LODParameter,GetPrimitiveFadeUniformBufferParameter(View, Proxy));
	}
	if (NonInstancedDitherLODFactorParameter.IsBound())
	{
		SetShaderValue(RHICmdList, ShaderRHI, NonInstancedDitherLODFactorParameter, DrawRenderState.GetDitheredLODTransitionAlpha());
	}
}

#define IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( ShaderRHIParamRef ) \
	template RENDERER_API void FMeshMaterialShader::SetMesh< ShaderRHIParamRef >( \
		FRHICommandList& RHICmdList,					 \
		const ShaderRHIParamRef ShaderRHI,				 \
		const FVertexFactory* VertexFactory,			 \
		const FSceneView& View,							 \
		const FPrimitiveSceneProxy* Proxy,				 \
		const FMeshBatchElement& BatchElement,			 \
		const FDrawingPolicyRenderState& DrawRenderState,\
		uint32 DataFlags								 \
	);

IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( FVertexShaderRHIParamRef );
IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( FHullShaderRHIParamRef );
IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( FDomainShaderRHIParamRef );
IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( FGeometryShaderRHIParamRef );
IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( FPixelShaderRHIParamRef );
IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( FComputeShaderRHIParamRef );

bool FMeshMaterialShader::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
	Ar << PassUniformBuffer;
	bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
	Ar << NonInstancedDitherLODFactorParameter;
	return bShaderHasOutdatedParameters;
}

uint32 FMeshMaterialShader::GetAllocatedSize() const
{
	return FMaterialShader::GetAllocatedSize()
		+ VertexFactoryParameters.GetAllocatedSize();
}

FUniformBufferRHIParamRef FMeshMaterialShader::GetPrimitiveFadeUniformBufferParameter(const FSceneView& View, const FPrimitiveSceneProxy* Proxy)
{
	FUniformBufferRHIParamRef FadeUniformBuffer = NULL;
	if( Proxy != NULL )
	{
		const FPrimitiveSceneInfo* PrimitiveSceneInfo = Proxy->GetPrimitiveSceneInfo();
		int32 PrimitiveIndex = PrimitiveSceneInfo->GetIndex();

		// This cast should always be safe. Check it :)
		checkSlow(View.bIsViewInfo);
		const FViewInfo& ViewInfo = (const FViewInfo&)View;
		FadeUniformBuffer = ViewInfo.PrimitiveFadeUniformBuffers[PrimitiveIndex];
	}
	if (FadeUniformBuffer == NULL)
	{
		FadeUniformBuffer = GDistanceCullFadedInUniformBuffer.GetUniformBufferRHI();
	}
	return FadeUniformBuffer;
}
