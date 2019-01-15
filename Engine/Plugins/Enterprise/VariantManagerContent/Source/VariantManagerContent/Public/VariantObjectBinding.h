// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/LazyObjectPtr.h"
#include "FunctionCaller.h"

#include "VariantObjectBinding.generated.h"

class UPropertyValue;

UCLASS(DefaultToInstanced)
class VARIANTMANAGERCONTENT_API UVariantObjectBinding : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	void Init(UObject* InObject);

	class UVariant* GetParent();

	FText GetDisplayText() const;

	FString GetObjectPath() const;
	UObject* GetObject() const;

	void AddCapturedProperties(const TArray<UPropertyValue*>& Properties);
	const TArray<UPropertyValue*>& GetCapturedProperties() const;
	void RemoveCapturedProperties(const TArray<UPropertyValue*>& Properties);
	void SortCapturedProperties();

	void AddFunctionCallers(const TArray<FFunctionCaller>& InFunctionCallers);
	TArray<FFunctionCaller>& GetFunctionCallers();
	void RemoveFunctionCallers(const TArray<FFunctionCaller>& InFunctionCallers);
	void ExecuteTargetFunction(FName FunctionName);
	void ExecuteAllTargetFunctions();

#if WITH_EDITORONLY_DATA
	void UpdateFunctionCallerNames();
#endif

private:

	UPROPERTY()
	mutable FSoftObjectPath ObjectPtr;

	UPROPERTY()
	mutable TLazyObjectPtr<UObject> LazyObjectPtr;

	UPROPERTY()
	TArray<UPropertyValue*> CapturedProperties;

	UPROPERTY()
	TArray<FFunctionCaller> FunctionCallers;
};
