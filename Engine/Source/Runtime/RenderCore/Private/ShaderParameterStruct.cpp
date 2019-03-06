// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameterStruct.cpp: Shader parameter struct implementations.
=============================================================================*/

#include "ShaderParameterStruct.h"


/** Context of binding a map. */
struct FShaderParameterStructBindingContext
{
	// Shader having its parameter bound.
	const FShader* Shader;

	// Bindings to bind.
	FShaderParameterBindings* Bindings;

	// The shader parameter map from the compilation.
	const FShaderParameterMap* ParametersMap;

	// Map of global shader name that were bound to C++ members.
	TMap<FString, FString> ShaderGlobalScopeBindings;

	// C++ name of the render target binding slot.
	FString RenderTargetBindingSlotCppName;

	// Whether this is for legacy shader parameter settings, or root shader parameter structures/
	bool bUseRootShaderParameters;


	void Bind(
		const FShaderParametersMetadata& StructMetaData,
		const TCHAR* MemberPrefix,
		uint32 GeneralByteOffset)
	{
		const TArray<FShaderParametersMetadata::FMember>& StructMembers = StructMetaData.GetMembers();

		for (const FShaderParametersMetadata::FMember& Member : StructMembers)
		{
			EUniformBufferBaseType BaseType = Member.GetBaseType();

			FString CppName = FString::Printf(TEXT("%s::%s"), StructMetaData.GetStructTypeName(), Member.GetName());

			// Ignore rasterizer binding slots entirely since this actually have nothing to do with a shader.
			if (BaseType == UBMT_RENDER_TARGET_BINDING_SLOTS)
			{
				if (!RenderTargetBindingSlotCppName.IsEmpty())
				{
					UE_LOG(LogShaders, Fatal, TEXT("Render target binding slots collision: %s & %s"),
						*RenderTargetBindingSlotCppName, *CppName);
				}
				RenderTargetBindingSlotCppName = CppName;
				continue;
			}

			// Compute the shader member name to look for according to nesting.
			FString ShaderBindingName = FString::Printf(TEXT("%s%s"), MemberPrefix, Member.GetName());

			uint16 ByteOffset = uint16(GeneralByteOffset + Member.GetOffset());
			check(uint32(ByteOffset) == GeneralByteOffset + Member.GetOffset());

			const uint32 ArraySize = Member.GetNumElements();
			const bool bIsArray = ArraySize > 0;
			const bool bIsRHIResource = (
				BaseType == UBMT_TEXTURE ||
				BaseType == UBMT_SRV ||
				BaseType == UBMT_SAMPLER);
			const bool bIsRDGResource = IsRDGResourceReferenceShaderParameterType(BaseType) && BaseType != UBMT_RDG_BUFFER;
			const bool bIsVariableNativeType = (
				BaseType == UBMT_BOOL ||
				BaseType == UBMT_INT32 ||
				BaseType == UBMT_UINT32 ||
				BaseType == UBMT_FLOAT32);

			if (BaseType == UBMT_NESTED_STRUCT || BaseType == UBMT_INCLUDED_STRUCT)
			{
				checkf(!bIsArray, TEXT("Array of structure bindings is not supported."));
				FString NewPrefix = FString::Printf(TEXT("%s%s_"), MemberPrefix, Member.GetName());
				Bind(
					*Member.GetStructMetadata(),
					/* MemberPrefix = */ BaseType == UBMT_INCLUDED_STRUCT ? MemberPrefix : *NewPrefix,
					/* GeneralByteOffset = */ ByteOffset);
				continue;
			}
			else if (BaseType == UBMT_REFERENCED_STRUCT)
			{
				checkf(!bIsArray, TEXT("Array of referenced structure is not supported, because the structure is globally unicaly named."));
				// The member name of a globally referenced struct is the not name on the struct.
				ShaderBindingName = Member.GetStructMetadata()->GetShaderVariableName();
			}
			else if (BaseType == UBMT_RDG_BUFFER)
			{
				// RHI does not support setting a buffer as a shader parameter.
				check(!bIsArray);
				if( ParametersMap->ContainsParameterAllocation(*ShaderBindingName) )
				{
					UE_LOG(LogShaders, Fatal, TEXT("%s can't bind shader parameter %s as buffer. Use buffer SRV for reading in shader."), *CppName, *ShaderBindingName);
				}
				continue;
			}
			else if (bUseRootShaderParameters && bIsVariableNativeType)
			{
				// Constants are stored in the root shader parameter cbuffer when bUseRootShaderParameters == true.
				continue;
			}

			const bool bIsResourceArray = bIsArray && (bIsRHIResource || bIsRDGResource);

			for (uint32 ArrayElementId = 0; ArrayElementId < (bIsResourceArray ? ArraySize : 1u); ArrayElementId++)
			{
				FString ElementShaderBindingName;
				if (bIsResourceArray)
				{
					if (0) // HLSLCC does not support array of resources.
						ElementShaderBindingName = FString::Printf(TEXT("%s[%d]"), *ShaderBindingName, ArrayElementId);
					else
						ElementShaderBindingName = FString::Printf(TEXT("%s_%d"), *ShaderBindingName, ArrayElementId);
				}
				else
				{
					ElementShaderBindingName = ShaderBindingName;
				}

				if (ShaderGlobalScopeBindings.Contains(ElementShaderBindingName))
				{
					UE_LOG(LogShaders, Fatal, TEXT("%s can't bind shader parameter %s, because it has already be bound by %s."), *CppName, *ElementShaderBindingName, **ShaderGlobalScopeBindings.Find(ShaderBindingName));
				}

				uint16 BufferIndex, BaseIndex, BoundSize;
				if (!ParametersMap->FindParameterAllocation(*ElementShaderBindingName, BufferIndex, BaseIndex, BoundSize))
				{
					continue;
				}
				ShaderGlobalScopeBindings.Add(ElementShaderBindingName, CppName);

				if (bIsVariableNativeType)
				{
					checkf(ArrayElementId == 0, TEXT("The entire array should be bound instead for RHI parameter submission performance."));
					uint32 ByteSize = Member.GetMemberSize();

					FShaderParameterBindings::FParameter Parameter;
					Parameter.BufferIndex = BufferIndex;
					Parameter.BaseIndex = BaseIndex;
					Parameter.ByteOffset = ByteOffset;
					Parameter.ByteSize = BoundSize;

					if (uint32(BoundSize) > ByteSize)
					{
						UE_LOG(LogShaders, Fatal, TEXT("The size required to bind shader %s's (Permutation Id %d) struct %s parameter %s is %i bytes, smaller than %s's %i bytes."),
							Shader->GetType()->GetName(), Shader->GetPermutationId(), StructMetaData.GetStructTypeName(),
							*ElementShaderBindingName, BoundSize, *CppName, ByteSize);
					}

					Bindings->Parameters.Add(Parameter);
				}
				else if (BaseType == UBMT_REFERENCED_STRUCT)
				{
					check(!bIsArray);
					FShaderParameterBindings::FParameterStructReference Parameter;
					Parameter.BufferIndex = BufferIndex;
					Parameter.ByteOffset = ByteOffset;

					Bindings->ParameterReferences.Add(Parameter);
				}
				else if (bIsRHIResource || bIsRDGResource)
				{
					FShaderParameterBindings::FResourceParameter Parameter;
					Parameter.BaseIndex = BaseIndex;
					Parameter.ByteOffset = ByteOffset + ArrayElementId * SHADER_PARAMETER_POINTER_ALIGNMENT;

					checkf(
						BoundSize == 1,
						TEXT("The shader compiler should give precisely which elements of an array did not get compiled out, ")
						TEXT("for optimal automatic render graph pass dependency with ClearUnusedGraphResources()."));

					if (BaseType == UBMT_TEXTURE)
						Bindings->Textures.Add(Parameter);
					else if (BaseType == UBMT_SRV)
						Bindings->SRVs.Add(Parameter);
					else if (BaseType == UBMT_SAMPLER)
						Bindings->Samplers.Add(Parameter);
					else if (BaseType == UBMT_RDG_TEXTURE)
						Bindings->GraphTextures.Add(Parameter);
					else if (BaseType == UBMT_RDG_TEXTURE_SRV || BaseType == UBMT_RDG_BUFFER_SRV)
						Bindings->GraphSRVs.Add(Parameter);
					else // if (BaseType == UBMT_RDG_TEXTURE_UAV || BaseType == UBMT_RDG_BUFFER_UAV)
						Bindings->GraphUAVs.Add(Parameter);
				}
				else
				{
					checkf(0, TEXT("Unexpected base type for a shader parameter struct member."));
				}
			} // for (uint32 ArrayElementId = 0; ArrayElementId < (bIsResourceArray ? ArraySize : 1u); ArrayElementId++)
		} // for (const FShaderParametersMetadata::FMember& Member : StructMembers)
	} // void Bind()
}; // struct FShaderParameterStructBindingContext

void FShaderParameterBindings::BindForLegacyShaderParameters(const FShader* Shader, const FShaderParameterMap& ParametersMap, const FShaderParametersMetadata& StructMetaData, bool bShouldBindEverything)
{
	checkf(StructMetaData.GetSize() < (1 << (sizeof(uint16) * 8)), TEXT("Shader parameter structure can only have a size < 65536 bytes."));
	check(this == &Shader->Bindings);

	FShaderParameterStructBindingContext BindingContext;
	BindingContext.Shader = Shader;
	BindingContext.Bindings = this;
	BindingContext.ParametersMap = &ParametersMap;
	BindingContext.bUseRootShaderParameters = false;
	BindingContext.Bind(
		StructMetaData,
		/* MemberPrefix = */ TEXT(""),
		/* ByteOffset = */ 0);

	RootParameterBufferIndex = kInvalidBufferIndex;

	TArray<FString> AllParameterNames;
	ParametersMap.GetAllParameterNames(AllParameterNames);
	if (bShouldBindEverything && BindingContext.ShaderGlobalScopeBindings.Num() != AllParameterNames.Num())
	{
		UE_LOG(LogShaders, Error, TEXT("%i shader parameters have not been bound for %s:"),
			AllParameterNames.Num() - BindingContext.ShaderGlobalScopeBindings.Num(),
			Shader->GetType()->GetName());

		for (const FString& GlobalParameterName : AllParameterNames)
		{
			if (!BindingContext.ShaderGlobalScopeBindings.Contains(GlobalParameterName))
			{
				UE_LOG(LogShaders, Error, TEXT("  %s"), *GlobalParameterName);
			}
		}

		UE_LOG(LogShaders, Fatal, TEXT("Unable to bind all shader parameters of %s."),
			Shader->GetType()->GetName());
	}
}

void FShaderParameterBindings::BindForRootShaderParameters(const FShader* Shader, const FShaderParameterMap& ParametersMap)
{
	check(this == &Shader->Bindings);
	check(Shader->GetType()->GetRootParametersMetadata());

	const FShaderParametersMetadata& StructMetaData = *Shader->GetType()->GetRootParametersMetadata();
	checkf(StructMetaData.GetSize() < (1 << (sizeof(uint16) * 8)), TEXT("Shader parameter structure can only have a size < 65536 bytes."));

	FShaderParameterStructBindingContext BindingContext;
	BindingContext.Shader = Shader;
	BindingContext.Bindings = this;
	BindingContext.ParametersMap = &ParametersMap;
	BindingContext.bUseRootShaderParameters = true;
	BindingContext.Bind(
		StructMetaData,
		/* MemberPrefix = */ TEXT(""),
		/* ByteOffset = */ 0);

	// Binds the uniform buffer that contains the root shader parameters.
	{
		const TCHAR* ShaderBindingName = FShaderParametersMetadata::kRootUniformBufferBindingName;
		uint16 BufferIndex, BaseIndex, BoundSize;
		if (ParametersMap.FindParameterAllocation(ShaderBindingName, BufferIndex, BaseIndex, BoundSize))
		{
			BindingContext.ShaderGlobalScopeBindings.Add(ShaderBindingName, ShaderBindingName);
			RootParameterBufferIndex = BufferIndex;
		}
		else
		{
			check(RootParameterBufferIndex == FShaderParameterBindings::kInvalidBufferIndex);
		}
	}

	TArray<FString> AllParameterNames;
	ParametersMap.GetAllParameterNames(AllParameterNames);
	if (BindingContext.ShaderGlobalScopeBindings.Num() != AllParameterNames.Num())
	{
		UE_LOG(LogShaders, Error, TEXT("%i shader parameters have not been bound for %s:"),
			AllParameterNames.Num() - BindingContext.ShaderGlobalScopeBindings.Num(),
			Shader->GetType()->GetName());

		for (const FString& GlobalParameterName : AllParameterNames)
		{
			if (!BindingContext.ShaderGlobalScopeBindings.Contains(GlobalParameterName))
			{
				UE_LOG(LogShaders, Error, TEXT("  %s"), *GlobalParameterName);
			}
		}

		UE_LOG(LogShaders, Fatal, TEXT("Unable to bind all shader parameters of %s."),
			Shader->GetType()->GetName());
	}
}

void EmitNullShaderParameterFatalError(const FShader* Shader, const FShaderParametersMetadata* ParametersMetadata, uint16 MemberOffset)
{
	const FShaderParametersMetadata* MemberContainingStruct = nullptr;
	const FShaderParametersMetadata::FMember* Member = nullptr;
	int32 ArrayElementId = 0;
	FString NamePrefix;
	ParametersMetadata->FindMemberFromOffset(MemberOffset, &MemberContainingStruct, &Member, &ArrayElementId, &NamePrefix);
	
	FString MemberName = FString::Printf(TEXT("%s%s"), *NamePrefix, Member->GetName());
	if (Member->GetNumElements() > 0)
	{
		MemberName = FString::Printf(TEXT("%s%s[%d]"), *NamePrefix, Member->GetName(), ArrayElementId); 
	}

	const TCHAR* ShaderClassName = Shader->GetType()->GetName();

	UE_LOG(LogShaders, Fatal,
		TEXT("%s's required shader parameter %s::%s was not set."),
		ShaderClassName,
		ParametersMetadata->GetStructTypeName(),
		*MemberName);
}

#if DO_CHECK

void ValidateShaderParameters(const FShader* Shader, const FShaderParametersMetadata* ParametersMetadata, const void* Parameters)
{
	const FShaderParameterBindings& Bindings = Shader->Bindings;
	const uint8* Base = reinterpret_cast<const uint8*>(Parameters);

	// Textures
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.Textures)
	{
		auto ShaderParameterRef = *reinterpret_cast<const FTextureRHIParamRef*>(Base + ParameterBinding.ByteOffset);
		if (!ShaderParameterRef)
		{
			EmitNullShaderParameterFatalError(Shader, ParametersMetadata, ParameterBinding.ByteOffset);
		}
	}

	// SRVs
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.SRVs)
	{
		auto ShaderParameterRef = *reinterpret_cast<const FShaderResourceViewRHIParamRef*>(Base + ParameterBinding.ByteOffset);
		if (!ShaderParameterRef)
		{
			EmitNullShaderParameterFatalError(Shader, ParametersMetadata, ParameterBinding.ByteOffset);
		}
	}

	// Samplers
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.Samplers)
	{
		auto ShaderParameterRef = *reinterpret_cast<const FSamplerStateRHIParamRef*>(Base + ParameterBinding.ByteOffset);
		if (!ShaderParameterRef)
		{
			EmitNullShaderParameterFatalError(Shader, ParametersMetadata, ParameterBinding.ByteOffset);
		}
	}

	// Graph Textures
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.GraphTextures)
	{
		auto GraphTexture = *reinterpret_cast<const FRDGTexture* const*>(Base + ParameterBinding.ByteOffset);
		if (!GraphTexture)
		{
			EmitNullShaderParameterFatalError(Shader, ParametersMetadata, ParameterBinding.ByteOffset);
		}
	}

	// Graph SRVs
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.GraphSRVs)
	{
		auto GraphSRV = *reinterpret_cast<const FRDGTextureSRV* const*>(Base + ParameterBinding.ByteOffset);
		if (!GraphSRV)
		{
			EmitNullShaderParameterFatalError(Shader, ParametersMetadata, ParameterBinding.ByteOffset);
		}
	}

	// Graph UAVs for compute shaders	
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.GraphUAVs)
	{
		auto GraphUAV = *reinterpret_cast<const FRDGTextureUAV* const*>(Base + ParameterBinding.ByteOffset);
		if (!GraphUAV)
		{
			EmitNullShaderParameterFatalError(Shader, ParametersMetadata, ParameterBinding.ByteOffset);
		}
	}

	// Reference structures
	for (const FShaderParameterBindings::FParameterStructReference& ParameterBinding : Bindings.ParameterReferences)
	{
		const TRefCountPtr<FRHIUniformBuffer>& ShaderParameterRef = *reinterpret_cast<const TRefCountPtr<FRHIUniformBuffer>*>(Base + ParameterBinding.ByteOffset);

		if (!ShaderParameterRef.IsValid())
		{
			EmitNullShaderParameterFatalError(Shader, ParametersMetadata, ParameterBinding.ByteOffset);
		}
	}
}

#endif // DO_CHECK
