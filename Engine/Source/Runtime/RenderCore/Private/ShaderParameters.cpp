// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameters.cpp: Shader parameter implementation.
=============================================================================*/

#include "ShaderParameters.h"
#include "Containers/List.h"
#include "UniformBuffer.h"
#include "ShaderCore.h"
#include "Shader.h"
#include "VertexFactory.h"
#include "ShaderCodeLibrary.h"

void FShaderParameter::Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,EShaderParameterFlags Flags)
{
#if UE_BUILD_DEBUG
	bInitialized = true;
#endif

	if (!ParameterMap.FindParameterAllocation(ParameterName,BufferIndex,BaseIndex,NumBytes) && Flags == SPF_Mandatory)
	{
		if (!UE_LOG_ACTIVE(LogShaders, Log))
		{
			UE_LOG(LogShaders, Fatal,TEXT("Failure to bind non-optional shader parameter %s!  The parameter is either not present in the shader, or the shader compiler optimized it out."),ParameterName);
		}
		else
		{
			// We use a non-Slate message box to avoid problem where we haven't compiled the shaders for Slate.
			FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, *FText::Format(
				NSLOCTEXT("UnrealEd", "Error_FailedToBindShaderParameter", "Failure to bind non-optional shader parameter {0}! The parameter is either not present in the shader, or the shader compiler optimized it out. This will be an assert with LogShaders suppressed!"),
				FText::FromString(ParameterName)).ToString(), TEXT("Warning"));
		}
	}
}

FArchive& operator<<(FArchive& Ar,FShaderParameter& P)
{
#if UE_BUILD_DEBUG
	if (Ar.IsLoading())
	{
		P.bInitialized = true;
	}
#endif

	uint16& PBufferIndex = P.BufferIndex;
	return Ar << P.BaseIndex << P.NumBytes << PBufferIndex;
}

void FShaderResourceParameter::Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,EShaderParameterFlags Flags)
{
	uint16 UnusedBufferIndex = 0;

#if UE_BUILD_DEBUG
	bInitialized = true;
#endif

	if(!ParameterMap.FindParameterAllocation(ParameterName,UnusedBufferIndex,BaseIndex,NumResources) && Flags == SPF_Mandatory)
	{
		if (!UE_LOG_ACTIVE(LogShaders, Log))
		{
			UE_LOG(LogShaders, Fatal,TEXT("Failure to bind non-optional shader resource parameter %s!  The parameter is either not present in the shader, or the shader compiler optimized it out."),ParameterName);
		}
		else
		{
			// We use a non-Slate message box to avoid problem where we haven't compiled the shaders for Slate.
			FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, *FText::Format(
				NSLOCTEXT("UnrealEd", "Error_FailedToBindShaderParameter", "Failure to bind non-optional shader parameter {0}! The parameter is either not present in the shader, or the shader compiler optimized it out. This will be an assert with LogShaders suppressed!"),
				FText::FromString(ParameterName)).ToString(), TEXT("Warning"));
		}
	}
}

FArchive& operator<<(FArchive& Ar,FShaderResourceParameter& P)
{
#if UE_BUILD_DEBUG
	if (Ar.IsLoading())
	{
		P.bInitialized = true;
	}
#endif

	return Ar << P.BaseIndex << P.NumResources;
}

void FShaderUniformBufferParameter::ModifyCompilationEnvironment(const TCHAR* ParameterName,const FShaderParametersMetadata& Struct,EShaderPlatform Platform,FShaderCompilerEnvironment& OutEnvironment)
{
	const FString IncludeName = FString::Printf(TEXT("/Engine/Generated/UniformBuffers/%s.ush"),ParameterName);
	// Add the uniform buffer declaration to the compilation environment as an include: UniformBuffers/<ParameterName>.usf
	FString Declaration;
	CreateUniformBufferShaderDeclaration(ParameterName, Struct, Declaration);
	OutEnvironment.IncludeVirtualPathToContentsMap.Add(IncludeName, Declaration);

	FString& GeneratedUniformBuffersInclude = OutEnvironment.IncludeVirtualPathToContentsMap.FindOrAdd("/Engine/Generated/GeneratedUniformBuffers.ush");
	FString Include = FString::Printf(TEXT("#include \"/Engine/Generated/UniformBuffers/%s.ush\"") LINE_TERMINATOR, ParameterName);

	GeneratedUniformBuffersInclude.Append(Include);
	Struct.AddResourceTableEntries(OutEnvironment.ResourceTableMap, OutEnvironment.ResourceTableLayoutHashes);
}

void FShaderUniformBufferParameter::Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,EShaderParameterFlags Flags)
{
	uint16 UnusedBaseIndex = 0;
	uint16 UnusedNumBytes = 0;

#if UE_BUILD_DEBUG
	bInitialized = true;
#endif

	if(!ParameterMap.FindParameterAllocation(ParameterName,BaseIndex,UnusedBaseIndex,UnusedNumBytes))
	{
		bIsBound = false;
		if(Flags == SPF_Mandatory)
		{
			if (!UE_LOG_ACTIVE(LogShaders, Log))
			{
				UE_LOG(LogShaders, Fatal,TEXT("Failure to bind non-optional shader resource parameter %s!  The parameter is either not present in the shader, or the shader compiler optimized it out."),ParameterName);
			}
			else
			{
				// We use a non-Slate message box to avoid problem where we haven't compiled the shaders for Slate.
				FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, *FText::Format(
					NSLOCTEXT("UnrealEd", "Error_FailedToBindShaderParameter", "Failure to bind non-optional shader parameter {0}! The parameter is either not present in the shader, or the shader compiler optimized it out. This will be an assert with LogShaders suppressed!"),
					FText::FromString(ParameterName)).ToString(), TEXT("Warning"));
			}
		}
	}
	else
	{
		bIsBound = true;
	}
}

/** The individual bits of a uniform buffer declaration. */
struct FUniformBufferDecl
{
	/** Members to place in the constant buffer. */
	FString ConstantBufferMembers;
	/** Members to place in the resource table. */
	FString ResourceMembers;
	/** Members in the struct HLSL shader code will access. */
	FString StructMembers;
	/** The HLSL initializer that will copy constants and resources in to the struct. */
	FString Initializer;
};

/** Generates a HLSL struct declaration for a uniform buffer struct. */
static void CreateHLSLUniformBufferStructMembersDeclaration(
	const FShaderParametersMetadata& UniformBufferStruct, 
	const FString& NamePrefix, 
	uint32 StructOffset, 
	FUniformBufferDecl& Decl, 
	uint32& HLSLBaseOffset)
{
	const TArray<FShaderParametersMetadata::FMember>& StructMembers = UniformBufferStruct.GetMembers();
	
	int32 OpeningBraceLocPlusOne = Decl.Initializer.Len();

	FString PreviousBaseTypeName = TEXT("float");
	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FShaderParametersMetadata::FMember& Member = StructMembers[MemberIndex];
		
		FString ArrayDim;
		if(Member.GetNumElements() > 0)
		{
			ArrayDim = FString::Printf(TEXT("[%u]"),Member.GetNumElements());
		}

		if(Member.GetBaseType() == UBMT_NESTED_STRUCT)
		{
			Decl.StructMembers += TEXT("struct {\r\n");
			Decl.Initializer += TEXT(",{");
			CreateHLSLUniformBufferStructMembersDeclaration(*Member.GetStructMetadata(), FString::Printf(TEXT("%s%s_"), *NamePrefix, Member.GetName()), StructOffset + Member.GetOffset(), Decl, HLSLBaseOffset);
			Decl.Initializer += TEXT("}");
			Decl.StructMembers += FString::Printf(TEXT("} %s%s;\r\n"),Member.GetName(),*ArrayDim);
		}
		else if (Member.GetBaseType() == UBMT_INCLUDED_STRUCT)
		{
			Decl.Initializer += TEXT(",");
			CreateHLSLUniformBufferStructMembersDeclaration(*Member.GetStructMetadata(), NamePrefix, StructOffset + Member.GetOffset(), Decl, HLSLBaseOffset);
		}
		else if (IsShaderParameterTypeForUniformBufferLayout(Member.GetBaseType()))
		{
			// Skip resources, they will be replaced with padding by the next member in the constant buffer.
			// This padding will cause gaps in the constant buffer.  Alternatively we could compact the constant buffer during RHICreateUniformBuffer.
			continue;
		}
		else 
		{
			// Generate the base type name.
			FString BaseTypeName;
			switch(Member.GetBaseType())
			{
			case UBMT_BOOL:    BaseTypeName = TEXT("bool"); break;
			case UBMT_INT32:   BaseTypeName = TEXT("int"); break;
			case UBMT_UINT32:  BaseTypeName = TEXT("uint"); break;
			case UBMT_FLOAT32: 
				if (Member.GetPrecision() == EShaderPrecisionModifier::Float)
				{
					BaseTypeName = TEXT("float"); 
				}
				else if (Member.GetPrecision() == EShaderPrecisionModifier::Half)
				{
					BaseTypeName = TEXT("half"); 
				}
				else if (Member.GetPrecision() == EShaderPrecisionModifier::Fixed)
				{
					BaseTypeName = TEXT("fixed"); 
				}
				break;
			default:           UE_LOG(LogShaders, Fatal,TEXT("Unrecognized uniform buffer struct member base type."));
			};

			// Generate the type dimensions for vectors and matrices.
			FString TypeDim;
			uint32 HLSLMemberSize = 4;
			if(Member.GetNumRows() > 1)
			{
				TypeDim = FString::Printf(TEXT("%ux%u"),Member.GetNumRows(),Member.GetNumColumns());

				// Each row of a matrix is 16 byte aligned.
				HLSLMemberSize = (Member.GetNumRows() - 1) * 16 + Member.GetNumColumns() * 4;
			}
			else if(Member.GetNumColumns() > 1)
			{
				TypeDim = FString::Printf(TEXT("%u"),Member.GetNumColumns());
				HLSLMemberSize = Member.GetNumColumns() * 4;
			}

			// Array elements are 16 byte aligned.
			if(Member.GetNumElements() > 0)
			{
				HLSLMemberSize = (Member.GetNumElements() - 1) * Align(HLSLMemberSize,16) + HLSLMemberSize;
			}

			const uint32 AbsoluteMemberOffset = StructOffset + Member.GetOffset();

			// If the HLSL offset doesn't match the C++ offset, generate padding to fix it.
			if(HLSLBaseOffset != AbsoluteMemberOffset)
			{
				check(HLSLBaseOffset < AbsoluteMemberOffset);
				while(HLSLBaseOffset < AbsoluteMemberOffset)
				{
					Decl.ConstantBufferMembers += FString::Printf(TEXT("\t%s PrePadding_%s%u;\r\n"), *PreviousBaseTypeName, *NamePrefix, HLSLBaseOffset);
					HLSLBaseOffset += 4;
				};
				check(HLSLBaseOffset == AbsoluteMemberOffset);
			}
			PreviousBaseTypeName = BaseTypeName;
			HLSLBaseOffset = AbsoluteMemberOffset + HLSLMemberSize;

			// Generate the member declaration.
			FString ParameterName = FString::Printf(TEXT("%s%s"),*NamePrefix,Member.GetName());
			Decl.ConstantBufferMembers += FString::Printf(TEXT("\t%s%s %s%s;\r\n"),*BaseTypeName,*TypeDim,*ParameterName,*ArrayDim);
			Decl.StructMembers += FString::Printf(TEXT("\t%s%s %s%s;\r\n"),*BaseTypeName,*TypeDim,Member.GetName(),*ArrayDim);
			Decl.Initializer += FString::Printf(TEXT(",%s"),*ParameterName);
		}
	}

	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FShaderParametersMetadata::FMember& Member = StructMembers[MemberIndex];

		if (IsShaderParameterTypeForUniformBufferLayout(Member.GetBaseType()))
		{
			check(Member.GetBaseType() != UBMT_RDG_TEXTURE_SRV && Member.GetBaseType() != UBMT_RDG_TEXTURE_UAV);
			checkf(Member.GetNumElements() == 0, TEXT("Resources array are not supported in uniform buffers yet."));
			if (Member.GetBaseType() == UBMT_SRV)
			{
				// TODO: handle arrays?
				FString ParameterName = FString::Printf(TEXT("%s%s"),*NamePrefix,Member.GetName());
				Decl.ResourceMembers += FString::Printf(TEXT("PLATFORM_SUPPORTS_SRV_UB_MACRO( %s %s; ) \r\n"), Member.GetShaderType(), *ParameterName);
				Decl.StructMembers += FString::Printf(TEXT("\tPLATFORM_SUPPORTS_SRV_UB_MACRO( %s %s; ) \r\n"), Member.GetShaderType(), Member.GetName());
				Decl.Initializer += FString::Printf(TEXT(" PLATFORM_SUPPORTS_SRV_UB_MACRO( ,%s ) "), *ParameterName);
			}
			else
			{
				// TODO: handle arrays?
				FString ParameterName = FString::Printf(TEXT("%s%s"),*NamePrefix,Member.GetName());
				Decl.ResourceMembers += FString::Printf(TEXT("%s %s;\r\n"), Member.GetShaderType(), *ParameterName);
				Decl.StructMembers += FString::Printf(TEXT("\t%s %s;\r\n"), Member.GetShaderType(), Member.GetName());
				Decl.Initializer += FString::Printf(TEXT(",%s"), *ParameterName);
			}
		}
	}

	if (Decl.Initializer[OpeningBraceLocPlusOne] == TEXT(','))
	{
		Decl.Initializer[OpeningBraceLocPlusOne] = TEXT(' ');
	}
}

/** Creates a HLSL declaration of a uniform buffer with the given structure. */
static FString CreateHLSLUniformBufferDeclaration(const TCHAR* Name,const FShaderParametersMetadata& UniformBufferStruct)
{
	// If the uniform buffer has no members, we don't want to write out anything.  Shader compilers throw errors when faced with empty cbuffers and structs.
	if (UniformBufferStruct.GetMembers().Num() > 0)
	{
		FString NamePrefix(FString(Name) + FString(TEXT("_")));
		FUniformBufferDecl Decl;
		uint32 HLSLBaseOffset = 0;
		CreateHLSLUniformBufferStructMembersDeclaration(UniformBufferStruct, NamePrefix, 0, Decl, HLSLBaseOffset);

		return FString::Printf(
			TEXT("#ifndef __UniformBuffer_%s_Definition__\r\n")
			TEXT("#define __UniformBuffer_%s_Definition__\r\n")
			TEXT("cbuffer %s\r\n")
			TEXT("{\r\n")
			TEXT("%s")
			TEXT("}\r\n")
				TEXT("%s")
				TEXT("static const struct\r\n")
				TEXT("{\r\n")
				TEXT("%s")
				TEXT("} %s = {%s};\r\n")
			TEXT("#endif\r\n"),
			Name,
			Name,
			Name,
			*Decl.ConstantBufferMembers,
			*Decl.ResourceMembers,
			*Decl.StructMembers,
			Name,
			*Decl.Initializer
			);
	}

	return FString(TEXT("\n"));
}

RENDERCORE_API void CreateUniformBufferShaderDeclaration(const TCHAR* Name,const FShaderParametersMetadata& UniformBufferStruct, FString& OutDeclaration)
{
	OutDeclaration = CreateHLSLUniformBufferDeclaration(Name, UniformBufferStruct);
}

RENDERCORE_API void CacheUniformBufferIncludes(TMap<const TCHAR*,FCachedUniformBufferDeclaration>& Cache)
{
	for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TIterator It(Cache); It; ++It)
	{
		FCachedUniformBufferDeclaration& BufferDeclaration = It.Value();
		check(BufferDeclaration.Declaration.Get() == NULL);

		for (TLinkedList<FShaderParametersMetadata*>::TIterator StructIt(FShaderParametersMetadata::GetStructList()); StructIt; StructIt.Next())
		{
			if (It.Key() == StructIt->GetShaderVariableName())
			{
				FString* NewDeclaration = new FString();
				CreateUniformBufferShaderDeclaration(StructIt->GetShaderVariableName(), **StructIt, *NewDeclaration);
				check(!NewDeclaration->IsEmpty());
				BufferDeclaration.Declaration = MakeShareable(NewDeclaration);
				break;
			}
		}
	}
}

void FShaderType::AddReferencedUniformBufferIncludes(FShaderCompilerEnvironment& OutEnvironment, FString& OutSourceFilePrefix, EShaderPlatform Platform)
{
	// Cache uniform buffer struct declarations referenced by this shader type's files
	if (!bCachedUniformBufferStructDeclarations)
	{
		CacheUniformBufferIncludes(ReferencedUniformBufferStructsCache);
		bCachedUniformBufferStructDeclarations = true;
	}

	FString UniformBufferIncludes;

	for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TIterator It(ReferencedUniformBufferStructsCache); It; ++It)
	{
		check(It.Value().Declaration.Get() != NULL);
		check(!It.Value().Declaration.Get()->IsEmpty());
		UniformBufferIncludes += FString::Printf(TEXT("#include \"/Engine/Generated/UniformBuffers/%s.ush\"") LINE_TERMINATOR, It.Key());
		OutEnvironment.IncludeVirtualPathToExternalContentsMap.Add(
			*FString::Printf(TEXT("/Engine/Generated/UniformBuffers/%s.ush"),It.Key()),
			It.Value().Declaration
			);

		for (TLinkedList<FShaderParametersMetadata*>::TIterator StructIt(FShaderParametersMetadata::GetStructList()); StructIt; StructIt.Next())
		{
			if (It.Key() == StructIt->GetShaderVariableName())
			{
				StructIt->AddResourceTableEntries(OutEnvironment.ResourceTableMap, OutEnvironment.ResourceTableLayoutHashes);
			}
		}
	}

	FString& GeneratedUniformBuffersInclude = OutEnvironment.IncludeVirtualPathToContentsMap.FindOrAdd("/Engine/Generated/GeneratedUniformBuffers.ush");
	GeneratedUniformBuffersInclude.Append(UniformBufferIncludes);

	ERHIFeatureLevel::Type MaxFeatureLevel = GetMaxSupportedFeatureLevel(Platform);
	if (MaxFeatureLevel >= ERHIFeatureLevel::SM4)
	{
		OutEnvironment.SetDefine(TEXT("PLATFORM_SUPPORTS_SRV_UB"), TEXT("1"));
	}
}

void FShaderType::DumpDebugInfo()
{
	UE_LOG(LogConsoleResponse, Display, TEXT("----------------------------- GLobalShader %s"), GetName());
	UE_LOG(LogConsoleResponse, Display, TEXT("               :Target %s"), GetShaderFrequencyString(GetFrequency()));
	UE_LOG(LogConsoleResponse, Display, TEXT("               :TotalPermutationCount %d"), TotalPermutationCount);
#if WITH_EDITOR
	UE_LOG(LogConsoleResponse, Display, TEXT("               :SourceHash %s"), *GetSourceHash(GMaxRHIShaderPlatform).ToString());
#endif
	switch (ShaderTypeForDynamicCast)
	{
	case EShaderTypeForDynamicCast::Global:
		UE_LOG(LogConsoleResponse, Display, TEXT("               :ShaderType Global"));
		break;
	case EShaderTypeForDynamicCast::Material:
		UE_LOG(LogConsoleResponse, Display, TEXT("               :ShaderType Material"));
		break;
	case EShaderTypeForDynamicCast::MeshMaterial:
		UE_LOG(LogConsoleResponse, Display, TEXT("               :ShaderType MeshMaterial"));
		break;
	case EShaderTypeForDynamicCast::Niagara:
		UE_LOG(LogConsoleResponse, Display, TEXT("               :ShaderType Niagara"));
		break;
	}

	UE_LOG(LogConsoleResponse, Display, TEXT("  --- %d shaders"), ShaderIdMap.Num());
	int32 Index = 0;
	for (auto& KeyValue : ShaderIdMap)
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("    --- shader %d"), Index);
		FShader* Shader = KeyValue.Value;
		Shader->DumpDebugInfo();
		Index++;
	}

}

void FShaderType::GetShaderStableKeyParts(FStableShaderKeyAndValue& SaveKeyVal)
{
#if WITH_EDITOR
	static FName NAME_Global(TEXT("Global"));
	static FName NAME_Material(TEXT("Material"));
	static FName NAME_MeshMaterial(TEXT("MeshMaterial"));
	static FName NAME_Niagara(TEXT("Niagara"));
	switch (ShaderTypeForDynamicCast)
	{
	case EShaderTypeForDynamicCast::Global:
		SaveKeyVal.ShaderClass = NAME_Global;
		break;
	case EShaderTypeForDynamicCast::Material:
		SaveKeyVal.ShaderClass = NAME_Material;
		break;
	case EShaderTypeForDynamicCast::MeshMaterial:
		SaveKeyVal.ShaderClass = NAME_MeshMaterial;
		break;
	case EShaderTypeForDynamicCast::Niagara:
		SaveKeyVal.ShaderClass = NAME_Niagara;
		break;
	}
	SaveKeyVal.ShaderType = FName(GetName() ? GetName() : TEXT("null"));
#endif
}

void FShaderType::SaveShaderStableKeys(EShaderPlatform TargetShaderPlatform)
{
#if WITH_EDITOR
	FStableShaderKeyAndValue SaveKeyVal;
	if (ShaderIdMap.Num())
	{
		for (auto& KeyValue : ShaderIdMap)
		{
			FShader* Shader = KeyValue.Value;
			check(Shader);
			check(Shader->Type == this);
			Shader->SaveShaderStableKeys(TargetShaderPlatform, SaveKeyVal);
		}
	}
#endif
}


void FVertexFactoryType::AddReferencedUniformBufferIncludes(FShaderCompilerEnvironment& OutEnvironment, FString& OutSourceFilePrefix, EShaderPlatform Platform)
{
	// Cache uniform buffer struct declarations referenced by this shader type's files
	if (!bCachedUniformBufferStructDeclarations)
	{
		CacheUniformBufferIncludes(ReferencedUniformBufferStructsCache);
		bCachedUniformBufferStructDeclarations = true;
	}

	FString UniformBufferIncludes;

	for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TIterator It(ReferencedUniformBufferStructsCache); It; ++It)
	{
		check(It.Value().Declaration.Get() != NULL);
		check(!It.Value().Declaration.Get()->IsEmpty());
		UniformBufferIncludes += FString::Printf(TEXT("#include \"/Engine/Generated/UniformBuffers/%s.ush\"") LINE_TERMINATOR, It.Key());
		OutEnvironment.IncludeVirtualPathToExternalContentsMap.Add(
			*FString::Printf(TEXT("/Engine/Generated/UniformBuffers/%s.ush"),It.Key()),
			It.Value().Declaration
		);

		for (TLinkedList<FShaderParametersMetadata*>::TIterator StructIt(FShaderParametersMetadata::GetStructList()); StructIt; StructIt.Next())
		{
			if (It.Key() == StructIt->GetShaderVariableName())
			{
				StructIt->AddResourceTableEntries(OutEnvironment.ResourceTableMap, OutEnvironment.ResourceTableLayoutHashes);
			}
		}
	}

	FString& GeneratedUniformBuffersInclude = OutEnvironment.IncludeVirtualPathToContentsMap.FindOrAdd("/Engine/Generated/GeneratedUniformBuffers.ush");
	GeneratedUniformBuffersInclude.Append(UniformBufferIncludes);

	ERHIFeatureLevel::Type MaxFeatureLevel = GetMaxSupportedFeatureLevel(Platform);
	if (MaxFeatureLevel >= ERHIFeatureLevel::SM4)
	{
		OutEnvironment.SetDefine(TEXT("PLATFORM_SUPPORTS_SRV_UB"), TEXT("1"));
	}
}
