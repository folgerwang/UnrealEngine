// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraEmitterInstance.h: Niagara emitter simulation class
==============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "NiagaraCommon.h"
#include "NiagaraDataSet.h"
#include "NiagaraEvents.h"
#include "NiagaraCollision.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraScriptExecutionParameterStore.h"
#include "NiagaraTypes.h"
#include "RHIGPUReadback.h"



struct FNiagaraDataSetExecutionInfo
{
	FNiagaraDataSetExecutionInfo()
		: DataSet(nullptr)
		, StartInstance(0)
		, bAllocate(false)
		, bUpdateInstanceCount(false)
	{
	}

	FNiagaraDataSetExecutionInfo(FNiagaraDataSet* InDataSet, int32 InStartInstance, bool bInAllocate, bool bInUpdateInstanceCount)
		: DataSet(InDataSet)
		, StartInstance(InStartInstance)
		, bAllocate(bInAllocate)
		, bUpdateInstanceCount(bInUpdateInstanceCount)
	{}

	FNiagaraDataSet* DataSet;
	int32 StartInstance;
	bool bAllocate;
	bool bUpdateInstanceCount;
};

struct FNiagaraScriptExecutionContext
{
	UNiagaraScript* Script;

	/** Table of external function delegates called from the VM. */
	TArray<FVMExternalFunction> FunctionTable;

	/** Table of instance data for data interfaces that require it. */
	TArray<void*> DataInterfaceInstDataTable;

	/** Parameter store. Contains all data interfaces and a parameter buffer that can be used directly by the VM or GPU. */
	FNiagaraScriptExecutionParameterStore Parameters;

	TArray<FDataSetMeta> DataSetMetaTable;

	static uint32 TickCounter;

	FNiagaraScriptExecutionContext();
	~FNiagaraScriptExecutionContext();

	bool Init(UNiagaraScript* InScript, ENiagaraSimTarget InTarget);
	

	bool Tick(class FNiagaraSystemInstance* Instance, ENiagaraSimTarget SimTarget = ENiagaraSimTarget::CPUSim);
	void PostTick();

	bool Execute(uint32 NumInstances, TArray<FNiagaraDataSetExecutionInfo, TInlineAllocator<8>>& DataSetInfos);

	const TArray<UNiagaraDataInterface*>& GetDataInterfaces()const { return Parameters.GetDataInterfaces(); }

	void DirtyDataInterfaces();

	bool CanExecute()const;
};




struct FNiagaraComputeExecutionContext
{
	FNiagaraComputeExecutionContext()
		: MainDataSet(nullptr)
		, SpawnRateInstances(0)
		, EventSpawnTotal(0)
		, SpawnScript(nullptr)
		, UpdateScript(nullptr)
		, GPUScript(nullptr)
		, RTUpdateScript(0)
		, RTSpawnScript(0)
		, RTGPUScript(0)
		, CBufferLayout(TEXT("Niagara Compute Sim CBuffer"))
		, GPUDataReadback(nullptr)
		, AccumulatedSpawnRate(0)
		, NumIndicesPerInstance(0)
		, PerInstanceData(nullptr)
		, PerInstanceDataSize(0)
		, PerInstanceDataInterfaceOffsets(nullptr)
		, PendingExecutionQueueMask(0)
#if WITH_EDITORONLY_DATA
		, GPUDebugDataReadbackFloat(nullptr)
		, GPUDebugDataReadbackInt(nullptr)
		, GPUDebugDataReadbackCounts(nullptr)
		, GPUDebugDataCurrBufferIdx(0xFFFFFFFF)
		, GPUDebugDataFloatSize(0)
		, GPUDebugDataIntSize(0)
#endif	  
	{
	}

	~FNiagaraComputeExecutionContext()
	{
		checkf(IsInRenderingThread(), TEXT("Can only delete the gpu readback from the render thread"));
		if (GPUDataReadback)
		{
			delete GPUDataReadback;
			GPUDataReadback = nullptr;
		}

#if WITH_EDITORONLY_DATA
		if (GPUDebugDataReadbackFloat)
		{
			delete GPUDebugDataReadbackFloat;
			GPUDebugDataReadbackFloat = nullptr;
		}
		if (GPUDebugDataReadbackInt)
		{
			delete GPUDebugDataReadbackInt;
			GPUDebugDataReadbackInt = nullptr;
		}
		if (GPUDebugDataReadbackCounts)
		{
			delete GPUDebugDataReadbackCounts;
			GPUDebugDataReadbackCounts = nullptr;
		}
#endif
	}

	void Reset()
	{
		FNiagaraComputeExecutionContext* Context = this;
		ENQUEUE_RENDER_COMMAND(ResetRT)(
			[Context](FRHICommandListImmediate& RHICmdList)
			{
				Context->ResetInternal();
			}
		);
	}

	void InitParams(UNiagaraScript* InGPUComputeScript, UNiagaraScript* InSpawnScript, UNiagaraScript *InUpdateScript, ENiagaraSimTarget InSimTarget, const FString& InDebugSimName)
	{
		DebugSimName = InDebugSimName;
		CombinedParamStore.InitFromOwningContext(InGPUComputeScript, InSimTarget, true);

		GPUScript = InGPUComputeScript;
		SpawnScript = InSpawnScript;
		UpdateScript = InUpdateScript;

#if DO_CHECK
		FNiagaraShader *Shader = InGPUComputeScript->GetRenderThreadScript()->GetShaderGameThread();
		DIParamInfo.Empty();
		if (Shader)
		{
			for (FNiagaraDataInterfaceParamRef& DIParams : Shader->GetDIParameters())
			{
				DIParamInfo.Add(DIParams.ParameterInfo);
			}
		}
		else
		{
			DIParamInfo = InGPUComputeScript->GetRenderThreadScript()->GetDataInterfaceParamInfo();
		}
#endif
	}

	void DirtyDataInterfaces()
	{
		CombinedParamStore.MarkInterfacesDirty();
	}

	bool Tick(FNiagaraSystemInstance* ParentSystemInstance)
	{
		if (CombinedParamStore.GetInterfacesDirty())
		{
#if DO_CHECK
			const TArray<UNiagaraDataInterface*> &DataInterfaces = CombinedParamStore.GetDataInterfaces();
			// We must make sure that the data interfaces match up between the original script values and our overrides...
			if (DIParamInfo.Num() != DataInterfaces.Num())
			{
				UE_LOG(LogNiagara, Warning, TEXT("Mismatch between Niagara GPU Execution Context data interfaces and those in its script!"));
				return false;
			}

			for (int32 i=0; i<DIParamInfo.Num(); ++i)
			{
				FString UsedClassName = DataInterfaces[i]->GetClass()->GetName();
				if (DIParamInfo[i].DIClassName != UsedClassName)
				{
					UE_LOG(LogNiagara, Warning, TEXT("Mismatched class between Niagara GPU Execution Context data interfaces and those in its script!\nIndex:%d\nShader:%s\nScript:%s")
						, i, *DIParamInfo[i].DIClassName, *UsedClassName);
				}
			}
#endif

			CombinedParamStore.Tick();
		}

		return true;
	}

private:
	void ResetInternal()
	{
		checkf(IsInRenderingThread(), TEXT("Can only reset the gpu context from the render thread"));
		AccumulatedSpawnRate = 0;
		PendingExecutionQueueMask = 0;
		if (GPUDataReadback)
		{
			delete GPUDataReadback;
			GPUDataReadback = nullptr;
		}

#if WITH_EDITORONLY_DATA
		if (GPUDebugDataReadbackFloat)
		{
			delete GPUDebugDataReadbackFloat;
			GPUDebugDataReadbackFloat = nullptr;
		}
		if (GPUDebugDataReadbackInt)
		{
			delete GPUDebugDataReadbackInt;
			GPUDebugDataReadbackInt = nullptr;
		}
		if (GPUDebugDataReadbackCounts)
		{
			delete GPUDebugDataReadbackCounts;
			GPUDebugDataReadbackCounts = nullptr;
		}
#endif
	}

public:
	const TArray<FNiagaraEventScriptProperties> &GetEventHandlers() const { return EventHandlerScriptProps; }
	FString DebugSimName;
	class FNiagaraDataSet *MainDataSet;
	TArray<FNiagaraDataSet*>UpdateEventWriteDataSets;
	TArray<FNiagaraEventScriptProperties> EventHandlerScriptProps;
	TArray<FNiagaraDataSet*> EventSets;
	uint32 SpawnRateInstances;

	TArray<int32> EventSpawnCounts;
	uint32 EventSpawnTotal;
	UNiagaraScript* SpawnScript;
	UNiagaraScript* UpdateScript;
	UNiagaraScript* GPUScript;
	class FNiagaraShaderScript*  RTUpdateScript;
	class FNiagaraShaderScript*  RTSpawnScript;
	class FNiagaraShaderScript*  RTGPUScript;	
	FRHIUniformBufferLayout CBufferLayout; // Persistent layouts used to create Compute Sim CBuffer
	TArray<uint8, TAlignedHeapAllocator<16>> ParamData_RT;		// RT side copy of the parameter data
	FNiagaraScriptExecutionParameterStore CombinedParamStore;
	static uint32 TickCounter;
#if DO_CHECK
	TArray< FNiagaraDataInterfaceGPUParamInfo >  DIParamInfo;
#endif

	FRHIGPUMemoryReadback *GPUDataReadback;
	uint32 AccumulatedSpawnRate;
	uint32 NumIndicesPerInstance;	// how many vtx indices per instance the renderer is going to have for its draw call

	void* PerInstanceData; // Data stored on parent system instance
	uint32 PerInstanceDataSize; // Size of data stored on parent system instance in bytes
	TMap<TWeakObjectPtr<UNiagaraDataInterface>, int32>* PerInstanceDataInterfaceOffsets;

	/** Ensures we only enqueue each context once per queue before they're dispatched. See SIMULATION_QUEUE_COUNT */
	uint32 PendingExecutionQueueMask;


#if WITH_EDITORONLY_DATA
	mutable FRHIGPUMemoryReadback *GPUDebugDataReadbackFloat;
	mutable FRHIGPUMemoryReadback *GPUDebugDataReadbackInt;
	mutable FRHIGPUMemoryReadback *GPUDebugDataReadbackCounts;
	mutable int32 GPUDebugDataCurrBufferIdx;
	mutable uint32 GPUDebugDataFloatSize;
	mutable uint32 GPUDebugDataIntSize;
	mutable TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe> DebugInfo;
#endif
};
