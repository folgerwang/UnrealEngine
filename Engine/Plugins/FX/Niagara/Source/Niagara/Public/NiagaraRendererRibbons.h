// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraRenderer.h: Base class for Niagara render modules
==============================================================================*/
#pragma once

#include "NiagaraRibbonVertexFactory.h"
#include "NiagaraRenderer.h"

class FNiagaraDataSet;

/**
* NiagaraRendererRibbons renders an FNiagaraEmitterInstance as a ribbon connecting all particles
* in order by particle age.
*/
class NIAGARA_API NiagaraRendererRibbons : public NiagaraRenderer
{
public:
	NiagaraRendererRibbons(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *Props);
	~NiagaraRendererRibbons()
	{
		ReleaseRenderThreadResources();
	}

	virtual void ReleaseRenderThreadResources() override;
	virtual void CreateRenderThreadResources() override;


	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const override;

	virtual bool SetMaterialUsage() override;

	virtual void TransformChanged() override;

	/** Update render data buffer from attributes */
	FNiagaraDynamicDataBase *GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target) override;

	void AddDynamicParam(TArray<FNiagaraRibbonVertexDynamicParameter>& ParamData, const FVector4& DynamicParam)
	{
		FNiagaraRibbonVertexDynamicParameter Param;
		Param.DynamicValue[0] = DynamicParam.X;
		Param.DynamicValue[1] = DynamicParam.Y;
		Param.DynamicValue[2] = DynamicParam.Z;
		Param.DynamicValue[3] = DynamicParam.W;
		ParamData.Add(Param);
	}



	virtual void SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData) override;
	int GetDynamicDataSize() override;
	bool HasDynamicData() override;

#if WITH_EDITORONLY_DATA
	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() override;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
#endif

	UClass *GetPropertiesClass() override { return UNiagaraRibbonRendererProperties::StaticClass(); }
	void SetRendererProperties(UNiagaraRendererProperties *Props) override { Properties = Cast<UNiagaraRibbonRendererProperties>(Props); }
	virtual UNiagaraRendererProperties* GetRendererProperties() const override 
	{
		return Properties;
	}

private:
	class FNiagaraRibbonVertexFactory *VertexFactory;
	UNiagaraRibbonRendererProperties *Properties;
	mutable TUniformBuffer<FPrimitiveUniformShaderParameters> WorldSpacePrimitiveUniformBuffer;
	int32 PositionDataOffset;
	int32 VelocityDataOffset;
	int32 WidthDataOffset;
	int32 TwistDataOffset;
	int32 FacingDataOffset;
	int32 ColorDataOffset;
	int32 NormalizedAgeDataOffset;
	int32 MaterialRandomDataOffset;
	int32 LastSyncedId;
	int32 MaterialParamOffset;
	int32 MaterialParamOffset1;
	int32 MaterialParamOffset2;
	int32 MaterialParamOffset3;
};


