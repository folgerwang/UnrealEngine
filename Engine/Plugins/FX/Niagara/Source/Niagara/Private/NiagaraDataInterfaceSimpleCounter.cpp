// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceSimpleCounter.h"
#include "NiagaraTypes.h"
#include "NiagaraCustomVersion.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceSimpleCounter"


//////////////////////////////////////////////////////////////////////////
//Color Curve


UNiagaraDataInterfaceSimpleCounter::UNiagaraDataInterfaceSimpleCounter(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UNiagaraDataInterfaceSimpleCounter::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* InSystemInstance)
{
	CounterInstanceData *PIData = new (PerInstanceData) CounterInstanceData;
	PIData->Counter = 0;
	return true;
}


void UNiagaraDataInterfaceSimpleCounter::PostInitProperties()
{
	Super::PostInitProperties();

	//Can we register data interfaces as regular types and fold them into the FNiagaraVariable framework for UI and function calls etc?
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
	}
}

void UNiagaraDataInterfaceSimpleCounter::PostLoad()
{
	Super::PostLoad();

	const int32 NiagaraVer = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);
}

#if WITH_EDITOR

void UNiagaraDataInterfaceSimpleCounter::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif


bool UNiagaraDataInterfaceSimpleCounter::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}
	return true;
}

bool UNiagaraDataInterfaceSimpleCounter::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	return true;
}


void UNiagaraDataInterfaceSimpleCounter::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	FNiagaraFunctionSignature Sig2;
	Sig2.Name = TEXT("GetNextValue");
	Sig2.bMemberFunction = true;
	Sig2.bRequiresContext = false;
	Sig2.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Counter")));
	Sig2.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Value")));
	Sig2.SetDescription(LOCTEXT("UNiagaraDataInterfaceSimpleCounter_GetNextValue", "Increment the internal counter. Note that it is possible for this counter to roll over eventually, so make sure that your particles do not live extremely long lifetimes."));
	OutFunctions.Add(Sig2);
}

bool UNiagaraDataInterfaceSimpleCounter::GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	return false;
}

void UNiagaraDataInterfaceSimpleCounter::GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
}


void UNiagaraDataInterfaceSimpleCounter::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	CounterInstanceData *InstData = (CounterInstanceData *)InstanceData;
	if (BindingInfo.Name == TEXT("GetNextValue"))
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceSimpleCounter::GetNextValue);
	}
	else
	{
		UE_LOG(LogNiagara, Error, TEXT("Could not find data interface external function. %s\n"),
			*BindingInfo.Name.ToString());
	}
}

void UNiagaraDataInterfaceSimpleCounter::GetNextValue(FVectorVMContext& Context)
{	
	VectorVM::FUserPtrHandler<CounterInstanceData> InstanceData(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutValue.GetDest() = FPlatformAtomics::InterlockedIncrement(&InstanceData->Counter);
		OutValue.Advance();
	}
}


bool UNiagaraDataInterfaceSimpleCounter::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* InSystemInstance, float DeltaSeconds)
{
	return false;
}

bool UNiagaraDataInterfaceSimpleCounter::PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* InSystemInstance, float DeltaSeconds)
{
	CounterInstanceData *PIData = static_cast<CounterInstanceData*>(PerInstanceData);
	return false;
}

#undef LOCTEXT_NAMESPACE