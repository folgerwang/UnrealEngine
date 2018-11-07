// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/TextProperty.h"

#include "Variant.generated.h"

class AActor;
class UVariantObjectBinding;


UCLASS(DefaultToInstanced)
class VARIANTMANAGERCONTENT_API UVariant : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	class UVariantSet* GetParent();

	UFUNCTION(BlueprintCallable, Category="Variant")
	void SetDisplayText(const FText& NewDisplayText);

	UFUNCTION(BlueprintCallable, Category="Variant")
	FText GetDisplayText() const;

	// In case of a duplicate binding these will destroy the older bindings
	void AddBindings(const TArray<UVariantObjectBinding*>& NewBindings, int32 Index = INDEX_NONE);
	const TArray<UVariantObjectBinding*>& GetBindings();
	void RemoveBindings(const TArray<UVariantObjectBinding*>& Bindings);

	UFUNCTION(BlueprintCallable, Category="Variant")
	int32 GetNumActors();

	UFUNCTION(BlueprintCallable, Category="Variant")
	AActor* GetActor(int32 ActorIndex);

	UFUNCTION(BlueprintCallable, Category="Variant")
	void SwitchOn();

private:

	UPROPERTY()
	FText DisplayText;

	UPROPERTY()
	TArray<UVariantObjectBinding*> ObjectBindings;
};
