// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraScriptExecutionParameterStore.h"
#include "NiagaraStats.h"
#include "NiagaraEmitterInstanceBatcher.h"
#include "NiagaraDataInterface.h"
#include "NiagaraSystemInstance.h"

FNiagaraScriptExecutionParameterStore::FNiagaraScriptExecutionParameterStore()
	: FNiagaraParameterStore(), bInitialized(false)
{

}

FNiagaraScriptExecutionParameterStore::FNiagaraScriptExecutionParameterStore(const FNiagaraParameterStore& Other)
{
	*this = Other;
}

FNiagaraScriptExecutionParameterStore& FNiagaraScriptExecutionParameterStore::operator=(const FNiagaraParameterStore& Other)
{
	FNiagaraParameterStore::operator=(Other);
	return *this;
}

uint32 OffsetAlign(uint32 SrcOffset, uint32 Size)
{
	uint32 OffsetRemaining = UNIFORM_BUFFER_STRUCT_ALIGNMENT - (SrcOffset % UNIFORM_BUFFER_STRUCT_ALIGNMENT);
	if (Size <= OffsetRemaining)
	{
		return SrcOffset;
	}
	else
	{
		return Align(SrcOffset, UNIFORM_BUFFER_STRUCT_ALIGNMENT);
	}
}

void FNiagaraScriptExecutionParameterStore::GenerateLayoutInfoInternal(TArray<FNiagaraScriptExecutionPaddingInfo>& Members, uint32& NextMemberOffset, const UStruct* InSrcStruct, uint32 InSrcOffset)
{
	uint32 VectorPaddedSize = (TUniformBufferTypeInfo<FVector4, false>::NumRows * TUniformBufferTypeInfo<FVector4, false>::NumColumns) * sizeof(float);

	// Now insert an appropriate data member into the mix...
	if (InSrcStruct == FNiagaraTypeDefinition::GetBoolStruct() || InSrcStruct == FNiagaraTypeDefinition::GetIntStruct())
	{
		uint32 IntSize = (TUniformBufferTypeInfo<uint32, false>::NumRows * TUniformBufferTypeInfo<uint32, false>::NumColumns) * sizeof(uint32);
		Members.Emplace(InSrcOffset, Align(NextMemberOffset, TUniformBufferTypeInfo<uint32, false>::Alignment), IntSize, IntSize);
		InSrcOffset += sizeof(uint32);
		NextMemberOffset = Members[Members.Num() - 1].DestOffset + Members[Members.Num() - 1].DestSize;
	}
	else if (InSrcStruct == FNiagaraTypeDefinition::GetFloatStruct())
	{
		uint32 FloatSize = (TUniformBufferTypeInfo<float, false>::NumRows * TUniformBufferTypeInfo<float, false>::NumColumns) * sizeof(float);
		Members.Emplace(InSrcOffset, Align(NextMemberOffset, TUniformBufferTypeInfo<float, false>::Alignment), FloatSize, FloatSize);
		InSrcOffset += sizeof(float);
		NextMemberOffset = Members[Members.Num() - 1].DestOffset + Members[Members.Num() - 1].DestSize;
	}
	else if (InSrcStruct == FNiagaraTypeDefinition::GetVec2Struct())
	{
		uint32 StructFinalSize = (TUniformBufferTypeInfo<FVector2D, false>::NumRows * TUniformBufferTypeInfo<FVector2D, false>::NumColumns) * sizeof(float);
		Members.Emplace(InSrcOffset, OffsetAlign(NextMemberOffset, VectorPaddedSize), StructFinalSize, VectorPaddedSize);
		InSrcOffset += sizeof(FVector2D);
		NextMemberOffset = Members[Members.Num() - 1].DestOffset + Members[Members.Num() - 1].DestSize;
	}
	else if (InSrcStruct == FNiagaraTypeDefinition::GetVec3Struct())
	{
		uint32 StructFinalSize = (TUniformBufferTypeInfo<FVector, false>::NumRows * TUniformBufferTypeInfo<FVector, false>::NumColumns) * sizeof(float);
		Members.Emplace(InSrcOffset, OffsetAlign(NextMemberOffset, VectorPaddedSize), StructFinalSize, VectorPaddedSize);
		InSrcOffset += sizeof(FVector);
		NextMemberOffset = Members[Members.Num() - 1].DestOffset + Members[Members.Num() - 1].DestSize;
	}
	else if (InSrcStruct == FNiagaraTypeDefinition::GetVec4Struct() || InSrcStruct == FNiagaraTypeDefinition::GetColorStruct() || InSrcStruct == FNiagaraTypeDefinition::GetQuatStruct())
	{
		uint32 StructFinalSize = (TUniformBufferTypeInfo<FVector4, false>::NumRows * TUniformBufferTypeInfo<FVector4, false>::NumColumns) * sizeof(float);
		Members.Emplace(InSrcOffset, Align(NextMemberOffset, TUniformBufferTypeInfo<FVector4, false>::Alignment), StructFinalSize, StructFinalSize);
		InSrcOffset += sizeof(FVector4);
		NextMemberOffset = Members[Members.Num() - 1].DestOffset + Members[Members.Num() - 1].DestSize;
	}
	else if (InSrcStruct == FNiagaraTypeDefinition::GetMatrix4Struct())
	{
		uint32 StructFinalSize = (TUniformBufferTypeInfo<FMatrix, false>::NumRows * TUniformBufferTypeInfo<FMatrix, false>::NumColumns) * sizeof(float);
		Members.Emplace(InSrcOffset, Align(NextMemberOffset, TUniformBufferTypeInfo<FMatrix, false>::Alignment), StructFinalSize, StructFinalSize);
		InSrcOffset += sizeof(FMatrix);
		NextMemberOffset = Members[Members.Num() - 1].DestOffset + Members[Members.Num() - 1].DestSize;
	}
	else
	{
		NextMemberOffset = Align(NextMemberOffset, UNIFORM_BUFFER_STRUCT_ALIGNMENT); // New structs should be aligned to the head..

		for (TFieldIterator<UProperty> PropertyIt(InSrcStruct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			const UStruct* Struct = nullptr;

			// First determine what struct type we're dealing with...
			if (Property->IsA(UFloatProperty::StaticClass()))
			{
				Struct = FNiagaraTypeDefinition::GetFloatStruct();
			}
			else if (Property->IsA(UIntProperty::StaticClass()))
			{
				Struct = FNiagaraTypeDefinition::GetIntStruct();
			}
			else if (Property->IsA(UBoolProperty::StaticClass()))
			{
				Struct = FNiagaraTypeDefinition::GetBoolStruct();
			}
			//Should be able to support double easily enough
			else if (UStructProperty* StructProp = CastChecked<UStructProperty>(Property))
			{
				Struct = StructProp->Struct;
			}
			else
			{
				check(false);
			}

			GenerateLayoutInfoInternal(Members, NextMemberOffset, Struct, InSrcOffset);
		}
	}

}

void FNiagaraScriptExecutionParameterStore::AddPaddedParamSize(const FNiagaraTypeDefinition& InParamType, uint32 InOffset)
{
	if (InParamType.IsDataInterface())
	{
		return;
	}

	uint32 NextMemberOffset = 0;
	if (PaddingInfo.Num() != 0)
	{
		NextMemberOffset = PaddingInfo[PaddingInfo.Num() - 1].DestOffset + PaddingInfo[PaddingInfo.Num() - 1].DestSize;
	}
	GenerateLayoutInfoInternal(PaddingInfo, NextMemberOffset, InParamType.GetScriptStruct(), InOffset);

	if (PaddingInfo.Num() != 0)
	{
		NextMemberOffset = PaddingInfo[PaddingInfo.Num() - 1].DestOffset + PaddingInfo[PaddingInfo.Num() - 1].DestSize;
		PaddedParameterSize = Align(NextMemberOffset, UNIFORM_BUFFER_STRUCT_ALIGNMENT);
	}
	else
	{
		PaddedParameterSize = 0;
	}
}


void FNiagaraScriptExecutionParameterStore::InitFromOwningScript(UNiagaraScript* Script, ENiagaraSimTarget SimTarget, bool bNotifyAsDirty)
{
	//TEMPORARTY:
	//We should replace the storage on the script with an FNiagaraParameterStore also so we can just copy that over here. Though that is an even bigger refactor job so this is a convenient place to break that work up.

	Empty();
	PaddedParameterSize = 0;
	PaddingInfo.Empty();

	if (Script)
	{
		AddScriptParams(Script, SimTarget, false);

		Script->RapidIterationParameters.Bind(this);

		if (bNotifyAsDirty)
		{
			MarkParametersDirty();
			MarkInterfacesDirty();
			OnLayoutChange();
		}
	}
	bInitialized = true;
}

void FNiagaraScriptExecutionParameterStore::InitFromOwningContext(UNiagaraScript* Script, ENiagaraSimTarget SimTarget, bool bNotifyAsDirty)
{
	Empty();
	PaddingInfo.Empty();

#if WITH_EDITORONLY_DATA
	if (Script != nullptr)
	{
		DebugName = FString::Printf(TEXT("ScriptExecParamStore %s %p"), *Script->GetFullName(), this);
	}
#endif

	const FNiagaraScriptExecutionParameterStore* SrcStore = Script->GetExecutionReadyParameterStore(SimTarget);
	if (SrcStore)
	{
		InitFromSource(SrcStore, false);
		ParameterSize = SrcStore->ParameterSize;
		PaddedParameterSize = SrcStore->PaddedParameterSize;
		PaddingInfo = SrcStore->PaddingInfo;

		if (bNotifyAsDirty)
		{
			MarkParametersDirty();
			MarkInterfacesDirty();
			OnLayoutChange();
		}
	}


	bInitialized = true;
}

void FNiagaraScriptExecutionParameterStore::AddScriptParams(UNiagaraScript* Script, ENiagaraSimTarget SimTarget, bool bTriggerRebind)
{
	if (Script == nullptr)
	{
		return;
	}
	PaddingInfo.Empty();

	//Here we add the current frame parameters.
	bool bAdded = false;
	for (FNiagaraVariable& Param : Script->GetVMExecutableData().Parameters.Parameters)
	{
		bAdded |= AddParameter(Param, false, false);
	}

#if WITH_EDITORONLY_DATA
	DebugName = FString::Printf(TEXT("ScriptExecParamStore %s %p"), *Script->GetFullName(), this);
#endif
	
	//Add previous frame values if we're interpolated spawn.
	bool bIsInterpolatedSpawn = Script->GetVMExecutableDataCompilationId().HasInterpolatedParameters();

	if (bIsInterpolatedSpawn)
	{
		for (FNiagaraVariable& Param : Script->GetVMExecutableData().Parameters.Parameters)
		{
			FNiagaraVariable PrevParam(Param.GetType(), FName(*(INTERPOLATED_PARAMETER_PREFIX + Param.GetName().ToString())));
			bAdded |= AddParameter(PrevParam, false, false);
		}
	}
	
	ParameterSize = GetParameterDataArray().Num();
	if (bIsInterpolatedSpawn)
	{
		CopyCurrToPrev();
		bAdded = true;
	}

	//Internal constants - only needed for non-GPU sim
	if (SimTarget != ENiagaraSimTarget::GPUComputeSim)
	{
		for (FNiagaraVariable& InternalVar : Script->GetVMExecutableData().InternalParameters.Parameters)
		{
			bAdded |= AddParameter(InternalVar, false, false);
		}
	}

	check(Script->GetVMExecutableData().DataInterfaceInfo.Num() == Script->GetCachedDefaultDataInterfaces().Num());
	for (FNiagaraScriptDataInterfaceInfo& Info : Script->GetCachedDefaultDataInterfaces())
	{
		FName ParameterName;
		if (Info.RegisteredParameterMapRead != NAME_None)
		{
			ParameterName = Info.RegisteredParameterMapRead;
		}
		else
		{
			// If the data interface wasn't used in a parameter map, mangle the name so that it doesn't accidentally bind to
			// a valid parameter.
			ParameterName = *(TEXT("__INTERNAL__.") + Info.Name.ToString());
		}

		FNiagaraVariable Var(Info.Type, ParameterName);
		bAdded |= AddParameter(Var, false, false);
		SetDataInterface(Info.DataInterface, IndexOf(Var));
	}

	if (bAdded && bTriggerRebind)
	{
		OnLayoutChange();
	}
}

void FNiagaraScriptExecutionParameterStore::CopyCurrToPrev()
{
	int32 ParamStart = ParameterSize / 2;
	checkSlow(FMath::Frac((float)ParameterSize / 2) == 0.0f);

	FMemory::Memcpy(GetParameterData_Internal(ParamStart), GetParameterData(0), ParamStart);
}


void FNiagaraScriptExecutionParameterStore::CopyParameterDataToPaddedBuffer(uint8* InTargetBuffer, uint32 InTargetBufferSizeInBytes)
{
	check((uint32)ParameterSize <= PaddedParameterSize);
	check(InTargetBufferSizeInBytes >= PaddedParameterSize);
	FMemory::Memzero(InTargetBuffer, InTargetBufferSizeInBytes);
	const uint8* SrcData = GetParameterDataArray().GetData();
	for (int32 i = 0; i < PaddingInfo.Num(); i++)
	{
		check((PaddingInfo[i].DestOffset + PaddingInfo[i].DestSize) <= InTargetBufferSizeInBytes);
		check((PaddingInfo[i].SrcOffset + PaddingInfo[i].SrcSize) <= (uint32)GetParameterDataArray().Num());
		FMemory::Memcpy(InTargetBuffer + PaddingInfo[i].DestOffset, SrcData + PaddingInfo[i].SrcOffset, PaddingInfo[i].SrcSize);
	}
}
