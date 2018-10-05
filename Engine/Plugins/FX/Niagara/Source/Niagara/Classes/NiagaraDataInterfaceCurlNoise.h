// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraCommon.h"
#include "VectorVM.h"
#include "NiagaraDataInterfaceCurlNoise.generated.h"

/** Data Interface allowing sampling of curl noise LUT. */
UCLASS(EditInlineNew, Category = "Curl Noise LUT", meta = (DisplayName = "Curl Noise"))
class NIAGARA_API UNiagaraDataInterfaceCurlNoise : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()
	bool bGPUBufferDirty;
public:

	UPROPERTY(EditAnywhere, Category = "Curl Noise")
	uint32 Seed;
	
	//UObject Interface
	virtual void PostInitProperties()override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//UObject Interface End

	//UNiagaraDataInterface Interface
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return true; }
	//UNiagaraDataInterface Interface End

	void SampleNoiseField(FVectorVMContext& Context);

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	// GPU sim functionality
	virtual bool GetFunctionHLSL(const FName&  DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual FNiagaraDataInterfaceParametersCS* ConstructComputeParameters()const override;

	void ReleaseResource();
	FRWBuffer& GetGPUBuffer();
	static const FString CurlNoiseBufferName;
protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	void InitNoiseLUT();

	VectorRegister NoiseTable[17][17][17];

	template<typename T>
	void ReplicateBorder(T* DestBuffer);

	TUniquePtr<FRWBuffer> GPUBuffer;
	/** A fence which is used to keep track of the rendering thread releasing RHI resources. */
	FRenderCommandFence ReleaseResourcesFence;

	static const FName SampleNoiseFieldName;
};
