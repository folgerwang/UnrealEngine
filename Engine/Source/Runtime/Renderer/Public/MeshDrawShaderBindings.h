// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshDrawShaderBindings.h: 
=============================================================================*/

#pragma once

// Whether to assert when mesh command shader bindings were not set by the pass processor.
// Enabled by default in debug
#define VALIDATE_MESH_COMMAND_BINDINGS DO_GUARD_SLOW

/** Stores the number of each resource type that will need to be bound to a single shader, computed during shader reflection. */
class FMeshDrawShaderBindingsLayout
{
public:
	EShaderFrequency Frequency : SF_NumBits + 1;
	const FShaderParameterMapInfo& ParameterMapInfo;

	FMeshDrawShaderBindingsLayout(const FShader* Shader) :
		ParameterMapInfo(Shader->GetParameterMapInfo())
	{
		check(Shader);
		Frequency = (EShaderFrequency)Shader->GetTarget().Frequency;
		checkSlow((EShaderFrequency)Frequency == (EShaderFrequency)Shader->GetTarget().Frequency);
	}

	bool operator==(const FMeshDrawShaderBindingsLayout& Rhs) const
	{
		return Frequency == Rhs.Frequency
			&& ParameterMapInfo == Rhs.ParameterMapInfo;
	}

	inline uint32 GetDataSizeBytes() const
	{
		uint32 DataSize = sizeof(void*) * 
			(ParameterMapInfo.UniformBuffers.Num() 
			+ ParameterMapInfo.TextureSamplers.Num() 
			+ ParameterMapInfo.SRVs.Num());

		// Allocate a bit for each SRV tracking whether it is a FTextureRHIParamRef or FShaderResourceViewRHIParamRef
		DataSize += FMath::DivideAndRoundUp(ParameterMapInfo.SRVs.Num(), 8);

		for (int32 LooseBufferIndex = 0; LooseBufferIndex < ParameterMapInfo.LooseParameterBuffers.Num(); LooseBufferIndex++)
		{
			DataSize += ParameterMapInfo.LooseParameterBuffers[LooseBufferIndex].BufferSize;
		}

		// Align to pointer size so subsequent packed shader bindings will have their pointers aligned
		return Align(DataSize, sizeof(void*));
	}

protected:

	// Note: pointers first in layout, so they stay aligned
	inline uint32 GetUniformBufferOffset() const
	{
		return 0;
	}

	inline uint32 GetSamplerOffset() const
	{
		return ParameterMapInfo.UniformBuffers.Num() * sizeof(FUniformBufferRHIParamRef);
	}

	inline uint32 GetSRVOffset() const
	{
		return GetSamplerOffset() 
			+ ParameterMapInfo.TextureSamplers.Num() * sizeof(FSamplerStateRHIParamRef);
	}

	inline uint32 GetSRVTypeOffset() const
	{
		return GetSRVOffset() 
			+ ParameterMapInfo.SRVs.Num() * sizeof(FShaderResourceViewRHIParamRef);
	}

	inline uint32 GetLooseDataOffset() const
	{
		return GetSRVTypeOffset()
			+ FMath::DivideAndRoundUp(ParameterMapInfo.SRVs.Num(), 8);
	}

	friend class FMeshDrawShaderBindings;
};

class FMeshDrawSingleShaderBindings : public FMeshDrawShaderBindingsLayout
{
public:
	FMeshDrawSingleShaderBindings(const FMeshDrawShaderBindingsLayout& InLayout, uint8* InData) :
		FMeshDrawShaderBindingsLayout(InLayout)
	{
		Data = InData;
	}

	template<typename UniformBufferStructType>
	void Add(const TShaderUniformBufferParameter<UniformBufferStructType>& Parameter, const TUniformBufferRef<UniformBufferStructType>& Value)
	{
		checkfSlow(Parameter.IsInitialized(), TEXT("Parameter was not serialized"));

		if (Parameter.IsBound())
		{
			checkf(Value.GetReference(), TEXT("Attempted to set null uniform buffer for type %s on %s"), UniformBufferStructType::StaticStructMetadata.GetStructTypeName(), GetShaderFrequencyString(Frequency));
			checkfSlow(Value.GetReference()->IsValid(), TEXT("Attempted to set already deleted uniform buffer for type %s on %s"), UniformBufferStructType::StaticStructMetadata.GetStructTypeName(), GetShaderFrequencyString(Frequency));
			WriteBindingUniformBuffer(Value.GetReference(), Parameter.GetBaseIndex());
		}
	}

	template<typename UniformBufferStructType>
	void Add(const TShaderUniformBufferParameter<UniformBufferStructType>& Parameter, const TUniformBuffer<UniformBufferStructType>& Value)
	{
		checkfSlow(Parameter.IsInitialized(), TEXT("Parameter was not serialized"));

		if (Parameter.IsBound())
		{
			checkf(Value.GetUniformBufferRHI(), TEXT("Attempted to set null uniform buffer for type %s on %s"), UniformBufferStructType::StaticStructMetadata.GetStructTypeName(), GetShaderFrequencyString(Frequency));
			checkfSlow(Value.GetUniformBufferRHI()->IsValid(), TEXT("Attempted to set already deleted uniform buffer for type %s on %s"), UniformBufferStructType::StaticStructMetadata.GetStructTypeName(), GetShaderFrequencyString(Frequency));
			WriteBindingUniformBuffer(Value.GetUniformBufferRHI(), Parameter.GetBaseIndex());
		}
	}

	void Add(FShaderUniformBufferParameter Parameter, FUniformBufferRHIParamRef Value)
	{
		checkfSlow(Parameter.IsInitialized(), TEXT("Parameter was not serialized"));

		if (Parameter.IsBound())
		{
			checkf(Value, TEXT("Attempted to set null uniform buffer with unknown type on %s"), GetShaderFrequencyString(Frequency));
			checkfSlow(Value->IsValid(), TEXT("Attempted to set already deleted uniform buffer of type %s on %s"), *Value->GetLayout().GetDebugName().ToString(), GetShaderFrequencyString(Frequency));
			WriteBindingUniformBuffer(Value, Parameter.GetBaseIndex());
		}
	}

	void Add(FShaderResourceParameter Parameter, FShaderResourceViewRHIParamRef Value)
	{
		checkfSlow(Parameter.IsInitialized(), TEXT("Parameter was not serialized"));

		if (Parameter.IsBound())
		{
			checkf(Value, TEXT("Attempted to set null SRV on slot %u of %s"), Parameter.GetBaseIndex(), GetShaderFrequencyString(Frequency));
			checkfSlow(Value->IsValid(), TEXT("Attempted to set already deleted SRV on slot %u of %s"), Parameter.GetBaseIndex(), GetShaderFrequencyString(Frequency));
			WriteBindingSRV(Value, Parameter.GetBaseIndex());
		}
	}

	void AddTexture(
		FShaderResourceParameter TextureParameter,
		FShaderResourceParameter SamplerParameter,
		FSamplerStateRHIParamRef SamplerStateRHI,
		FTextureRHIParamRef TextureRHI)
	{
		checkfSlow(TextureParameter.IsInitialized(), TEXT("Parameter was not serialized"));
		checkfSlow(SamplerParameter.IsInitialized(), TEXT("Parameter was not serialized"));

		if (TextureParameter.IsBound())
		{
			checkf(TextureRHI, TEXT("Attempted to set null Texture on slot %u of %s"), TextureParameter.GetBaseIndex(), GetShaderFrequencyString(Frequency));
			WriteBindingTexture(TextureRHI, TextureParameter.GetBaseIndex());
		}

		if (SamplerParameter.IsBound())
		{
			checkf(SamplerStateRHI, TEXT("Attempted to set null Sampler on slot %u of %s"), SamplerParameter.GetBaseIndex(), GetShaderFrequencyString(Frequency));
			WriteBindingSampler(SamplerStateRHI, SamplerParameter.GetBaseIndex());
		}
	}

	template<class ParameterType>
	void Add(FShaderParameter Parameter, const ParameterType& Value)
	{
		static_assert(!TIsPointer<ParameterType>::Value, "Passing by pointer is not valid.");
		checkfSlow(Parameter.IsInitialized(), TEXT("Parameter was not serialized"));

		if (Parameter.IsBound())
		{
			bool bFoundParameter = false;
			uint8* LooseDataOffset = GetLooseDataStart();

			for (int32 LooseBufferIndex = 0; LooseBufferIndex < ParameterMapInfo.LooseParameterBuffers.Num(); LooseBufferIndex++)
			{
				const FShaderLooseParameterBufferInfo& LooseParameterBuffer = ParameterMapInfo.LooseParameterBuffers[LooseBufferIndex];

				if (LooseParameterBuffer.BufferIndex == Parameter.GetBufferIndex())
				{
					for (int32 LooseParameterIndex = 0; LooseParameterIndex < LooseParameterBuffer.Parameters.Num(); LooseParameterIndex++)
					{
						FShaderParameterInfo LooseParameter = LooseParameterBuffer.Parameters[LooseParameterIndex];

						if (Parameter.GetBaseIndex() == LooseParameter.BaseIndex)
						{
							checkSlow(Parameter.GetNumBytes() == LooseParameter.Size);
							ensureMsgf(sizeof(ParameterType) == Parameter.GetNumBytes(), TEXT("Attempted to set fewer bytes than the shader required.  Setting %u bytes on loose parameter at BaseIndex %u, Size %u.  This can cause GPU hangs, depending on usage."), sizeof(ParameterType), Parameter.GetBaseIndex(), Parameter.GetNumBytes());
							const int32 NumBytesToSet = FMath::Min<int32>(sizeof(ParameterType), Parameter.GetNumBytes());
							FMemory::Memcpy(LooseDataOffset, &Value, NumBytesToSet);
							bFoundParameter = true;
							break;
						}

						LooseDataOffset += LooseParameter.Size;
					}
					break;
				}

				LooseDataOffset += LooseParameterBuffer.BufferSize;
			}

			checkfSlow(bFoundParameter, TEXT("Attempted to set loose parameter at BaseIndex %u, Size %u which was never in the shader's parameter map."), Parameter.GetBaseIndex(), Parameter.GetNumBytes());
		}
	}

private:
	uint8* Data;

	inline FUniformBufferRHIParamRef* GetUniformBufferStart() const
	{
		return (FUniformBufferRHIParamRef*)(Data + GetUniformBufferOffset());
	}

	inline FSamplerStateRHIParamRef* GetSamplerStart() const
	{
		uint8* SamplerDataStart = Data + GetSamplerOffset();
		return (FSamplerStateRHIParamRef*)SamplerDataStart;
	}

	inline FRHIResource** GetSRVStart() const
	{
		uint8* SRVDataStart = Data + GetSRVOffset();
		checkfSlow(Align(*SRVDataStart, sizeof(void*)) == *SRVDataStart, TEXT("FMeshDrawSingleShaderBindings should have been laid out so that stored pointers are aligned"));
		return (FRHIResource**)SRVDataStart;
	}

	inline uint8* GetSRVTypeStart() const
	{
		uint8* SRVTypeDataStart = Data + GetSRVTypeOffset();
		return SRVTypeDataStart;
	}

	inline uint8* GetLooseDataStart() const
	{
		uint8* LooseDataStart = Data + GetLooseDataOffset();
		return LooseDataStart;
	}

	inline void WriteBindingUniformBuffer(FUniformBufferRHIParamRef Value, uint32 BaseIndex)
	{
		int32 FoundIndex = -1;

		for (int32 SearchIndex = 0; SearchIndex < ParameterMapInfo.UniformBuffers.Num(); SearchIndex++)
		{
			FShaderParameterInfo Parameter = ParameterMapInfo.UniformBuffers[SearchIndex];

			if (Parameter.BaseIndex == BaseIndex)
			{
				FoundIndex = SearchIndex;
				break;
			}
		}

		if (FoundIndex >= 0)
		{
#if VALIDATE_UNIFORM_BUFFER_LIFETIME
			if (GetUniformBufferStart()[FoundIndex])
			{
				GetUniformBufferStart()[FoundIndex]->NumMeshCommandReferencesForDebugging--;
				check(GetUniformBufferStart()[FoundIndex]->NumMeshCommandReferencesForDebugging >= 0);
			}
			Value->NumMeshCommandReferencesForDebugging++;
#endif

			GetUniformBufferStart()[FoundIndex] = Value;
		}

		checkfSlow(FoundIndex >= 0, TEXT("Attempted to set a uniform buffer at BaseIndex %u which was never in the shader's parameter map."), BaseIndex);
	}

	inline void WriteBindingSampler(FSamplerStateRHIParamRef Value, uint32 BaseIndex)
	{
		int32 FoundIndex = -1;

		for (int32 SearchIndex = 0; SearchIndex < ParameterMapInfo.TextureSamplers.Num(); SearchIndex++)
		{
			FShaderParameterInfo Parameter = ParameterMapInfo.TextureSamplers[SearchIndex];

			if (Parameter.BaseIndex == BaseIndex)
			{
				FoundIndex = SearchIndex;
				break;
			}
		}

		if (FoundIndex >= 0)
		{
			GetSamplerStart()[FoundIndex] = Value;
		}

		checkfSlow(FoundIndex >= 0, TEXT("Attempted to set a texture sampler at BaseIndex %u which was never in the shader's parameter map."), BaseIndex);
	}

	inline void WriteBindingSRV(FShaderResourceViewRHIParamRef Value, uint32 BaseIndex)
	{
		int32 FoundIndex = -1;

		for (int32 SearchIndex = 0; SearchIndex < ParameterMapInfo.SRVs.Num(); SearchIndex++)
		{
			FShaderParameterInfo Parameter = ParameterMapInfo.SRVs[SearchIndex];

			if (Parameter.BaseIndex == BaseIndex)
			{
				FoundIndex = SearchIndex;
				break;
			}
		}

		if (FoundIndex >= 0)
		{
			uint32 TypeByteIndex = FoundIndex / 8;
			uint32 TypeBitIndex = FoundIndex - TypeByteIndex;
			GetSRVTypeStart()[TypeByteIndex] |= 1 << TypeBitIndex;
			GetSRVStart()[FoundIndex] = Value;
		}

		checkfSlow(FoundIndex >= 0, TEXT("Attempted to set SRV at BaseIndex %u which was never in the shader's parameter map."), BaseIndex);
	}

	inline void WriteBindingTexture(FTextureRHIParamRef Value, uint32 BaseIndex)
	{
		int32 FoundIndex = -1;

		for (int32 SearchIndex = 0; SearchIndex < ParameterMapInfo.SRVs.Num(); SearchIndex++)
		{
			FShaderParameterInfo Parameter = ParameterMapInfo.SRVs[SearchIndex];

			if (Parameter.BaseIndex == BaseIndex)
			{
				FoundIndex = SearchIndex;
				break;
			}
		}

		if (FoundIndex >= 0)
		{
			GetSRVStart()[FoundIndex] = Value;
		}

		checkfSlow(FoundIndex >= 0, TEXT("Attempted to set Texture at BaseIndex %u which was never in the shader's parameter map."), BaseIndex);
	}

	friend class FMeshDrawShaderBindings;
};
