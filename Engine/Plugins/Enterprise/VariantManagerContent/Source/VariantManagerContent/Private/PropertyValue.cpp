// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PropertyValue.h"

#include "CoreMinimal.h"
#include "HAL/UnrealMemory.h"
#include "VariantObjectBinding.h"

#define LOCTEXT_NAMESPACE "PropertyValue"

DEFINE_LOG_CATEGORY(LogVariantContent);

UPropertyValue::UPropertyValue(const FObjectInitializer& Init)
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

const TArray<UProperty*>& UPropertyValue::GetProperties() const
{
	return Properties;
}

const TArray<int32>& UPropertyValue::GetPropertyIndices() const
{
	return PropertyIndices;
}

UVariantObjectBinding* UPropertyValue::GetParent() const
{
	return Cast<UVariantObjectBinding>(GetOuter());
}

void UPropertyValue::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

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
	}
	else if (Ar.IsLoading())
	{
		Ar << TempObjPtr;

	}
}

bool UPropertyValue::Resolve()
{
	UObject* Object = nullptr;
	UVariantObjectBinding* Parent = GetParent();
	if (Parent)
	{
		Object = Parent->GetObject();
	}

	if (Object == nullptr)
	{
		return false;
	}

	if (!ResolvePropertiesRecursive(Object->GetClass(), Object, 0))
	{
		UE_LOG(LogVariantContent, Error, TEXT("Failed to resolve UPropertyValue '%s' on UObject '%s'"), *GetFullDisplayString(), *Object->GetName());
		return false;
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
	if (ContainerObject)
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

FName UPropertyValue::GetPropertyName() const
{
	UProperty* Prop = GetProperty();
	if (Prop)
	{
		return Prop->GetFName();
	}

	return FName();
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
	if (Prop)
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

void UPropertyValue::SetRecordedData(const uint8* NewDataBytes, int32 NumBytes)
{
	Modify();

	if (NumBytes > 0)
	{
		ValueBytes.SetNumUninitialized(NumBytes);
		FMemory::Memcpy(ValueBytes.GetData(), NewDataBytes, NumBytes);
		bHasRecordedData = true;
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

bool UPropertyValue::ResolvePropertiesRecursive(UStruct* ContainerClass, void* ContainerAddress, int32 SegmentIndex)
{
	// Adapted from PropertyPathHelpers.cpp because it is incomplete for arrays of UObjects (important for components)

	UProperty* Property = Properties[SegmentIndex];;
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

				int32 InnerArrayIndex = PropertyIndices[SegmentIndex+1] == INDEX_NONE ? 0 : PropertyIndices[SegmentIndex+1];

				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ContainerAddress));
				if (!ArrayHelper.IsValidIndex(InnerArrayIndex))
				{
					UE_LOG(LogVariantContent, Error, TEXT("Failed to resolve: UArrayProperty '%s' does not have an inner with index %d!"), *ArrayProp->GetName(), InnerArrayIndex);
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
					return ResolvePropertiesRecursive(ArrayOfStructsProp->Struct, StructAddress, SegmentIndex + 2);
				}
				if ( UObjectProperty* InnerObjectProperty = Cast<UObjectProperty>(ArrayProp->Inner) )
				{
					void* ObjPtrContainer = ArrayHelper.GetRawPtr(InnerArrayIndex);
					UObject* CurrentObject = InnerObjectProperty->GetObjectPropertyValue(ObjPtrContainer);
					if (CurrentObject)
					{
						ParentContainerClass = CurrentObject->GetClass();
						ParentContainerAddress =  CurrentObject;

						// The next link in the chain is just this array's inner. Let's just skip it instead
						return ResolvePropertiesRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 2);
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
