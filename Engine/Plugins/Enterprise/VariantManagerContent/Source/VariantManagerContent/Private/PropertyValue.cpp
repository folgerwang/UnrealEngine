// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PropertyValue.h"

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "HAL/UnrealMemory.h"
#include "UObject/Package.h"
#include "UObject/TextProperty.h"
#include "VariantObjectBinding.h"
#include "VariantManagerObjectVersion.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/Script.h"
#if WITH_EDITOR
#include "EdGraphSchema_K2.h"
#endif

#define LOCTEXT_NAMESPACE "PropertyValue"

DEFINE_LOG_CATEGORY(LogVariantContent);

UPropertyValue::UPropertyValue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPropertyValue::Init(const TArray<FCapturedPropSegment>& InCapturedPropSegments, UClass* InLeafPropertyClass, const FString& InFullDisplayString, const FName& InPropertySetterName, EPropertyValueCategory InCategory)
{
	CapturedPropSegments = InCapturedPropSegments;
	LeafPropertyClass = InLeafPropertyClass;
	FullDisplayString = InFullDisplayString;
	PropertySetterName = InPropertySetterName;
	PropCategory = InCategory;

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
	for (const FCapturedPropSegment& Seg : CapturedPropSegments)
	{
		Hash = HashCombine(Hash, GetTypeHash(Seg.PropertyName));
		Hash = HashCombine(Hash, GetTypeHash(Seg.PropertyIndex));
		Hash = HashCombine(Hash, GetTypeHash(Seg.ComponentName));
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
			UClass* PropClass = GetPropertyClass();
			if (PropClass && PropClass->IsChildOf(UObjectProperty::StaticClass()) && HasRecordedData())
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
			if (UClass* PropClass = GetPropertyClass())
			{
				if (PropClass->IsChildOf(UNameProperty::StaticClass()))
				{
					Name = *((FName*)ValueBytes.GetData());
				}
			}

			Ar << Name;
		}
	}
	else if (Ar.IsLoading())
	{
		Ar << TempObjPtr;

		// Before this version, properties were stored an array of UProperty*. Convert them to
		// CapturedPropSegment and clear the deprecated arrays
		if (CustomVersion < FVariantManagerObjectVersion::SerializePropertiesAsNames)
		{
			UE_LOG(LogVariantContent, Warning, TEXT("Captured property '%s' was created with an older Unreal Studio version (4.21 or less). A conversion to the new storage format is required and will be attempted. There may be some data loss."), *FullDisplayString);

			int32 NumDeprecatedProps = Properties_DEPRECATED.Num();
			if (NumDeprecatedProps > 0)
			{
				// Back then we didn't store the class directly, and just fetched it from the leaf-most property
				// Try to do that again as it might help decode ValueBytes if those properties were string types
				UProperty* LastProp = Properties_DEPRECATED[Properties_DEPRECATED.Num() -1];
				if (LastProp && LastProp->IsValidLowLevel())
				{
					LeafPropertyClass = LastProp->GetClass();
				}

				CapturedPropSegments.Reserve(NumDeprecatedProps);
				int32 Index = 0;
				for (Index = 0; Index < NumDeprecatedProps; Index++)
				{
					UProperty* Prop = Properties_DEPRECATED[Index];
					if (Prop == nullptr || !Prop->IsValidLowLevel() || !PropertyIndices_DEPRECATED.IsValidIndex(Index))
					{
						break;
					}

					FCapturedPropSegment* NewSeg = new(CapturedPropSegments) FCapturedPropSegment;
					NewSeg->PropertyName = Prop->GetName();
					NewSeg->PropertyIndex = PropertyIndices_DEPRECATED[Index];
				}

				// Conversion succeeded
				if (Index == NumDeprecatedProps)
				{
					Properties_DEPRECATED.Reset();
					PropertyIndices_DEPRECATED.Reset();
				}
				else
				{
					UE_LOG(LogVariantContent, Warning, TEXT("Failed to convert property '%s'! Captured data will be ignored and property will fail to resolve."), *FullDisplayString);
					CapturedPropSegments.Reset();
				}
			}
		}

		if (CustomVersion >= FVariantManagerObjectVersion::CorrectSerializationOfFStringBytes)
		{
			Ar << TempName;
			Ar << TempStr;
			Ar << TempText;

			if (UClass* PropClass = GetPropertyClass())
			{
				if (PropClass->IsChildOf(UNameProperty::StaticClass()))
				{
					int32 NumBytes = sizeof(FName);
					ValueBytes.SetNumUninitialized(NumBytes);
					FMemory::Memcpy(ValueBytes.GetData(), &TempName, NumBytes);
				}
				else if (PropClass->IsChildOf(UStrProperty::StaticClass()))
				{
					int32 NumBytes = sizeof(FString);
					ValueBytes.SetNumUninitialized(NumBytes);
					FMemory::Memcpy(ValueBytes.GetData(), &TempStr, NumBytes);
				}
				else if (PropClass->IsChildOf(UTextProperty::StaticClass()))
				{
					int32 NumBytes = sizeof(FText);
					ValueBytes.SetNumUninitialized(NumBytes);
					FMemory::Memcpy(ValueBytes.GetData(), &TempText, NumBytes);
				}
			}
			else
			{
				//UE_LOG(LogVariantContent, Error, TEXT("Failed to retrieve property class for property '%s'"), *GetFullDisplayString());
				bHasRecordedData = false;
			}
		}
		else if (CustomVersion >= FVariantManagerObjectVersion::CorrectSerializationOfFNameBytes)
		{
			FName Name;
			Ar << Name;

			if (UClass* PropClass = GetPropertyClass())
			{
				if (PropClass == UNameProperty::StaticClass())
				{
					int32 NumBytes = sizeof(FName);
					ValueBytes.SetNumUninitialized(NumBytes);
					FMemory::Memcpy(ValueBytes.GetData(), &Name, NumBytes);
				}
			}
			else
			{
				//UE_LOG(LogVariantContent, Error, TEXT("Failed to retrieve property class for property '%s'"), *GetFullDisplayString());
				bHasRecordedData = false;
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

	if (CapturedPropSegments.Num() == 0)
	{
		return false;
	}

	if (!ResolvePropertiesRecursive(Object->GetClass(), Object, 0))
	{
		// UE_LOG(LogVariantContent, Error, TEXT("Failed to resolve UPropertyValue '%s' on UObject '%s'"), *GetFullDisplayString(), *Object->GetName());
		return false;
	}

	// Try to recover if we had a project that didn't have the LeafPropertyClass fix, so that
	// we don't lose all our variants
	if (LeafPropertyClass == nullptr && LeafProperty != nullptr)
	{
		LeafPropertyClass = LeafProperty->GetClass();
	}

	if (ParentContainerClass)
	{
		if (const UClass* Class = Cast<const UClass>(ParentContainerClass))
		{
			PropertySetter = Class->FindFunctionByName(PropertySetterName);
			if (PropertySetter)
			{
				UClass* ThisClass = GetPropertyClass();
				bool bFoundParameterWithClassType = false;

				for (TFieldIterator<UProperty> It(PropertySetter); It; ++It)
				{
					UProperty* Prop = *It;

					if (ThisClass == Prop->GetClass())
					{
						bFoundParameterWithClassType = true;
						//break;
					}
				}

				if (!bFoundParameterWithClassType)
				{
					UE_LOG(LogVariantContent, Error, TEXT("Property setter does not have a parameter that can receive an object of the property type (%s)!"), *ThisClass->GetName());
					PropertySetter = nullptr;
				}
			}
		}
	}

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

	// If we don't have parameter defaults, try fetching them
#if WITH_EDITOR
	if (PropertySetter && PropertySetterParameterDefaults.Num() == 0)
	{
		for (TFieldIterator<UProperty> It(PropertySetter); It; ++It)
		{
			UProperty* Prop = *It;
			FString Defaults;

			// Store property setter parameter defaults, as this is kept in metadata which is not available at runtime
			UEdGraphSchema_K2::FindFunctionParameterDefaultValue(PropertySetter, Prop, Defaults);

			if (!Defaults.IsEmpty())
			{
				PropertySetterParameterDefaults.Add(Prop->GetName(), Defaults);
			}
		}
	}
#endif //WITH_EDITOR

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

	if (PropertySetter)
	{
		// If we resolved, these are valid
		ApplyViaFunctionSetter((UObject*)ParentContainerAddress);
	}
	// Bool properties need to be set in a particular way since they hold internal private
	// masks and offsets
	else if (UBoolProperty* PropAsBool = Cast<UBoolProperty>(LeafProperty))
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
	return LeafPropertyClass;
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
	UClass* PropClass = GetPropertyClass();
	if (bHasRecordedData && PropClass && PropClass->IsChildOf(UObjectProperty::StaticClass()) && !TempObjPtr.IsNull())
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
			UClass* PropClass = GetPropertyClass();
			if (PropClass && PropClass->IsChildOf(UObjectProperty::StaticClass()))
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
	return LeafProperty;
}

void UPropertyValue::ApplyViaFunctionSetter(UObject* TargetObject)
{
	//Reference: ScriptCore.cpp, UObject::CallFunctionByNameWithArguments

	if (!TargetObject)
	{
		UE_LOG(LogVariantContent, Error, TEXT("Trying to apply via function setter with a nullptr target object! (UPropertyValue: %s)"), *GetFullDisplayString());
		return;
	}
	if (!PropertySetter)
	{
		UE_LOG(LogVariantContent, Error, TEXT("Trying to apply via function setter with a nullptr function setter! (UPropertyValue: %s)"), *GetFullDisplayString());
		return;
	}

	UProperty* LastParameter = nullptr;

	// find the last parameter
	for ( TFieldIterator<UProperty> It(PropertySetter); It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm)) == CPF_Parm; ++It )
	{
		LastParameter = *It;
	}

	// Parse all function parameters.
	uint8* Parms = (uint8*)FMemory_Alloca(PropertySetter->ParmsSize);
	FMemory::Memzero( Parms, PropertySetter->ParmsSize );

	for (TFieldIterator<UProperty> It(PropertySetter); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		UProperty* LocalProp = *It;
		checkSlow(LocalProp);
		if (!LocalProp->HasAnyPropertyFlags(CPF_ZeroConstructor))
		{
			LocalProp->InitializeValue_InContainer(Parms);
		}
	}

	const uint32 ExportFlags = PPF_None;
	int32 NumParamsEvaluated = 0;
	bool bAppliedRecordedData = false;

	UClass* ThisValueClass = GetPropertyClass();
	int32 ThisValueSize = GetValueSizeInBytes();
	const TArray<uint8>& RecordedData = GetRecordedData();

	for( TFieldIterator<UProperty> It(PropertySetter); It && It->HasAnyPropertyFlags(CPF_Parm) && !It->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm); ++It, NumParamsEvaluated++ )
	{
		UProperty* PropertyParam = *It;
		checkSlow(PropertyParam); // Fix static analysis warning

		// Check for a default value
		FString* Defaults = PropertySetterParameterDefaults.Find(PropertyParam->GetName());
		if (Defaults)
		{
			const TCHAR* Result = PropertyParam->ImportText( **Defaults, PropertyParam->ContainerPtrToValuePtr<uint8>(Parms), ExportFlags, NULL );
			if (!Result)
			{
				UE_LOG(LogVariantContent, Error, TEXT("Failed at applying the default value for parameter '%s' of PropertyValue '%s'"), *PropertyParam->GetName(), *GetFullDisplayString());
			}
		}

		// Try adding our recorded data bytes
		if (!bAppliedRecordedData && PropertyParam->GetClass() == ThisValueClass)
		{
			bool bParamMatchesThisProperty = true;

			if (ThisValueClass->IsChildOf(UStructProperty::StaticClass()))
			{
				UScriptStruct* ThisStruct = GetStructPropertyStruct();
				UScriptStruct* PropStruct = Cast<UStructProperty>(PropertyParam)->Struct;

				bParamMatchesThisProperty = (ThisStruct == PropStruct);
			}

			if (bParamMatchesThisProperty)
			{
				uint8* StartAddr = It->ContainerPtrToValuePtr<uint8>(Parms);
				FMemory::Memcpy(StartAddr, RecordedData.GetData(), ThisValueSize);
				bAppliedRecordedData = true;
			}
		}
	}

	// HACK: Restore Visibility properties to operating recursively. Temporary until 4.23
	if (PropertySetter->GetName() == TEXT("SetVisibility") && PropertySetter->ParmsSize == 2 && Parms)
	{
		Parms[1] = true;
	}

	// Only actually call the function if we managed to pack our recorded bytes in the params. Else we will
	// just reset the object to defaults
	if (bAppliedRecordedData)
	{
		FEditorScriptExecutionGuard ScriptGuard;
		TargetObject->ProcessEvent(PropertySetter, Parms);
	}
	else
	{
		UE_LOG(LogVariantContent, Error, TEXT("Did not find a parameter that could receive our value of class %s"), *GetPropertyClass()->GetName());
	}

	// Destroy our params
	for( TFieldIterator<UProperty> It(PropertySetter); It && It->HasAnyPropertyFlags(CPF_Parm); ++It )
	{
		It->DestroyValue_InContainer(Parms);
	}
}

bool UPropertyValue::ResolvePropertiesRecursive(UStruct* ContainerClass, void* ContainerAddress, int32 SegmentIndex)
{
	// Adapted from PropertyPathHelpers.cpp because it is incomplete for arrays of UObjects (important for components)

	FCapturedPropSegment& Seg = CapturedPropSegments[SegmentIndex];

	const int32 ArrayIndex = Seg.PropertyIndex == INDEX_NONE ? 0 : Seg.PropertyIndex;

	if (SegmentIndex == 0)
	{
		ParentContainerClass = ContainerClass;
		ParentContainerAddress = ContainerAddress;
	}

	UProperty* Property = FindField<UProperty>(ContainerClass, *Seg.PropertyName);
	if (Property)
	{
		// Not the last link in the chain --> Dig down deeper updating our class/address if we jump an UObjectProp/UStructProp
		if (SegmentIndex < (CapturedPropSegments.Num()-1))
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

					return ResolvePropertiesRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 1);
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

					return ResolvePropertiesRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 1);
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

					return ResolvePropertiesRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 1);
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

				return ResolvePropertiesRecursive(StructProp->Struct, StructAddress, SegmentIndex + 1);
			}
			else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property))
			{
				// We have to replicate these cases in here because we need to access the inner properties
				// with the FScriptArrayHelper. If we do another recursive call and try parsing the inner
				// property just as a regular property with an ArrayIndex, it will fail getting the ValuePtr
				// because for some reason properties always have ArrayDim = 1

				FCapturedPropSegment NextSeg = CapturedPropSegments[SegmentIndex+1];

				int32 InnerArrayIndex = NextSeg.PropertyIndex == INDEX_NONE ? 0 : NextSeg.PropertyIndex;

				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ContainerAddress));

				// In the case of a component, this also ensures we have at least one component in the array, as
				// InnerArrayIndex will always be zero
				if (!ArrayHelper.IsValidIndex(InnerArrayIndex))
				{
					return false;
				}

				// Array properties show up in the path as two entries (one for the array prop and one for the inner)
				// so if we're on the second-to-last path segment, it means we want to capture the inner property, so don't
				// step into it
				// This also handles generic arrays of UObject* and structs without stepping into them (that is,
				// prevents us from going into the ifs below)
				if (SegmentIndex == CapturedPropSegments.Num() - 2)
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
					return ResolvePropertiesRecursive(ArrayOfStructsProp->Struct, StructAddress, SegmentIndex + 2);
				}
				if ( UObjectProperty* InnerObjectProperty = Cast<UObjectProperty>(ArrayProp->Inner) )
				{
					// If we make it in here we know it's a property inside a component as we don't
					// step into generic UObject properties. We also know its a component of our actor
					// as we don't capture components from other actors

					// This lets us search for the component by name instead, ignoring our InnerArrayIndex
					// This is intuitive because if a component is reordered in the details panel, we kind
					// of expect our bindings to 'follow'.
					if (!NextSeg.ComponentName.IsEmpty())
					{
						for (int32 ComponentIndex = 0; ComponentIndex < ArrayHelper.Num(); ComponentIndex++)
						{
							void* ObjPtrContainer = ArrayHelper.GetRawPtr(ComponentIndex);
							UObject* CurrentObject = InnerObjectProperty->GetObjectPropertyValue(ObjPtrContainer);

							if (CurrentObject && CurrentObject->IsA(UActorComponent::StaticClass()) && CurrentObject->GetName() == NextSeg.ComponentName)
							{
								ParentContainerClass = CurrentObject->GetClass();
								ParentContainerAddress =  CurrentObject;

								// The next link in the chain is just this array's inner. Let's just skip it instead
								return ResolvePropertiesRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 2);
							}
						}
					}
					// If we're a property recovered from 4.21, we won't have a component name, so we'll have to
					// try finding our target component by index. We will first check InnerArrayIndex, and if that fails, we
					// will check the other components until we either find something that resolves or we just fall
					// out of this scope
					else
					{
						// First check our actual inner array index
						if (ArrayHelper.IsValidIndex(InnerArrayIndex))
						{
							void* ObjPtrContainer = ArrayHelper.GetRawPtr(InnerArrayIndex);
							UObject* CurrentObject = InnerObjectProperty->GetObjectPropertyValue(ObjPtrContainer);
							if (CurrentObject && CurrentObject->IsA(UActorComponent::StaticClass()))
							{
								if (ResolvePropertiesRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 2))
								{
									ParentContainerClass = CurrentObject->GetClass();
									ParentContainerAddress =  CurrentObject;
									NextSeg.ComponentName = CurrentObject->GetName();
									return true;
								}
							}
						}

						// Check every component for something that resolves. It's the best we can do
						for (int32 ComponentIndex = 0; ComponentIndex < ArrayHelper.Num(); ComponentIndex++)
						{
							// Already checked that one
							if (ComponentIndex == InnerArrayIndex)
							{
								continue;
							}

							void* ObjPtrContainer = ArrayHelper.GetRawPtr(ComponentIndex);
							UObject* CurrentObject = InnerObjectProperty->GetObjectPropertyValue(ObjPtrContainer);
							if (CurrentObject && CurrentObject->IsA(UActorComponent::StaticClass()))
							{
								if (ResolvePropertiesRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 2))
								{
									ParentContainerClass = CurrentObject->GetClass();
									ParentContainerAddress =  CurrentObject;
									NextSeg.ComponentName = CurrentObject->GetName();
									return true;
								}
							}
						}
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

UPropertyValueTransform::UPropertyValueTransform(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPropertyValueTransform::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Don't need to patch up the property setter name as this won't be used
	if (this->HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	switch (PropCategory)
	{
	case EPropertyValueCategory::RelativeLocation:
		PropertySetterName = FName(TEXT("SetRelativeLocation"));
		break;
	case EPropertyValueCategory::RelativeRotation:
		PropertySetterName = FName(TEXT("SetRelativeLocation"));
		break;
	case EPropertyValueCategory::RelativeScale3D:
		PropertySetterName = FName(TEXT("SetRelativeScale3D"));
		break;
	default:
		UE_LOG(LogVariantContent, Error, TEXT("Problem serializing old PropertyValueTransform '%s'"), *GetFullDisplayString());
		break;
	}
}

UPropertyValueVisibility::UPropertyValueVisibility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPropertyValueVisibility::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Don't need to patch up the property setter name as this won't be used
	if (this->HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	switch (PropCategory)
	{
	case EPropertyValueCategory::bVisible:
		PropertySetterName = FName(TEXT("SetVisibility"));
		break;
	default:
		UE_LOG(LogVariantContent, Error, TEXT("Problem serializing old PropertyValueVisibility '%s'"), *GetFullDisplayString());
		break;
	}
}

#undef LOCTEXT_NAMESPACE
