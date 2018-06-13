// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataSet.h"
#include "NiagaraCommon.h"
#include "NiagaraShader.h"
#include "GlobalShader.h"
#include "UpdateTextureShaders.h"
#include "ShaderParameterUtils.h"
#include "NiagaraStats.h"
#include "NiagaraRenderer.h"

DECLARE_CYCLE_STAT(TEXT("InitRenderData"), STAT_InitRenderData, STATGROUP_Niagara);


//////////////////////////////////////////////////////////////////////////
void FNiagaraDataSet::SetShaderParams(FNiagaraShader *Shader, FRHICommandList &CommandList)
{
	check(IsInRenderingThread());

	if (Shader->FloatInputBufferParam.IsBound())
	{
		CommandList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, PrevData().GetGPUBufferFloat()->UAV);
		if (PrevData().GetNumInstancesAllocated() > 0)
		{
			CommandList.SetShaderResourceViewParameter(Shader->GetComputeShader(), Shader->FloatInputBufferParam.GetBaseIndex(), PrevData().GetGPUBufferFloat()->SRV);
		}
		else
		{
			CommandList.SetShaderResourceViewParameter(Shader->GetComputeShader(), Shader->FloatInputBufferParam.GetBaseIndex(), NiagaraRenderer::GetDummyFloatBuffer().SRV);
		}
	}
	if (Shader->IntInputBufferParam.IsBound())
	{
		CommandList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, PrevData().GetGPUBufferInt()->UAV);
		if (PrevData().GetNumInstancesAllocated() > 0)
		{
			CommandList.SetShaderResourceViewParameter(Shader->GetComputeShader(), Shader->IntInputBufferParam.GetBaseIndex(), PrevData().GetGPUBufferInt()->SRV);
		}
		else
		{
			CommandList.SetShaderResourceViewParameter(Shader->GetComputeShader(), Shader->IntInputBufferParam.GetBaseIndex(), NiagaraRenderer::GetDummyIntBuffer().SRV);
		}
	}
	if (Shader->FloatOutputBufferParam.IsUAVBound())
	{
		CommandList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, CurrData().GetGPUBufferFloat()->UAV);
		CommandList.SetUAVParameter(Shader->GetComputeShader(), Shader->FloatOutputBufferParam.GetUAVIndex(), CurrData().GetGPUBufferFloat()->UAV);
	}
	if (Shader->IntOutputBufferParam.IsUAVBound())
	{
		CommandList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, CurrData().GetGPUBufferInt()->UAV);
		CommandList.SetUAVParameter(Shader->GetComputeShader(), Shader->IntOutputBufferParam.GetUAVIndex(), CurrData().GetGPUBufferInt()->UAV);
	}

	if (Shader->ComponentBufferSizeWriteParam.IsBound())
	{
		uint32 SafeBufferSize = CurrData().GetFloatStride() / sizeof(float);
		CommandList.SetShaderParameter(Shader->GetComputeShader(), Shader->ComponentBufferSizeWriteParam.GetBufferIndex(), Shader->ComponentBufferSizeWriteParam.GetBaseIndex(), Shader->ComponentBufferSizeWriteParam.GetNumBytes(), &SafeBufferSize);
	}

	if (Shader->ComponentBufferSizeReadParam.IsBound())
	{
		uint32 SafeBufferSize = PrevData().GetFloatStride() / sizeof(float);
		CommandList.SetShaderParameter(Shader->GetComputeShader(), Shader->ComponentBufferSizeReadParam.GetBufferIndex(), Shader->ComponentBufferSizeReadParam.GetBaseIndex(), Shader->ComponentBufferSizeReadParam.GetNumBytes(), &SafeBufferSize);
	}
}



void FNiagaraDataSet::UnsetShaderParams(FNiagaraShader *Shader, FRHICommandList &RHICmdList)
{
	check(IsInRenderingThread());

	if (Shader->FloatOutputBufferParam.IsUAVBound())
	{
#if !PLATFORM_PS4
		Shader->FloatOutputBufferParam.UnsetUAV(RHICmdList, Shader->GetComputeShader());
#endif
		//RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, CurrDataRender().GetGPUBufferFloat()->UAV);
	}

	if (Shader->IntOutputBufferParam.IsUAVBound())
	{
#if !PLATFORM_PS4
		Shader->IntOutputBufferParam.UnsetUAV(RHICmdList, Shader->GetComputeShader());
#endif
		//RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, CurrDataRender().GetGPUBufferInt()->UAV);
	}
}

//////////////////////////////////////////////////////////////////////////
static int32 GRenderDataBlockSize = 4096;
static FAutoConsoleVariableRef CVarRenderDataBlockSize(
	TEXT("fx.RenderDataBlockSize"),
	GRenderDataBlockSize,
	TEXT("Size of alloaction blocks for Niagara render data. \n"),
	ECVF_Default
);

static float GGPUBufferShrinkFactor = .5f;
static FAutoConsoleVariableRef CVarNiagaraRenderBufferShrinkFactor(
	TEXT("fx.RenderBufferShrinkFactor"),
	GGPUBufferShrinkFactor,
	TEXT("What factor should the render buffers grow by when they need to grow. \n"),
	ECVF_Default
);

FNiagaraDataSet::~FNiagaraDataSet()
{
// 	int32 CurrBytes = RenderDataFloat.NumBytes + RenderDataInt.NumBytes;
// 	DEC_MEMORY_STAT_BY(STAT_NiagaraVBMemory, CurrBytes);

}

void FNiagaraDataSet::Dump(FNiagaraDataSet& Other, bool bCurr, int32 StartIdx , int32 NumInstances)const
{
	Other.Reset();
	Other.Variables = Variables;
	Other.VariableLayouts = VariableLayouts;

	const FNiagaraDataBuffer& DataBuffer = bCurr ? CurrData() : PrevData();
	FNiagaraDataBuffer& OtherDataBuffer = Other.CurrData();

	
	if (OtherDataBuffer.GetNumInstancesAllocated() != DataBuffer.GetNumInstancesAllocated())
	{
		Other.Finalize();
		OtherDataBuffer.Allocate(DataBuffer.GetNumInstancesAllocated());
	}

	DataBuffer.CopyTo(OtherDataBuffer, StartIdx, NumInstances);
}


void FNiagaraDataSet::Dump(bool bCurr, int32 StartIdx, int32 NumInstances)const
{
	TArray<FNiagaraVariable> Vars(Variables);

	FNiagaraDataSetVariableIteratorConst Itr(*this, StartIdx, bCurr);
	Itr.AddVariables(Vars);

	if (NumInstances == INDEX_NONE)
	{
		NumInstances = bCurr ? GetNumInstances() : GetPrevNumInstances();
		NumInstances -= StartIdx;
	}

	int32 NumInstancesDumped = 0;
	TArray<FString> Lines;
	Lines.Reserve(GetNumInstances());
	while (Itr.IsValid() && NumInstancesDumped < NumInstances)
	{
		Itr.Get();

		FString Line = TEXT("| ");
		for (FNiagaraVariable& Var : Vars)
		{
			Line += Var.ToString() + TEXT(" | ");
		}
		Lines.Add(Line);
		Itr.Advance();
		NumInstancesDumped++;
	}

	static FString Sep;
	if (Sep.Len() == 0)
	{
		for (int32 i = 0; i < 50; ++i)
		{
			Sep.AppendChar(TEXT('='));
		}
	}

	UE_LOG(LogNiagara, Log, TEXT("%s"), *Sep);
	UE_LOG(LogNiagara, Log, TEXT(" Buffer: %d"), CurrBuffer);
 	UE_LOG(LogNiagara, Log, TEXT("%s"), *Sep);
// 	UE_LOG(LogNiagara, Log, TEXT("%s"), *HeaderStr);
// 	UE_LOG(LogNiagara, Log, TEXT("%s"), *Sep);
	for (FString& Str : Lines)
	{
		UE_LOG(LogNiagara, Log, TEXT("%s"), *Str);
	}
	UE_LOG(LogNiagara, Log, TEXT("%s"), *Sep);
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraDataBuffer::Init(FNiagaraDataSet* InOwner)
{
	Owner = InOwner;
}

FNiagaraDataBuffer::~FNiagaraDataBuffer()
{
	Reset();
}

void FNiagaraDataBuffer::Reset()
{
	DEC_MEMORY_STAT_BY(STAT_NiagaraParticleMemory, FloatData.Max() + Int32Data.Max());
	FloatData.Empty();
	Int32Data.Empty();
	FloatStride = 0;
	Int32Stride = 0;
	NumInstances = 0;
	NumInstancesAllocated = 0;
	NumChunksAllocatedForGPU = 0;
}

int32 FNiagaraDataBuffer::TransferInstance(FNiagaraDataBuffer& SourceBuffer, int32 InstanceIndex)
{
	if (SourceBuffer.GetNumInstances() > (uint32)InstanceIndex)
	{
		int32 OldNumInstances = NumInstances;
		if (NumInstances == NumInstancesAllocated)
		{
			//Have to allocate some more space.
			Allocate(NumInstancesAllocated + 1, true);
		}

		SetNumInstances(OldNumInstances + 1);

		/** Copy the instance data. */
		for (int32 CompIdx = (int32)Owner->TotalFloatComponents - 1; CompIdx >= 0; --CompIdx)
		{
			float* Src = SourceBuffer.GetInstancePtrFloat(CompIdx, InstanceIndex);
			float* Dst = GetInstancePtrFloat(CompIdx, OldNumInstances);
			*Dst = *Src;
		}
		for (int32 CompIdx = (int32)Owner->TotalInt32Components - 1; CompIdx >= 0; --CompIdx)
		{
			int32* Src = SourceBuffer.GetInstancePtrInt32(CompIdx, InstanceIndex);
			int32* Dst = GetInstancePtrInt32(CompIdx, OldNumInstances);
			*Dst = *Src;
		}

		return OldNumInstances;
	}

	return INDEX_NONE;
}

bool FNiagaraDataBuffer::CheckForNaNs()const
{
	bool bContainsNaNs = false;
	int32 NumFloatComponents = Owner->GetNumFloatComponents();
	for (int32 CompIdx = 0; CompIdx < NumFloatComponents && !bContainsNaNs; ++CompIdx)
	{
		for (int32 InstIdx = 0; InstIdx < (int32)NumInstances && !bContainsNaNs; ++InstIdx)
		{
			float Val = *GetInstancePtrFloat(CompIdx, InstIdx);
			bContainsNaNs = FMath::IsNaN(Val) || !FMath::IsFinite(Val);
		}
	}

	return bContainsNaNs;
}

void FNiagaraDataBuffer::Allocate(uint32 InNumInstances, bool bMaintainExisting)
{
	check(Owner);
	if (Owner->SimTarget == ENiagaraSimTarget::CPUSim)
	{
		NumInstancesAllocated = InNumInstances;
		NumInstances = 0;

		DEC_MEMORY_STAT_BY(STAT_NiagaraParticleMemory, FloatData.Max() + Int32Data.Max());

		int32 OldFloatStride = FloatStride;
		TArray<uint8> OldFloatData;
		int32 OldInt32Stride = Int32Stride;
		TArray<uint8> OldIntData;

		if (bMaintainExisting)
		{
			//Need to copy off old data so we can copy it back into the newly laid out buffers. TODO: Avoid this needless copying.
			OldFloatData = FloatData;
			OldIntData = Int32Data;
		}

		FloatStride = GetSafeComponentBufferSize(NumInstancesAllocated * sizeof(float));
		FloatData.SetNum(FloatStride * Owner->GetNumFloatComponents(), false);

		Int32Stride = GetSafeComponentBufferSize(NumInstancesAllocated * sizeof(int32));
		Int32Data.SetNum(Int32Stride * Owner->GetNumInt32Components(), false);

		INC_MEMORY_STAT_BY(STAT_NiagaraParticleMemory, FloatData.Max() + Int32Data.Max());

		//In some cases we want the existing data in the buffer to be maintained which due to the data layout requires some fix up.
		if (bMaintainExisting)
		{
			if (FloatStride != OldFloatStride && FloatStride > 0 && OldFloatStride > 0)
			{
				for (int32 CompIdx = (int32)Owner->TotalFloatComponents-1; CompIdx >= 0; --CompIdx)
				{
					uint8* Src = OldFloatData.GetData() + OldFloatStride * CompIdx;
					uint8* Dst = FloatData.GetData() + FloatStride * CompIdx;
					FMemory::Memcpy(Dst, Src, OldFloatStride);
				}
			}
			if (Int32Stride != OldInt32Stride && Int32Stride > 0 && OldInt32Stride > 0)
			{
				for (int32 CompIdx = (int32)Owner->TotalInt32Components - 1; CompIdx >= 0; --CompIdx)
				{
					uint8* Src = OldIntData.GetData() + OldInt32Stride * CompIdx;
					uint8* Dst = Int32Data.GetData() + Int32Stride * CompIdx;
					FMemory::Memcpy(Dst, Src, OldInt32Stride);
				}
			}
		}
	}
}


void FNiagaraDataBuffer::AllocateGPU(uint32 InNumInstances, FRHICommandList &RHICmdList)
{
	if (Owner == 0)
	{
		return;
	}
	check(IsInRenderingThread());
	const uint32 ALLOC_CHUNKSIZE = 4096;

	NumInstancesAllocated = InNumInstances;

	uint32 PaddedNumInstances = ((InNumInstances + NIAGARA_COMPUTE_THREADGROUP_SIZE -1) / NIAGARA_COMPUTE_THREADGROUP_SIZE) * NIAGARA_COMPUTE_THREADGROUP_SIZE;
	FloatStride = PaddedNumInstances * sizeof(float);
	Int32Stride = PaddedNumInstances * sizeof(int32);

	if (NumInstancesAllocated > NumChunksAllocatedForGPU * ALLOC_CHUNKSIZE)
	{
		uint32 PrevChunks = NumChunksAllocatedForGPU;
		NumChunksAllocatedForGPU = ((InNumInstances + ALLOC_CHUNKSIZE - 1) / ALLOC_CHUNKSIZE);
		uint32 NumElementsToAlloc = NumChunksAllocatedForGPU * ALLOC_CHUNKSIZE;

		if (NumElementsToAlloc == 0)
		{
			return;
		}

		if (Owner->GetNumFloatComponents())
		{
			if (GPUBufferFloat.Buffer)
			{
				GPUBufferFloat.Release();
			}
			GPUBufferFloat.Initialize(sizeof(float), NumElementsToAlloc * Owner->GetNumFloatComponents(), EPixelFormat::PF_R32_FLOAT, BUF_Static);
		}
		if (Owner->GetNumInt32Components())
		{
			if (GPUBufferInt.Buffer)
			{
				GPUBufferInt.Release();
			}
			GPUBufferInt.Initialize(sizeof(int32), NumElementsToAlloc * Owner->GetNumInt32Components(), EPixelFormat::PF_R32_SINT, BUF_Static);
		}
	}
}

void FNiagaraDataBuffer::SwapInstances(uint32 OldIndex, uint32 NewIndex) 
{
	for (uint32 CompIdx = 0; CompIdx < Owner->TotalFloatComponents; ++CompIdx)
	{
		float* Src = GetInstancePtrFloat(CompIdx, OldIndex);
		float* Dst = GetInstancePtrFloat(CompIdx, NewIndex);
		float Temp = *Dst;
		*Dst = *Src;
		*Src = Temp;
	}
	for (uint32 CompIdx = 0; CompIdx < Owner->TotalInt32Components; ++CompIdx)
	{
		int32* Src = GetInstancePtrInt32(CompIdx, OldIndex);
		int32* Dst = GetInstancePtrInt32(CompIdx, NewIndex);
		int32 Temp = *Dst;
		*Dst = *Src;
		*Src = Temp;
	}
}

void FNiagaraDataBuffer::KillInstance(uint32 InstanceIdx)
{
	check(InstanceIdx < NumInstances);
	--NumInstances;

	for (uint32 CompIdx = 0; CompIdx < Owner->TotalFloatComponents; ++CompIdx)
	{
		float* Src = GetInstancePtrFloat(CompIdx, NumInstances);
		float* Dst = GetInstancePtrFloat(CompIdx, InstanceIdx);
		*Dst = *Src;
	}
	for (uint32 CompIdx = 0; CompIdx < Owner->TotalInt32Components; ++CompIdx)
	{
		int32* Src = GetInstancePtrInt32(CompIdx, NumInstances);
		int32* Dst = GetInstancePtrInt32(CompIdx, InstanceIdx);
		*Dst = *Src;
	}

#if NIAGARA_NAN_CHECKING
	CheckForNaNs();
#endif
}

void FNiagaraDataBuffer::CopyTo(FNiagaraDataBuffer& DestBuffer, int32 InStartIdx, int32 InNumInstances)const
{
	if (InStartIdx < 0 || (uint32)InStartIdx > NumInstances)
	{
		InStartIdx = NumInstances;
	}
	if (InNumInstances < 0 || ((uint32)InNumInstances + (uint32)InStartIdx) > NumInstances)
	{
		InNumInstances = NumInstances - InStartIdx;
	}

	if (InNumInstances != 0)
	{
		if (DestBuffer.NumInstancesAllocated != NumInstancesAllocated)
		{
			DestBuffer.Allocate(NumInstancesAllocated);
		}

		for (uint32 CompIdx = 0; CompIdx < Owner->TotalFloatComponents; ++CompIdx)
		{
			const float* SrcStart = GetInstancePtrFloat(CompIdx, InStartIdx);
			const float* SrcEnd = GetInstancePtrFloat(CompIdx, InStartIdx + InNumInstances);
			float* Dst = DestBuffer.GetInstancePtrFloat(CompIdx, 0);
			size_t Count = SrcEnd - SrcStart;
			FMemory::Memcpy(Dst, SrcStart, Count*sizeof(float));

			if (Count > 0)
			{
				for (size_t i = 0; i < Count; i++)
				{
					check(SrcStart[i] == Dst[i]);
				}
			}
		}
		for (uint32 CompIdx = 0; CompIdx < Owner->TotalInt32Components; ++CompIdx)
		{
			const int32* SrcStart = GetInstancePtrInt32(CompIdx, InStartIdx);
			const int32* SrcEnd = GetInstancePtrInt32(CompIdx, InStartIdx + InNumInstances);
			int32* Dst = DestBuffer.GetInstancePtrInt32(CompIdx, 0);
			size_t Count = SrcEnd - SrcStart;
			FMemory::Memcpy(Dst, SrcStart, Count * sizeof(int32));

			if (Count > 0)
			{
				for (size_t i = 0; i < Count; i++)
				{
					check(SrcStart[i] == Dst[i]);
				}
			}
		}
		DestBuffer.SetNumInstances(InNumInstances);
	}
}

void FNiagaraDataBuffer::CopyTo(FNiagaraDataBuffer& DestBuffer)const
{
	DestBuffer.FloatStride = FloatStride;
	DestBuffer.FloatData = FloatData;
	DestBuffer.Int32Stride = Int32Stride;
	DestBuffer.Int32Data = Int32Data;
	DestBuffer.NumInstancesAllocated = NumInstancesAllocated;
	DestBuffer.NumInstances = NumInstances;
}
