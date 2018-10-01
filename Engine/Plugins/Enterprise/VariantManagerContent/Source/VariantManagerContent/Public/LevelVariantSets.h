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

	void AddVariantSets(const TArray<UVariantSet*>& NewVariantSets, int32 Index = INDEX_NONE);
	const TArray<UVariantSet*>& GetVariantSets() const;
	void RemoveVariantSets(const TArray<UVariantSet*> InVariantSets);

	FString GetUniqueVariantSetName(const FString& Prefix);

	void SaveExpansionState(UVariantSet* VarSetOfNode, bool bExpanded);
	bool GetExpansionState(UVariantSet* VarSetOfNode);

	UFUNCTION(BlueprintCallable, Category="LevelVariantSets")
	int32 GetNumVariantSets();

	UFUNCTION(BlueprintCallable, Category="LevelVariantSets")
	UVariantSet* GetVariantSet(int32 VariantSetIndex);

private:

	UPROPERTY()
	TArray<UVariantSet*> VariantSets;

	UPROPERTY()
	TMap<UVariantSet*, bool> DisplayNodeExpansionStates;
};
