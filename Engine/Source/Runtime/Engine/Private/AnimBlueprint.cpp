// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimBlueprint.h"
#include "UObject/FrameworkObjectVersion.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#if WITH_EDITOR
#include "Settings/EditorExperimentalSettings.h"
#endif
#if WITH_EDITORONLY_DATA
#include "AnimationEditorUtils.h"
#endif

//////////////////////////////////////////////////////////////////////////
// UAnimBlueprint

UAnimBlueprint::UAnimBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseMultiThreadedAnimationUpdate = true;
}

UAnimBlueprintGeneratedClass* UAnimBlueprint::GetAnimBlueprintGeneratedClass() const
{
	UAnimBlueprintGeneratedClass* Result = Cast<UAnimBlueprintGeneratedClass>(*GeneratedClass);
	return Result;
}

UAnimBlueprintGeneratedClass* UAnimBlueprint::GetAnimBlueprintSkeletonClass() const
{
	UAnimBlueprintGeneratedClass* Result = Cast<UAnimBlueprintGeneratedClass>(*SkeletonGeneratedClass);
	return Result;
}

void UAnimBlueprint::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::Animation);

	Super::Serialize(Ar);

#if WITH_EDITOR
	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
#endif
}

#if WITH_EDITOR

UClass* UAnimBlueprint::GetBlueprintClass() const
{
	return UAnimBlueprintGeneratedClass::StaticClass();
}

int32 UAnimBlueprint::FindOrAddGroup(FName GroupName)
{
	if (GroupName == NAME_None)
	{
		return INDEX_NONE;
	}
	else
	{
		// Look for an existing group
		for (int32 Index = 0; Index < Groups.Num(); ++Index)
		{
			if (Groups[Index].Name == GroupName)
			{
				return Index;
			}
		}

		// Create a new group
		MarkPackageDirty();
		FAnimGroupInfo& NewGroup = *(new (Groups) FAnimGroupInfo());
		NewGroup.Name = GroupName;

		return Groups.Num() - 1;
	}
}


/** Returns the most base anim blueprint for a given blueprint (if it is inherited from another anim blueprint, returning null if only native / non-anim BP classes are it's parent) */
UAnimBlueprint* UAnimBlueprint::FindRootAnimBlueprint(UAnimBlueprint* DerivedBlueprint)
{
	UAnimBlueprint* ParentBP = NULL;

	// Determine if there is an anim blueprint in the ancestry of this class
	for (UClass* ParentClass = DerivedBlueprint->ParentClass; ParentClass && (UObject::StaticClass() != ParentClass); ParentClass = ParentClass->GetSuperClass())
	{
		if (UAnimBlueprint* TestBP = Cast<UAnimBlueprint>(ParentClass->ClassGeneratedBy))
		{
			ParentBP = TestBP;
		}
	}

	return ParentBP;
}

FAnimParentNodeAssetOverride* UAnimBlueprint::GetAssetOverrideForNode(FGuid NodeGuid, bool bIgnoreSelf) const
{
	TArray<UBlueprint*> Hierarchy;
	GetBlueprintHierarchyFromClass(GetAnimBlueprintGeneratedClass(), Hierarchy);

	for (int32 Idx = bIgnoreSelf ? 1 : 0 ; Idx < Hierarchy.Num() ; ++Idx)
	{
		if (UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Hierarchy[Idx]))
		{
			FAnimParentNodeAssetOverride* Override = AnimBlueprint->ParentAssetOverrides.FindByPredicate([NodeGuid](const FAnimParentNodeAssetOverride& Other)
			{
				return Other.ParentNodeGuid == NodeGuid;
			});

			if (Override)
			{
				return Override;
			}
		}
	}

	return nullptr;
}

bool UAnimBlueprint::GetAssetOverrides(TArray<FAnimParentNodeAssetOverride*>& OutOverrides)
{
	TArray<UBlueprint*> Hierarchy;
	GetBlueprintHierarchyFromClass(GetAnimBlueprintGeneratedClass(), Hierarchy);

	for (UBlueprint* Blueprint : Hierarchy)
	{
		if (UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Blueprint))
		{
			for (FAnimParentNodeAssetOverride& Override : AnimBlueprint->ParentAssetOverrides)
			{
				bool OverrideExists = OutOverrides.ContainsByPredicate([Override](const FAnimParentNodeAssetOverride* Other)
				{
					return Override.ParentNodeGuid == Other->ParentNodeGuid;
				});

				if (!OverrideExists)
				{
					OutOverrides.Add(&Override);
				}
			}
		}
	}

	return OutOverrides.Num() > 0;
}

void UAnimBlueprint::PostLoad()
{
	LLM_SCOPE(ELLMTag::Animation);

	Super::PostLoad();
#if WITH_EDITOR
	// Validate animation overrides
	UAnimBlueprintGeneratedClass* AnimBPGeneratedClass = GetAnimBlueprintGeneratedClass();
	
	if (AnimBPGeneratedClass)
	{
		// If there is no index for the guid, remove the entry.
		ParentAssetOverrides.RemoveAll([AnimBPGeneratedClass](const FAnimParentNodeAssetOverride& Element)
		{
			if (!AnimBPGeneratedClass->GetNodePropertyIndexFromGuid(Element.ParentNodeGuid, EPropertySearchMode::Hierarchy))
			{
				return true;
			}

			return false;
		});
	}
#endif

#if WITH_EDITORONLY_DATA
	if(GetLinkerCustomVersion(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::AnimBlueprintSubgraphFix)
	{
		AnimationEditorUtils::RegenerateSubGraphArrays(this);
	}
#endif
}

bool UAnimBlueprint::CanRecompileWhilePlayingInEditor() const
{
	return true;
}
#endif

USkeletalMesh* UAnimBlueprint::GetPreviewMesh(bool bFindIfNotSet/*=false*/)
{
#if WITH_EDITORONLY_DATA
	USkeletalMesh* PreviewMesh = PreviewSkeletalMesh.LoadSynchronous();
	// if somehow skeleton changes, just nullify it. 
	if (PreviewMesh && PreviewMesh->Skeleton != TargetSkeleton)
	{
		PreviewMesh = nullptr;
		SetPreviewMesh(nullptr);
	}

	return PreviewMesh;
#else
	return nullptr;
#endif
}

USkeletalMesh* UAnimBlueprint::GetPreviewMesh() const
{
#if WITH_EDITORONLY_DATA
	if (!PreviewSkeletalMesh.IsValid())
	{
		PreviewSkeletalMesh.LoadSynchronous();
	}
	return PreviewSkeletalMesh.Get();
#else
	return nullptr;
#endif
}

void UAnimBlueprint::SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty/*=true*/)
{
#if WITH_EDITORONLY_DATA
	if(bMarkAsDirty)
	{
		Modify();
	}
	PreviewSkeletalMesh = PreviewMesh;
#endif
}