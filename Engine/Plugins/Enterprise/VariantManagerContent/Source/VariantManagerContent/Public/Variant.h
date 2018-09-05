// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "Variant.generated.h"


class UVariantObjectBinding;


UCLASS(DefaultToInstanced)
class VARIANTMANAGERCONTENT_API UVariant : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**~ UObject implementation */
	//virtual void Serialize(FArchive& Ar) override;

	// We need this because UVariantObjectBindings have non-UPROPERTY FLazyObjectPtrs
	// that need to be manually copied
	UVariant* Clone(UObject* ClonesOuter = INVALID_OBJECT);

	class UVariantSet* GetParent();

	void SetDisplayName(const FText& NewDisplayName);
	FText GetDisplayName() const;
	FText GetDefaultDisplayName() const;

	void AddActors(TWeakObjectPtr<AActor> InActor);
	void AddActors(const TArray<TWeakObjectPtr<AActor>>& InActors);

	void AddBinding(UVariantObjectBinding* NewBinding);

	const TArray<UVariantObjectBinding*>& GetBindings()
	{
		return ObjectBindings;
	}

	void RemoveBinding(UVariantObjectBinding* ThisBinding);

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
	TArray<UVariantObjectBinding*> ObjectBindings;
};
