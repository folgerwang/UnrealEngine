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

TSet<FGraphicsMinimalPipelineStateInitializer> FGraphicsMinimalPipelineStateId::GlobalTable;
FCriticalSection GlobalTableCriticalSection;

const FMeshDrawCommandSortKey FMeshDrawCommandSortKey::Default = { 0 };

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
	int32 MaxSRVUsed = 0;
	FShaderResourceViewRHIParamRef SRVs[MAX_SRVs_PER_SHADER_STAGE] = {};
	int32 MaxUniformBufferUsed = 0;
	FUniformBufferRHIParamRef UniformBuffers[MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE] = {};
	int32 MaxTextureUsed = 0;
	FTextureRHIParamRef Textures[MAX_SRVs_PER_SHADER_STAGE] = {};
	int32 MaxSamplerUsed = 0;
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

	inline const FShaderResourceViewRHIParamRef* GetSRVStart() const
	{
		const uint8* SRVDataStart = Data + GetSRVOffset();
		return (const FShaderResourceViewRHIParamRef*)SRVDataStart;
	}

	inline const FTextureRHIParamRef* GetTextureStart() const
	{
		const uint8* TextureDataStart = Data + GetTextureOffset();
		return (const FTextureRHIParamRef*)TextureDataStart;
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
		checkSlow(Parameter.BaseIndex <= ARRAY_COUNT(ShaderBindingState.UniformBuffers));
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
		checkSlow(Parameter.BaseIndex <= ARRAY_COUNT(ShaderBindingState.Samplers));
		FSamplerStateRHIParamRef Sampler = SamplerBindings[SamplerIndex];

		if (Sampler != ShaderBindingState.Samplers[Parameter.BaseIndex])
		{
			RHICmdList.SetShaderSampler(Shader, Parameter.BaseIndex, Sampler);
			ShaderBindingState.Samplers[Parameter.BaseIndex] = Sampler;
			ShaderBindingState.MaxSamplerUsed = FMath::Max((int32)Parameter.BaseIndex, ShaderBindingState.MaxSamplerUsed);
		}
	}

	const FShaderResourceViewRHIParamRef* RESTRICT SRVBindings = SingleShaderBindings.GetSRVStart();
	const FShaderParameterInfo* RESTRICT SRVParameters = SingleShaderBindings.ParameterMapInfo.SRVs.GetData();
	const int32 NumSRVs = SingleShaderBindings.ParameterMapInfo.SRVs.Num();

	for (int32 SRVIndex = 0; SRVIndex < NumSRVs; SRVIndex++)
	{
		FShaderParameterInfo Parameter = SRVParameters[SRVIndex];
		checkSlow(Parameter.BaseIndex <= ARRAY_COUNT(ShaderBindingState.SRVs));
		FShaderResourceViewRHIParamRef SRV = SRVBindings[SRVIndex];

		if (SRV != ShaderBindingState.SRVs[Parameter.BaseIndex])
		{
			RHICmdList.SetShaderResourceViewParameter(Shader, Parameter.BaseIndex, SRV);
			ShaderBindingState.SRVs[Parameter.BaseIndex] = SRV;
			ShaderBindingState.MaxSRVUsed = FMath::Max((int32)Parameter.BaseIndex, ShaderBindingState.MaxSRVUsed);
		}
	}

	const FTextureRHIParamRef* RESTRICT TextureBindings = SingleShaderBindings.GetTextureStart();

	for (int32 TextureIndex = 0; TextureIndex < NumSRVs; TextureIndex++)
	{
		FShaderParameterInfo Parameter = SRVParameters[TextureIndex];
		checkSlow(Parameter.BaseIndex <= ARRAY_COUNT(ShaderBindingState.Textures));
		FTextureRHIParamRef Texture = TextureBindings[TextureIndex];

		if (Texture != ShaderBindingState.Textures[Parameter.BaseIndex])
		{
			RHICmdList.SetShaderTexture(Shader, Parameter.BaseIndex, Texture);
			ShaderBindingState.Textures[Parameter.BaseIndex] = Texture;
			ShaderBindingState.MaxTextureUsed = FMath::Max((int32)Parameter.BaseIndex, ShaderBindingState.MaxTextureUsed);
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

	const FShaderParameterInfo* RESTRICT SRVParameters = SingleShaderBindings.ParameterMapInfo.SRVs.GetData();
	const int32 NumSRVs = SingleShaderBindings.ParameterMapInfo.SRVs.Num();

	const FShaderResourceViewRHIParamRef* RESTRICT SRVBindings = SingleShaderBindings.GetSRVStart();
	const FTextureRHIParamRef* RESTRICT TextureBindings = SingleShaderBindings.GetTextureStart();

	for (int32 SRVIndex = 0; SRVIndex < NumSRVs; SRVIndex++)
	{
		FShaderParameterInfo Parameter = SRVParameters[SRVIndex];

		FShaderResourceViewRHIParamRef SRV = SRVBindings[SRVIndex];
		FTextureRHIParamRef Texture = TextureBindings[SRVIndex];

		// Check for both being non-null
		checkf(!(SRV != nullptr && Texture != nullptr), TEXT("SRV and Texture cannot be set to the same slot"));

		if (SRV != nullptr)
		{
			RHICmdList.SetShaderResourceViewParameter(Shader, Parameter.BaseIndex, SRV);
		}
		else if (Texture != nullptr)
		{
			RHICmdList.SetShaderTexture(Shader, Parameter.BaseIndex, Texture);
		}
		else
		{
			// Both are null, empty slot.
			RHICmdList.SetShaderResourceViewParameter(Shader, Parameter.BaseIndex, nullptr);
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
void FMeshDrawShaderBindings::SetOnRayTracingStructure(FRHICommandList& RHICmdList, FRayTracingSceneRHIParamRef Scene, uint32 InstanceIndex, uint32 SegmentIndex, FRayTracingPipelineStateRHIParamRef PipelineState, uint32 HitGroupIndex) const
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
		checkSlow(Parameter.BaseIndex <= ARRAY_COUNT(LocalUniformBuffers));
		FUniformBufferRHIParamRef UniformBuffer = UniformBufferBindings[UniformBufferIndex];
		if (Parameter.BaseIndex <= ARRAY_COUNT(LocalUniformBuffers))
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
	RHICmdList.SetRayTracingHitGroup(Scene, InstanceIndex, SegmentIndex, PipelineState, HitGroupIndex, NumUniformBuffersToSet, LocalUniformBuffers);
}
#endif // RHI_RAYTRACING

void FGraphicsMinimalPipelineStateId::Setup(const FGraphicsMinimalPipelineStateInitializer& InPipelineState)
{
	// Need to lock as this is called from multiple parallel tasks during mesh draw command generation or patching.
	FScopeLock Lock(&GlobalTableCriticalSection);

	FSetElementId TableId = GlobalTable.FindId(InPipelineState);

	if (!TableId.IsValidId())
	{
		// Note: grow-only table.  Assuming finite and small enough set of FGraphicsMinimalPipelineStateInitializer permutations.
		TableId = GlobalTable.Add(InPipelineState);
	}

	checkf(TableId.AsInteger() < MAX_int32, TEXT("FGraphicsMinimalPipelineStateId overflow!"));

	Id = TableId;
}

class FMeshDrawCommandStateCache
{
public:

	int32 PipelineId;
	FGraphicsMinimalPipelineStateInitializer PipelineState;
	uint32 StencilRef;
	FShaderBindingState ShaderBindings[SF_NumFrequencies];
	FVertexInputStream VertexStreams[MaxVertexElementCount];

	FMeshDrawCommandStateCache()
	{
		// Must init to impossible values to avoid filtering the first draw's state
		PipelineId = -1;
		StencilRef = -1;
	}

	inline void SetPipelineState(const FGraphicsMinimalPipelineStateInitializer& NewPipelineState, int32 NewPipelineId)
	{
		PipelineState = NewPipelineState;
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

			ShaderBinding.MaxSRVUsed = 0;

			for (int32 SlotIndex = 0; SlotIndex <= ShaderBinding.MaxUniformBufferUsed; SlotIndex++)
			{
				ShaderBinding.UniformBuffers[SlotIndex] = nullptr;
			}

			ShaderBinding.MaxUniformBufferUsed = 0;
			
			for (int32 SlotIndex = 0; SlotIndex <= ShaderBinding.MaxTextureUsed; SlotIndex++)
			{
				ShaderBinding.Textures[SlotIndex] = nullptr;
			}

			ShaderBinding.MaxTextureUsed = 0;

			for (int32 SlotIndex = 0; SlotIndex <= ShaderBinding.MaxSamplerUsed; SlotIndex++)
			{
				ShaderBinding.Samplers[SlotIndex] = nullptr;
			}

			ShaderBinding.MaxSamplerUsed = 0;
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

void FMeshDrawShaderBindings::Finalize(const FMeshDrawCommandDebugData& DebugData)
{
#if VALIDATE_MESH_COMMAND_BINDINGS
	const uint8* ShaderBindingDataPtr = GetData();

	for (int32 ShaderBindingsIndex = 0; ShaderBindingsIndex < ShaderLayouts.Num(); ShaderBindingsIndex++)
	{
		const FMeshDrawShaderBindingsLayout& ShaderLayout = ShaderLayouts[ShaderBindingsIndex];

		FMeshMaterialShader* Shader = DebugData.Shaders.GetShader(ShaderLayout.Frequency);
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

		const FShaderResourceViewRHIParamRef* SRVBindings = SingleShaderBindings.GetSRVStart();
		const FTextureRHIParamRef* TextureBindings = SingleShaderBindings.GetTextureStart();

		for (int32 BindingIndex = 0; BindingIndex < ShaderLayout.ParameterMapInfo.SRVs.Num(); BindingIndex++)
		{
			FShaderParameterInfo ParameterInfo = ShaderLayout.ParameterMapInfo.SRVs[BindingIndex];
			FShaderResourceViewRHIParamRef SRVValue = SRVBindings[BindingIndex];
			FTextureRHIParamRef TextureValue = TextureBindings[BindingIndex];
			ensureMsgf(SRVValue || TextureValue, TEXT("Shader %s with vertex factory %s never set texture or SRV at BaseIndex %u.  This can cause GPU hangs, depending on how the shader uses it."), 
				Shader->GetType()->GetName(), 
				Shader->GetVertexFactoryType()->GetName(),
				ParameterInfo.BaseIndex);
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

void FMeshDrawCommand::SetShaders(FVertexDeclarationRHIParamRef VertexDeclaration, const FMeshProcessorShaders& Shaders)
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
void FMeshDrawCommand::SetRayTracingShaders(const FMeshProcessorShaders& Shaders)
{
	check(Shaders.RayHitGroupShader)
	RayTracingMaterialLibraryIndex = Shaders.RayHitGroupShader->GetRayTracingMaterialLibraryIndex();
	ShaderBindings.Initialize(Shaders);
}
#endif // RHI_RAYTRACING

void FMeshDrawCommand::SetDrawParametersAndFinalize(const FMeshBatch& MeshBatch, int32 BatchElementIndex, int32 BatchInstanceFactor, bool bDoSetupPsoStateForRasterization)
{
	const FMeshBatchElement& BatchElement = MeshBatch.Elements[BatchElementIndex];

	check(!BatchElement.IndexBuffer || (BatchElement.IndexBuffer && BatchElement.IndexBuffer->IsInitialized() && BatchElement.IndexBuffer->IndexBufferRHI));
	checkSlow(!BatchElement.bIsInstanceRuns);
	IndexBuffer = BatchElement.IndexBuffer ? BatchElement.IndexBuffer->IndexBufferRHI : nullptr;
	FirstIndex = BatchElement.FirstIndex;
	NumPrimitives = BatchElement.NumPrimitives;
	NumInstances = BatchElement.NumInstances;
	InstanceFactor = BatchInstanceFactor;
	BaseVertexIndex = BatchElement.BaseVertexIndex;
	NumVertices = BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1;
	IndirectArgsBuffer = BatchElement.IndirectArgsBuffer;

	int32 SegmentIndex = MeshBatch.SegmentIndex + BatchElementIndex;
	RayTracedSegmentIndex = (SegmentIndex < UINT8_MAX) ? uint8(MeshBatch.SegmentIndex + BatchElementIndex) : UINT8_MAX;

	Finalize(bDoSetupPsoStateForRasterization);
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
	FRHICommandList& RHICmdList, 
	FMeshDrawCommandStateCache& RESTRICT StateCache)
{
	checkSlow(MeshDrawCommand.CachedPipelineId.IsValid());
		
#if WANTS_DRAW_MESH_EVENTS
	TDrawEvent<FRHICommandList> MeshEvent;

	if (GShowMaterialDrawEvents)
	{
		const uint32 Instances = MeshDrawCommand.NumInstances * MeshDrawCommand.InstanceFactor;
		if (Instances > 1)
		{
			BEGIN_DRAW_EVENTF(
				RHICmdList,
				MaterialEvent,
				MeshEvent,
				TEXT("%s %u instances"),
				*MeshDrawCommand.DrawEventName,
				Instances);
		}
		else
		{
			BEGIN_DRAW_EVENTF(RHICmdList, MaterialEvent, MeshEvent, *MeshDrawCommand.DrawEventName);
		}
	}
#endif

	if (MeshDrawCommand.CachedPipelineId.GetId() != StateCache.PipelineId)
	{
		FGraphicsPipelineStateInitializer GraphicsPSOInit = MeshDrawCommand.PipelineState;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
		StateCache.SetPipelineState(MeshDrawCommand.PipelineState, MeshDrawCommand.CachedPipelineId.GetId());
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

	MeshDrawCommand.ShaderBindings.SetOnCommandList(RHICmdList, MeshDrawCommand.PipelineState.BoundShaderState, StateCache.ShaderBindings);

	if (MeshDrawCommand.IndexBuffer)
	{
		if (MeshDrawCommand.IndirectArgsBuffer)
		{
			RHICmdList.DrawIndexedPrimitiveIndirect(
				MeshDrawCommand.IndexBuffer, 
				MeshDrawCommand.IndirectArgsBuffer, 
				0
				);
		}
		else
		{
			RHICmdList.DrawIndexedPrimitive(
				MeshDrawCommand.IndexBuffer,
				MeshDrawCommand.BaseVertexIndex,
				0,
				MeshDrawCommand.NumVertices,
				MeshDrawCommand.FirstIndex,
				MeshDrawCommand.NumPrimitives,
				MeshDrawCommand.NumInstances * MeshDrawCommand.InstanceFactor
			);
		}
	}
	else
	{
		RHICmdList.DrawPrimitive(
			MeshDrawCommand.BaseVertexIndex + MeshDrawCommand.FirstIndex,
			MeshDrawCommand.NumPrimitives,
			MeshDrawCommand.NumInstances * MeshDrawCommand.InstanceFactor
		);
	}

}

void SubmitMeshDrawCommands(
	const FMeshCommandOneFrameArray& VisibleMeshDrawCommands,
	FVertexBufferRHIParamRef PrimitiveIdsBuffer,
	int32 BasePrimitiveIdsOffset,
	bool bDynamicInstancing,
	FRHICommandList& RHICmdList)
{
	SubmitMeshDrawCommandsRange(VisibleMeshDrawCommands, PrimitiveIdsBuffer, BasePrimitiveIdsOffset, bDynamicInstancing, 0, VisibleMeshDrawCommands.Num(), RHICmdList);
}

void SubmitMeshDrawCommandsRange(
	const FMeshCommandOneFrameArray& VisibleMeshDrawCommands,
	FVertexBufferRHIParamRef PrimitiveIdsBuffer,
	int32 BasePrimitiveIdsOffset,
	bool bDynamicInstancing,
	int32 StartIndex,
	int32 NumMeshDrawCommands,
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
		FMeshDrawCommand::SubmitDraw(*VisibleMeshDrawCommand.MeshDrawCommand, PrimitiveIdsBuffer, PrimitiveIdBufferOffset, RHICmdList, StateCache);
	}
}

void DrawDynamicMeshPassPrivate(
	const FSceneView& View,
	FRHICommandList& RHICmdList,
	FMeshCommandOneFrameArray& VisibleMeshDrawCommands,
	FDynamicMeshDrawCommandStorage& DynamicMeshDrawCommandStorage)
{
	if (VisibleMeshDrawCommands.Num() > 0)
	{
		FVertexBufferRHIParamRef PrimitiveIdVertexBuffer = nullptr;

		SortPassMeshDrawCommands(View.GetFeatureLevel(), VisibleMeshDrawCommands, DynamicMeshDrawCommandStorage, PrimitiveIdVertexBuffer);

		const bool bDynamicInstancing = IsDynamicInstancingEnabled() && UseGPUScene(GMaxRHIShaderPlatform, View.GetFeatureLevel());

		SubmitMeshDrawCommandsRange(VisibleMeshDrawCommands, PrimitiveIdVertexBuffer, 0, bDynamicInstancing, 0, VisibleMeshDrawCommands.Num(), RHICmdList);
	}
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

void FMeshPassProcessor::SetDrawCommandEvent(const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMaterial& RESTRICT MaterialResource, FMeshDrawCommand& MeshDrawCommand) const
{
#if WANTS_DRAW_MESH_EVENTS
	if (GShowMaterialDrawEvents)
	{
		if (PrimitiveSceneProxy)
		{
			MeshDrawCommand.DrawEventName = FString::Printf(
				TEXT("%s %s"),
				// Note: this is the parent's material name, not the material instance
				*MaterialResource.GetFriendlyName(),
				PrimitiveSceneProxy->GetResourceName().IsValid() ? *PrimitiveSceneProxy->GetResourceName().ToString() : TEXT(""));
		}
		else
		{
			MeshDrawCommand.DrawEventName = MaterialResource.GetFriendlyName();
		}
	}
#endif
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
{}

FMeshDrawCommand& FCachedPassMeshDrawListContext::AddCommand(const FMeshDrawCommand& Initializer)
{
	// Only one FMeshDrawCommand supported per FStaticMesh in a pass
	check(CommandInfo.CommandIndex == -1);
	CommandInfo.CommandIndex = DrawList.MeshDrawCommands.Add(Initializer);
	return DrawList.MeshDrawCommands[CommandInfo.CommandIndex];
}

void FCachedPassMeshDrawListContext::FinalizeCommand(
	const FMeshBatch& MeshBatch, 
	int32 BatchElementIndex,
	int32 DrawPrimitiveId,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode,
	int32 InstanceFactor,
	FMeshDrawCommandSortKey SortKey,
	FMeshDrawCommand& MeshDrawCommand,
	bool bDoSetupPsoStateForRasterization)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FinalizeCachedMeshDrawCommand);

	MeshDrawCommand.SetDrawParametersAndFinalize(MeshBatch, BatchElementIndex, InstanceFactor, bDoSetupPsoStateForRasterization);

	check(CommandInfo.CommandIndex != -1);
	const bool bCanUseGPUScene = UseGPUScene(GMaxRHIShaderPlatform, GMaxRHIFeatureLevel);

	if (bCanUseGPUScene /* && bDoSetupPsoStateForRasterization*/)
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

		CommandInfo.StateBucketId = SetId.AsInteger();
	}
	CommandInfo.SortKey = SortKey;
	CommandInfo.MeshFillMode = MeshFillMode;
	CommandInfo.MeshCullMode = MeshCullMode;
}

PassProcessorCreateFunction FPassProcessorManager::JumpTable[(int32)EShadingPath::Num][EMeshPass::Num] = {};
EMeshPassFlags FPassProcessorManager::Flags[(int32)EShadingPath::Num][EMeshPass::Num] = {};
