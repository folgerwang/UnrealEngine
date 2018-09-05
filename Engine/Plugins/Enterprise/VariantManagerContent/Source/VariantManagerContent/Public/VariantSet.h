// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "VariantSet.generated.h"

class UVariant;


UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UVariantSet : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**~ UObject implementation */
	//virtual void Serialize(FArchive& Ar) override;

	// We need this because UVariantObjectBindings have non-UPROPERTY FLazyObjectPtrs
	// that need to be manually copied
	UVariantSet* Clone(UObject* ClonesOuter = INVALID_OBJECT);

	class ULevelVariantSets* GetParent();

	void SetDisplayName(const FText& NewDisplayName);
	FText GetDisplayName() const;
	FText GetDefaultDisplayName() const;

	void AddVariant(UVariant* NewVariant);
	const TArray<UVariant*>& GetVariants() const
	{
		return Variants;
	}

	void RemoveVariant(UVariant* ThisVariant);

	int32 GetSortingOrder() const
	{
		return SortingOrder;
	}

	void SetSortingOrder(const int32 InSortingOrder)
	{
		SortingOrder = InSortingOrder;
	}

private:
	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	int32 SortingOrder;

	// We manually duplicate these within our Clone function
	UPROPERTY(DuplicateTransient)
	TArray<UVariant*> Variants;
};
