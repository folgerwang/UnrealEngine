// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameterMetadata.cpp: Shader parameter metadata implementations.
=============================================================================*/

#include "ShaderParameterMetadata.h"
#include "RenderCore.h"
#include "ShaderCore.h"


static TLinkedList<FShaderParametersMetadata*>* GUniformStructList = nullptr;

TLinkedList<FShaderParametersMetadata*>*& FShaderParametersMetadata::GetStructList()
{
	return GUniformStructList;
}

TMap<FName, FShaderParametersMetadata*>& FShaderParametersMetadata::GetNameStructMap()
{
	static TMap<FName, FShaderParametersMetadata*> GlobalNameStructMap;
	return GlobalNameStructMap;
}

FShaderParametersMetadata* FindUniformBufferStructByName(const TCHAR* StructName)
{
	FName FindByName(StructName, FNAME_Find);
	FShaderParametersMetadata* FoundStruct = FShaderParametersMetadata::GetNameStructMap().FindRef(FindByName);
	return FoundStruct;
}

FShaderParametersMetadata* FindUniformBufferStructByFName(FName StructName)
{
	return FShaderParametersMetadata::GetNameStructMap().FindRef(StructName);
}

class FUniformBufferMemberAndOffset
{
public:
	FUniformBufferMemberAndOffset(const FShaderParametersMetadata& InContainingStruct, const FShaderParametersMetadata::FMember& InMember, int32 InStructOffset) :
		ContainingStruct(InContainingStruct),
		Member(InMember),
		StructOffset(InStructOffset)
	{}

	const FShaderParametersMetadata& ContainingStruct;
	const FShaderParametersMetadata::FMember& Member;
	int32 StructOffset;
};

FShaderParametersMetadata::FShaderParametersMetadata(
	EUseCase InUseCase,
	const FName& InLayoutName,
	const TCHAR* InStructTypeName,
	const TCHAR* InShaderVariableName,
	uint32 InSize,
	const TArray<FMember>& InMembers)
	: StructTypeName(InStructTypeName)
	, ShaderVariableName(InShaderVariableName)
	, Size(InSize)
	, UseCase(InUseCase)
	, Layout(InLayoutName)
	, Members(InMembers)
	, GlobalListLink(this)
	, bLayoutInitialized(false)
{
	check(StructTypeName);
	if (UseCase == EUseCase::ShaderParameterStruct)
	{
		check(ShaderVariableName == nullptr);
	}
	else
	{
		check(ShaderVariableName);
	}

	if (UseCase == EUseCase::GlobalShaderParameterStruct)
	{
		// Register this uniform buffer struct in global list.
		GlobalListLink.LinkHead(GetStructList());

		FName StructTypeFName(StructTypeName);
		// Verify that during FName creation there's no case conversion
		checkSlow(FCString::Strcmp(StructTypeName, *StructTypeFName.GetPlainNameString()) == 0);
		GetNameStructMap().Add(FName(StructTypeFName), this);
	}
	else
	{
		// We cannot initialize the layout during global initialization, since we have to walk nested struct members.
		// Structs created during global initialization will have bRegisterForAutoBinding==false, and are initialized during startup.
		// Structs created at runtime with bRegisterForAutoBinding==true can be initialized now.
		InitializeLayout();
	}
}

void FShaderParametersMetadata::InitializeAllGlobalStructs()
{
	for (TLinkedList<FShaderParametersMetadata*>::TIterator StructIt(FShaderParametersMetadata::GetStructList()); StructIt; StructIt.Next())
	{
		StructIt->InitializeLayout();
	}
}

void FShaderParametersMetadata::InitializeLayout()
{
	check(!bLayoutInitialized);
	Layout.ConstantBufferSize = Size;

	TArray<FUniformBufferMemberAndOffset> MemberStack;
	MemberStack.Reserve(Members.Num());

	for (int32 MemberIndex = 0; MemberIndex < Members.Num(); MemberIndex++)
	{
		MemberStack.Push(FUniformBufferMemberAndOffset(*this, Members[MemberIndex], 0));
	}

	/** The point of RDG is to track resources that have deferred allocation. Could deffer the creation of uniform buffer,
	 * but there is a risk where it create more resource dependency than necessary on passes that reference this deferred
	 * uniform buffers. Therefore only allow graph resources in shader parameter structures.
	 */
	bool bAllowGraphResources = UseCase == EUseCase::ShaderParameterStruct;

	/** Uniform buffer references are only allowed in shader parameter structures that may be used as a root shader parameter
	 * structure.
	 */
	bool bAllowUniformBufferReferences = UseCase == EUseCase::ShaderParameterStruct;

	/** Resource array are currently only supported for shader parameter structures. */
	bool bAllowResourceArrays = UseCase == EUseCase::ShaderParameterStruct;

	/** White list all use cases that inline a structure within another. Data driven are not known to inline structures. */
	bool bAllowStructureInlining = UseCase == EUseCase::ShaderParameterStruct || UseCase == EUseCase::GlobalShaderParameterStruct;

	for (int32 i = 0; i < MemberStack.Num(); ++i)
	{
		const FShaderParametersMetadata& CurrentStruct = MemberStack[i].ContainingStruct;
		const FMember& CurrentMember = MemberStack[i].Member;

		EUniformBufferBaseType BaseType = CurrentMember.GetBaseType();
		const uint32 ArraySize = CurrentMember.GetNumElements();
		const FShaderParametersMetadata* ChildStruct = CurrentMember.GetStructMetadata();

		const bool bIsArray = ArraySize > 0;
		const bool bIsRHIResource = (
			BaseType == UBMT_TEXTURE ||
			BaseType == UBMT_SRV ||
			BaseType == UBMT_SAMPLER);
		const bool bIsRDGResource = IsRDGResourceReferenceShaderParameterType(BaseType);
		const bool bIsVariableNativeType = (
			BaseType == UBMT_BOOL ||
			BaseType == UBMT_INT32 ||
			BaseType == UBMT_UINT32 ||
			BaseType == UBMT_FLOAT32);

		if (DO_CHECK)
		{
			const FString CppName = FString::Printf(TEXT("%s::%s"), CurrentStruct.GetStructTypeName(), CurrentMember.GetName());

			if (IsRDGResourceReferenceShaderParameterType(BaseType) || BaseType == UBMT_RENDER_TARGET_BINDING_SLOTS)
			{
				if (!bAllowGraphResources)
				{
					UE_LOG(LogRendererCore, Fatal, TEXT("Shader parameter %s error: Graph resources are only allowed in shader parameter structs."), *CppName);
				}
			}
			else if (BaseType == UBMT_REFERENCED_STRUCT)
			{
				if (!bAllowUniformBufferReferences)
				{
					UE_LOG(LogRendererCore, Fatal, TEXT("Shader parameter %s error: Shader parameter struct reference can only be done in shader parameter structs."), *CppName);
				}
			}
			else if (BaseType == UBMT_NESTED_STRUCT || BaseType == UBMT_INCLUDED_STRUCT)
			{
				check(ChildStruct);

				if (!bAllowStructureInlining)
				{
					UE_LOG(LogRendererCore, Fatal, TEXT("Shader parameter %s error: Shader parameter struct is not known inline other structures."), *CppName);
				}
				else if (ChildStruct->GetUseCase() != EUseCase::ShaderParameterStruct && UseCase == EUseCase::ShaderParameterStruct)
				{
					UE_LOG(LogRendererCore, Fatal, TEXT("Shader parameter %s error: can only nests or include shader parameter struct define with BEGIN_SHADER_PARAMETER_STRUCT(), but %s is not."), *CppName, ChildStruct->GetStructTypeName());
				}
			}

			const bool bTypeCanBeArray = (bAllowResourceArrays && (bIsRHIResource || bIsRDGResource)) || bIsVariableNativeType;
			if (bIsArray && !bTypeCanBeArray)
			{
				UE_LOG(LogRendererCore, Fatal, TEXT("Shader parameter %s error: Not allowed to be an array."), *CppName);
			}
		}

		if (IsShaderParameterTypeForUniformBufferLayout(BaseType))
		{
			for (uint32 ArrayElementId = 0; ArrayElementId < (bIsArray ? ArraySize : 1u); ArrayElementId++)
			{
				const uint32 AbsoluteMemberOffset = CurrentMember.GetOffset() + MemberStack[i].StructOffset + ArrayElementId * SHADER_PARAMETER_POINTER_ALIGNMENT;
				check(AbsoluteMemberOffset < (1u << (sizeof(FRHIUniformBufferLayout::FResourceParameter::MemberOffset) * 8)));
				Layout.Resources.Add(FRHIUniformBufferLayout::FResourceParameter{ uint16(AbsoluteMemberOffset), BaseType });
			}
		}

		if (ChildStruct && BaseType != UBMT_REFERENCED_STRUCT)
		{
			int32 AbsoluteStructOffset = CurrentMember.GetOffset() + MemberStack[i].StructOffset;

			for (int32 StructMemberIndex = 0; StructMemberIndex < ChildStruct->Members.Num(); StructMemberIndex++)
			{
				const FMember& StructMember = ChildStruct->Members[StructMemberIndex];
				MemberStack.Insert(FUniformBufferMemberAndOffset(*ChildStruct, StructMember, AbsoluteStructOffset), i + 1 + StructMemberIndex);
			}
		}
	} // for (int32 i = 0; i < MemberStack.Num(); ++i)

#if 0
	/** Sort the resource on MemberType first to avoid CPU miss predictions when iterating over the resources. Then based on ascending offset
	 * to still allow O(N) complexity on offset cross referencing such as done in ClearUnusedGraphResourcesImpl().
	 */
	Layout.Resources.Sort([](
		const FRHIUniformBufferLayout::FResourceParameter& A,
		const FRHIUniformBufferLayout::FResourceParameter& B)
	{
		if (A.MemberType == B.MemberType)
		{
			return A.MemberOffset < B.MemberOffset;
		}
		return A.MemberType < B.MemberType;
	});
#endif

	Layout.ComputeHash();

	bLayoutInitialized = true;
}

void FShaderParametersMetadata::GetNestedStructs(TArray<const FShaderParametersMetadata*>& OutNestedStructs) const
{
	for (int32 i = 0; i < Members.Num(); ++i)
	{
		const FMember& CurrentMember = Members[i];

		const FShaderParametersMetadata* MemberStruct = CurrentMember.GetStructMetadata();

		if (MemberStruct)
		{
			OutNestedStructs.Add(MemberStruct);
			MemberStruct->GetNestedStructs(OutNestedStructs);
		}
	}
}

void FShaderParametersMetadata::AddResourceTableEntries(TMap<FString, FResourceTableEntry>& ResourceTableMap, TMap<FString, uint32>& ResourceTableLayoutHashes) const
{
	uint16 ResourceIndex = 0;
	FString Prefix = FString::Printf(TEXT("%s_"), ShaderVariableName);
	AddResourceTableEntriesRecursive(ShaderVariableName, *Prefix, ResourceIndex, ResourceTableMap);
	ResourceTableLayoutHashes.Add(ShaderVariableName, GetLayout().GetHash());
}

void FShaderParametersMetadata::AddResourceTableEntriesRecursive(const TCHAR* UniformBufferName, const TCHAR* Prefix, uint16& ResourceIndex, TMap<FString, FResourceTableEntry>& ResourceTableMap) const
{
	for (int32 MemberIndex = 0; MemberIndex < Members.Num(); ++MemberIndex)
	{
		const FMember& Member = Members[MemberIndex];
		if (IsShaderParameterTypeForUniformBufferLayout(Member.GetBaseType()))
		{
			FResourceTableEntry& Entry = ResourceTableMap.FindOrAdd(FString::Printf(TEXT("%s%s"), Prefix, Member.GetName()));
			if (Entry.UniformBufferName.IsEmpty())
			{
				Entry.UniformBufferName = UniformBufferName;
				Entry.Type = Member.GetBaseType();
				Entry.ResourceIndex = ResourceIndex++;
			}
		}
		else if (Member.GetBaseType() == UBMT_NESTED_STRUCT)
		{
			check(Member.GetStructMetadata());
			FString MemberPrefix = FString::Printf(TEXT("%s%s_"), Prefix, Member.GetName());
			Member.GetStructMetadata()->AddResourceTableEntriesRecursive(UniformBufferName, *MemberPrefix, ResourceIndex, ResourceTableMap);
		}
		else if (Member.GetBaseType() == UBMT_INCLUDED_STRUCT)
		{
			check(Member.GetStructMetadata());
			Member.GetStructMetadata()->AddResourceTableEntriesRecursive(UniformBufferName, Prefix, ResourceIndex, ResourceTableMap);
		}
	}
}

void FShaderParametersMetadata::FindMemberFromOffset(uint16 MemberOffset, const FShaderParametersMetadata** OutContainingStruct, const FShaderParametersMetadata::FMember** OutMember, int32* ArrayElementId, FString* NamePrefix) const
{
	check(MemberOffset < GetSize());

	for (const FMember& Member : Members)
	{
		EUniformBufferBaseType BaseType = Member.GetBaseType();
		if (BaseType == UBMT_NESTED_STRUCT || BaseType == UBMT_INCLUDED_STRUCT)
		{
			const FShaderParametersMetadata* SubStruct = Member.GetStructMetadata();
			if (MemberOffset < Member.GetOffset() + SubStruct->GetSize())
			{
				if (NamePrefix)
				{
					*NamePrefix = FString::Printf(TEXT("%s%s::"), **NamePrefix, Member.GetName());
				}

				return SubStruct->FindMemberFromOffset(MemberOffset - Member.GetOffset(), OutContainingStruct, OutMember, ArrayElementId, NamePrefix);
			}
		}
		else if (Member.GetNumElements() > 0 && (
			BaseType == UBMT_TEXTURE ||
			BaseType == UBMT_SRV ||
			BaseType == UBMT_SAMPLER ||
			IsRDGResourceReferenceShaderParameterType(BaseType)))
		{
			uint16 ArrayStartOffset = Member.GetOffset();
			uint16 ArrayEndOffset = ArrayStartOffset + SHADER_PARAMETER_POINTER_ALIGNMENT * Member.GetNumElements();

			if (MemberOffset >= ArrayStartOffset && MemberOffset < ArrayEndOffset)
			{
				check((MemberOffset % SHADER_PARAMETER_POINTER_ALIGNMENT) == 0);
				*OutContainingStruct = this;
				*OutMember = &Member;
				*ArrayElementId = (MemberOffset - ArrayStartOffset) / SHADER_PARAMETER_POINTER_ALIGNMENT;
				return;
			}
		}
		else if (Member.GetOffset() == MemberOffset)
		{
			*OutContainingStruct = this;
			*OutMember = &Member;
			*ArrayElementId = 0;
			return;
		}
	}

	checkf(0, TEXT("Looks like this offset is invalid."));
}
