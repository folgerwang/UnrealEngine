// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "LevelVariantSets.generated.h"


class UVariantSet;


UCLASS(DefaultToInstanced)
class VARIANTMANAGERCONTENT_API ULevelVariantSets : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**~ UObject implementation */
	//virtual void Serialize(FArchive& Ar) override;

	void AddVariantSet(UVariantSet* NewVariantSet);
	const TArray<UVariantSet*>& GetVariantSets() const
	{
		return VariantSets;
	}
	void RemoveVariantSet(UVariantSet* ThisVariantSet);

	void SaveExpansionState(UVariantSet* VarSetOfNode, bool bExpanded);
	bool GetExpansionState(UVariantSet* VarSetOfNode);

private:

	UPROPERTY()
	TArray<UVariantSet*> VariantSets;

	UPROPERTY()
	TMap<UVariantSet*, bool> DisplayNodeExpansionStates;
};
