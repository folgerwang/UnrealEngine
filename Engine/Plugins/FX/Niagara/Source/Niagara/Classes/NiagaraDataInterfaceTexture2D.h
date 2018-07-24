// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraCommon.h"
#include "VectorVM.h"
#include "NiagaraDataInterfaceTexture2D.generated.h"

/** Data Interface allowing sampling of a texture */
UCLASS(EditInlineNew, Category = "Texture", meta = (DisplayName = "Texture Sample 2D"))
class NIAGARA_API UNiagaraDataInterfaceTexture2D : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()
		bool bGPUBufferDirty;
public:

	UPROPERTY(EditAnywhere, Category = "Texture")
	UTexture2D* Texture;

	//UObject Interface
	virtual void PostInitProperties()override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void Serialize(FArchive& Ar) override;
	//UObject Interface End

	//UNiagaraDataInterface Interface
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return Target==ENiagaraSimTarget::GPUComputeSim; }
	//UNiagaraDataInterface Interface End

	template<typename XType, typename YType>
	void SampleTexture(FVectorVMContext& Context);

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	// GPU sim functionality
	virtual bool GetFunctionHLSL(const FName&  DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual FNiagaraDataInterfaceParametersCS* ConstructComputeParameters()const override;

	//FRWBuffer& GetGPUBuffer();
	static const FString TextureName;
	static const FString SamplerName;
protected:
#if WITH_EDITOR
	bool CopyTextureToCPUBackup(UTexture* SourceTexture, TArray<uint8>& TargetBuffer);
	void CopyTextureData(const uint8* Source, uint8* Dest, uint32 SizeX, uint32 SizeY, uint32 BytesPerPixel, uint32 SourceStride, uint32 DestStride);

#endif
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	TArray<uint8> CPUTextureData;

	static const FName SampleTextureName;
};
