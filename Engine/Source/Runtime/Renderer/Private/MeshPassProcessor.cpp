// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshPassProcessor.cpp: 
=============================================================================*/

#include "MeshPassProcessor.h"
#include "SceneUtils.h"
#include "SceneRendering.h"
#include "Logging/LogMacros.h"
#include "SceneCore.h"
#include "ScenePrivate.h"
#include "SceneInterface.h"
#include "MeshPassProcessor.inl"
#include "PipelineStateCache.h"

TSet<FRefCountedGraphicsMinimalPipelineStateInitializer, RefCountedGraphicsMinimalPipelineStateInitializerKeyFuncs> FGraphicsMinimalPipelineStateId::PersistentIdTable;
TSet<FGraphicsMinimalPipelineStateInitializer> FGraphicsMinimalPipelineStateId::OneFrameIdTable;
FCriticalSection FGraphicsMinimalPipelineStateId::OneFrameIdTableCriticalSection;
FRWLock OneFrameIdTableHotfixRWLock;

const FMeshDrawCommandSortKey FMeshDrawCommandSortKey::Default = { {0} };

int32 GEmitMeshDrawEvent = 0;
static FAutoConsoleVariableRef CVarEmitMeshDrawEvent(
	TEXT("r.EmitMeshDrawEvents"),
	GEmitMeshDrawEvent,
	TEXT("Emits a GPU event around each drawing policy draw call.  /n")
	TEXT("Useful for seeing stats about each draw call, however it greatly distorts total time and time per draw call."),
	ECVF_RenderThreadSafe
);

enum { MAX_SRVs_PER_SHADER_STAGE = 128 };
enum { MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE = 14 };
enum { MAX_SAMPLERS_PER_SHADER_STAGE = 32 };

class FShaderBindingState
{
public:
	int32 MaxSRVUsed = -1;
	FShaderResourceViewRHIParamRef SRVs[MAX_SRVs_PER_SHADER_STAGE] = {};
	int32 MaxUniformBufferUsed = -1;
	FUniformBufferRHIParamRef UniformBuffers[MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE] = {};
	int32 MaxTextureUsed = -1;
	FTextureRHIParamRef Textures[MAX_SRVs_PER_SHADER_STAGE] = {};
	int32 MaxSamplerUsed = -1;
	FSamplerStateRHIParamRef Samplers[MAX_SAMPLERS_PER_SHADER_STAGE] = {};
};

class FReadOnlyMeshDrawSingleShaderBindings : public FMeshDrawShaderBindingsLayout
{
public:
	FReadOnlyMeshDrawSingleShaderBindings(const FMeshDrawShaderBindingsLayout& InLayout, const uint8* InData) :
		FMeshDrawShaderBindingsLayout(InLayout)
	{
		Data = InData;
	}

	inline const FUniformBufferRHIParamRef* GetUniformBufferStart() const
	{
		return (const FUniformBufferRHIParamRef*)(Data + GetUniformBufferOffset());
	}

	inline const FSamplerStateRHIParamRef* GetSamplerStart() const
	{
		const uint8* SamplerDataStart = Data + GetSamplerOffset();
		return (const FSamplerStateRHIParamRef*)SamplerDataStart;
	}

	inline const FRHIResource** GetSRVStart() const
	{
		const uint8* SRVDataStart = Data + GetSRVOffset();
		return (const FRHIResource**)SRVDataStart;
	}

	inline const uint8* GetSRVTypeStart() const
	{
		const uint8* SRVTypeDataStart = Data + GetSRVTypeOffset();
		return SRVTypeDataStart;
	}

	inline const uint8* GetLooseDataStart() const
	{
		const uint8* LooseDataStart = Data + GetLooseDataOffset();
		return LooseDataStart;
	}

private:
	const uint8* Data;
};

template<class RHIShaderType>
void FMeshDrawShaderBindings::SetShaderBindings(
	FRHICommandList& RHICmdList,
	RHIShaderType Shader,
	const FReadOnlyMeshDrawSingleShaderBindings& RESTRICT SingleShaderBindings,
	FShaderBindingState& RESTRICT ShaderBindingState)
{
	const FUniformBufferRHIParamRef* RESTRICT UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
	const FShaderParameterInfo* RESTRICT UniformBufferParameters = SingleShaderBindings.ParameterMapInfo.UniformBuffers.GetData();
	const int32 NumUniformBuffers = SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num();

	for (int32 UniformBufferIndex = 0; UniformBufferIndex < NumUniformBuffers; UniformBufferIndex++)
	{
		FShaderParameterInfo Parameter = UniformBufferParameters[UniformBufferIndex];
		checkSlow(Parameter.BaseIndex < ARRAY_COUNT(ShaderBindingState.UniformBuffers));
		FUniformBufferRHIParamRef UniformBuffer = UniformBufferBindings[UniformBufferIndex];

		if (UniformBuffer != ShaderBindingState.UniformBuffers[Parameter.BaseIndex])
		{
			RHICmdList.SetShaderUniformBuffer(Shader, Parameter.BaseIndex, UniformBuffer);
			ShaderBindingState.UniformBuffers[Parameter.BaseIndex] = UniformBuffer;
			ShaderBindingState.MaxUniformBufferUsed = FMath::Max((int32)Parameter.BaseIndex, ShaderBindingState.MaxUniformBufferUsed);
		}
	}

	const FSamplerStateRHIParamRef* RESTRICT SamplerBindings = SingleShaderBindings.GetSamplerStart();
	const FShaderParameterInfo* RESTRICT TextureSamplerParameters = SingleShaderBindings.ParameterMapInfo.TextureSamplers.GetData();
	const int32 NumTextureSamplers = SingleShaderBindings.ParameterMapInfo.TextureSamplers.Num();

	for (int32 SamplerIndex = 0; SamplerIndex < NumTextureSamplers; SamplerIndex++)
	{
		FShaderParameterInfo Parameter = TextureSamplerParameters[SamplerIndex];
		checkSlow(Parameter.BaseIndex < ARRAY_COUNT(ShaderBindingState.Samplers));
		FSamplerStateRHIParamRef Sampler = SamplerBindings[SamplerIndex];

		if (Sampler != ShaderBindingState.Samplers[Parameter.BaseIndex])
		{
			RHICmdList.SetShaderSampler(Shader, Parameter.BaseIndex, Sampler);
			ShaderBindingState.Samplers[Parameter.BaseIndex] = Sampler;
			ShaderBindingState.MaxSamplerUsed = FMath::Max((int32)Parameter.BaseIndex, ShaderBindingState.MaxSamplerUsed);
		}
	}

	const uint8* RESTRICT SRVType = SingleShaderBindings.GetSRVTypeStart();
	const FRHIResource** RESTRICT SRVBindings = SingleShaderBindings.GetSRVStart();
	const FShaderParameterInfo* RESTRICT SRVParameters = SingleShaderBindings.ParameterMapInfo.SRVs.GetData();
	const uint32 NumSRVs = SingleShaderBindings.ParameterMapInfo.SRVs.Num();

	for (uint32 SRVIndex = 0; SRVIndex < NumSRVs; SRVIndex++)
	{
		FShaderParameterInfo Parameter = SRVParameters[SRVIndex];
		checkSlow(Parameter.BaseIndex < ARRAY_COUNT(ShaderBindingState.SRVs));

		uint32 TypeByteIndex = SRVIndex / 8;
		uint32 TypeBitIndex = SRVIndex - TypeByteIndex;

		if (SRVType[TypeByteIndex] & (1 << TypeBitIndex))
		{
			FShaderResourceViewRHIParamRef SRV = (FShaderResourceViewRHIParamRef)SRVBindings[SRVIndex];

			if (SRV != ShaderBindingState.SRVs[Parameter.BaseIndex])
			{
				RHICmdList.SetShaderResourceViewParameter(Shader, Parameter.BaseIndex, SRV);
				ShaderBindingState.SRVs[Parameter.BaseIndex] = SRV;
				ShaderBindingState.MaxSRVUsed = FMath::Max((int32)Parameter.BaseIndex, ShaderBindingState.MaxSRVUsed);
			}
		}
		else
		{
			FTextureRHIParamRef Texture = (FTextureRHIParamRef)SRVBindings[SRVIndex];

			if (Texture != ShaderBindingState.Textures[Parameter.BaseIndex])
			{
				RHICmdList.SetShaderTexture(Shader, Parameter.BaseIndex, Texture);
				ShaderBindingState.Textures[Parameter.BaseIndex] = Texture;
				ShaderBindingState.MaxTextureUsed = FMath::Max((int32)Parameter.BaseIndex, ShaderBindingState.MaxTextureUsed);
			}
		}
	}

	const uint8* LooseDataStart = SingleShaderBindings.GetLooseDataStart();

	for (const FShaderLooseParameterBufferInfo& LooseParameterBuffer : SingleShaderBindings.ParameterMapInfo.LooseParameterBuffers)
	{
		for (FShaderParameterInfo Parameter : LooseParameterBuffer.Parameters)
		{
			RHICmdList.SetShaderParameter(
				Shader,
				LooseParameterBuffer.BufferIndex,
				Parameter.BaseIndex,
				Parameter.Size,
				LooseDataStart
			);

			LooseDataStart += Parameter.Size;
		}
	}
}

template<class RHIShaderType>
void FMeshDrawShaderBindings::SetShaderBindings(
	FRHICommandList& RHICmdList,
	RHIShaderType Shader,
	const FReadOnlyMeshDrawSingleShaderBindings& RESTRICT SingleShaderBindings)
{
	const FUniformBufferRHIParamRef* RESTRICT UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
	const FShaderParameterInfo* RESTRICT UniformBufferParameters = SingleShaderBindings.ParameterMapInfo.UniformBuffers.GetData();
	const int32 NumUniformBuffers = SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num();

	for (int32 UniformBufferIndex = 0; UniformBufferIndex < NumUniformBuffers; UniformBufferIndex++)
	{
		FShaderParameterInfo Parameter = UniformBufferParameters[UniformBufferIndex];
		FUniformBufferRHIParamRef UniformBuffer = UniformBufferBindings[UniformBufferIndex];

		RHICmdList.SetShaderUniformBuffer(Shader, Parameter.BaseIndex, UniformBuffer);
	}

	const FSamplerStateRHIParamRef* RESTRICT SamplerBindings = SingleShaderBindings.GetSamplerStart();
	const FShaderParameterInfo* RESTRICT TextureSamplerParameters = SingleShaderBindings.ParameterMapInfo.TextureSamplers.GetData();
	const int32 NumTextureSamplers = SingleShaderBindings.ParameterMapInfo.TextureSamplers.Num();

	for (int32 SamplerIndex = 0; SamplerIndex < NumTextureSamplers; SamplerIndex++)
	{
		FShaderParameterInfo Parameter = TextureSamplerParameters[SamplerIndex];
		FSamplerStateRHIParamRef Sampler = SamplerBindings[SamplerIndex];

		RHICmdList.SetShaderSampler(Shader, Parameter.BaseIndex, Sampler);
	}

	const uint8* RESTRICT SRVType = SingleShaderBindings.GetSRVTypeStart();
	const FRHIResource** RESTRICT SRVBindings = SingleShaderBindings.GetSRVStart();
	const FShaderParameterInfo* RESTRICT SRVParameters = SingleShaderBindings.ParameterMapInfo.SRVs.GetData();
	const uint32 NumSRVs = SingleShaderBindings.ParameterMapInfo.SRVs.Num();

	for (uint32 SRVIndex = 0; SRVIndex < NumSRVs; SRVIndex++)
	{
		FShaderParameterInfo Parameter = SRVParameters[SRVIndex];

		uint32 TypeByteIndex = SRVIndex / 8;
		uint32 TypeBitIndex = SRVIndex - TypeByteIndex;

		if (SRVType[TypeByteIndex] & (1 << TypeBitIndex))
		{
			FShaderResourceViewRHIParamRef SRV = (FShaderResourceViewRHIParamRef)SRVBindings[SRVIndex];
			RHICmdList.SetShaderResourceViewParameter(Shader, Parameter.BaseIndex, SRV);
		}
		else
		{
			FTextureRHIParamRef Texture = (FTextureRHIParamRef)SRVBindings[SRVIndex];
			RHICmdList.SetShaderTexture(Shader, Parameter.BaseIndex, Texture);
		}
	}
	
	const uint8* LooseDataStart = SingleShaderBindings.GetLooseDataStart();

	for (const FShaderLooseParameterBufferInfo& LooseParameterBuffer : SingleShaderBindings.ParameterMapInfo.LooseParameterBuffers)
	{
		for (FShaderParameterInfo Parameter : LooseParameterBuffer.Parameters)
		{
			RHICmdList.SetShaderParameter(
				Shader,
				LooseParameterBuffer.BufferIndex,
				Parameter.BaseIndex,
				Parameter.Size,
				LooseDataStart
			);

			LooseDataStart += Parameter.Size;
		}
	}
}

#if RHI_RAYTRACING
void FMeshDrawShaderBindings::SetRayTracingShaderBindingsForHitGroup(
	FRHICommandList& RHICmdList, 
	FRayTracingSceneRHIParamRef Scene, 
	uint32 InstanceIndex, 
	uint32 SegmentIndex,
	FRayTracingPipelineStateRHIParamRef PipelineState, 
	uint32 HitGroupIndex,
	uint32 ShaderSlot) const
{
	check(ShaderLayouts.Num() == 1);

	FReadOnlyMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[0], GetData());

	FUniformBufferRHIParamRef LocalUniformBuffers[MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE] = {};

	const FUniformBufferRHIParamRef* RESTRICT UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
	const FShaderParameterInfo* RESTRICT UniformBufferParameters = SingleShaderBindings.ParameterMapInfo.UniformBuffers.GetData();
	const int32 NumUniformBuffers = SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num();

	int32 MaxUniformBufferUsed = -1;
	for (int32 UniformBufferIndex = 0; UniformBufferIndex < NumUniformBuffers; UniformBufferIndex++)
	{
		FShaderParameterInfo Parameter = UniformBufferParameters[UniformBufferIndex];
		checkSlow(Parameter.BaseIndex < ARRAY_COUNT(LocalUniformBuffers));
		FUniformBufferRHIParamRef UniformBuffer = UniformBufferBindings[UniformBufferIndex];
		if (Parameter.BaseIndex < ARRAY_COUNT(LocalUniformBuffers))
		{
			LocalUniformBuffers[Parameter.BaseIndex] = UniformBuffer;
			MaxUniformBufferUsed = FMath::Max((int32)Parameter.BaseIndex, MaxUniformBufferUsed);
		}
	}

	checkf(SingleShaderBindings.ParameterMapInfo.TextureSamplers.Num() == 0, TEXT("Texture sampler parameters are not supported for ray tracing. UniformBuffers must be used for all resource binding."));
	checkf(SingleShaderBindings.ParameterMapInfo.SRVs.Num() == 0, TEXT("SRV parameters are not supported for ray tracing. UniformBuffers must be used for all resource binding."));
	checkf(SingleShaderBindings.ParameterMapInfo.LooseParameterBuffers.Num() == 0, TEXT("Loose parameter buffers are not supported for ray tracing. UniformBuffers must be used for all resource binding."));

	check(SegmentIndex < 0xFF);
	uint32 NumUniformBuffersToSet = MaxUniformBufferUsed + 1;
	const uint32 UserData = 0; // UserData could be used to store material ID or any other kind of per-material constant. This can be retrieved in hit shaders via GetHitGroupUserData().
	RHICmdList.SetRayTracingHitGroup(Scene, InstanceIndex, SegmentIndex, ShaderSlot, PipelineState, HitGroupIndex, NumUniformBuffersToSet, LocalUniformBuffers, UserData);
}
#endif // RHI_RAYTRACING

FGraphicsMinimalPipelineStateId FGraphicsMinimalPipelineStateId::GetPersistentId(const FGraphicsMinimalPipelineStateInitializer& InPipelineState)
{
	checkSlow(IsInRenderingThread());

	FSetElementId TableId = PersistentIdTable.FindId(InPipelineState);
	if (TableId.IsValidId())
	{
		++PersistentIdTable[TableId].RefNum;
	}
	else
	{
		TableId = PersistentIdTable.Add(FRefCountedGraphicsMinimalPipelineStateInitializer(InPipelineState, 1));
	}

	checkf(TableId.AsInteger() < (MAX_uint32 >> 2), TEXT("Persistent FGraphicsMinimalPipelineStateId table overflow!"));

	FGraphicsMinimalPipelineStateId Ret;
	Ret.bValid = 1;
	Ret.bOneFrameId = 0;
	Ret.SetElementIndex = TableId.AsInteger();
	return Ret;
}

void FGraphicsMinimalPipelineStateId::RemovePersistentId(FGraphicsMinimalPipelineStateId Id)
{
	check(!Id.bOneFrameId && Id.bValid);

	const FSetElementId SetElementId = FSetElementId::FromInteger(Id.SetElementIndex);
	FRefCountedGraphicsMinimalPipelineStateInitializer& RefCountedStateInitializer = PersistentIdTable[SetElementId];

	check(RefCountedStateInitializer.RefNum > 0);
	--RefCountedStateInitializer.RefNum;
	if (RefCountedStateInitializer.RefNum <= 0)
	{
		PersistentIdTable.Remove(SetElementId);
	}
}

FGraphicsMinimalPipelineStateId FGraphicsMinimalPipelineStateId::GetOneFrameId(const FGraphicsMinimalPipelineStateInitializer& InPipelineState)
{
	FGraphicsMinimalPipelineStateId Ret;
	Ret.bValid = 1;
	Ret.bOneFrameId = 0;

	FSetElementId TableId = PersistentIdTable.FindId(InPipelineState);
	if (!TableId.IsValidId())
	{
		// Need to lock as this is called from multiple parallel tasks during mesh draw command generation or patching.
		FRWScopeLock ScopeLock(OneFrameIdTableHotfixRWLock, SLT_Write);
		Ret.bOneFrameId = 1;

		TableId = OneFrameIdTable.FindId(InPipelineState);
		if (!TableId.IsValidId())
		{
			TableId = OneFrameIdTable.Add(InPipelineState);
		}
	}

	checkf(TableId.AsInteger() < (MAX_uint32 >> 2), TEXT("One frame FGraphicsMinimalPipelineStateId table overflow!"));

	Ret.SetElementIndex = TableId.AsInteger();
	return Ret;
}

void FGraphicsMinimalPipelineStateId::ResetOneFrameIdTable()
{
	OneFrameIdTable.Reset();
}

class FMeshDrawCommandStateCache
{
public:

	uint32 PipelineId;
	uint32 StencilRef;
	FShaderBindingState ShaderBindings[SF_NumStandardFrequencies];
	FVertexInputStream VertexStreams[MaxVertexElementCount];

	FMeshDrawCommandStateCache()
	{
		// Must init to impossible values to avoid filtering the first draw's state
		PipelineId = -1;
		StencilRef = -1;
	}

	inline void SetPipelineState(int32 NewPipelineId)
	{
		PipelineId = NewPipelineId;
		StencilRef = -1;

		// Vertex streams must be reset if PSO changes.
		for (int32 VertexStreamIndex = 0; VertexStreamIndex < ARRAY_COUNT(VertexStreams); ++VertexStreamIndex)
		{
			VertexStreams[VertexStreamIndex].VertexBuffer = nullptr;
		}

		// Shader bindings must be reset if PSO changes
		for (int32 FrequencyIndex = 0; FrequencyIndex < ARRAY_COUNT(ShaderBindings); FrequencyIndex++)
		{
			FShaderBindingState& RESTRICT ShaderBinding = ShaderBindings[FrequencyIndex];

			for (int32 SlotIndex = 0; SlotIndex <= ShaderBinding.MaxSRVUsed; SlotIndex++)
			{
				ShaderBinding.SRVs[SlotIndex] = nullptr;
			}

			ShaderBinding.MaxSRVUsed = -1;

			for (int32 SlotIndex = 0; SlotIndex <= ShaderBinding.MaxUniformBufferUsed; SlotIndex++)
			{
				ShaderBinding.UniformBuffers[SlotIndex] = nullptr;
			}

			ShaderBinding.MaxUniformBufferUsed = -1;
			
			for (int32 SlotIndex = 0; SlotIndex <= ShaderBinding.MaxTextureUsed; SlotIndex++)
			{
				ShaderBinding.Textures[SlotIndex] = nullptr;
			}

			ShaderBinding.MaxTextureUsed = -1;

			for (int32 SlotIndex = 0; SlotIndex <= ShaderBinding.MaxSamplerUsed; SlotIndex++)
			{
				ShaderBinding.Samplers[SlotIndex] = nullptr;
			}

			ShaderBinding.MaxSamplerUsed = -1;
		}
	}
};

FMeshDrawShaderBindings::~FMeshDrawShaderBindings()
{
#if VALIDATE_UNIFORM_BUFFER_LIFETIME
	uint8* ShaderBindingDataPtr = GetData();

	for (int32 ShaderBindingsIndex = 0; ShaderBindingsIndex < ShaderLayouts.Num(); ShaderBindingsIndex++)
	{
		FMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[ShaderBindingsIndex], ShaderBindingDataPtr);

		const FUniformBufferRHIParamRef* RESTRICT UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
		const int32 NumUniformBuffers = SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num();

		for (int32 UniformBufferIndex = 0; UniformBufferIndex < NumUniformBuffers; UniformBufferIndex++)
		{
			FUniformBufferRHIParamRef UniformBuffer = UniformBufferBindings[UniformBufferIndex];

			if (UniformBuffer)
			{
				UniformBuffer->NumMeshCommandReferencesForDebugging--;
				check(UniformBuffer->NumMeshCommandReferencesForDebugging >= 0);
			}
		}

		ShaderBindingDataPtr += ShaderLayouts[ShaderBindingsIndex].GetDataSizeBytes();
	}
#endif

	if (Size > ARRAY_COUNT(InlineStorage))
	{
		delete[] HeapData;
	}
}

void FMeshDrawShaderBindings::Initialize(FMeshProcessorShaders Shaders)
{
	const int32 NumShaderFrequencies = (Shaders.VertexShader ? 1 : 0) + (Shaders.HullShader ? 1 : 0) + (Shaders.DomainShader ? 1 : 0) + (Shaders.PixelShader ? 1 : 0) + (Shaders.GeometryShader ? 1 : 0) + (Shaders.ComputeShader ? 1 : 0)
#if RHI_RAYTRACING
		+ (Shaders.RayHitGroupShader ? 1 : 0)
#endif
		;

	ShaderLayouts.Empty(NumShaderFrequencies);
	int32 ShaderBindingDataSize = 0;

	if (Shaders.VertexShader)
	{
		ShaderLayouts.Add(FMeshDrawShaderBindingsLayout(Shaders.VertexShader));
		ShaderBindingDataSize += ShaderLayouts.Last().GetDataSizeBytes();
	}

	if (Shaders.HullShader)
	{
		ShaderLayouts.Add(FMeshDrawShaderBindingsLayout(Shaders.HullShader));
		ShaderBindingDataSize += ShaderLayouts.Last().GetDataSizeBytes();
	}

	if (Shaders.DomainShader)
	{
		ShaderLayouts.Add(FMeshDrawShaderBindingsLayout(Shaders.DomainShader));
		ShaderBindingDataSize += ShaderLayouts.Last().GetDataSizeBytes();
	}

	if (Shaders.PixelShader)
	{
		ShaderLayouts.Add(FMeshDrawShaderBindingsLayout(Shaders.PixelShader));
		ShaderBindingDataSize += ShaderLayouts.Last().GetDataSizeBytes();
	}

	if (Shaders.GeometryShader)
	{
		ShaderLayouts.Add(FMeshDrawShaderBindingsLayout(Shaders.GeometryShader));
		ShaderBindingDataSize += ShaderLayouts.Last().GetDataSizeBytes();
	}

	if (Shaders.ComputeShader)
	{
		ShaderLayouts.Add(FMeshDrawShaderBindingsLayout(Shaders.ComputeShader));
		ShaderBindingDataSize += ShaderLayouts.Last().GetDataSizeBytes();
	}

#if RHI_RAYTRACING
	if (Shaders.RayHitGroupShader)
	{
		ShaderLayouts.Add(FMeshDrawShaderBindingsLayout(Shaders.RayHitGroupShader));
		ShaderBindingDataSize += ShaderLayouts.Last().GetDataSizeBytes();
	}
#endif

	checkSlow(ShaderLayouts.Num() == NumShaderFrequencies);

	if (ShaderBindingDataSize > 0)
	{
		AllocateZeroed(ShaderBindingDataSize);
	}
}

void FMeshDrawShaderBindings::Finalize(const FMeshProcessorShaders* ShadersForDebugging)
{
#if VALIDATE_MESH_COMMAND_BINDINGS
	if (!ShadersForDebugging)
	{
		return;
	}

	const uint8* ShaderBindingDataPtr = GetData();

	for (int32 ShaderBindingsIndex = 0; ShaderBindingsIndex < ShaderLayouts.Num(); ShaderBindingsIndex++)
	{
		const FMeshDrawShaderBindingsLayout& ShaderLayout = ShaderLayouts[ShaderBindingsIndex];

		FMeshMaterialShader* Shader = ShadersForDebugging->GetShader(ShaderLayout.Frequency);
		check(Shader);

		FReadOnlyMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayout, ShaderBindingDataPtr);

		const FUniformBufferRHIParamRef* UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();

		for (int32 BindingIndex = 0; BindingIndex < ShaderLayout.ParameterMapInfo.UniformBuffers.Num(); BindingIndex++)
		{
			FShaderParameterInfo ParameterInfo = ShaderLayout.ParameterMapInfo.UniformBuffers[BindingIndex];

			FUniformBufferRHIParamRef UniformBufferValue = UniformBufferBindings[BindingIndex];

			if (!UniformBufferValue)
			{
				// Search the automatically bound uniform buffers for more context if available
				const FShaderParametersMetadata* AutomaticallyBoundUniformBufferStruct = Shader->FindAutomaticallyBoundUniformBufferStruct(ParameterInfo.BaseIndex);

				if (AutomaticallyBoundUniformBufferStruct)
				{
					ensureMsgf(UniformBufferValue, TEXT("Shader %s with vertex factory %s never set automatically bound uniform buffer at BaseIndex %i.  Expected buffer of type %s.  This can cause GPU hangs, depending on how the shader uses it."),
						Shader->GetType()->GetName(), 
						Shader->GetVertexFactoryType()->GetName(),
						ParameterInfo.BaseIndex,
						AutomaticallyBoundUniformBufferStruct->GetStructTypeName());
				}
				else
				{
					ensureMsgf(UniformBufferValue, TEXT("Shader %s with vertex factory %s never set uniform buffer at BaseIndex %i.  This can cause GPU hangs, depending on how the shader uses it."), 
						Shader->GetVertexFactoryType()->GetName(),
						Shader->GetType()->GetName(), 
						ParameterInfo.BaseIndex);
				}
			}
		}

		const FSamplerStateRHIParamRef* SamplerBindings = SingleShaderBindings.GetSamplerStart();

		for (int32 BindingIndex = 0; BindingIndex < ShaderLayout.ParameterMapInfo.TextureSamplers.Num(); BindingIndex++)
		{
			FShaderParameterInfo ParameterInfo = ShaderLayout.ParameterMapInfo.TextureSamplers[BindingIndex];
			FSamplerStateRHIParamRef SamplerValue = SamplerBindings[BindingIndex];
			ensureMsgf(SamplerValue, TEXT("Shader %s with vertex factory %s never set sampler at BaseIndex %u.  This can cause GPU hangs, depending on how the shader uses it."), 
				Shader->GetType()->GetName(), 
				Shader->GetVertexFactoryType()->GetName(),
				ParameterInfo.BaseIndex);
		}

		const uint8* RESTRICT SRVType = SingleShaderBindings.GetSRVTypeStart();
		const FRHIResource** RESTRICT SRVBindings = SingleShaderBindings.GetSRVStart();
		const FShaderParameterInfo* RESTRICT SRVParameters = SingleShaderBindings.ParameterMapInfo.SRVs.GetData();
		const uint32 NumSRVs = SingleShaderBindings.ParameterMapInfo.SRVs.Num();

		for (uint32 SRVIndex = 0; SRVIndex < NumSRVs; SRVIndex++)
		{
			FShaderParameterInfo Parameter = SRVParameters[SRVIndex];

			uint32 TypeByteIndex = SRVIndex / 8;
			uint32 TypeBitIndex = SRVIndex - TypeByteIndex;

			if (SRVType[TypeByteIndex] & (1 << TypeBitIndex))
			{
				FShaderResourceViewRHIParamRef SRV = (FShaderResourceViewRHIParamRef)SRVBindings[SRVIndex];

				ensureMsgf(SRV, TEXT("Shader %s with vertex factory %s never set SRV at BaseIndex %u.  This can cause GPU hangs, depending on how the shader uses it."), 
					Shader->GetType()->GetName(), 
					Shader->GetVertexFactoryType()->GetName(),
					Parameter.BaseIndex);
			}
			else
			{
				FTextureRHIParamRef Texture = (FTextureRHIParamRef)SRVBindings[SRVIndex];

				ensureMsgf(Texture, TEXT("Shader %s with vertex factory %s never set texture at BaseIndex %u.  This can cause GPU hangs, depending on how the shader uses it."), 
					Shader->GetType()->GetName(), 
					Shader->GetVertexFactoryType()->GetName(),
					Parameter.BaseIndex);
			}
		}

		ShaderBindingDataPtr += ShaderLayout.GetDataSizeBytes();
	}
#endif
}

void FMeshDrawShaderBindings::CopyFrom(const FMeshDrawShaderBindings& Other)
{
	ShaderLayouts = Other.ShaderLayouts;

	Allocate(Other.Size);
	FPlatformMemory::Memcpy(GetData(), Other.GetData(), Size);

#if VALIDATE_UNIFORM_BUFFER_LIFETIME
	uint8* ShaderBindingDataPtr = GetData();

	for (int32 ShaderBindingsIndex = 0; ShaderBindingsIndex < ShaderLayouts.Num(); ShaderBindingsIndex++)
	{
		FMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[ShaderBindingsIndex], ShaderBindingDataPtr);
		const FUniformBufferRHIParamRef* RESTRICT UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
		const int32 NumUniformBuffers = SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num();

		for (int32 UniformBufferIndex = 0; UniformBufferIndex < NumUniformBuffers; UniformBufferIndex++)
		{
			FUniformBufferRHIParamRef UniformBuffer = UniformBufferBindings[UniformBufferIndex];

			if (UniformBuffer)
			{
				UniformBuffer->NumMeshCommandReferencesForDebugging++;
			}
		}

		ShaderBindingDataPtr += ShaderLayouts[ShaderBindingsIndex].GetDataSizeBytes();
	}
#endif
}

void FMeshDrawCommand::SetShaders(FVertexDeclarationRHIParamRef VertexDeclaration, const FMeshProcessorShaders& Shaders, FGraphicsMinimalPipelineStateInitializer& PipelineState)
{
	PipelineState.BoundShaderState = FBoundShaderStateInput(
		VertexDeclaration,
		GETSAFERHISHADER_VERTEX(Shaders.VertexShader),
		GETSAFERHISHADER_HULL(Shaders.HullShader),
		GETSAFERHISHADER_DOMAIN(Shaders.DomainShader),
		GETSAFERHISHADER_PIXEL(Shaders.PixelShader),
		GETSAFERHISHADER_GEOMETRY(Shaders.GeometryShader)
	);

	ShaderBindings.Initialize(Shaders);
}

#if RHI_RAYTRACING
void FRayTracingMeshCommand::SetShaders(const FMeshProcessorShaders& Shaders)
{
	check(Shaders.RayHitGroupShader)
	MaterialShaderIndex = Shaders.RayHitGroupShader->GetRayTracingMaterialLibraryIndex();
	ShaderBindings.Initialize(Shaders);
}
#endif // RHI_RAYTRACING

void FMeshDrawCommand::SetDrawParametersAndFinalize(
	const FMeshBatch& MeshBatch, 
	int32 BatchElementIndex,
	FGraphicsMinimalPipelineStateId PipelineId,
	const FMeshProcessorShaders* ShadersForDebugging)
{
	const FMeshBatchElement& BatchElement = MeshBatch.Elements[BatchElementIndex];

	check(!BatchElement.IndexBuffer || (BatchElement.IndexBuffer && BatchElement.IndexBuffer->IsInitialized() && BatchElement.IndexBuffer->IndexBufferRHI));
	checkSlow(!BatchElement.bIsInstanceRuns);
	IndexBuffer = BatchElement.IndexBuffer ? BatchElement.IndexBuffer->IndexBufferRHI : nullptr;
	FirstIndex = BatchElement.FirstIndex;
	NumPrimitives = BatchElement.NumPrimitives;
	NumInstances = BatchElement.NumInstances;

	if (NumPrimitives > 0)
	{
		VertexParams.BaseVertexIndex = BatchElement.BaseVertexIndex;
		VertexParams.NumVertices = BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1;
		checkf(!BatchElement.IndirectArgsBuffer, TEXT("FMeshBatchElement::NumPrimitives must be set to 0 when a IndirectArgsBuffer is used"));
	}
	else
	{
		checkf(BatchElement.IndirectArgsBuffer, TEXT("It is only valid to set BatchElement.NumPrimitives == 0 when a IndirectArgsBuffer is used"));
		IndirectArgsBuffer = BatchElement.IndirectArgsBuffer;
	}

	Finalize(PipelineId, ShadersForDebugging);
}

void FMeshDrawShaderBindings::SetOnCommandList(FRHICommandList& RHICmdList, FBoundShaderStateInput Shaders, FShaderBindingState* StateCacheShaderBindings) const
{
	const uint8* ShaderBindingDataPtr = GetData();

	for (int32 ShaderBindingsIndex = 0; ShaderBindingsIndex < ShaderLayouts.Num(); ShaderBindingsIndex++)
	{
		FReadOnlyMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[ShaderBindingsIndex], ShaderBindingDataPtr);
		const EShaderFrequency Frequency = SingleShaderBindings.Frequency;
		FShaderBindingState& ShaderBindingState = StateCacheShaderBindings[Frequency];

		if (Frequency == SF_Vertex)
		{
			SetShaderBindings(RHICmdList, Shaders.VertexShaderRHI, SingleShaderBindings, ShaderBindingState);
		} 
		else if (Frequency == SF_Pixel)
		{
			SetShaderBindings(RHICmdList, Shaders.PixelShaderRHI, SingleShaderBindings, ShaderBindingState);
		}
		else if (Frequency == SF_Hull)
		{
			SetShaderBindings(RHICmdList, Shaders.HullShaderRHI, SingleShaderBindings, ShaderBindingState);
		}
		else if (Frequency == SF_Domain)
		{
			SetShaderBindings(RHICmdList, Shaders.DomainShaderRHI, SingleShaderBindings, ShaderBindingState);
		}
		else if (Frequency == SF_Geometry)
		{
			SetShaderBindings(RHICmdList, Shaders.GeometryShaderRHI, SingleShaderBindings, ShaderBindingState);
		}
		else
		{
			checkf(0, TEXT("Unknown shader frequency"));
		}

		ShaderBindingDataPtr += ShaderLayouts[ShaderBindingsIndex].GetDataSizeBytes();
	}
}

void FMeshDrawShaderBindings::SetOnCommandListForCompute(FRHICommandList& RHICmdList, FComputeShaderRHIParamRef Shader) const
{
	check(ShaderLayouts.Num() == 1);
	FReadOnlyMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[0], GetData());
	check(SingleShaderBindings.Frequency == SF_Compute);

	SetShaderBindings(RHICmdList, Shader, SingleShaderBindings);
}

bool FMeshDrawShaderBindings::MatchesForDynamicInstancing(const FMeshDrawShaderBindings& Rhs) const
{
	if (!(ShaderLayouts == Rhs.ShaderLayouts
		&& Size == Rhs.Size))
	{
		return false;
	}

	const uint8* ShaderBindingDataPtr = GetData();
	const uint8* OtherShaderBindingDataPtr = Rhs.GetData();

	for (int32 ShaderBindingsIndex = 0; ShaderBindingsIndex < ShaderLayouts.Num(); ShaderBindingsIndex++)
	{
		FReadOnlyMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[ShaderBindingsIndex], ShaderBindingDataPtr);
		FReadOnlyMeshDrawSingleShaderBindings OtherSingleShaderBindings(Rhs.ShaderLayouts[ShaderBindingsIndex], OtherShaderBindingDataPtr);

		if (SingleShaderBindings.ParameterMapInfo.SRVs.Num() > 0 || SingleShaderBindings.ParameterMapInfo.LooseParameterBuffers.Num() > 0 || SingleShaderBindings.ParameterMapInfo.TextureSamplers.Num() > 0)
		{
			// Not implemented
			return false;
		}

		const FUniformBufferRHIParamRef* UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
		const FUniformBufferRHIParamRef* OtherUniformBufferBindings = OtherSingleShaderBindings.GetUniformBufferStart();

		for (int32 UniformBufferIndex = 0; UniformBufferIndex < SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num(); UniformBufferIndex++)
		{
			FUniformBufferRHIParamRef UniformBuffer = UniformBufferBindings[UniformBufferIndex];
			FUniformBufferRHIParamRef OtherUniformBuffer = OtherUniformBufferBindings[UniformBufferIndex];
			
			if (UniformBuffer != OtherUniformBuffer)
			{
				return false;
			}
		}

		ShaderBindingDataPtr += ShaderLayouts[ShaderBindingsIndex].GetDataSizeBytes();
		OtherShaderBindingDataPtr += Rhs.ShaderLayouts[ShaderBindingsIndex].GetDataSizeBytes();
	}

	return true;
} 

void FMeshDrawCommand::SubmitDraw(
	const FMeshDrawCommand& RESTRICT MeshDrawCommand, 
	FVertexBufferRHIParamRef ScenePrimitiveIdsBuffer,
	int32 PrimitiveIdOffset,
	uint32 InstanceFactor,
	FRHICommandList& RHICmdList,
	FMeshDrawCommandStateCache& RESTRICT StateCache)
{
	checkSlow(MeshDrawCommand.CachedPipelineId.IsValid());
		
#if WANTS_DRAW_MESH_EVENTS
	TDrawEvent<FRHICommandList> MeshEvent;

	if (GShowMaterialDrawEvents)
	{
		const FPrimitiveSceneProxy* PrimitiveSceneProxy = MeshDrawCommand.DebugData.PrimitiveSceneProxy;
		const FMaterial* Material = MeshDrawCommand.DebugData.Material;

		FString DrawEventName;

		if (PrimitiveSceneProxy)
		{
			DrawEventName = FString::Printf(
				TEXT("%s %s"),
				// Note: this is the parent's material name, not the material instance
				*Material->GetFriendlyName(),
				PrimitiveSceneProxy->GetResourceName().IsValid() ? *PrimitiveSceneProxy->GetResourceName().ToString() : TEXT(""));
		}
		else
		{
			DrawEventName = Material->GetFriendlyName();
		}

		const uint32 Instances = MeshDrawCommand.NumInstances * InstanceFactor;
		if (Instances > 1)
		{
			BEGIN_DRAW_EVENTF(
				RHICmdList,
				MaterialEvent,
				MeshEvent,
				TEXT("%s %u instances"),
				*DrawEventName,
				Instances);
		}
		else
		{
			BEGIN_DRAW_EVENTF(RHICmdList, MaterialEvent, MeshEvent, *DrawEventName);
		}
	}
#endif
	{
		FRWScopeLock ScopeLock(OneFrameIdTableHotfixRWLock, SLT_ReadOnly);
		const FGraphicsMinimalPipelineStateInitializer& MeshPipelineState = MeshDrawCommand.CachedPipelineId.GetPipelineState();

		if (MeshDrawCommand.CachedPipelineId.GetId() != StateCache.PipelineId)
		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit = MeshPipelineState;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			StateCache.SetPipelineState(MeshDrawCommand.CachedPipelineId.GetId());
		}

		if (MeshDrawCommand.StencilRef != StateCache.StencilRef)
		{
			RHICmdList.SetStencilRef(MeshDrawCommand.StencilRef);
			StateCache.StencilRef = MeshDrawCommand.StencilRef;
		}

		for (int32 VertexBindingIndex = 0; VertexBindingIndex < MeshDrawCommand.VertexStreams.Num(); VertexBindingIndex++)
		{
			const FVertexInputStream& Stream = MeshDrawCommand.VertexStreams[VertexBindingIndex];

			if (MeshDrawCommand.PrimitiveIdStreamIndex != -1 && Stream.StreamIndex == MeshDrawCommand.PrimitiveIdStreamIndex)
			{
				RHICmdList.SetStreamSource(Stream.StreamIndex, ScenePrimitiveIdsBuffer, PrimitiveIdOffset);
				StateCache.VertexStreams[Stream.StreamIndex] = Stream;
			}
			else if (StateCache.VertexStreams[Stream.StreamIndex] != Stream)
			{
				RHICmdList.SetStreamSource(Stream.StreamIndex, Stream.VertexBuffer, Stream.Offset);
				StateCache.VertexStreams[Stream.StreamIndex] = Stream;
			}
		}

		MeshDrawCommand.ShaderBindings.SetOnCommandList(RHICmdList, MeshPipelineState.BoundShaderState, StateCache.ShaderBindings);
	}

	if (MeshDrawCommand.IndexBuffer)
	{
		if (MeshDrawCommand.NumPrimitives > 0)
		{
			RHICmdList.DrawIndexedPrimitive(
				MeshDrawCommand.IndexBuffer,
				MeshDrawCommand.VertexParams.BaseVertexIndex,
				0,
				MeshDrawCommand.VertexParams.NumVertices,
				MeshDrawCommand.FirstIndex,
				MeshDrawCommand.NumPrimitives,
				MeshDrawCommand.NumInstances * InstanceFactor
			);
		}
		else
		{
			RHICmdList.DrawIndexedPrimitiveIndirect(
				MeshDrawCommand.IndexBuffer, 
				MeshDrawCommand.IndirectArgsBuffer, 
				0
				);
		}
	}
	else
	{
		RHICmdList.DrawPrimitive(
			MeshDrawCommand.VertexParams.BaseVertexIndex + MeshDrawCommand.FirstIndex,
			MeshDrawCommand.NumPrimitives,
			MeshDrawCommand.NumInstances * InstanceFactor
		);
	}

}

void SubmitMeshDrawCommands(
	const FMeshCommandOneFrameArray& VisibleMeshDrawCommands,
	FVertexBufferRHIParamRef PrimitiveIdsBuffer,
	int32 BasePrimitiveIdsOffset,
	bool bDynamicInstancing,
	uint32 InstanceFactor,
	FRHICommandList& RHICmdList)
{
	SubmitMeshDrawCommandsRange(VisibleMeshDrawCommands, PrimitiveIdsBuffer, BasePrimitiveIdsOffset, bDynamicInstancing, 0, VisibleMeshDrawCommands.Num(), InstanceFactor, RHICmdList);
}

void SubmitMeshDrawCommandsRange(
	const FMeshCommandOneFrameArray& VisibleMeshDrawCommands,
	FVertexBufferRHIParamRef PrimitiveIdsBuffer,
	int32 BasePrimitiveIdsOffset,
	bool bDynamicInstancing,
	int32 StartIndex,
	int32 NumMeshDrawCommands,
	uint32 InstanceFactor,
	FRHICommandList& RHICmdList)
{
	FMeshDrawCommandStateCache StateCache;
	INC_DWORD_STAT_BY(STAT_MeshDrawCalls, NumMeshDrawCommands);

	for (int32 DrawCommandIndex = StartIndex; DrawCommandIndex < StartIndex + NumMeshDrawCommands; DrawCommandIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, MeshEvent, GEmitMeshDrawEvent != 0, TEXT("Mesh Draw"));

		const FVisibleMeshDrawCommand& VisibleMeshDrawCommand = VisibleMeshDrawCommands[DrawCommandIndex];
		const int32 PrimitiveIdBufferOffset = BasePrimitiveIdsOffset + (bDynamicInstancing ? VisibleMeshDrawCommand.PrimitiveIdBufferOffset : DrawCommandIndex) * sizeof(int32);
		checkSlow(!bDynamicInstancing || VisibleMeshDrawCommand.PrimitiveIdBufferOffset >= 0);
		FMeshDrawCommand::SubmitDraw(*VisibleMeshDrawCommand.MeshDrawCommand, PrimitiveIdsBuffer, PrimitiveIdBufferOffset, InstanceFactor, RHICmdList, StateCache);
	}
}

void DrawDynamicMeshPassPrivate(
	const FSceneView& View,
	FRHICommandList& RHICmdList,
	FMeshCommandOneFrameArray& VisibleMeshDrawCommands,
	FDynamicMeshDrawCommandStorage& DynamicMeshDrawCommandStorage,
	uint32 InstanceFactor)
{
	if (VisibleMeshDrawCommands.Num() > 0)
	{
		const bool bDynamicInstancing = IsDynamicInstancingEnabled(View.GetFeatureLevel());

		FVertexBufferRHIParamRef PrimitiveIdVertexBuffer = nullptr;

		SortAndMergeDynamicPassMeshDrawCommands(View.GetFeatureLevel(), VisibleMeshDrawCommands, DynamicMeshDrawCommandStorage, PrimitiveIdVertexBuffer, InstanceFactor);

		SubmitMeshDrawCommandsRange(VisibleMeshDrawCommands, PrimitiveIdVertexBuffer, 0, bDynamicInstancing, 0, VisibleMeshDrawCommands.Num(), InstanceFactor, RHICmdList);
	}
}

FMeshDrawCommandSortKey CalculateMeshStaticSortKey(const FMeshMaterialShader* VertexShader, const FMeshMaterialShader* PixelShader)
{
	FMeshDrawCommandSortKey SortKey;
	SortKey.Generic.VertexShaderHash = PointerHash(VertexShader);
	SortKey.Generic.PixelShaderHash = PointerHash(PixelShader);

	return SortKey;
}

FMeshPassProcessor::FMeshPassProcessor(const FScene* InScene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext) 
	: Scene(InScene)
	, FeatureLevel(InFeatureLevel)
	, ViewIfDynamicMeshCommand(InViewIfDynamicMeshCommand)
	, DrawListContext(InDrawListContext)
{
}

enum class EDrawingPolicyOverrideFlags
{
	None = 0,
	TwoSided = 1 << 0,
	DitheredLODTransition = 1 << 1,
	Wireframe = 1 << 2,
	ReverseCullMode = 1 << 3,
};
ENUM_CLASS_FLAGS(EDrawingPolicyOverrideFlags);

struct FMeshDrawingPolicyOverrideSettings
{
	EDrawingPolicyOverrideFlags	MeshOverrideFlags = EDrawingPolicyOverrideFlags::None;
	EPrimitiveType				MeshPrimitiveType = PT_TriangleList;
};

FORCEINLINE_DEBUGGABLE FMeshDrawingPolicyOverrideSettings ComputeMeshOverrideSettings(const FMeshBatch& Mesh)
{
	FMeshDrawingPolicyOverrideSettings OverrideSettings;
	OverrideSettings.MeshPrimitiveType = (EPrimitiveType)Mesh.Type;

	OverrideSettings.MeshOverrideFlags |= Mesh.bDisableBackfaceCulling ? EDrawingPolicyOverrideFlags::TwoSided : EDrawingPolicyOverrideFlags::None;
	OverrideSettings.MeshOverrideFlags |= Mesh.bDitheredLODTransition ? EDrawingPolicyOverrideFlags::DitheredLODTransition : EDrawingPolicyOverrideFlags::None;
	OverrideSettings.MeshOverrideFlags |= Mesh.bWireframe ? EDrawingPolicyOverrideFlags::Wireframe : EDrawingPolicyOverrideFlags::None;
	OverrideSettings.MeshOverrideFlags |= Mesh.ReverseCulling ? EDrawingPolicyOverrideFlags::ReverseCullMode : EDrawingPolicyOverrideFlags::None;
	return OverrideSettings;
}

ERasterizerFillMode FMeshPassProcessor::ComputeMeshFillMode(const FMeshBatch& Mesh, const FMaterial& InMaterialResource) const
{
	const FMeshDrawingPolicyOverrideSettings InOverrideSettings = ComputeMeshOverrideSettings(Mesh);

	const bool bMaterialResourceIsTwoSided = InMaterialResource.IsTwoSided();
	const bool bIsWireframeMaterial = InMaterialResource.IsWireframe() || !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::Wireframe);
	return bIsWireframeMaterial ? FM_Wireframe : FM_Solid;
}

ERasterizerCullMode FMeshPassProcessor::ComputeMeshCullMode(const FMeshBatch& Mesh, const FMaterial& InMaterialResource) const
{
	const FMeshDrawingPolicyOverrideSettings InOverrideSettings = ComputeMeshOverrideSettings(Mesh);
	const bool bMaterialResourceIsTwoSided = InMaterialResource.IsTwoSided();
	const bool bInTwoSidedOverride = !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::TwoSided);
	const bool bInReverseCullModeOverride = !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::ReverseCullMode);
	const bool bIsTwoSided = (bMaterialResourceIsTwoSided || bInTwoSidedOverride);
	const bool bMeshRenderTwoSided = bIsTwoSided || bInTwoSidedOverride;
	return bMeshRenderTwoSided ? CM_None : (bInReverseCullModeOverride ? CM_CCW : CM_CW);
}

int32 FMeshPassProcessor::GetDrawCommandPrimitiveId(const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo, const FMeshBatchElement& BatchElement) const
{
	int32 DrawPrimitiveId;

	if (UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel))
	{
		if (BatchElement.PrimitiveIdMode == PrimID_FromPrimitiveSceneInfo)
		{
			ensureMsgf(BatchElement.PrimitiveUniformBufferResource == nullptr, TEXT("PrimitiveUniformBufferResource should not be setup when PrimitiveIdMode == PrimID_FromPrimitiveSceneInfo"));
			DrawPrimitiveId = PrimitiveSceneInfo->GetIndex();
		}
		else if (BatchElement.PrimitiveIdMode == PrimID_DynamicPrimitiveShaderData)
		{
			DrawPrimitiveId = Scene->Primitives.Num() + BatchElement.DynamicPrimitiveShaderDataIndex;
		}
		else
		{
			check(BatchElement.PrimitiveIdMode == PrimID_ForceZero);
			DrawPrimitiveId = 0;
		}
	}
	else
	{
		DrawPrimitiveId = PrimitiveSceneInfo ? PrimitiveSceneInfo->GetIndex() : INT32_MAX;
	}

	return DrawPrimitiveId;
}

FCachedPassMeshDrawListContext::FCachedPassMeshDrawListContext(FCachedMeshDrawCommandInfo& InCommandInfo, FCachedPassMeshDrawList& InDrawList, FScene& InScene) :
	CommandInfo(InCommandInfo),
	DrawList(InDrawList),
	Scene(InScene)
{
	bUseStateBuckets = UseGPUScene(GMaxRHIShaderPlatform, GMaxRHIFeatureLevel);
}

FMeshDrawCommand& FCachedPassMeshDrawListContext::AddCommand(const FMeshDrawCommand& Initializer)
{
	if (bUseStateBuckets)
	{
		MeshDrawCommandForStateBucketing = Initializer;
		return MeshDrawCommandForStateBucketing;
	}
	else
	{
		// Only one FMeshDrawCommand supported per FStaticMesh in a pass
		check(CommandInfo.CommandIndex == -1);
		// Allocate at lowest free index so that 'r.DoLazyStaticMeshUpdate' can shrink the TSparseArray more effectively
		CommandInfo.CommandIndex = DrawList.MeshDrawCommands.AddAtLowestFreeIndex(Initializer, DrawList.LowestFreeIndexSearchStart);
		return DrawList.MeshDrawCommands[CommandInfo.CommandIndex];
	}
}

void FCachedPassMeshDrawListContext::FinalizeCommand(
	const FMeshBatch& MeshBatch, 
	int32 BatchElementIndex,
	int32 DrawPrimitiveId,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode,
	FMeshDrawCommandSortKey SortKey,
	const FGraphicsMinimalPipelineStateInitializer& PipelineState,
	const FMeshProcessorShaders* ShadersForDebugging,
	FMeshDrawCommand& MeshDrawCommand)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FinalizeCachedMeshDrawCommand);

	FGraphicsMinimalPipelineStateId PipelineId;
	PipelineId = FGraphicsMinimalPipelineStateId::GetPersistentId(PipelineState);

	MeshDrawCommand.SetDrawParametersAndFinalize(MeshBatch, BatchElementIndex, PipelineId, ShadersForDebugging);

	if (bUseStateBuckets)
	{
		FSetElementId SetId = Scene.CachedMeshDrawCommandStateBuckets.FindId(MeshDrawCommand);

		if (SetId.IsValidId())
		{
			Scene.CachedMeshDrawCommandStateBuckets[SetId].Num++;
		}
		else
		{
			SetId = Scene.CachedMeshDrawCommandStateBuckets.Add(FMeshDrawCommandStateBucket(1, MeshDrawCommand));
		}

		check(CommandInfo.StateBucketId == -1);
		CommandInfo.StateBucketId = SetId.AsInteger();
		check(CommandInfo.CommandIndex == -1);
	}
	else
	{
		check(CommandInfo.CommandIndex != -1);
	}

	CommandInfo.SortKey = SortKey;
	CommandInfo.MeshFillMode = MeshFillMode;
	CommandInfo.MeshCullMode = MeshCullMode;
}

PassProcessorCreateFunction FPassProcessorManager::JumpTable[(int32)EShadingPath::Num][EMeshPass::Num] = {};
EMeshPassFlags FPassProcessorManager::Flags[(int32)EShadingPath::Num][EMeshPass::Num] = {};
