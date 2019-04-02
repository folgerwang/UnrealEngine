// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/TextProperty.h"

#include "Variant.generated.h"


class UVariantObjectBinding;


UCLASS(DefaultToInstanced)
class VARIANTMANAGERCONTENT_API UVariant : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	class UVariantSet* GetParent();

	// UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	UFUNCTION(BlueprintCallable, Category="Variant")
	void SetDisplayText(const FText& NewDisplayText);

	UFUNCTION(BlueprintPure, Category="Variant")
	FText GetDisplayText() const;

	// In case of a duplicate binding these will destroy the older bindings
	void AddBindings(const TArray<UVariantObjectBinding*>& NewBindings, int32 Index = INDEX_NONE);
	int32 GetBindingIndex(UVariantObjectBinding* Binding);
	const TArray<UVariantObjectBinding*>& GetBindings() const;
	void RemoveBindings(const TArray<UVariantObjectBinding*>& Bindings);

	UFUNCTION(BlueprintPure, Category="Variant")
	int32 GetNumActors();

	UFUNCTION(BlueprintPure, Category="Variant")
	AActor* GetActor(int32 ActorIndex);

	UVariantObjectBinding* GetBindingByName(const FString& ActorName);

	UFUNCTION(BlueprintCallable, Category="Variant")
	void SwitchOn();

private:

	// The display name used to be a property. Use the non-deprecated, non-property version from now on
	UPROPERTY()
	FText DisplayText_DEPRECATED;

	FText DisplayText;

	UPROPERTY()
	TArray<UVariantObjectBinding*> ObjectBindings;
};
