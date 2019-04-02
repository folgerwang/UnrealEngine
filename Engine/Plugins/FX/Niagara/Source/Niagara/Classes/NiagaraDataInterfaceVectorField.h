// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraDataInterface.h"
#include "VectorField/VectorField.h"
#include "NiagaraDataInterfaceVectorField.generated.h"

class FNiagaraSystemInstance;

UCLASS(EditInlineNew, Category = "Vector Field", meta = (DisplayName = "Vector Field"))
class NIAGARA_API UNiagaraDataInterfaceVectorField : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	/** Vector field to sample from. */
	UPROPERTY(EditAnywhere, Category = VectorField)
	UVectorField* Field;

	UPROPERTY(EditAnywhere, Category = VectorField)
	bool bTileX;
	UPROPERTY(EditAnywhere, Category = VectorField)
	bool bTileY;
	UPROPERTY(EditAnywhere, Category = VectorField)
	bool bTileZ;

public:
	//~ UObject interface

	virtual void PostInitProperties() override;
	virtual void PostLoad() override; 
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
#endif
	//~ UObject interface END

	//~ UNiagaraDataInterface interface
	// VM functionality
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override;

#if WITH_EDITOR	
	// Editor functionality
	virtual TArray<FNiagaraDataInterfaceError> GetErrors() override;
#endif

	// GPU sim functionality
	virtual void GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override; 
	virtual bool GetFunctionHLSL(const FName&  DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual FNiagaraDataInterfaceParametersCS* ConstructComputeParameters() const override;
	//~ UNiagaraDataInterface interface END

	// VM functions
	void GetFieldDimensions(FVectorVMContext& Context);
	void GetFieldBounds(FVectorVMContext& Context); 
	void GetFieldTilingAxes(FVectorVMContext& Context);
	void SampleVectorField(FVectorVMContext& Context);
	
	//	
	FVector GetTilingAxes() const;
	FVector GetDimensions() const;
	FVector GetMinBounds() const;
	FVector GetMaxBounds() const;

protected:
	//~ UNiagaraDataInterface interface
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	//~ UNiagaraDataInterface interface END
};
