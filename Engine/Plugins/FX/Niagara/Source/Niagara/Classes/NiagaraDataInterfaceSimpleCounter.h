// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraCollision.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "NiagaraDataInterface.h"
#include "HAL/PlatformAtomics.h"
#include "NiagaraDataInterfaceSimpleCounter.generated.h"

class INiagaraCompiler;
class FNiagaraSystemInstance;


struct CounterInstanceData
{
	int32 Counter;
};


/** Data Interface allowing a counter that starts at zero when created and increases every time it is sampled. Note that for now this is a signed integer, meaning that it can go negative when it loops past INT_MAX. */
UCLASS(EditInlineNew, Category = "Counting", meta = (DisplayName = "Simple Counter"))
class NIAGARA_API UNiagaraDataInterfaceSimpleCounter : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()
public:

	//UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//UObject Interface End

	/** Initializes the per instance data for this interface. Returns false if there was some error and the simulation should be disabled. */
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* InSystemInstance) override;
	/** Destroys the per instence data for this interface. */
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* InSystemInstance) {}

	/** Ticks the per instance data for this interface, if it has any. */
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds);
	virtual bool PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds);

	virtual int32 PerInstanceDataSize()const { return sizeof(CounterInstanceData); }


	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;

	// VM functions
	void GetNextValue(FVectorVMContext& Context);

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }

	virtual bool GetFunctionHLSL(const FName&  DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

private:
};
