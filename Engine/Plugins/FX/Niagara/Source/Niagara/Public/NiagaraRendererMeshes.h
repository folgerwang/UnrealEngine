// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraRenderer.h: Base class for Niagara render modules
==============================================================================*/
#pragma once

#include "NiagaraMeshVertexFactory.h"
#include "NiagaraRenderer.h"

class FNiagaraDataSet;



struct FNiagaraDynamicDataMesh : public FNiagaraDynamicDataBase
{
	//Direct ptr to the dataset. ONLY FOR USE BE GPU EMITTERS.
	//TODO: Even this needs to go soon.
	const FNiagaraDataSet *DataSet;
};



/**
* NiagaraRendererSprites renders an FNiagaraEmitterInstance as sprite particles
*/
class NIAGARA_API NiagaraRendererMeshes : public NiagaraRenderer
{
public:

	explicit NiagaraRendererMeshes(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *Props);
	~NiagaraRendererMeshes()
	{
		ReleaseRenderThreadResources();
	}


	virtual void ReleaseRenderThreadResources() override;

	// FPrimitiveSceneProxy interface.
	virtual void CreateRenderThreadResources() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const override;
	virtual bool SetMaterialUsage() override;
	virtual void TransformChanged() override;
	/** Update render data buffer from attributes */
	FNiagaraDynamicDataBase *GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target) override;

	virtual void SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData) override;
	int GetDynamicDataSize() override;
	bool HasDynamicData() override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View, const FNiagaraSceneProxy *SceneProxy)
	{
		FPrimitiveViewRelevance Result;
		bool bHasDynamicData = HasDynamicData();
		Result.bDrawRelevance = bHasDynamicData && SceneProxy->IsShown(View) && View->Family->EngineShowFlags.Particles;
		Result.bShadowRelevance = bHasDynamicData && SceneProxy->IsShadowCast(View);
		Result.bDynamicRelevance = bHasDynamicData;

		if (bHasDynamicData)
		{
			Result.bOpaqueRelevance = MaterialRelevance.bOpaque;
			Result.bNormalTranslucencyRelevance = MaterialRelevance.bNormalTranslucency;
			Result.bSeparateTranslucencyRelevance = MaterialRelevance.bSeparateTranslucency;
			Result.bDistortionRelevance = MaterialRelevance.bDistortion;
		}

		return Result;
	}




	UClass *GetPropertiesClass() override { return UNiagaraMeshRendererProperties::StaticClass(); }
	void SetRendererProperties(UNiagaraRendererProperties *Props) override { Properties = Cast<UNiagaraMeshRendererProperties>(Props); }
	virtual UNiagaraRendererProperties* GetRendererProperties()  const override {
		return Properties;
	}
#if WITH_EDITORONLY_DATA
	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() override;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
#endif
	void SetupVertexFactory(FNiagaraMeshVertexFactory *InVertexFactory, const FStaticMeshLODResources& LODResources) const;

private:
	UNiagaraMeshRendererProperties *Properties;
	mutable TUniformBuffer<FPrimitiveUniformShaderParameters> WorldSpacePrimitiveUniformBuffer;
	class FNiagaraMeshVertexFactory* VertexFactory;

	int32 PositionOffset;
	int32 VelocityOffset;
	int32 ColorOffset;
	int32 ScaleOffset;
	int32 SizeOffset;
	int32 MaterialParamOffset;
	int32 MaterialParamOffset1;
	int32 MaterialParamOffset2;
	int32 MaterialParamOffset3;
	int32 TransformOffset;
	int32 NormalizedAgeOffset;
	int32 MaterialRandomOffset;
	int32 CustomSortingOffset;
	int32 LastSyncedId;
};
