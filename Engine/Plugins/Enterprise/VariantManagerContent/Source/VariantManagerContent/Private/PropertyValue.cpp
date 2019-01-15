// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PropertyValue.h"

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "HAL/UnrealMemory.h"
#include "UObject/Package.h"
#include "UObject/TextProperty.h"
#include "VariantObjectBinding.h"
#include "VariantManagerObjectVersion.h"

#define LOCTEXT_NAMESPACE "PropertyValue"

DEFINE_LOG_CATEGORY(LogVariantContent);

UPropertyValue::UPropertyValue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPropertyValue::Init(const TArray<UProperty*>& InProps, const TArray<int32> InIndices, const FString& InFullDisplayString, EPropertyValueCategory InCategory)
{
	Properties = InProps;
	PropertyIndices = InIndices;
	FullDisplayString = InFullDisplayString;
	PropCategory = InCategory;

	int32 NumProps = Properties.Num();
	if (NumProps > 0 && Properties[NumProps-1]->IsA(UObjectProperty::StaticClass()))
	{
		bIsObjectProperty = true;
	}

	ClearLastResolve();
	ValueBytes.SetNumUninitialized(GetValueSizeInBytes());
	TempObjPtr.Reset();
}

UVariantObjectBinding* UPropertyValue::GetParent() const
{
	return Cast<UVariantObjectBinding>(GetOuter());
}

uint32 UPropertyValue::GetPropertyPathHash()
{
	uint32 Hash = 0;
	for (UProperty* Prop : Properties)
	{
		Hash = HashCombine(Hash, GetTypeHash(Prop));
	}
	for (int32 Index : PropertyIndices)
	{
		Hash = HashCombine(Hash, GetTypeHash(Index));
	}
	return Hash;
}

void UPropertyValue::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FVariantManagerObjectVersion::GUID);
	int32 CustomVersion = Ar.CustomVer(FVariantManagerObjectVersion::GUID);

	if (Ar.IsSaving())
	{
		// If the pointer is not null, means we haven't dealt with it yet (haven't needed GetRecordedData)
		// so just save it back the way we received it
		// If our pointer is null but we have bytes representing an UObject reference, it means we
		// read our pointer at some point (or just created this propertyvalue), so we need to create it
		if (TempObjPtr.IsNull())
		{
			if (bIsObjectProperty && HasRecordedData())
			{
				UObject* Obj = *((UObject**)ValueBytes.GetData());
				if (Obj && Obj->IsValidLowLevel())
				{
					TempObjPtr = Obj;
				}
			}
			else
			{
				TempObjPtr.Reset();
			}
		}

		Ar << TempObjPtr;

		if (CustomVersion >= FVariantManagerObjectVersion::CorrectSerializationOfFStringBytes)
		{
			// These are either setup during IsLoading() or when SetRecordedData
			Ar << TempName;
			Ar << TempStr;
			Ar << TempText;
		}
		else if (CustomVersion >= FVariantManagerObjectVersion::CorrectSerializationOfFNameBytes)
		{
			FName Name;
			if (GetPropertyClass()->IsChildOf(UNameProperty::StaticClass()))
			{
				Name = *((FName*)ValueBytes.GetData());
			}

			Ar << Name;
		}
	}
	else if (Ar.IsLoading())
	{
		Ar << TempObjPtr;

		if (CustomVersion >= FVariantManagerObjectVersion::CorrectSerializationOfFStringBytes)
		{
			Ar << TempName;
			Ar << TempStr;
			Ar << TempText;

			if (GetPropertyClass()->IsChildOf(UNameProperty::StaticClass()))
			{
				int32 NumBytes = sizeof(FName);
				ValueBytes.SetNumUninitialized(NumBytes);
				FMemory::Memcpy(ValueBytes.GetData(), &TempName, NumBytes);
			}
			else if (GetPropertyClass()->IsChildOf(UStrProperty::StaticClass()))
			{
				int32 NumBytes = sizeof(FString);
				ValueBytes.SetNumUninitialized(NumBytes);
				FMemory::Memcpy(ValueBytes.GetData(), &TempStr, NumBytes);
			}
			else if (GetPropertyClass()->IsChildOf(UTextProperty::StaticClass()))
			{
				int32 NumBytes = sizeof(FText);
				ValueBytes.SetNumUninitialized(NumBytes);
				FMemory::Memcpy(ValueBytes.GetData(), &TempText, NumBytes);
			}
		}
		else if (CustomVersion >= FVariantManagerObjectVersion::CorrectSerializationOfFNameBytes)
		{
			FName Name;
			Ar << Name;
			if (GetPropertyClass() == UNameProperty::StaticClass())
			{
				int32 NumBytes = sizeof(FName);
				ValueBytes.SetNumUninitialized(NumBytes);
				FMemory::Memcpy(ValueBytes.GetData(), &Name, NumBytes);
			}
		}
	}
}

bool UPropertyValue::Resolve(UObject* Object)
{
	if (Object == nullptr)
	{
		UVariantObjectBinding* Parent = GetParent();
		if (Parent)
		{
			Object = Parent->GetObject();
		}
	}

	if (Object == nullptr)
	{
		return false;
	}

	TArray<FString> SegmentedFullPath;
	FullDisplayString.ParseIntoArray(SegmentedFullPath, PATH_DELIMITER);

	if (!ResolvePropertiesRecursive(Object->GetClass(), Object, 0, SegmentedFullPath))
	{
		// UE_LOG(LogVariantContent, Error, TEXT("Failed to resolve UPropertyValue '%s' on UObject '%s'"), *GetFullDisplayString(), *Object->GetName());
		return false;
	}

	FString StringAfter = SegmentedFullPath[0];
	for (int32 Index = 1; Index < SegmentedFullPath.Num(); Index++)
	{
		StringAfter += PATH_DELIMITER + SegmentedFullPath[Index];
	}

	FullDisplayString = StringAfter;

	return true;
}

bool UPropertyValue::HasValidResolve() const
{
	return ParentContainerAddress != nullptr;
}

void UPropertyValue::ClearLastResolve()
{
	LeafProperty = nullptr;
	ParentContainerClass = nullptr;
	ParentContainerAddress = nullptr;
	PropertyValuePtr = nullptr;
}

void* UPropertyValue::GetPropertyParentContainerAddress() const
{
	return ParentContainerAddress;
}

UStruct* UPropertyValue::GetPropertyParentContainerClass() const
{
	return ParentContainerClass;
}

void UPropertyValue::RecordDataFromResolvedObject()
{
	if (!Resolve())
	{
		return;
	}

	int32 PropertySizeBytes = GetValueSizeInBytes();

	if (UBoolProperty* PropAsBool = Cast<UBoolProperty>(LeafProperty))
	{
		// This could probably be done in a cleaner way since we know its an UBoolProperty...
		TArray<uint8> BoolBytes;
		BoolBytes.SetNum(PropertySizeBytes);

		bool* BoolBytesAddress = (bool*)BoolBytes.GetData();
		*BoolBytesAddress = PropAsBool->GetPropertyValue(PropertyValuePtr);

		SetRecordedData(BoolBytes.GetData(), PropertySizeBytes);
	}
	else if (UEnumProperty* PropAsEnum = Cast<UEnumProperty>(LeafProperty))
	{
		UNumericProperty* UnderlyingProp = PropAsEnum->GetUnderlyingProperty();
		PropertySizeBytes = UnderlyingProp->ElementSize;

		SetRecordedData(PropertyValuePtr, PropertySizeBytes);
	}
	else
	{
		SetRecordedData(PropertyValuePtr, PropertySizeBytes);
	}

	OnPropertyRecorded.Broadcast();
}

void UPropertyValue::ApplyDataToResolvedObject()
{
	if (!HasRecordedData() || !Resolve())
	{
		return;
	}

	// Ready to transact
	UObject* ContainerOwnerObject = nullptr;
	UVariantObjectBinding* Parent = GetParent();
	if (Parent)
	{
		ContainerOwnerObject = Parent->GetObject();
		if (ContainerOwnerObject)
		{
			ContainerOwnerObject->SetFlags(RF_Transactional);
			ContainerOwnerObject->Modify();
		}
	}
	// We might also need to modify a component if we're nested in one
	UObject* ContainerObject = (UObject*) ParentContainerAddress;
	if (ContainerObject && ContainerObject->IsA(UActorComponent::StaticClass()))
	{
		ContainerObject->SetFlags(RF_Transactional);
		ContainerObject->Modify();
	}

	// Bool properties need to be set in a particular way since they hold internal private
	// masks and offsets
	if (UBoolProperty* PropAsBool = Cast<UBoolProperty>(LeafProperty))
	{
		bool* ValueBytesAsBool = (bool*)ValueBytes.GetData();
		PropAsBool->SetPropertyValue(PropertyValuePtr, *ValueBytesAsBool);
	}
	else if (UEnumProperty* PropAsEnum = Cast<UEnumProperty>(LeafProperty))
	{
		UNumericProperty* UnderlyingProp = PropAsEnum->GetUnderlyingProperty();
		int32 PropertySizeBytes = UnderlyingProp->ElementSize;

		ValueBytes.SetNum(PropertySizeBytes);
		FMemory::Memcpy(PropertyValuePtr, ValueBytes.GetData(), PropertySizeBytes);
	}
	else if (UNameProperty* PropAsName = Cast<UNameProperty>(LeafProperty))
	{
		FName Value = GetNamePropertyName();
		PropAsName->SetPropertyValue(PropertyValuePtr, Value);
	}
	else if (UStrProperty* PropAsStr = Cast<UStrProperty>(LeafProperty))
	{
		FString Value = GetStrPropertyString();
		PropAsStr->SetPropertyValue(PropertyValuePtr, Value);
	}
	else if (UTextProperty* PropAsText = Cast<UTextProperty>(LeafProperty))
	{
		FText Value = GetTextPropertyText();
		PropAsText->SetPropertyValue(PropertyValuePtr, Value);
	}
	else
	{
		// Actually change the object through its property value ptr
		int32 PropertySizeBytes = LeafProperty->ElementSize;
		ValueBytes.SetNum(PropertySizeBytes);
		FMemory::Memcpy(PropertyValuePtr, ValueBytes.GetData(), PropertySizeBytes);
	}

#if WITH_EDITOR
	// Update object on viewport
	if (ContainerObject)
	{
		ContainerObject->PostEditChange();
	}
	if (ContainerOwnerObject)
	{
		ContainerOwnerObject->PostEditChange();
	}
#endif
	OnPropertyApplied.Broadcast();
}

UClass* UPropertyValue::GetPropertyClass() const
{
	UProperty* Prop = GetProperty();
	if (Prop)
	{
		return Prop->GetClass();
	}

	return nullptr;
}

EPropertyValueCategory UPropertyValue::GetPropCategory() const
{
	return PropCategory;
}

UScriptStruct* UPropertyValue::GetStructPropertyStruct() const
{
	UProperty* Prop = GetProperty();
	if (UStructProperty* StructProp = Cast<UStructProperty>(Prop))
	{
		return StructProp->Struct;
	}

	return nullptr;
}

UClass* UPropertyValue::GetObjectPropertyObjectClass() const
{
	if (UObjectProperty* ObjProp = Cast<UObjectProperty>(GetProperty()))
	{
		return ObjProp->PropertyClass;
	}

	return nullptr;
}

UEnum* UPropertyValue::GetEnumPropertyEnum() const
{
	UProperty* Property = GetProperty();
	if (UEnumProperty* EnumProp = Cast<UEnumProperty>(Property))
	{
		return EnumProp->GetEnum();
	}
	else if (UNumericProperty* NumProp = Cast<UNumericProperty>(Property))
	{
		return NumProp->GetIntPropertyEnum();
	}

	return nullptr;
}

// @Copypaste from PropertyEditorHelpers
TArray<FName> UPropertyValue::GetValidEnumsFromPropertyOverride()
{
	UEnum* Enum = GetEnumPropertyEnum();
	if (Enum == nullptr)
	{
		return TArray<FName>();
	}

	TArray<FName> ValidEnumValues;

#if WITH_EDITOR
	static const FName ValidEnumValuesName("ValidEnumValues");
	if(LeafProperty->HasMetaData(ValidEnumValuesName))
	{
		TArray<FString> ValidEnumValuesAsString;

		LeafProperty->GetMetaData(ValidEnumValuesName).ParseIntoArray(ValidEnumValuesAsString, TEXT(","));
		for(auto& Value : ValidEnumValuesAsString)
		{
			Value.TrimStartInline();
			ValidEnumValues.Add(*Enum->GenerateFullEnumName(*Value));
		}
	}
#endif

	return ValidEnumValues;
}

// @Copypaste from PropertyEditorHelpers
FString UPropertyValue::GetEnumDocumentationLink()
{
#if WITH_EDITOR
	if(LeafProperty != nullptr)
	{
		const UByteProperty* ByteProperty = Cast<UByteProperty>(LeafProperty);
		const UEnumProperty* EnumProperty = Cast<UEnumProperty>(LeafProperty);
		if(ByteProperty || EnumProperty || (LeafProperty->IsA(UStrProperty::StaticClass()) && LeafProperty->HasMetaData(TEXT("Enum"))))
		{
			UEnum* Enum = nullptr;
			if(ByteProperty)
			{
				Enum = ByteProperty->Enum;
			}
			else if (EnumProperty)
			{
				Enum = EnumProperty->GetEnum();
			}
			else
			{

				const FString& EnumName = LeafProperty->GetMetaData(TEXT("Enum"));
				Enum = FindObject<UEnum>(ANY_PACKAGE, *EnumName, true);
			}

			if(Enum)
			{
				return FString::Printf(TEXT("Shared/Enums/%s"), *Enum->GetName());
			}
		}
	}
#endif

	return TEXT("");
}

bool UPropertyValue::IsNumericPropertySigned()
{
	UProperty* Prop = GetProperty();
	if (UNumericProperty* NumericProp = Cast<UNumericProperty>(Prop))
	{
		return NumericProp->IsInteger() && NumericProp->CanHoldValue(-1);
	}
	else if (UEnumProperty* EnumProp = Cast<UEnumProperty>(Prop))
	{
		NumericProp = EnumProp->GetUnderlyingProperty();
		return NumericProp->IsInteger() && NumericProp->CanHoldValue(-1);
	}

	return false;
}

bool UPropertyValue::IsNumericPropertyUnsigned()
{
	UProperty* Prop = GetProperty();
	if (UNumericProperty* NumericProp = Cast<UNumericProperty>(Prop))
	{
		return NumericProp->IsInteger() && !NumericProp->CanHoldValue(-1);
	}
	else if (UEnumProperty* EnumProp = Cast<UEnumProperty>(Prop))
	{
		NumericProp = EnumProp->GetUnderlyingProperty();
		return NumericProp->IsInteger() && !NumericProp->CanHoldValue(-1);
	}

	return false;
}

bool UPropertyValue::IsNumericPropertyFloatingPoint()
{
	UProperty* Prop = GetProperty();
	if (UNumericProperty* NumericProp = Cast<UNumericProperty>(Prop))
	{
		return NumericProp->IsFloatingPoint();
	}
	else if (UEnumProperty* EnumProp = Cast<UEnumProperty>(Prop))
	{
		NumericProp = EnumProp->GetUnderlyingProperty();
		return NumericProp->IsFloatingPoint();
	}

	return false;
}

const FName& UPropertyValue::GetNamePropertyName() const
{
	return TempName;
}

const FString& UPropertyValue::GetStrPropertyString() const
{
	return TempStr;
}

const FText& UPropertyValue::GetTextPropertyText() const
{
	return TempText;
}

FName UPropertyValue::GetPropertyName() const
{
	UProperty* Prop = GetProperty();
	if (Prop)
	{
		return Prop->GetFName();
	}

	return FName();
}

FText UPropertyValue::GetPropertyTooltip() const
{
#if WITH_EDITOR
	UProperty* Prop = GetProperty();
	if (Prop)
	{
		return Prop->GetToolTipText();
	}
#endif

	return FText();
}

const FString& UPropertyValue::GetFullDisplayString() const
{
	return FullDisplayString;
}

FString UPropertyValue::GetLeafDisplayString() const
{
	FString LeftString;
	FString RightString;

	if(FullDisplayString.Split(PATH_DELIMITER, &LeftString, &RightString, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		return RightString;
	}

	return FullDisplayString;
}

int32 UPropertyValue::GetValueSizeInBytes() const
{
	UProperty* Prop = GetProperty();
	if (UEnumProperty* PropAsEnumProp = Cast<UEnumProperty>(Prop))
	{
		return PropAsEnumProp->GetUnderlyingProperty()->ElementSize;
	}
	else if (Prop)
	{
		return Prop->ElementSize;
	}

	return 0;
}

int32 UPropertyValue::GetPropertyOffsetInBytes() const
{
	UProperty* Prop = GetProperty();
	if (Prop)
	{
		return Prop->GetOffset_ForInternal();
	}

	// Dangerous
	return 0;
}

bool UPropertyValue::HasRecordedData() const
{
	return bHasRecordedData;
}

const TArray<uint8>& UPropertyValue::GetRecordedData()
{
	check(bHasRecordedData);

	ValueBytes.SetNum(GetValueSizeInBytes());

	// We need to resolve our softpath still
	if (bHasRecordedData && bIsObjectProperty && !TempObjPtr.IsNull())
	{
		// Force resolve of our soft object pointer
		UObject* Obj = TempObjPtr.LoadSynchronous();

		if (Obj && Obj->IsValidLowLevel())
		{
			int32 NumBytes = sizeof(UObject**);
			ValueBytes.SetNumUninitialized(NumBytes);
			FMemory::Memcpy(ValueBytes.GetData(), &Obj, NumBytes);

			bHasRecordedData = true;
		}
		else
		{
			bHasRecordedData = false;
		}

		TempObjPtr.Reset();
	}

	return ValueBytes;
}

void UPropertyValue::SetRecordedData(const uint8* NewDataBytes, int32 NumBytes, int32 Offset)
{
	Modify();

	if (NumBytes > 0)
	{
		// Because the string types are all handles into arrays/data, we need to reinterpret NewDataBytes
		// first, then copy that object into our Temps and have our ValueBytes refer to it instead.
		// This ensures we own the FString that we're pointing at (and so its internal data array)
		if (NumBytes == sizeof(FName) && GetPropertyClass()->IsChildOf(UNameProperty::StaticClass()))
		{
			TempName = *((FName*)NewDataBytes);
			ValueBytes.SetNumUninitialized(NumBytes);

			FMemory::Memcpy(ValueBytes.GetData(), (uint8*)&TempName, NumBytes);
			bHasRecordedData = true;
		}
		else if (NumBytes == sizeof(FString) && GetPropertyClass()->IsChildOf(UStrProperty::StaticClass()))
		{
			TempStr = *((FString*)NewDataBytes);
			ValueBytes.SetNumUninitialized(NumBytes);

			FMemory::Memcpy(ValueBytes.GetData(), (uint8*)&TempStr, NumBytes);
			bHasRecordedData = true;
		}
		else if (NumBytes == sizeof(FText) && GetPropertyClass()->IsChildOf(UTextProperty::StaticClass()))
		{
			TempText = *((FText*)NewDataBytes);
			ValueBytes.SetNumUninitialized(NumBytes);

			FMemory::Memcpy(ValueBytes.GetData(), (uint8*)&TempText, NumBytes);
			bHasRecordedData = true;
		}
		else
		{
			if (ValueBytes.Num() < NumBytes + Offset)
			{
				ValueBytes.SetNumUninitialized(NumBytes+Offset);
			}

			FMemory::Memcpy(ValueBytes.GetData() + Offset, NewDataBytes, NumBytes);
			bHasRecordedData = true;

			// Don't need to actually update the pointer, as that will be done when serializing
			// But we do need to reset it or else GetRecordedData will read its data instead of ValueBytes
			if (bIsObjectProperty)
			{
				TempObjPtr.Reset();
			}
		}
	}
}

FOnPropertyApplied& UPropertyValue::GetOnPropertyApplied()
{
	return OnPropertyApplied;
}

FOnPropertyRecorded& UPropertyValue::GetOnPropertyRecorded()
{
	return OnPropertyRecorded;
}

UProperty* UPropertyValue::GetProperty() const
{
	int32 NumProps = Properties.Num();
	if (NumProps > 0)
	{
		return Properties[NumProps-1];
	}

	return nullptr;
}

bool UPropertyValue::ResolvePropertiesRecursive(UStruct* ContainerClass, void* ContainerAddress, int32 SegmentIndex, TArray<FString>& SegmentedFullPath)
{
	// Adapted from PropertyPathHelpers.cpp because it is incomplete for arrays of UObjects (important for components)

	UProperty* Property = Properties[SegmentIndex];

	// This can happen if we capture a property then recompile without it
	if (Property == nullptr)
	{
		return false;
	}

	const int32 ArrayIndex = PropertyIndices[SegmentIndex] == INDEX_NONE ? 0 : PropertyIndices[SegmentIndex];

	if (SegmentIndex == 0)
	{
		ParentContainerClass = ContainerClass;
		ParentContainerAddress = ContainerAddress;
	}

	Property = FindField<UProperty>(ContainerClass, Property->GetFName());
	if (Property)
	{
		// Not the last link in the chain --> Dig down deeper updating our class/address if we jump an UObjectProp/UStructProp
		if (SegmentIndex < (Properties.Num()-1))
		{
			// Check first to see if this is a simple object (eg. not an array of objects)
			if (UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property))
			{
				// If it's an object we need to get the value of the property in the container first before we
				// can continue, if the object is null we safely stop processing the chain of properties.
				if ( UObject* CurrentObject = ObjectProperty->GetPropertyValue_InContainer(ContainerAddress, ArrayIndex) )
				{
					ParentContainerClass = CurrentObject->GetClass();
					ParentContainerAddress = CurrentObject;

					return ResolvePropertiesRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 1, SegmentedFullPath);
				}
			}
			// Check to see if this is a simple weak object property (eg. not an array of weak objects).
			else if (UWeakObjectProperty* WeakObjectProperty = Cast<UWeakObjectProperty>(Property))
			{
				FWeakObjectPtr WeakObject = WeakObjectProperty->GetPropertyValue_InContainer(ContainerAddress, ArrayIndex);

				// If it's an object we need to get the value of the property in the container first before we
				// can continue, if the object is null we safely stop processing the chain of properties.
				if ( UObject* CurrentObject = WeakObject.Get() )
				{
					ParentContainerClass = CurrentObject->GetClass();
					ParentContainerAddress = CurrentObject;

					return ResolvePropertiesRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 1, SegmentedFullPath);
				}
			}
			// Check to see if this is a simple soft object property (eg. not an array of soft objects).
			else if (USoftObjectProperty* SoftObjectProperty = Cast<USoftObjectProperty>(Property))
			{
				FSoftObjectPtr SoftObject = SoftObjectProperty->GetPropertyValue_InContainer(ContainerAddress, ArrayIndex);

				// If it's an object we need to get the value of the property in the container first before we
				// can continue, if the object is null we safely stop processing the chain of properties.
				if ( UObject* CurrentObject = SoftObject.Get() )
				{
					ParentContainerClass = CurrentObject->GetClass();
					ParentContainerAddress = CurrentObject;

					return ResolvePropertiesRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 1, SegmentedFullPath);
				}
			}
			// Check to see if this is a simple structure (eg. not an array of structures)
			// Note: We don't actually capture properties *inside* UStructProperties, so this path won't be taken. It is here
			// if we ever wish to change that in the future
			else if (UStructProperty* StructProp = Cast<UStructProperty>(Property))
			{
				void* StructAddress = StructProp->ContainerPtrToValuePtr<void>(ContainerAddress, ArrayIndex);

				ParentContainerClass = StructProp->Struct;
				ParentContainerAddress = StructAddress;

				return ResolvePropertiesRecursive(StructProp->Struct, StructAddress, SegmentIndex + 1, SegmentedFullPath);
			}
			else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property))
			{
				// We have to replicate these cases in here because we need to access the inner properties
				// with the FScriptArrayHelper. If we do another recursive call and try parsing the inner
				// property just as a regular property with an ArrayIndex, it will fail getting the ValuePtr
				// because for some reason properties always have ArrayDim = 1

				int32 InnerArrayIndex = PropertyIndices[SegmentIndex+1] == INDEX_NONE ? 0 : PropertyIndices[SegmentIndex+1];

				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ContainerAddress));
				if (!ArrayHelper.IsValidIndex(InnerArrayIndex))
				{
					// UE_LOG(LogVariantContent, Error, TEXT("Failed to resolve: UArrayProperty '%s' does not have an inner with index %d!"), *ArrayProp->GetName(), InnerArrayIndex);
					return false;
				}

				// Array properties show up in the path as two entries (one for the array prop and one for the inner)
				// so if we're on the second-to-last path segment, it means we want to capture the inner property, so don't
				// step into it
				if (SegmentIndex == Properties.Num() - 2)
				{
					LeafProperty = ArrayProp->Inner;
					PropertyValuePtr = ArrayHelper.GetRawPtr(InnerArrayIndex);

					return true;
				}

				if ( UStructProperty* ArrayOfStructsProp = Cast<UStructProperty>(ArrayProp->Inner) )
				{
					void* StructAddress = static_cast<void*>(ArrayHelper.GetRawPtr(InnerArrayIndex));

					ParentContainerClass = ArrayOfStructsProp->Struct;
					ParentContainerAddress = StructAddress;

					// The next link in the chain is just this array's inner. Let's just skip it instead
					return ResolvePropertiesRecursive(ArrayOfStructsProp->Struct, StructAddress, SegmentIndex + 2, SegmentedFullPath);
				}
				if ( UObjectProperty* InnerObjectProperty = Cast<UObjectProperty>(ArrayProp->Inner) )
				{
					void* ObjPtrContainer = ArrayHelper.GetRawPtr(InnerArrayIndex);
					UObject* CurrentObject = InnerObjectProperty->GetObjectPropertyValue(ObjPtrContainer);
					if (CurrentObject)
					{
						ParentContainerClass = CurrentObject->GetClass();
						ParentContainerAddress =  CurrentObject;

						if (CurrentObject->IsA(UActorComponent::StaticClass()))
						{
							// +0 instead of +2 here because we don't write two entries in the full path per array property, this
							// SegmentIndex is the array segment index
							SegmentedFullPath[SegmentIndex] = FString(ATTACH_CHILDREN_NAME) + TEXT("[") + FString::FromInt(InnerArrayIndex) + TEXT("] (") + CurrentObject->GetName() + TEXT(")");
						}

						// The next link in the chain is just this array's inner. Let's just skip it instead
						return ResolvePropertiesRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 2, SegmentedFullPath);
					}
				}
			}
			else if( USetProperty* SetProperty = Cast<USetProperty>(Property) )
			{
				// TODO: we dont support set properties yet
			}
			else if( UMapProperty* MapProperty = Cast<UMapProperty>(Property) )
			{
				// TODO: we dont support map properties yet
			}
		}
		// Last link, the thing we actually want to capture
		else
		{
			LeafProperty = Property;
			PropertyValuePtr = LeafProperty->ContainerPtrToValuePtr<uint8>(ContainerAddress, ArrayIndex);

			return true;
		}
	}

	ClearLastResolve();
	return false;
}

#undef LOCTEXT_NAMESPACE
