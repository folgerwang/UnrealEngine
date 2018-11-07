// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/TextProperty.h"

#include "VariantSet.generated.h"


class UVariant;


UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UVariantSet : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	class ULevelVariantSets* GetParent();

	UFUNCTION(BlueprintCallable, Category="VariantSet")
	void SetDisplayText(const FText& NewDisplayText);

	UFUNCTION(BlueprintCallable, Category="VariantSet")
	FText GetDisplayText() const;

	FString GetUniqueVariantName(const FString& InPrefix);

	void AddVariants(const TArray<UVariant*>& NewVariants, int32 Index = INDEX_NONE);
	const TArray<UVariant*>& GetVariants() const;
	void RemoveVariants(const TArray<UVariant*>& InVariants);

	UFUNCTION(BlueprintCallable, Category="VariantSet")
	int32 GetNumVariants();

	UFUNCTION(BlueprintCallable, Category="VariantSet")
	UVariant* GetVariant(int32 VariantIndex);

private:

	UPROPERTY()
	FText DisplayText;

	UPROPERTY()
	TArray<UVariant*> Variants;
};
