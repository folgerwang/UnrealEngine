// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "NiagaraParameterStore.h"
#include "NiagaraScriptExecutionParameterStore.generated.h"

USTRUCT()
struct FNiagaraScriptExecutionPaddingInfo
{
	GENERATED_USTRUCT_BODY()
public:
	FNiagaraScriptExecutionPaddingInfo() : SrcOffset(0), DestOffset(0), SrcSize(0), DestSize(0) {}
	FNiagaraScriptExecutionPaddingInfo(uint32 InSrcOffset, uint32 InDestOffset, uint32 InSrcSize, uint32 InDestSize) : SrcOffset(InSrcOffset), DestOffset(InDestOffset), SrcSize(InSrcSize), DestSize(InDestSize) {}
	uint32 SrcOffset;
	uint32 DestOffset;
	uint32 SrcSize;
	uint32 DestSize;
};

/**
Storage class containing actual runtime buffers to be used by the VM and the GPU.
Is not the actual source for any parameter data, rather just the final place it's gathered from various other places ready for execution.
*/
USTRUCT()
struct FNiagaraScriptExecutionParameterStore : public FNiagaraParameterStore
{
	GENERATED_USTRUCT_BODY()
public:
	FNiagaraScriptExecutionParameterStore();
	FNiagaraScriptExecutionParameterStore(const FNiagaraParameterStore& Other);
	FNiagaraScriptExecutionParameterStore& operator=(const FNiagaraParameterStore& Other);

	//TODO: These function can probably go away entirely when we replace the FNiagaraParameters and DataInterface info in the script with an FNiagaraParameterStore.
	//Special care with prev params and internal params will have to be taken there.
	/** Call this init function if you are using a Niagara parameter store within a UNiagaraScript.*/
	void InitFromOwningScript(UNiagaraScript* Script, ENiagaraSimTarget SimTarget, bool bNotifyAsDirty);
	/** Call this init function if you are using a Niagara parameter store within an FNiagaraScriptExecutionContext.*/
	void InitFromOwningContext(UNiagaraScript* Script, ENiagaraSimTarget SimTarget, bool bNotifyAsDirty);
	void AddScriptParams(UNiagaraScript* Script, ENiagaraSimTarget SimTarget, bool bTriggerRebind);
	void CopyCurrToPrev();

	bool AddParameter(const FNiagaraVariable& Param, bool bInitInterfaces = true, bool bTriggerRebind = true)
	{
		if (FNiagaraParameterStore::AddParameter(Param, bInitInterfaces, bTriggerRebind))
		{
			AddPaddedParamSize(Param.GetType(), IndexOf(Param));
			return true;
		}
		return false;
	}

	bool RemoveParameter(FNiagaraVariable& Param)
	{
		check(0);//Not allowed to remove parameters from an execution store as it will adjust the table layout mess up the 
		return false;
	}

	void RenameParameter(FNiagaraVariable& Param, FName NewName)
	{
		check(0);//Can't rename parameters for an execution store.
	}

	void Empty(bool bClearBindings=true)
	{
		FNiagaraParameterStore::Empty(bClearBindings);
		PaddingInfo.Empty();
		PaddedParameterSize = 0;
		bInitialized = false;
	}

	// Just the external parameters, not previous or internal...
	uint32 GetExternalParameterSize() { return ParameterSize; }

	// The entire buffer padded out by the required alignment of the types..
	uint32 GetPaddedParameterSizeInBytes() { return PaddedParameterSize; }

	// Helper that converts the data from the base type array internally into the padded out renderer-ready format.
	void CopyParameterDataToPaddedBuffer(uint8* InTargetBuffer, uint32 InTargetBufferSizeInBytes);


	bool IsInitialized() const {return bInitialized;}

protected:
	void AddPaddedParamSize(const FNiagaraTypeDefinition& InParamType, uint32 InOffset);

private:

	/** Size of the parameter data not including prev frame values or internal constants. Allows copying into previous parameter values for interpolated spawn scripts. */
	UPROPERTY()
	int32 ParameterSize;

	UPROPERTY()
	uint32 PaddedParameterSize;

	static void GenerateLayoutInfoInternal(TArray<FNiagaraScriptExecutionPaddingInfo>& Members, uint32& NextMemberOffset, const UStruct* InSrcStruct, uint32 InSrcOffset);

	UPROPERTY()
	TArray<FNiagaraScriptExecutionPaddingInfo> PaddingInfo;

	UPROPERTY()
	uint8 bInitialized : 1;
};