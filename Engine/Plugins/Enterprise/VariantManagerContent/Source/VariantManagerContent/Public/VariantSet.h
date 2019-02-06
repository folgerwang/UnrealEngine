// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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

	// UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	// Sets whether this variant set is expanded or not when displayed
	// in a variant manager
	bool IsExpanded();
	void SetExpanded(bool bInExpanded);

	UFUNCTION(BlueprintCallable, Category="VariantSet")
	void SetDisplayText(const FText& NewDisplayText);

	UFUNCTION(BlueprintPure, Category="VariantSet")
	FText GetDisplayText() const;

	FString GetUniqueVariantName(const FString& InPrefix);

	void AddVariants(const TArray<UVariant*>& NewVariants, int32 Index = INDEX_NONE);
	int32 GetVariantIndex(UVariant* Var);
	const TArray<UVariant*>& GetVariants() const;
	void RemoveVariants(const TArray<UVariant*>& InVariants);

	UFUNCTION(BlueprintPure, Category="VariantSet")
	int32 GetNumVariants();

	UFUNCTION(BlueprintPure, Category="VariantSet")
	UVariant* GetVariant(int32 VariantIndex);

	UFUNCTION(BlueprintPure, Category="VariantSet")
	UVariant* GetVariantByName(FString VariantName);

private:

	// The display name used to be a property. Use the non-deprecated, non-property version from now on
	UPROPERTY()
	FText DisplayText_DEPRECATED;

	FText DisplayText;

	UPROPERTY()
	bool bExpanded;

	UPROPERTY()
	TArray<UVariant*> Variants;
};
