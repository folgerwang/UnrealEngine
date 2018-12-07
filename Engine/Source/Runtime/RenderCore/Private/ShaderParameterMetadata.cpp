// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

	// TODO(RDG): Render graph tracked resources in GlobalShaderParameterStruct could technically be possible if the creation is deferred.
	bool bAllowGraphResources = UseCase == EUseCase::ShaderParameterStruct;

	for (int32 i = 0; i < MemberStack.Num(); ++i)
	{
		const FShaderParametersMetadata& CurrentStruct = MemberStack[i].ContainingStruct;
		const FMember& CurrentMember = MemberStack[i].Member;
		EUniformBufferBaseType BaseType = CurrentMember.GetBaseType();

		const FShaderParametersMetadata* ChildStruct = CurrentMember.GetStructMetadata();

		if (DO_CHECK)
		{
			const FString CppName = FString::Printf(TEXT("%s::%s"), CurrentStruct.GetStructTypeName(), CurrentMember.GetName());

			if (!bAllowGraphResources && (
				BaseType == UBMT_GRAPH_TRACKED_TEXTURE ||
				BaseType == UBMT_GRAPH_TRACKED_SRV ||
				BaseType == UBMT_GRAPH_TRACKED_UAV ||
				BaseType == UBMT_GRAPH_TRACKED_BUFFER ||
				BaseType == UBMT_GRAPH_TRACKED_BUFFER_SRV ||
				BaseType == UBMT_GRAPH_TRACKED_BUFFER_UAV ||
				BaseType == UBMT_RENDER_TARGET_BINDING_SLOTS))
			{
				UE_LOG(LogRendererCore, Fatal, TEXT("Shader parameter %s error: Graph resources are only allowed in shader parameter structs."), *CppName);
			}

			if (BaseType == UBMT_REFERENCED_STRUCT)
			{
				if (UseCase != EUseCase::ShaderParameterStruct)
				{
					UE_LOG(LogRendererCore, Fatal, TEXT("Shader parameter %s error: Shader parameter struct reference can only be done in shader parameter structs."), *CppName);
				}
			}

			if (BaseType == UBMT_NESTED_STRUCT || BaseType == UBMT_INCLUDED_STRUCT)
			{
				check(ChildStruct);
			}

			if (UseCase == EUseCase::ShaderParameterStruct &&
				(BaseType == UBMT_NESTED_STRUCT || BaseType == UBMT_INCLUDED_STRUCT) &&
				ChildStruct->GetUseCase() != EUseCase::ShaderParameterStruct && 0)
			{
				UE_LOG(LogRendererCore, Fatal, TEXT("Shader parameter %s error: shader parameter structs can only nests or include shader parameter struct, but %s is not."), *CppName, ChildStruct->GetStructTypeName());
			}
		}

		if (IsUniformBufferResourceType(BaseType))
		{
			Layout.Resources.Add(BaseType);
			const uint32 AbsoluteMemberOffset = CurrentMember.GetOffset() + MemberStack[i].StructOffset;
			check(AbsoluteMemberOffset < (1u << (Layout.ResourceOffsets.GetTypeSize() * 8)));
			Layout.ResourceOffsets.Add(AbsoluteMemberOffset);
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
	}

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
		if (IsUniformBufferResourceType(Member.GetBaseType()))
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

void FShaderParametersMetadata::FindMemberFromOffset(uint16 MemberOffset, const FShaderParametersMetadata** OutContainingStruct, const FShaderParametersMetadata::FMember** OutMember) const
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
				return SubStruct->FindMemberFromOffset(MemberOffset - Member.GetOffset(), OutContainingStruct, OutMember);
			}
		}
		else if (Member.GetOffset() == MemberOffset)
		{
			*OutContainingStruct = this;
			*OutMember = &Member;
			return;
		}
	}

	checkf(0, TEXT("Looks like this offset is invalid."));
}
