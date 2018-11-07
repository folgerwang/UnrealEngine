// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/LazyObjectPtr.h"

#include "VariantObjectBinding.generated.h"


class UPropertyValue;


UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UVariantObjectBinding : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	void Init(UObject* InObject);
	class UVariant* GetParent();

	FText GetDisplayText() const;

	FString GetObjectPath() const;
	UObject* GetObject() const;

	// Calls FixupForPIE on our ObjectPtr. It is necessary to call this from a moment where
	// GPlayInEditorID has a valid ID (like an AActor's BeginPlay), as UMG blueprint function calls
	// are not one of those. See the comment in the implementation of this function for more details
	void FixupForPIE();

	void AddCapturedProperties(const TArray<UPropertyValue*>& Properties, int32 Index = INDEX_NONE);
	const TArray<UPropertyValue*>& GetCapturedProperties() const;
	void RemoveCapturedProperties(const TArray<UPropertyValue*>& Properties);

private:

	UPROPERTY()
	mutable FSoftObjectPath ObjectPtr;

	UPROPERTY()
	TLazyObjectPtr<UObject> LazyObjectPtr;

	UPROPERTY()
	TArray<UPropertyValue*> CapturedProperties;
};
