// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "RHI.h"
#include "VectorVM.h"
#include "RenderingThread.h"


/** Helper class defining the layout and location of an FNiagaraVariable in an FNiagaraDataBuffer-> */
struct FNiagaraVariableLayoutInfo
{
	/** Start index for the float components in the main buffer. */
	uint32 FloatComponentStart;
	/** Start index for the int32 components in the main buffer. */
	uint32 Int32ComponentStart;

	uint32 GetNumFloatComponents()const { return LayoutInfo.FloatComponentByteOffsets.Num(); }
	uint32 GetNumInt32Components()const { return LayoutInfo.Int32ComponentByteOffsets.Num(); }

	/** This variable's type layout info. */
	FNiagaraTypeLayoutInfo LayoutInfo;
};

class FNiagaraDataSet;

/** Buffer containing one frame of Niagara simulation data. */
class NIAGARA_API FNiagaraDataBuffer
{
public:
	FNiagaraDataBuffer() : Owner(nullptr)
	{
		Reset();
	}
	~FNiagaraDataBuffer();
	void Init(FNiagaraDataSet* InOwner);
	void Allocate(uint32 NumInstances, bool bMaintainExisting=false);
	void AllocateGPU(uint32 InNumInstances, FRHICommandList &RHICmdList);
	void SwapInstances(uint32 OldIndex, uint32 NewIndex);
	void KillInstance(uint32 InstanceIdx);
	void CopyTo(FNiagaraDataBuffer& DestBuffer, int32 StartIdx, int32 NumInstances)const;
	void CopyTo(FNiagaraDataBuffer& DestBuffer)const;
	void GPUCopyTo(FNiagaraDataBuffer& DestBuffer, float* GPUReadBackFloat, int* GPUReadBackInt, int32 StartIdx, int32 NumInstances)const;

	const TArray<uint8>& GetFloatBuffer()const { return FloatData; }
	const TArray<uint8>& GetInt32Buffer()const { return Int32Data; }

	FORCEINLINE const uint8* GetComponentPtrFloat(uint32 ComponentIdx)const
	{
		return FloatData.GetData() + FloatStride * ComponentIdx;
	}

	FORCEINLINE const uint8* GetComponentPtrInt32(uint32 ComponentIdx)const
	{
		return Int32Data.GetData() + Int32Stride * ComponentIdx;
	}

	FORCEINLINE uint8* GetComponentPtrFloat(uint32 ComponentIdx)
	{
		return FloatData.GetData() + FloatStride * ComponentIdx;
	}

	FORCEINLINE uint8* GetComponentPtrInt32(uint32 ComponentIdx)
	{
		return Int32Data.GetData() + Int32Stride * ComponentIdx;
	}

	FORCEINLINE float* GetInstancePtrFloat(uint32 ComponentIdx, uint32 InstanceIdx)
	{
		return (float*)GetComponentPtrFloat(ComponentIdx) + InstanceIdx;
	}

	FORCEINLINE int32* GetInstancePtrInt32(uint32 ComponentIdx, uint32 InstanceIdx)
	{
		return (int32*)GetComponentPtrInt32(ComponentIdx) + InstanceIdx;
	}

	FORCEINLINE float* GetInstancePtrFloat(uint32 ComponentIdx, uint32 InstanceIdx)const
	{
		return (float*)GetComponentPtrFloat(ComponentIdx) + InstanceIdx;
	}

	FORCEINLINE int32* GetInstancePtrInt32(uint32 ComponentIdx, uint32 InstanceIdx)const
	{
		return (int32*)GetComponentPtrInt32(ComponentIdx) + InstanceIdx;
	}

	FORCEINLINE uint8* GetComponentPtrFloat(float* BasePtr, uint32 ComponentIdx) const
	{
		return (uint8*)BasePtr + FloatStride * ComponentIdx;
	}

	FORCEINLINE uint8* GetComponentPtrInt32(int* BasePtr, uint32 ComponentIdx) const
	{
		return (uint8*)BasePtr + Int32Stride * ComponentIdx;
	}

	FORCEINLINE float* GetInstancePtrFloat(float* BasePtr, uint32 ComponentIdx, uint32 InstanceIdx)const
	{
		return (float*)GetComponentPtrFloat(BasePtr, ComponentIdx) + InstanceIdx;
	}

	FORCEINLINE int32* GetInstancePtrInt32(int* BasePtr, uint32 ComponentIdx, uint32 InstanceIdx)const
	{
		return (int32*)GetComponentPtrInt32(BasePtr, ComponentIdx) + InstanceIdx;
	}

	uint32 GetNumInstances()const { return NumInstances; }
	uint32 GetNumInstancesAllocated()const { return NumInstancesAllocated; }

	void SetNumInstances(uint32 InNumInstances) 
	{ 
		NumInstances = InNumInstances;
	}

	void Reset();

	FORCEINLINE uint32 GetSizeBytes()const
	{
		return FloatData.Num() + Int32Data.Num();
	}

	const FRWBuffer *GetGPUBufferFloat() const
	{
		return &GPUBufferFloat;
	}

	const FRWBuffer *GetGPUBufferInt() const
	{
		return &GPUBufferInt;
	}

	int32 GetSafeComponentBufferSize() const
	{
		return GetSafeComponentBufferSize(GetNumInstancesAllocated());
	}

	uint32 GetFloatStride() const { return FloatStride; }
	uint32 GetInt32Stride() const { return Int32Stride; }

	FORCEINLINE const FNiagaraDataSet* GetOwner()const { return Owner; }

	int32 TransferInstance(FNiagaraDataBuffer& SourceBuffer, int32 InstanceIndex);

	bool CheckForNaNs()const;
private:

	FORCEINLINE int32 GetSafeComponentBufferSize(int32 RequiredSize) const
	{
		//Round up to VECTOR_WIDTH_BYTES.
		//Both aligns the component buffers to the vector width but also ensures their ops cannot stomp over one another.		
		return RequiredSize + VECTOR_WIDTH_BYTES - (RequiredSize % VECTOR_WIDTH_BYTES) + VECTOR_WIDTH_BYTES;
	}

	/** Back ptr to our owning data set. Used to access layout info for the buffer. */
	FNiagaraDataSet* Owner;

	/** Float components of simulation data. */
	TArray<uint8> FloatData;
	/** Int32 components of simulation data. */
	TArray<uint8> Int32Data;

	/** Stride between components in the float buffer. */
	uint32 FloatStride;
	/** Stride between components in the int32 buffer. */
	uint32 Int32Stride;

	uint32 NumChunksAllocatedForGPU;

	/** Number of instances in data. */
	uint32 NumInstances;
	/** Number of instances the buffer has been allocated for. */
	uint32 NumInstancesAllocated;

	FRWBuffer GPUBufferFloat;
	FRWBuffer GPUBufferInt;
};

//////////////////////////////////////////////////////////////////////////

/**
General storage class for all per instance simulation data in Niagara.
*/
class NIAGARA_API FNiagaraDataSet
{
	friend FNiagaraDataBuffer;

	void Reset()
	{
		Variables.Empty();
		VariableLayouts.Empty();
		CurrBuffer = 0;
		bFinalized = false;
		TotalFloatComponents = 0;
		TotalInt32Components = 0;
		MaxBufferIdx = 1;
		bNeedsPersistentIDs = 0;

		SimTarget = ENiagaraSimTarget::CPUSim;

		ResetBuffersInternal();
	}

public:
	FNiagaraDataSet()
	{
		Reset();
	}
	
	~FNiagaraDataSet();

	void Init(FNiagaraDataSetID InID, ENiagaraSimTarget InSimTarget)
	{
		Reset();
		ID = InID;
		SimTarget = InSimTarget;
	}


	void AddVariable(FNiagaraVariable& Variable)
	{
		check(!bFinalized);
		Variables.AddUnique(Variable);
	}

	void AddVariables(const TArray<FNiagaraVariable>& Vars)
	{
		check(!bFinalized);
		for (const FNiagaraVariable& Var : Vars)
		{
			Variables.AddUnique(Var);
		}
	}

	FORCEINLINE void SetNeedsPersistentIDs(bool bNeedsIDs)
	{
		bNeedsPersistentIDs = bNeedsIDs;
	}

	FORCEINLINE bool GetNeedsPersistentIDs()const { return bNeedsPersistentIDs; }

	/** Finalize the addition of variables and other setup before this data set can be used. */
	FORCEINLINE void Finalize()
	{
		check(!bFinalized);
		bFinalized = true;
		BuildLayout();
	}

	/** Removes a specific instance from the current frame's data buffer. */
	FORCEINLINE void KillInstance(uint32 InstanceIdx)
	{
		check(bFinalized);
		CheckCorrectThread();
		CurrData().KillInstance(InstanceIdx);
	}

	FORCEINLINE void SwapInstances(uint32 OldIndex, uint32 NewIndex)
	{
		check(bFinalized);
		CheckCorrectThread();
		PrevData().SwapInstances(OldIndex, NewIndex);
	}

	int32 TransferInstance(FNiagaraDataSet& SourceDataset, int32 InstanceIndex)
	{
		check(bFinalized);
		CheckCorrectThread();
		return CurrData().TransferInstance(SourceDataset.CurrData(), InstanceIndex);
	}

	/** Appends all variables in this dataset to a register table ready for execution by the VectorVM. */
	bool AppendToRegisterTable(uint8** InputRegisters, int32& NumInputRegisters, uint8** OutputRegisters, int32& NumOutputRegisters, int32 StartInstance)
	{
		check(bFinalized);
		CheckCorrectThread();
		int32 TotalComponents = GetNumFloatComponents() + GetNumInt32Components();
		if (NumInputRegisters + TotalComponents > VectorVM::MaxInputRegisters || NumOutputRegisters + TotalComponents > VectorVM::MaxOutputRegisters)
		{
			UE_LOG(LogNiagara, Error, TEXT("Niagara Script is using too many IO registers!"));
			return false;
		}
		else
		{
			for (FNiagaraVariableLayoutInfo& VarLayout : VariableLayouts)
			{
				int32 NumFloats = VarLayout.GetNumFloatComponents();
				int32 NumInts = VarLayout.GetNumInt32Components();
				for (int32 CompIdx = 0; CompIdx < NumFloats; ++CompIdx)
				{
					uint32 CompBufferOffset = VarLayout.FloatComponentStart + CompIdx;
					uint32 CompRegisterOffset = VarLayout.LayoutInfo.FloatComponentRegisterOffsets[CompIdx];
					InputRegisters[NumInputRegisters + CompRegisterOffset] = (uint8*)PrevData().GetInstancePtrFloat(CompBufferOffset, StartInstance);
					OutputRegisters[NumOutputRegisters + CompRegisterOffset] = (uint8*)CurrData().GetInstancePtrFloat(CompBufferOffset, StartInstance);
				}
				for (int32 CompIdx = 0; CompIdx < NumInts; ++CompIdx)
				{
					uint32 CompBufferOffset = VarLayout.Int32ComponentStart + CompIdx;
					uint32 CompRegisterOffset = VarLayout.LayoutInfo.Int32ComponentRegisterOffsets[CompIdx];
					InputRegisters[NumInputRegisters + CompRegisterOffset] = (uint8*)PrevData().GetInstancePtrInt32(CompBufferOffset, StartInstance);
					OutputRegisters[NumOutputRegisters + CompRegisterOffset] = (uint8*)CurrData().GetInstancePtrInt32(CompBufferOffset, StartInstance);
				}
				NumInputRegisters += NumFloats + NumInts;
				NumOutputRegisters += NumFloats + NumInts;
			}
			return true;
		}
	}

	void SetShaderParams(class FNiagaraShader *Shader, FRHICommandList &CommandList, uint32& WriteBufferIdx, uint32& ReadBufferIdx);
	void UnsetShaderParams(class FNiagaraShader *Shader, FRHICommandList &CommandList);
	void Allocate(int32 NumInstances, bool bMaintainExisting=false)
	{
		check(bFinalized);
		CheckCorrectThread();
		CurrData().Allocate(NumInstances, bMaintainExisting);

#if NIAGARA_NAN_CHECKING
		CheckForNaNs();
#endif

		if (bNeedsPersistentIDs)
		{
			int32 NumUsedIDs = MaxUsedID + 1;

			int32 RequiredIDs = FMath::Max(NumInstances, NumUsedIDs);
			int32 ExistingNumIDs = PrevIDTable().Num();
			int32 NumNewIDs = RequiredIDs - ExistingNumIDs;

			if (RequiredIDs > ExistingNumIDs)
			{
				//UE_LOG(LogNiagara, Warning, TEXT("Growing ID Table! OldSize:%d | NewSize:%d"), ExistingNumIDs, RequiredIDs);
				IDToIndexTable[CurrBuffer].SetNumUninitialized(RequiredIDs);

				//Free ID Table must always be at least as large as the data buffer + it's current size in the case all particles die this frame.
				FreeIDsTable.AddUninitialized(NumNewIDs);

				//Free table should always have enough room for these new IDs.
				check(NumFreeIDs + NumNewIDs <= FreeIDsTable.Num());

				//ID Table grows so add any new IDs to the free array. Add in reverse order to maintain a continuous increasing allocation when popping.
				for (int32 NewFreeID = RequiredIDs - 1; NewFreeID >= ExistingNumIDs; --NewFreeID)
				{
					FreeIDsTable[NumFreeIDs++] = NewFreeID ;
				}
				//UE_LOG(LogNiagara, Warning, TEXT("DataSetAllocate: Adding New Free IDs: %d - "), RequiredIDs - 1, ExistingNumIDs);
			}
#if 0
			else if (RequiredIDs < ExistingNumIDs >> 1)//Configurable?
			{
				//If the max id we use has reduced significantly then we can shrink the tables.
				//Have to go through the FreeIDs and remove any that are greater than the new table size.
				//UE_LOG(LogNiagara, Warning, TEXT("DataSetAllocate: Shrinking ID Table! OldSize:%d | NewSize:%d"), ExistingNumIDs, RequiredIDs);
				for (int32 CheckedFreeID = 0; CheckedFreeID < NumFreeIDs;)
				{
					checkSlow(NumFreeIDs <= FreeIDsTable.Num());
					if (FreeIDsTable[CheckedFreeID] >= RequiredIDs)
					{
						//UE_LOG(LogNiagara, Warning, TEXT("RemoveSwap FreeID: Removed:%d | Swapped:%d"), FreeIDsTable[CheckedFreeID], FreeIDsTable.Last());		
						int32 FreeIDIndex = --NumFreeIDs;
						FreeIDsTable[CheckedFreeID] = FreeIDsTable[FreeIDIndex];
						FreeIDsTable[FreeIDIndex] = INDEX_NONE;
					}
					else
					{
						++CheckedFreeID;
					}
				}

				check(NumFreeIDs <= RequiredIDs);
				FreeIDsTable.SetNumUninitialized(NumFreeIDs);
			}
#endif
			else
			{
				//Drop in required size not great enough so just allocate same size.
				RequiredIDs = ExistingNumIDs;
			}

			IDToIndexTable[CurrBuffer].SetNumUninitialized(RequiredIDs);
			MaxUsedID = INDEX_NONE;//reset the max ID ready for it to be filled in during simulation.

			//UE_LOG(LogNiagara, Warning, TEXT("DataSetAllocate: NumInstances:%d | ID Table Size:%d | NumFreeIDs:%d | FreeTableSize:%d"), NumInstances, IDToIndexTable[CurrBuffer].Num(), NumFreeIDs, FreeIDsTable.Num());
		}
	}
	
	FORCEINLINE void Tick()
	{
#if NIAGARA_NAN_CHECKING
		CheckForNaNs();
#endif
		SwapBuffers();
	}
	
	FORCEINLINE void CopyCurToPrev()
	{
		CurrData().CopyTo(PrevData());
	}

	FORCEINLINE FNiagaraDataSetID GetID()const { return ID; }
	FORCEINLINE void SetID(FNiagaraDataSetID InID) { ID = InID; }

	FORCEINLINE int32 GetPrevBufferIdx() const
	{
		return CurrBuffer > 0 ? CurrBuffer - 1 : MaxBufferIdx;
	}

	FORCEINLINE int32 GetCurrBufferIdx() const
	{
		return CurrBuffer;
	}

	FORCEINLINE FNiagaraDataBuffer& GetDataByIndex(int32 InIdx)
	{
		check(InIdx < 3);
		//CheckCorrectThread();//TODO: We should be able to enable these checks and have well defined GT/RT ownership but GPU sims fire these a lot currently.
		return Data[InIdx];
	}

	FORCEINLINE FNiagaraDataBuffer& CurrData() 
	{
		//CheckCorrectThread();//TODO: We should be able to enable these checks and have well defined GT/RT ownership but GPU sims fire these a lot currently.
		return Data[CurrBuffer]; 
	}
	FORCEINLINE FNiagaraDataBuffer& PrevData() 
	{
		//CheckCorrectThread();//TODO: We should be able to enable these checks and have well defined GT/RT ownership but GPU sims fire these a lot currently.
		return Data[GetPrevBufferIdx()];
	}
	FORCEINLINE const FNiagaraDataBuffer& CurrData()const 
	{
		//CheckCorrectThread();//TODO: We should be able to enable these checks and have well defined GT/RT ownership but GPU sims fire these a lot currently.
		return Data[CurrBuffer];
	}
	FORCEINLINE const FNiagaraDataBuffer& PrevData()const 
	{
		//CheckCorrectThread();//TODO: We should be able to enable these checks and have well defined GT/RT ownership but GPU sims fire these a lot currently.
		return Data[GetPrevBufferIdx()];
	}

	FORCEINLINE uint32 GetNumInstances()const { return CurrData().GetNumInstances(); }
	FORCEINLINE uint32 GetNumInstancesAllocated()const { return CurrData().GetNumInstancesAllocated(); }
	FORCEINLINE void SetNumInstances(uint32 InNumInstances) { CurrData().SetNumInstances(InNumInstances); }

	void ResetCurrentBuffers()
	{
		SetNumInstances(0);
		if (bNeedsPersistentIDs)
		{
			CurrIDTable().Empty();
			FreeIDsTable.Empty();
			NumFreeIDs = 0;
			MaxUsedID = INDEX_NONE;
		}
	}

	FORCEINLINE TArray<int32>& CurrIDTable()
	{
		//check(IsInGameThread());
		return IDToIndexTable[CurrBuffer];
	}

	FORCEINLINE TArray<int32>& PrevIDTable()
	{
		//check(IsInGameThread());
		return IDToIndexTable[GetPrevBufferIdx()];
	}

	FORCEINLINE TArray<int32>& GetFreeIDTable()
	{
		return FreeIDsTable;
	}
	
	FORCEINLINE int32& GetNumFreeIDs()
	{
		return NumFreeIDs;
	}

	FORCEINLINE int32& GetMaxUsedID()
	{
		return MaxUsedID;
	}

	FORCEINLINE int32& GetIDAcquireTag()
	{
		return IDAcquireTag;
	}

	FORCEINLINE void SetIDAcquireTag(int32 InTag)
	{
		IDAcquireTag = InTag;
	}

	FORCEINLINE ENiagaraSimTarget GetSimTarget()const { return SimTarget; }

	FORCEINLINE void ResetBuffersInternal()
	{
		Data[0].Reset();
		Data[1].Reset();
		Data[2].Reset();

		FreeIDsTable.Reset();
		NumFreeIDs = 0;

		IDToIndexTable[0].Reset();
		IDToIndexTable[1].Reset();
		IDToIndexTable[2].Reset();
		MaxUsedID = INDEX_NONE;
	}

	FORCEINLINE void ResetBuffers()
	{
		if (SimTarget == ENiagaraSimTarget::CPUSim)
		{
			ResetBuffersInternal();
		}
		else
		{			
			FNiagaraDataSet* DataSet = this;
			ENQUEUE_RENDER_COMMAND(ResetBuffersRT)(
				[DataSet](FRHICommandListImmediate& RHICmdList)
				{
					DataSet->ResetBuffersInternal();
				}
			);
		}
	}

	FORCEINLINE uint32 GetPrevNumInstances()const { return PrevData().GetNumInstances(); }

	FORCEINLINE uint32 GetNumVariables()const { return Variables.Num(); }

	FORCEINLINE uint32 GetSizeBytes()const
	{
		return Data[0].GetSizeBytes() + Data[1].GetSizeBytes();
	}

	FORCEINLINE bool HasVariable(const FNiagaraVariable& Var)const
	{
		return Variables.Contains(Var);
	}

	FORCEINLINE const FNiagaraVariableLayoutInfo* GetVariableLayout(const FNiagaraVariable& Var)const
	{
		int32 VarLayoutIndex = Variables.IndexOfByKey(Var);
		return VarLayoutIndex != INDEX_NONE ? &VariableLayouts[VarLayoutIndex] : nullptr;
	}

	// get the float and int component offsets of a variable; if the variable doesn't exist, returns -1
	//
	FORCEINLINE const bool GetVariableComponentOffsets(const FNiagaraVariable& Var, int32 &FloatStart, int32 &IntStart) const
	{
		const FNiagaraVariableLayoutInfo *Info = GetVariableLayout(Var);
		if (Info)
		{
			FloatStart = Info->FloatComponentStart;
			IntStart = Info->Int32ComponentStart;
			return true;
		}

		FloatStart = -1;
		IntStart = -1;
		return false;
	}

	void Dump(bool bCurr, int32 StartIdx = 0, int32 NumInstances = INDEX_NONE)const;
	void Dump(FNiagaraDataSet& Other, bool bCurr, int32 StartIdx = 0, int32 NumInstances = INDEX_NONE)const;
	void DumpGPU(FNiagaraDataSet& Other, float* GPUReadBackFloat, int* GPUReadBackInt, int32 StartIdx = 0, int32 NumInstances = INDEX_NONE)const;
	FORCEINLINE const TArray<FNiagaraVariable> &GetVariables() { return Variables; }

	void CheckForNaNs()const
	{
		if (CurrData().CheckForNaNs())
		{
			Dump(true);
			ensureAlwaysMsgf(false, TEXT("NiagaraDataSet contains NaNs!"));
		}

		if (PrevData().CheckForNaNs())
		{
			Dump(false);
			ensureAlwaysMsgf(false, TEXT("NiagaraDataSet contains NaNs!"));
		}
	}

	// Data set index buffer management
	// these buffers hold the number of instances for the buffers; the first five uint32s are the DrawIndirect parameters for rendering of the main particle data set
	//
	FRWBuffer &GetCurDataSetIndices()		{	return GetDataSetIndices(CurrBuffer);			}
	FRWBuffer &GetPrevDataSetIndices()		{	return GetDataSetIndices(GetPrevBufferIdx());	}

	bool HasDatasetIndices(bool bCur = true) const
	{
		uint32 BufIdx = bCur == true ? CurrBuffer : GetPrevBufferIdx();
		CheckCorrectThread();
		return DataSetIndices[BufIdx].Buffer != nullptr;
	}

	const FRWBuffer &GetCurDataSetIndices() const	
	{
		CheckCorrectThread();
		return DataSetIndices[CurrBuffer];
	}

	const FRWBuffer &GetPrevDataSetIndices() const
	{
		CheckCorrectThread();
		return DataSetIndices[GetPrevBufferIdx()];
	}

	void SetupCurDatasetIndices()
	{
		if (DataSetIndices[CurrBuffer].Buffer != nullptr)
		{
			DataSetIndices[CurrBuffer].Release();
		}
		// Use BUF_KeepCPUAccessible here since some platforms will lock it for readonly (depending on the implementation of RHIEnqueueStagedRead) after GPU simulation.
		DataSetIndices[CurrBuffer].Initialize(sizeof(int32), 64 /*Context->NumDataSets*/, EPixelFormat::PF_R32_UINT, BUF_DrawIndirect | BUF_Static | BUF_KeepCPUAccessible);	// always allocate for up to 64 data sets
	}

	FORCEINLINE uint32 GetNumFloatComponents()const { return TotalFloatComponents; }
	FORCEINLINE uint32 GetNumInt32Components()const { return TotalInt32Components; }

private:
	FRWBuffer &GetDataSetIndices(uint32 BufIdx)
	{
		CheckCorrectThread();
		return DataSetIndices[BufIdx];
	}

	FORCEINLINE void SwapBuffers()
	{
		CheckCorrectThread();
		if (SimTarget == ENiagaraSimTarget::CPUSim)
		{
			MaxBufferIdx = 2;
			CurrBuffer = CurrBuffer < 2 ? CurrBuffer+1 : 0;
		}
		else
		{
			MaxBufferIdx = 1;
			CurrBuffer = CurrBuffer == 0 ? 1 : 0;
		}
	}

	FORCEINLINE void CheckCorrectThread()const
	{
		// In some rare occasions, the render thread might be null, like when offloading work to Lightmass 
		// The final GRenderingThread check keeps us from inadvertently failing when that happens.
#if DO_CHECK
		bool CPUSimOK = (SimTarget == ENiagaraSimTarget::CPUSim && !IsInRenderingThread());
		bool GPUSimOK = (SimTarget == ENiagaraSimTarget::GPUComputeSim && IsInRenderingThread());
		checkf(!GRenderingThread || CPUSimOK || GPUSimOK, TEXT("NiagaraDataSet function being called on incorrect thread."));
#endif
	}

	void BuildLayout()
	{
		VariableLayouts.Empty();
		TotalFloatComponents = 0;
		TotalInt32Components = 0;

		VariableLayouts.Reserve(Variables.Num());
		for (FNiagaraVariable& Var : Variables)
		{
			FNiagaraVariableLayoutInfo& VarInfo = VariableLayouts[VariableLayouts.AddDefaulted()];
			FNiagaraTypeLayoutInfo::GenerateLayoutInfo(VarInfo.LayoutInfo, Var.GetType().GetScriptStruct());
			VarInfo.FloatComponentStart = TotalFloatComponents;
			VarInfo.Int32ComponentStart = TotalInt32Components;
			TotalFloatComponents += VarInfo.GetNumFloatComponents();
			TotalInt32Components += VarInfo.GetNumInt32Components();
		}

		Data[0].Init(this);
		Data[1].Init(this);
		Data[2].Init(this);
	}
		
	/** Unique ID for this data set. Used to allow referencing from other emitters and Systems. */
	FNiagaraDataSetID ID;
	
	//////////////////////////////////////////////////////////////////////////
	//TODO: All this layout is known per emitter / system so doesn't need to be generated and stored for every dataset!
	/** Variables in the data set. */
	TArray<FNiagaraVariable> Variables;
	/** Data describing the layout of variable data. */
	TArray<FNiagaraVariableLayoutInfo> VariableLayouts;
	/** Total number of components of each type in the data set. */
	uint32 TotalFloatComponents;
	uint32 TotalInt32Components;
	//////////////////////////////////////////////////////////////////////////
	
	/** Index of current state data. */
	uint32 CurrBuffer;
	uint32 MaxBufferIdx;

	ENiagaraSimTarget SimTarget;

	/** Once finalized, the data layout etc is built and no more variables can be added. */
	uint32 bFinalized : 1;
	uint32 bNeedsPersistentIDs : 1;
	
	/** Table of IDs to real buffer indices. Multi buffered so we can access previous frame data. */
	TArray<int32> IDToIndexTable[3];

	/** Table of free IDs available to allocate next tick. */
	TArray<int32> FreeIDsTable;

	/** Number of free IDs in FreeIDTable. */
	int32 NumFreeIDs;

	/** Max ID seen in last execution. Allows us to shrink the IDTable. */
	int32 MaxUsedID;

	/** Tag to use when new IDs are acquired. Should be unique per tick. */
	int32 IDAcquireTag;

	FNiagaraDataBuffer Data[3];
	FRWBuffer DataSetIndices[3]; 
};

/**
General iterator for getting and setting data in and FNiagaraDataSet.
*/
struct FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessorBase()
		: DataSet(nullptr)
		, DataBuffer(nullptr)
		, VarLayout(nullptr)
	{}

	FNiagaraDataSetAccessorBase(FNiagaraDataSet* InDataSet, FNiagaraVariable InVar, bool bCurrBuffer = true)
		: DataSet(InDataSet)
		, DataBuffer(bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData())
	{
		VarLayout = DataSet->GetVariableLayout(InVar);
	}

	void Create(FNiagaraDataSet* InDataSet, FNiagaraVariable InVar)
	{
		DataSet = InDataSet;
		VarLayout = DataSet->GetVariableLayout(InVar);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
	}

	FORCEINLINE bool IsValid()const { return VarLayout != nullptr && DataBuffer && DataBuffer->GetNumInstances() > 0; }
protected:

	FNiagaraDataSet* DataSet;
	FNiagaraDataBuffer* DataBuffer;
	const FNiagaraVariableLayoutInfo* VarLayout;
};

template<typename T>
struct FNiagaraDataSetAccessor : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<T>() {}
	FNiagaraDataSetAccessor(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer=true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(T) == InVar.GetType().GetSize());
		checkf(false, TEXT("You must provide a fast runtime specialization for this type."));// Allow this slow generic version?
	}

	FORCEINLINE T operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE T GetSafe(int32 Index, T Default)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE T Get(int32 Index)const
	{
		T Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, T& OutValue)const
	{
		uint8* ValuePtr = (uint8*)&OutValue;

		for (uint32 CompIdx = 0; CompIdx < VarLayout->GetNumFloatComponents(); ++CompIdx)
		{
			uint32 CompBufferOffset = VarLayout->FloatComponentStart + CompIdx;
			float* Src = DataBuffer->GetInstancePtrFloat(CompBufferOffset, Index);
			float* Dst = (float*)(ValuePtr + VarLayout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
			*Dst = *Src;
		}

		for (uint32 CompIdx = 0; CompIdx < VarLayout->GetNumInt32Components(); ++CompIdx)
		{
			uint32 CompBufferOffset = VarLayout->FloatComponentStart + CompIdx;
			int32* Src = DataBuffer->GetInstancePtrInt32(CompBufferOffset, Index);
			int32* Dst = (int32*)(ValuePtr + VarLayout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
			*Dst = *Src;
		}
	}

	FORCEINLINE void Set(int32 Index, const T& InValue)
	{
		uint8* ValuePtr = (uint8*)&InValue;

		for (uint32 CompIdx = 0; CompIdx < VarLayout->GetNumFloatComponents(); ++CompIdx)
		{
			uint32 CompBufferOffset = VarLayout->FloatComponentStart + CompIdx;
			float* Dst = DataBuffer->GetInstancePtrFloat(CompBufferOffset, Index);
			float* Src = (float*)(ValuePtr + VarLayout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
			*Dst = *Src;
		}

		for (uint32 CompIdx = 0; CompIdx < VarLayout->GetNumInt32Components(); ++CompIdx)
		{
			uint32 CompBufferOffset = VarLayout->FloatComponentStart + CompIdx;
			int32* Dst = DataBuffer->GetInstancePtrInt32(CompBufferOffset, Index);
			int32* Src = (int32*)(ValuePtr + VarLayout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
			*Dst = *Src;
		}
	}
};

template<>
struct FNiagaraDataSetAccessor<FNiagaraBool> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FNiagaraBool>() {}
	FNiagaraDataSetAccessor<FNiagaraBool>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer = true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(FNiagaraBool) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout != nullptr)
		{
			Base = (int32*)DataBuffer->GetComponentPtrInt32(VarLayout->Int32ComponentStart);
		}
		else
		{
			Base = nullptr;
		}
	}

	FORCEINLINE FNiagaraBool operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FNiagaraBool Get(int32 Index)const
	{
		FNiagaraBool Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE FNiagaraBool GetSafe(int32 Index, bool Default = true)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE void Get(int32 Index, FNiagaraBool& OutValue)const
	{
		OutValue.SetRawValue(Base[Index]);
	}

	FORCEINLINE void Set(int32 Index, const FNiagaraBool& InValue)
	{
		Base[Index] = InValue.GetRawValue();
	}

	FORCEINLINE bool BaseIsValid() const { return Base != nullptr; }

private:

	int32* Base;
};

template<>
struct FNiagaraDataSetAccessor<int32> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<int32>() {}
	FNiagaraDataSetAccessor<int32>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer = true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(int32) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}
	
	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout != nullptr)
		{
			Base = (int32*)DataBuffer->GetComponentPtrInt32(VarLayout->Int32ComponentStart);
			if (DataBuffer->GetNumInstances() != 0)
			{
				check(Base != nullptr);
			}
		}
		else
		{
			Base = nullptr;
		}
	}

	FORCEINLINE int32 operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE int32 Get(int32 Index)const
	{
		int32 Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE int32 GetSafe(int32 Index, int32 Default = 0)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE void Get(int32 Index, int32& OutValue)const
	{
		check(Base != nullptr);
		OutValue = Base[Index];
	}

	FORCEINLINE void Set(int32 Index, const int32& InValue)
	{
		check(Base != nullptr);
		Base[Index] = InValue;
	}

	FORCEINLINE bool BaseIsValid() const { return Base != nullptr; }

private:

	int32* Base;
};

template<>
struct FNiagaraDataSetAccessor<float> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<float>() {}
	FNiagaraDataSetAccessor<float>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer=true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(float) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout != nullptr)
		{
			Base = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart);
		}
		else
		{
			Base = nullptr;
		}
	}

	FORCEINLINE float operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE float GetSafe(int32 Index, float Default = 0.0f)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE float Get(int32 Index)const
	{
		float Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, float& OutValue)const
	{
		OutValue = Base[Index];
	}

	FORCEINLINE void Set(int32 Index, const float& InValue)
	{
		Base[Index] = InValue;
	}
	FORCEINLINE bool BaseIsValid() const { return Base != nullptr; }

private:
	float* Base;
};

template<>
struct FNiagaraDataSetAccessor<FVector2D> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FVector2D>() {}
	FNiagaraDataSetAccessor<FVector2D>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer=true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(FVector2D) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout != nullptr)
		{
			XBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart);
			YBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 1);
		}
		else
		{
			XBase = nullptr;
			YBase = nullptr;
		}
	}

	FORCEINLINE FVector2D operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FVector2D GetSafe(int32 Index, FVector2D Default = FVector2D::ZeroVector)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE FVector2D Get(int32 Index)const
	{
		FVector2D Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, FVector2D& OutValue)const
	{
		OutValue.X = XBase[Index];
		OutValue.Y = YBase[Index];
	}

	FORCEINLINE void Set(int32 Index, const FVector2D& InValue)
	{
		XBase[Index] = InValue.X;
		YBase[Index] = InValue.Y;
	}	
	
	FORCEINLINE bool BaseIsValid() const { return XBase != nullptr && YBase != nullptr; }

private:

	float* XBase;
	float* YBase;
};

template<>
struct FNiagaraDataSetAccessor<FVector> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FVector>() {}
	FNiagaraDataSetAccessor<FVector>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer=true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(FVector) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout != nullptr)
		{
			XBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart);
			YBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 1);
			ZBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 2);
		}
		else
		{
			XBase = nullptr;
			YBase = nullptr;
			ZBase = nullptr;
		}
	}

	FORCEINLINE FVector operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FVector GetSafe(int32 Index, FVector Default = FVector::ZeroVector)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE FVector Get(int32 Index)const
	{
		FVector Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, FVector& OutValue)const
	{
		OutValue.X = XBase[Index];
		OutValue.Y = YBase[Index];
		OutValue.Z = ZBase[Index];
	}

	FORCEINLINE void Set(int32 Index, const FVector& InValue)
	{
		XBase[Index] = InValue.X;
		YBase[Index] = InValue.Y;
		ZBase[Index] = InValue.Z;
	}

	FORCEINLINE bool BaseIsValid() const { return XBase != nullptr && YBase != nullptr && ZBase != nullptr; }
private:

	float* XBase;
	float* YBase;
	float* ZBase;
};

template<>
struct FNiagaraDataSetAccessor<FVector4> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FVector4>() {}
	FNiagaraDataSetAccessor<FVector4>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer=true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(FVector4) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout)
		{
			XBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart);
			YBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 1);
			ZBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 2);
			WBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 3);
		}
		else
		{
			XBase = nullptr;
			YBase = nullptr;
			ZBase = nullptr;
			WBase = nullptr;
		}
	}

	FORCEINLINE FVector4 operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FVector4 GetSafe(int32 Index, const FVector4& Default = FVector4(0.0f,0.0f,0.0f,0.0f))const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE FVector4 Get(int32 Index)const
	{
		FVector4 Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, FVector4& OutValue)const
	{
		OutValue.X = XBase[Index];
		OutValue.Y = YBase[Index];
		OutValue.Z = ZBase[Index];
		OutValue.W = WBase[Index];
	}

	FORCEINLINE void Set(int32 Index, const FVector4& InValue)
	{
		XBase[Index] = InValue.X;
		YBase[Index] = InValue.Y;
		ZBase[Index] = InValue.Z;
		WBase[Index] = InValue.W;
	}

	FORCEINLINE bool BaseIsValid() const { return XBase != nullptr && YBase != nullptr && ZBase != nullptr && WBase != nullptr; }
private:

	float* XBase;
	float* YBase;
	float* ZBase;
	float* WBase;
};


template<>
struct FNiagaraDataSetAccessor<FQuat> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FQuat>() {}
	FNiagaraDataSetAccessor<FQuat>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer = true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(FQuat) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout)
		{
			XBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart);
			YBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 1);
			ZBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 2);
			WBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 3);
		}
		else
		{
			XBase = nullptr;
			YBase = nullptr;
			ZBase = nullptr;
			WBase = nullptr;
		}
	}

	FORCEINLINE FQuat operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FQuat GetSafe(int32 Index, const FQuat& Default = FQuat(0.0f, 0.0f, 0.0f, 1.0f))const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE FQuat Get(int32 Index)const
	{
		FQuat Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, FQuat& OutValue)const
	{
		OutValue.X = XBase[Index];
		OutValue.Y = YBase[Index];
		OutValue.Z = ZBase[Index];
		OutValue.W = WBase[Index];
	}

	FORCEINLINE void Set(int32 Index, const FQuat& InValue)
	{
		XBase[Index] = InValue.X;
		YBase[Index] = InValue.Y;
		ZBase[Index] = InValue.Z;
		WBase[Index] = InValue.W;
	}

	FORCEINLINE bool BaseIsValid() const { return XBase != nullptr && YBase != nullptr && ZBase != nullptr && WBase != nullptr; }
private:

	float* XBase;
	float* YBase;
	float* ZBase;
	float* WBase;
};

template<>
struct FNiagaraDataSetAccessor<FLinearColor> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FLinearColor>() {}
	FNiagaraDataSetAccessor<FLinearColor>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer=true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(FLinearColor) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout)
		{
			RBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart);
			GBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 1);
			BBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 2);
			ABase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 3);
		}
		else
		{
			RBase = nullptr;
			GBase = nullptr;
			BBase = nullptr;
			ABase = nullptr;
		}
	}

	FORCEINLINE FLinearColor operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FLinearColor GetSafe(int32 Index, FLinearColor Default = FLinearColor::White)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE FLinearColor Get(int32 Index)const
	{
		FLinearColor Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, FLinearColor& OutValue)const
	{
		OutValue.R = RBase[Index];
		OutValue.G = GBase[Index];
		OutValue.B = BBase[Index];
		OutValue.A = ABase[Index];
	}

	FORCEINLINE void Set(int32 Index, const FLinearColor& InValue)
	{
		RBase[Index] = InValue.R;
		GBase[Index] = InValue.G;
		BBase[Index] = InValue.B;
		ABase[Index] = InValue.A;
	}
	FORCEINLINE bool BaseIsValid() const { return RBase != nullptr && GBase != nullptr && BBase != nullptr && ABase != nullptr; }

private:

	float* RBase;
	float* GBase;
	float* BBase;
	float* ABase;
};

template<>
struct FNiagaraDataSetAccessor<FNiagaraSpawnInfo> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FNiagaraSpawnInfo>() {}
	FNiagaraDataSetAccessor<FNiagaraSpawnInfo>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer = true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(FNiagaraSpawnInfo) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout != nullptr)
		{
			CountBase = (int32*)DataBuffer->GetComponentPtrInt32(VarLayout->Int32ComponentStart);
			InterpStartDtBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart);
			IntervalDtBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 1);
			GroupBase = (int32*)DataBuffer->GetComponentPtrInt32(VarLayout->Int32ComponentStart + 1);
		}
		else
		{
			CountBase = nullptr;
			InterpStartDtBase = nullptr;
			IntervalDtBase = nullptr;
			GroupBase = nullptr;
		}
	}

	FORCEINLINE FNiagaraSpawnInfo operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FNiagaraSpawnInfo GetSafe(int32 Index, FNiagaraSpawnInfo Default = FNiagaraSpawnInfo())const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE FNiagaraSpawnInfo Get(int32 Index)const
	{
		FNiagaraSpawnInfo Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, FNiagaraSpawnInfo& OutValue)const
	{
		OutValue.Count = CountBase[Index];
		OutValue.InterpStartDt = InterpStartDtBase[Index];
		OutValue.IntervalDt = IntervalDtBase[Index];
		OutValue.SpawnGroup = GroupBase[Index];
	}

	FORCEINLINE void Set(int32 Index, const FNiagaraSpawnInfo& InValue)
	{
		CountBase[Index] = InValue.Count;
		InterpStartDtBase[Index] = InValue.InterpStartDt;
		IntervalDtBase[Index] = InValue.IntervalDt;
		GroupBase[Index] = InValue.SpawnGroup;
	}


	FORCEINLINE bool BaseIsValid() const { return CountBase != nullptr && InterpStartDtBase != nullptr && IntervalDtBase != nullptr && GroupBase != nullptr; }

private:

	int32* CountBase;
	float* InterpStartDtBase;
	float* IntervalDtBase;
	int32* GroupBase;
};

template<>
struct FNiagaraDataSetAccessor<FNiagaraID> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FNiagaraID>() {}
	FNiagaraDataSetAccessor<FNiagaraID>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer = true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout != nullptr)
		{
			IndexBase = (int32*)DataBuffer->GetComponentPtrInt32(VarLayout->Int32ComponentStart);
			TagBase = (int32*)DataBuffer->GetComponentPtrInt32(VarLayout->Int32ComponentStart + 1);
		}
		else
		{
			IndexBase = nullptr;
			TagBase = nullptr;
		}
	}

	FORCEINLINE FNiagaraID operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FNiagaraID GetSafe(int32 Index, FNiagaraID Default = FNiagaraID())const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE FNiagaraID Get(int32 Index)const
	{
		FNiagaraID Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, FNiagaraID& OutValue)const
	{
		OutValue.Index = IndexBase[Index];
		OutValue.AcquireTag = TagBase[Index];
	}

	FORCEINLINE void Set(int32 Index, const FNiagaraID& InValue)
	{
		IndexBase[Index] = InValue.Index;
		TagBase[Index] = InValue.AcquireTag;
	}

	FORCEINLINE bool BaseIsValid() const { return IndexBase != nullptr && TagBase != nullptr; }

private:

	int32* IndexBase;
	int32* TagBase;
};

//Provide iterator to keep iterator access patterns still in use.
template<typename T>
struct FNiagaraDataSetIterator : public FNiagaraDataSetAccessor<T>
{
	FNiagaraDataSetIterator<T>() {}
	FNiagaraDataSetIterator(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, uint32 StartIndex = 0, bool bCurrBuffer = true)
		: FNiagaraDataSetAccessor<T>(InDataSet, InVar, bCurrBuffer)
		, CurrIdx(StartIndex)
	{}

	void Create(FNiagaraDataSet* InDataSet, FNiagaraVariable InVar, uint32 StartIndex=0)
	{
		FNiagaraDataSetAccessor<T>::Create(InDataSet, InVar);
		CurrIdx = StartIndex;
	}

	FORCEINLINE T operator*() { return Get(); }
	FORCEINLINE T Get()const
	{
		T Ret;
		Get(Ret);
		return Ret;
	}

	FORCEINLINE T GetAdvance()
	{
		T Ret;
		Get(Ret);
		Advance();
		return Ret;
	}

	FORCEINLINE T GetAdvanceWithDefault(const T& Default)
	{
		T Ret = IsValid() ? Get() : Default;
		Advance();
		return Ret;
	}

	FORCEINLINE void Get(T& OutValue)const { FNiagaraDataSetAccessor<T>::Get(CurrIdx, OutValue); }

	FORCEINLINE void Set(const T& InValue)
	{
		FNiagaraDataSetAccessor<T>::Set(CurrIdx, InValue);
	}

	FORCEINLINE void Advance() { ++CurrIdx; }
	FORCEINLINE bool IsValid()const
	{
		return FNiagaraDataSetAccessorBase::VarLayout != nullptr && CurrIdx < FNiagaraDataSetAccessorBase::DataBuffer->GetNumInstances();
	}

	uint32 GetCurrIndex()const { return CurrIdx; }

private:
	uint32 CurrIdx;
};

/**
Iterator that will pull or push data between a DataSet and some FNiagaraVariables it contains.
Super slow. Don't use at runtime.
*/
struct FNiagaraDataSetVariableIterator
{
	FNiagaraDataSetVariableIterator(FNiagaraDataSet& InDataSet, uint32 StartIdx = 0, bool bCurrBuffer = true)
		: DataSet(InDataSet)
		, DataBuffer(bCurrBuffer ? DataSet.CurrData() : DataSet.PrevData())
		, CurrIdx(StartIdx)
	{
	}

	void Get()
	{
		for (int32 VarIdx = 0; VarIdx < Variables.Num(); ++VarIdx)
		{
			FNiagaraVariable* Var = Variables[VarIdx];
			const FNiagaraVariableLayoutInfo* Layout = VarLayouts[VarIdx];
			check(Var && Layout);
			uint8* ValuePtr = Var->GetData();

			for (uint32 CompIdx = 0; CompIdx < Layout->GetNumFloatComponents(); ++CompIdx)
			{
				uint32 CompBufferOffset = Layout->FloatComponentStart + CompIdx;
				float* Src = DataBuffer.GetInstancePtrFloat(CompBufferOffset, CurrIdx);
				float* Dst = (float*)(ValuePtr + Layout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
				*Dst = *Src;
			}

			for (uint32 CompIdx = 0; CompIdx < Layout->GetNumInt32Components(); ++CompIdx)
			{
				uint32 CompBufferOffset = Layout->Int32ComponentStart + CompIdx;
				int32* Src = DataBuffer.GetInstancePtrInt32(CompBufferOffset, CurrIdx);
				int32* Dst = (int32*)(ValuePtr + Layout->LayoutInfo.Int32ComponentByteOffsets[CompIdx]);
				*Dst = *Src;
			}
		}
	}

	void Set()
	{
		for (int32 VarIdx = 0; VarIdx < Variables.Num(); ++VarIdx)
		{
			FNiagaraVariable* Var = Variables[VarIdx];
			const FNiagaraVariableLayoutInfo* Layout = VarLayouts[VarIdx];

			check(Var && Layout);
			uint8* ValuePtr = Var->GetData();

			for (uint32 CompIdx = 0; CompIdx < Layout->GetNumFloatComponents(); ++CompIdx)
			{
				uint32 CompBufferOffset = Layout->FloatComponentStart + CompIdx;
				float* Dst = DataBuffer.GetInstancePtrFloat(CompBufferOffset, CurrIdx);
				float* Src = (float*)(ValuePtr + Layout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
				*Dst = *Src;
			}

			for (uint32 CompIdx = 0; CompIdx < Layout->GetNumInt32Components(); ++CompIdx)
			{
				uint32 CompBufferOffset = Layout->FloatComponentStart + CompIdx;
				int32* Dst = DataBuffer.GetInstancePtrInt32(CompBufferOffset, CurrIdx);
				int32* Src = (int32*)(ValuePtr + Layout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
				*Dst = *Src;
			}
		}
	}

	void Advance() { ++CurrIdx; }

	bool IsValid()const
	{
		return CurrIdx < DataBuffer.GetNumInstances();
	}

	uint32 GetCurrIndex()const { return CurrIdx; }

	void AddVariable(FNiagaraVariable* InVar)
	{
		check(InVar);
		Variables.AddUnique(InVar);
		VarLayouts.AddUnique(DataSet.GetVariableLayout(*InVar));
		InVar->AllocateData();
	}

	void AddVariables(TArray<FNiagaraVariable>& Vars)
	{
		for (FNiagaraVariable& Var : Vars)
		{
			AddVariable(&Var);
		}
	}
private:

	FNiagaraDataSet& DataSet;
	FNiagaraDataBuffer& DataBuffer;
	TArray<FNiagaraVariable*> Variables;
	TArray<const FNiagaraVariableLayoutInfo*> VarLayouts;

	uint32 CurrIdx;
};

/**
Iterator that will pull or push data between a DataSet and some FNiagaraVariables it contains.
Super slow. Don't use at runtime.
*/
struct FNiagaraDataSetVariableIteratorConst
{
	FNiagaraDataSetVariableIteratorConst(const FNiagaraDataSet& InDataSet, uint32 StartIdx = 0, bool bCurrBuffer = true)
		: DataSet(InDataSet)
		, DataBuffer(bCurrBuffer ? DataSet.CurrData() : DataSet.PrevData())
		, CurrIdx(StartIdx)
	{
	}

	void Get()
	{
		for (int32 VarIdx = 0; VarIdx < Variables.Num(); ++VarIdx)
		{
			FNiagaraVariable* Var = Variables[VarIdx];
			const FNiagaraVariableLayoutInfo* Layout = VarLayouts[VarIdx];
			check(Var && Layout);
			uint8* ValuePtr = Var->GetData();

			for (uint32 CompIdx = 0; CompIdx < Layout->GetNumFloatComponents(); ++CompIdx)
			{
				uint32 CompBufferOffset = Layout->FloatComponentStart + CompIdx;
				float* Src = DataBuffer.GetInstancePtrFloat(CompBufferOffset, CurrIdx);
				float* Dst = (float*)(ValuePtr + Layout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
				*Dst = *Src;
			}

			for (uint32 CompIdx = 0; CompIdx < Layout->GetNumInt32Components(); ++CompIdx)
			{
				uint32 CompBufferOffset = Layout->Int32ComponentStart + CompIdx;
				int32* Src = DataBuffer.GetInstancePtrInt32(CompBufferOffset, CurrIdx);
				int32* Dst = (int32*)(ValuePtr + Layout->LayoutInfo.Int32ComponentByteOffsets[CompIdx]);
				*Dst = *Src;
			}
		}
	}
	
	void Advance() { ++CurrIdx; }

	bool IsValid()const
	{
		return CurrIdx < DataBuffer.GetNumInstances();
	}

	uint32 GetCurrIndex()const { return CurrIdx; }

	void AddVariable(FNiagaraVariable* InVar)
	{
		check(InVar);
		Variables.AddUnique(InVar);
		VarLayouts.AddUnique(DataSet.GetVariableLayout(*InVar));
		InVar->AllocateData();
	}

	void AddVariables(TArray<FNiagaraVariable>& Vars)
	{
		for (FNiagaraVariable& Var : Vars)
		{
			AddVariable(&Var);
		}
	}
private:

	const FNiagaraDataSet & DataSet;
	const FNiagaraDataBuffer& DataBuffer;
	TArray<FNiagaraVariable*> Variables;
	TArray<const FNiagaraVariableLayoutInfo*> VarLayouts;

	uint32 CurrIdx;
};