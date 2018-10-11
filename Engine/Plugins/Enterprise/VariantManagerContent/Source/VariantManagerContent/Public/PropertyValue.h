// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "PropertyValue.generated.h"

#define PATH_DELIMITER TEXT(" / ")

VARIANTMANAGERCONTENT_API DECLARE_LOG_CATEGORY_EXTERN(LogVariantContent, Log, All);


DECLARE_MULTICAST_DELEGATE(FOnPropertyRecorded);
DECLARE_MULTICAST_DELEGATE(FOnPropertyApplied);


class UVariantObjectBinding;


UENUM()
enum class EPropertyValueCategory : uint8
{
	Generic,
	RelativeLocation,
	RelativeRotation,
	RelativeScale3D,
	bVisible,
	Material
};

UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValue : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	void Init(const TArray<UProperty*>& InProps, const TArray<int32> InIndices, const FString& InFullDisplayString, EPropertyValueCategory InCategory = EPropertyValueCategory::Generic);
	const TArray<UProperty*>& GetProperties() const;
	const TArray<int32>& GetPropertyIndices() const;
	class UVariantObjectBinding* GetParent() const;

	// UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	// Checks if our parent object binding has our property. Once resolved we can use the other utility
	// functions like get container or property addresses
	virtual bool Resolve();
	bool HasValidResolve() const;
	void ClearLastResolve();

	// Need valid resolve
	void* GetPropertyParentContainerAddress() const;
	UStruct* GetPropertyParentContainerClass() const;
	virtual void RecordDataFromResolvedObject();
	virtual void ApplyDataToResolvedObject();
	//~ End valid resolve requirement

	// Returns the type of UProperty (UObjectProperty, UFloatProperty, etc)
	UClass* GetPropertyClass() const;
	// If we're a UStructProperty, returns the type of UScriptStruct that we can point at
	UScriptStruct* GetStructPropertyStruct() const;
	// If we're an UObjectProperty, returns the UClass of UObjects that we can point at
	UClass* GetObjectPropertyObjectClass() const;
	FName GetPropertyName() const;
	const FString& GetFullDisplayString() const;
	FString GetLeafDisplayString() const;
	int32 GetValueSizeInBytes() const;
	int32 GetPropertyOffsetInBytes() const;

	bool HasRecordedData() const;
	const TArray<uint8>& GetRecordedData();
	void SetRecordedData(const uint8* NewDataBytes, int32 NumBytes);

	FOnPropertyApplied& GetOnPropertyApplied();
	FOnPropertyRecorded& GetOnPropertyRecorded();

protected:

	UProperty* GetProperty() const;

	bool ResolvePropertiesRecursive(UStruct* ContainerClass, void* ContainerAddress, int32 PropertyIndex);

	FOnPropertyApplied OnPropertyApplied;
	FOnPropertyRecorded OnPropertyRecorded;

	// Temp data cached from last resolve
	UProperty* LeafProperty;
	UStruct* ParentContainerClass;
	void* ParentContainerAddress;
	uint8* PropertyValuePtr;

	UPROPERTY()
	FString FullDisplayString;

	UPROPERTY()
	bool bHasRecordedData;

	UPROPERTY()
	bool bIsObjectProperty;

	UPROPERTY()
	TArray<uint8> ValueBytes;

	UPROPERTY()
	EPropertyValueCategory PropCategory;

	UPROPERTY()
	TArray<UProperty*> Properties;

	UPROPERTY()
	TArray<int32> PropertyIndices;

	TSoftObjectPtr<UObject> TempObjPtr;
};