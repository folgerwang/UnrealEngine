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
	Material = 32,
	Color = 64
};
ENUM_CLASS_FLAGS(EPropertyValueCategory)

// Describes one link in a full property path
// For array properties, a link might be the outer (e.g. AttachChildren, -1, None)
// while also it may be an inner (e.g. AttachChildren, 2, Cube)
// Doing this allows us to resolve components regardless of their order, which
// is important for handling component reordering and transient components (e.g.
// runtime billboard components, etc)
USTRUCT()
struct FCapturedPropSegment
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString PropertyName;

	UPROPERTY()
	int32 PropertyIndex = INDEX_NONE;

	UPROPERTY()
	FString ComponentName;
};

UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValue : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	void Init(const TArray<FCapturedPropSegment>& InCapturedPropSegments, UClass* InLeafPropertyClass, const FString& InFullDisplayString, const FName& InPropertySetterName, EPropertyValueCategory InCategory = EPropertyValueCategory::Generic);

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
	virtual UStruct* GetPropertyParentContainerClass() const;
	virtual void RecordDataFromResolvedObject();
	virtual void ApplyDataToResolvedObject();
	//~ End valid resolve requirement

	// Returns the type of UProperty (UObjectProperty, UFloatProperty, etc)
	virtual UClass* GetPropertyClass() const;
	EPropertyValueCategory GetPropCategory() const;
	virtual UScriptStruct* GetStructPropertyStruct() const;
	virtual UClass* GetObjectPropertyObjectClass() const;
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
	virtual int32 GetValueSizeInBytes() const;
	int32 GetPropertyOffsetInBytes() const;

	UFUNCTION(BlueprintCallable, Category="PropertyValue")
	bool HasRecordedData() const;
	const TArray<uint8>& GetRecordedData();
	void SetRecordedData(const uint8* NewDataBytes, int32 NumBytes, int32 Offset = 0);

	FOnPropertyApplied& GetOnPropertyApplied();
	FOnPropertyRecorded& GetOnPropertyRecorded();

protected:

	UProperty* GetProperty() const;

	// Applies the recorded data to the TargetObject via the PropertySetter function
	// (e.g. SetIntensity instead of setting the Intensity UPROPERTY directly)
	void ApplyViaFunctionSetter(UObject* TargetObject);

	// Check if our parent object has the property path we captured
	bool ResolvePropertiesRecursive(UStruct* ContainerClass, void* ContainerAddress, int32 PropertyIndex);

	FOnPropertyApplied OnPropertyApplied;
	FOnPropertyRecorded OnPropertyRecorded;

	// Temp data cached from last resolve
	UProperty* LeafProperty;
	UStruct* ParentContainerClass;
	void* ParentContainerAddress;
	uint8* PropertyValuePtr;
	UFunction* PropertySetter;

	// Properties were previously stored like this. Use CapturedPropSegments from now on, which stores
	// properties by name instead. It is much safer, as we can't guarantee these pointers will be valid
	// if they point at other packages (will depend on package load order, etc).
	UPROPERTY()
	TArray<UProperty*> Properties_DEPRECATED;
	UPROPERTY()
	TArray<int32> PropertyIndices_DEPRECATED;

	UPROPERTY()
	TArray<FCapturedPropSegment> CapturedPropSegments;

	UPROPERTY()
	FString FullDisplayString;

	UPROPERTY()
	FName PropertySetterName;

	UPROPERTY()
	TMap<FString, FString> PropertySetterParameterDefaults;

	UPROPERTY()
	bool bHasRecordedData;

	// We use these mainly to know how to serialize/deserialize the values of properties that need special care
	// (e.g. UObjectProperties, name properties, text properties, etc)
	UPROPERTY()
	UClass* LeafPropertyClass;

	UPROPERTY()
	TArray<uint8> ValueBytes;

	UPROPERTY()
	EPropertyValueCategory PropCategory;

	TSoftObjectPtr<UObject> TempObjPtr;
	FName TempName;
	FString TempStr;
	FText TempText;
};

// Deprecated: Only here for backwards compatibility with 4.21
UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValueTransform : public UPropertyValue
{
	GENERATED_UCLASS_BODY()

	// UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface
};

// Deprecated: Only here for backwards compatibility
UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValueVisibility : public UPropertyValue
{
	GENERATED_UCLASS_BODY()

	// UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface
};