// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/LazyObjectPtr.h"
#include "Misc/Guid.h"

#include "VariantObjectBinding.generated.h"


UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UVariantObjectBinding : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	void Init(const UObject* InObject, int32 InSortingOrder = -1)
	{
		ObjectPtr = FLazyObjectPtr(InObject);
		SetSortingOrder(InSortingOrder);
	}

	// We need this because UVariantObjectBindings have non-UPROPERTY FLazyObjectPtrs
	// that need to be manually copied
	UVariantObjectBinding* Clone(UObject* ClonesOuter = INVALID_OBJECT);

	class UVariant* GetParent();

	const FGuid& GetObjectGuid() const
	{
		return ObjectPtr.GetUniqueID().GetGuid();
	}

    UObject* GetObject() const
    {
        return ObjectPtr.Get();
    }

	int32 GetSortingOrder() const
	{
		return SortingOrder;
	}

	void SetSortingOrder(const int32 InSortingOrder)
	{
		SortingOrder = InSortingOrder;
	}

private:
	FLazyObjectPtr ObjectPtr;

	UPROPERTY()
	int32 SortingOrder;
};
