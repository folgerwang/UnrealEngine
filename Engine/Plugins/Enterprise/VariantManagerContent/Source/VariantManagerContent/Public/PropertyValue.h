// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "PropertyValue.generated.h"

#define PATH_DELIMITER TEXT(" / ")
#define ATTACH_CHILDREN_NAME TEXT("Children")

VARIANTMANAGERCONTENT_API DECLARE_LOG_CATEGORY_EXTERN(LogVariantContent, Log, All);

DECLARE_MULTICAST_DELEGATE(FOnPropertyRecorded);
DECLARE_MULTICAST_DELEGATE(FOnPropertyApplied);

class UVariantObjectBinding;


UENUM()
enum class EPropertyValueCategory : uint8
{
	Undefined = 0,
	Generic = 1,
	RelativeLocation = 2,
	RelativeRotation = 4,
	RelativeScale3D = 8,
	bVisible = 16,
	Material = 32
};
ENUM_CLASS_FLAGS(EPropertyValueCategory)

UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValue : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	void Init(const TArray<UProperty*>& InProps, const TArray<int32> InIndices, const FString& InFullDisplayString, EPropertyValueCategory InCategory = EPropertyValueCategory::Generic);

	class UVariantObjectBinding* GetParent() const;

	// Combined hash of this property and its indices
	// We don't use GetTypeHash for this because almost always we want to hash UPropertyValues by
	// the pointer instead, for complete uniqueness even with the same propertypath
	// This is mostly just used for grouping UPropertyValues together for editing multiple at once
	uint32 GetPropertyPathHash();

	// UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	// Tries to resolve the property value on the passed object, or the parent binding's bound object if the argument is nullptr
	virtual bool Resolve(UObject* OnObject = nullptr);
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
	EPropertyValueCategory GetPropCategory() const;
	UScriptStruct* GetStructPropertyStruct() const;
	UClass* GetObjectPropertyObjectClass() const;
	UEnum* GetEnumPropertyEnum() const;

	// Utility functions for UEnumProperties
	TArray<FName> GetValidEnumsFromPropertyOverride();
	FString GetEnumDocumentationLink();
	bool IsNumericPropertySigned();
	bool IsNumericPropertyUnsigned();
	bool IsNumericPropertyFloatingPoint();

	// Utility functions for string properties
	const FName& GetNamePropertyName() const;
	const FString& GetStrPropertyString() const;
	const FText& GetTextPropertyText() const;

	FName GetPropertyName() const;
	UFUNCTION(BlueprintCallable, Category="PropertyValue")
	FText GetPropertyTooltip() const;
	UFUNCTION(BlueprintCallable, Category="PropertyValue")
	const FString& GetFullDisplayString() const;
	FString GetLeafDisplayString() const;
	int32 GetValueSizeInBytes() const;
	int32 GetPropertyOffsetInBytes() const;

	UFUNCTION(BlueprintCallable, Category="PropertyValue")
	bool HasRecordedData() const;
	const TArray<uint8>& GetRecordedData();
	void SetRecordedData(const uint8* NewDataBytes, int32 NumBytes, int32 Offset = 0);

	FOnPropertyApplied& GetOnPropertyApplied();
	FOnPropertyRecorded& GetOnPropertyRecorded();

protected:

	UProperty* GetProperty() const;

	// Check if our parent object has the property path we captured. We take the segmented path here because when re-resolving
	// we might encounter another component in the same hierarchy position. Since we display the component names, its nice to
	// update the displayed name too
	bool ResolvePropertiesRecursive(UStruct* ContainerClass, void* ContainerAddress, int32 PropertyIndex, TArray<FString>& SegmentedFullPath);

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
	FName TempName;
	FString TempStr;
	FText TempText;
};