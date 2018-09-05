// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LevelVariantSets.h"

#include "VariantSet.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"


ULevelVariantSets::ULevelVariantSets(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULevelVariantSets::AddVariantSet(UVariantSet* NewVariantSet)
{
	// Take ownership of NewVariantSet
	NewVariantSet->Rename(*NewVariantSet->GetName(), this);

	VariantSets.Add(NewVariantSet);
}

void ULevelVariantSets::RemoveVariantSet(UVariantSet* ThisVariantSet)
{
	VariantSets.RemoveSingle(ThisVariantSet);
}

void ULevelVariantSets::SaveExpansionState(UVariantSet* VarSetOfNode, bool bExpanded)
{
	DisplayNodeExpansionStates.Add(VarSetOfNode, bExpanded);
}

bool ULevelVariantSets::GetExpansionState(UVariantSet* VarSetOfNode)
{
	if (DisplayNodeExpansionStates.Contains(VarSetOfNode))
	{
		return DisplayNodeExpansionStates[VarSetOfNode];
	}
	return false;
}