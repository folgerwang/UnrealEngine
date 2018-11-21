// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameterStruct.cpp: Shader parameter struct implementations.
=============================================================================*/

#include "ShaderParameterStruct.h"


/** Context of binding a map. */
struct FShaderParameterStructBindingContext
{
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
			else if (BaseType == UBMT_GRAPH_TRACKED_BUFFER)
			{
				continue;
			}

			// Compute the shader member name to look for according to nesting.
			FString ShaderBindingName = FString::Printf(TEXT("%s%s"), MemberPrefix, Member.GetName());

			uint16 ByteOffset = uint16(GeneralByteOffset + Member.GetOffset());
			check(uint32(ByteOffset) == GeneralByteOffset + Member.GetOffset());

			if (BaseType == UBMT_NESTED_STRUCT || BaseType == UBMT_INCLUDED_STRUCT)
			{
				FString NewPrefix = FString::Printf(TEXT("%s%s_"), MemberPrefix, Member.GetName());
				Bind(
					*Member.GetStructMetadata(),
					/* MemberPrefix = */ BaseType == UBMT_INCLUDED_STRUCT ? MemberPrefix : *NewPrefix,
					/* GeneralByteOffset = */ ByteOffset);
				continue;
			}
			else if (BaseType == UBMT_REFERENCED_STRUCT)
			{
				// The member name of a globally referenced struct is the not name on the struct.
				ShaderBindingName = Member.GetStructMetadata()->GetShaderVariableName();
			}
			else if (bUseRootShaderParameters && (BaseType == UBMT_INT32 || BaseType == UBMT_UINT32 || BaseType == UBMT_FLOAT32))
			{
				// Constants are stored in the root shader parameter cbuffer when bUseRootShaderParameters == true.
				continue;
			}

			if (ShaderGlobalScopeBindings.Contains(ShaderBindingName))
			{
				UE_LOG(LogShaders, Fatal, TEXT("%s can't bind shader parameter %s, because it has already be bound by %s."), *CppName, *ShaderBindingName, **ShaderGlobalScopeBindings.Find(ShaderBindingName));
			}

			uint16 BufferIndex, BaseIndex, BoundSize;
			if (!ParametersMap->FindParameterAllocation(*ShaderBindingName, BufferIndex, BaseIndex, BoundSize))
			{
				continue;
			}
			ShaderGlobalScopeBindings.Add(ShaderBindingName, CppName);

			if (BaseType == UBMT_INT32 || BaseType == UBMT_UINT32 || BaseType == UBMT_FLOAT32)
			{
				uint32 ByteSize = Member.GetMemberSize();

				FShaderParameterBindings::FParameter Parameter;
				Parameter.BufferIndex = BufferIndex;
				Parameter.BaseIndex = BaseIndex;
				Parameter.ByteOffset = ByteOffset;
				Parameter.ByteSize = BoundSize;

				if (uint32(BoundSize) > ByteSize)
				{
					UE_LOG(LogShaders, Fatal, TEXT("The size required to bind shader parameter %s is %i bytes, smaller than %s's %i bytes."),
						*ShaderBindingName, BoundSize, *CppName, ByteSize);
				}

				Bindings->Parameters.Add(Parameter);
			}
			else if (BaseType == UBMT_REFERENCED_STRUCT)
			{
				FShaderParameterBindings::FParameterStructReference Parameter;
				Parameter.BufferIndex = BufferIndex;
				Parameter.ByteOffset = ByteOffset;

				Bindings->ParameterReferences.Add(Parameter);
			}
			else if (
				BaseType == UBMT_TEXTURE ||
				BaseType == UBMT_SRV ||
				BaseType == UBMT_SAMPLER ||
				BaseType == UBMT_GRAPH_TRACKED_TEXTURE ||
				BaseType == UBMT_GRAPH_TRACKED_SRV ||
				BaseType == UBMT_GRAPH_TRACKED_UAV ||
				BaseType == UBMT_GRAPH_TRACKED_BUFFER_SRV ||
				BaseType == UBMT_GRAPH_TRACKED_BUFFER_UAV)
			{
				FShaderParameterBindings::FResourceParameter Parameter;
				Parameter.BaseIndex = BaseIndex;
				Parameter.NumResources = BoundSize;
				Parameter.ByteOffset = ByteOffset;

				checkf(BoundSize <= 1, TEXT("Not enough room in shader parameter struct for the shader."));

				if (BaseType == UBMT_TEXTURE)
					Bindings->Textures.Add(Parameter);
				else if (BaseType == UBMT_SRV)
					Bindings->SRVs.Add(Parameter);
				else if (BaseType == UBMT_SAMPLER)
					Bindings->Samplers.Add(Parameter);
				else if (BaseType == UBMT_GRAPH_TRACKED_TEXTURE)
					Bindings->GraphTextures.Add(Parameter);
				else if (BaseType == UBMT_GRAPH_TRACKED_SRV || BaseType == UBMT_GRAPH_TRACKED_BUFFER_SRV)
					Bindings->GraphSRVs.Add(Parameter);
				else // if (BaseType == UBMT_GRAPH_TRACKED_UAV || BaseType == UBMT_GRAPH_TRACKED_BUFFER_UAV)
					Bindings->GraphUAVs.Add(Parameter);
			}
			else
			{
				checkf(0, TEXT("Unexpected base type for a shader parameter struct member."));
			}
		} // for (const FShaderParametersMetadata::FMember& Member : StructMembers)
	}
}; // struct FShaderParameterStructBindingContext

void FShaderParameterBindings::BindForLegacyShaderParameters(const FShaderParameterMap& ParametersMap, const FShaderParametersMetadata& StructMetaData)
{
	checkf(StructMetaData.GetSize() < (1 << (sizeof(uint16) * 8)), TEXT("Shader parameter structure can only have a size < 65536 bytes."));

	FShaderParameterStructBindingContext BindingContext;
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
	if (0 && BindingContext.ShaderGlobalScopeBindings.Num() != AllParameterNames.Num()) // TODO: enable that once we can.
	{
		UE_LOG(LogShaders, Error, TEXT("%i shader parameters have not been bound:"), AllParameterNames.Num() - BindingContext.ShaderGlobalScopeBindings.Num());
		for (const FString& GlobalParameterName : AllParameterNames)
		{
			if (!BindingContext.ShaderGlobalScopeBindings.Contains(GlobalParameterName))
			{
				UE_LOG(LogShaders, Error, TEXT("  %s"), *GlobalParameterName);
			}
		}

		// TODO: would be great to have the shader name for the error message.
		UE_LOG(LogShaders, Fatal, TEXT("Some shader parameters have not been bound."));
	}
}

void FShaderParameterBindings::BindForRootShaderParameters(const FShaderParameterMap& ParametersMap, const FShaderParametersMetadata& StructMetaData)
{
	checkf(StructMetaData.GetSize() < (1 << (sizeof(uint16) * 8)), TEXT("Shader parameter structure can only have a size < 65536 bytes."));

	FShaderParameterStructBindingContext BindingContext;
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
		UE_LOG(LogShaders, Error, TEXT("%i shader parameters have not been bound:"), AllParameterNames.Num() - BindingContext.ShaderGlobalScopeBindings.Num());
		for (const FString& GlobalParameterName : AllParameterNames)
		{
			if (!BindingContext.ShaderGlobalScopeBindings.Contains(GlobalParameterName))
			{
				UE_LOG(LogShaders, Error, TEXT("  %s"), *GlobalParameterName);
			}
		}

		// TODO: would be great to have the shader name for the error message.
		UE_LOG(LogShaders, Fatal, TEXT("Some shader parameters have not been bound."));
	}
}

void SetNullShaderParameterFatalError(const FShader* Shader, const FShaderParametersMetadata* ParametersMetadata, uint16 MemberOffset)
{
	const FShaderParametersMetadata* MemberContainingStruct = nullptr;
	const FShaderParametersMetadata::FMember* Member = nullptr;
	ParametersMetadata->FindMemberFromOffset(MemberOffset, &MemberContainingStruct, &Member);

	const TCHAR* ShaderClassName = Shader->GetType()->GetName();

	UE_LOG(LogShaders, Fatal,
		TEXT("%s's required shader parameter %s::%s was not set."),
		ShaderClassName,
		MemberContainingStruct->GetStructTypeName(),
		Member->GetName());
}
