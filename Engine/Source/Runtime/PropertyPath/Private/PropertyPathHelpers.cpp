// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PropertyPathHelpers.h"
#include "UObject/UnrealType.h"

/** Internal helper functions */
namespace PropertyPathHelpersInternal
{
	template<typename ContainerType>
	static bool IteratePropertyPathRecursive(UStruct* InStruct, ContainerType* InContainer, int32 SegmentIndex, const FCachedPropertyPath& InPropertyPath, FPropertyPathResolver& InResolver)
	{
		const FPropertyPathSegment& Segment = InPropertyPath.GetSegment(SegmentIndex);
		const int32 ArrayIndex = Segment.GetArrayIndex() == INDEX_NONE ? 0 : Segment.GetArrayIndex();

		// Reset cached address usage flag at the path root. This will be reset later in the recursion if conditions are not met in the path.
		if(SegmentIndex == 0)
		{
#if DO_CHECK
			InPropertyPath.SetCachedContainer(InContainer);
#endif
			InPropertyPath.SetCanSafelyUsedCachedAddress(true);
		}

		// Obtain the property info from the given structure definition
		if ( UField* Field = Segment.Resolve(InStruct) )
		{
			if ( UProperty* Property = Cast<UProperty>(Field) )
			{
				if ( SegmentIndex < ( InPropertyPath.GetNumSegments() - 1 ) )
				{
					// Check first to see if this is a simple object (eg. not an array of objects)
					if ( UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property) )
					{
						// Object boundary that can change, so we cant use the cached address safely
						InPropertyPath.SetCanSafelyUsedCachedAddress(false);

						// If it's an object we need to get the value of the property in the container first before we 
						// can continue, if the object is null we safely stop processing the chain of properties.
						if ( UObject* CurrentObject = ObjectProperty->GetPropertyValue_InContainer(InContainer, ArrayIndex) )
						{
							return IteratePropertyPathRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 1, InPropertyPath, InResolver);
						}
					}
					// Check to see if this is a simple weak object property (eg. not an array of weak objects).
					if ( UWeakObjectProperty* WeakObjectProperty = Cast<UWeakObjectProperty>(Property) )
					{
						// Object boundary that can change, so we cant use the cached address safely
						InPropertyPath.SetCanSafelyUsedCachedAddress(false);

						FWeakObjectPtr WeakObject = WeakObjectProperty->GetPropertyValue_InContainer(InContainer, ArrayIndex);

						// If it's an object we need to get the value of the property in the container first before we 
						// can continue, if the object is null we safely stop processing the chain of properties.
						if ( UObject* CurrentObject = WeakObject.Get() )
						{
							return IteratePropertyPathRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 1, InPropertyPath, InResolver);
						}
					}
					// Check to see if this is a simple soft object property (eg. not an array of soft objects).
					else if ( USoftObjectProperty* SoftObjectProperty = Cast<USoftObjectProperty>(Property) )
					{
						// Object boundary that can change, so we cant use the cached address safely
						InPropertyPath.SetCanSafelyUsedCachedAddress(false);

						FSoftObjectPtr SoftObject = SoftObjectProperty->GetPropertyValue_InContainer(InContainer, ArrayIndex);

						// If it's an object we need to get the value of the property in the container first before we 
						// can continue, if the object is null we safely stop processing the chain of properties.
						if ( UObject* CurrentObject = SoftObject.Get() )
						{
							return IteratePropertyPathRecursive(CurrentObject->GetClass(), CurrentObject, SegmentIndex + 1, InPropertyPath, InResolver);
						}
					}
					// Check to see if this is a simple structure (eg. not an array of structures)
					else if ( UStructProperty* StructProp = Cast<UStructProperty>(Property) )
					{
						// Recursively call back into this function with the structure property and container value
						return IteratePropertyPathRecursive<void>(StructProp->Struct, StructProp->ContainerPtrToValuePtr<void>(InContainer, ArrayIndex), SegmentIndex + 1, InPropertyPath, InResolver);
					}
					else if ( UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property) )
					{
						// Dynamic array boundary that can change, so we cant use the cached address safely
						InPropertyPath.SetCanSafelyUsedCachedAddress(false);

						// It is an array, now check to see if this is an array of structures
						if ( UStructProperty* ArrayOfStructsProp = Cast<UStructProperty>(ArrayProp->Inner) )
						{
							FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);
							if ( ArrayHelper.IsValidIndex(ArrayIndex) )
							{
								// Recursively call back into this function with the array element and container value
								return IteratePropertyPathRecursive<void>(ArrayOfStructsProp->Struct, static_cast<void*>(ArrayHelper.GetRawPtr(ArrayIndex)), SegmentIndex + 1, InPropertyPath, InResolver);
							}
						}
						// if it's not an array of structs, maybe it's an array of classes
						//else if ( UObjectProperty* ObjectProperty = Cast<UObjectProperty>(ArrayProp->Inner) )
						{
							//TODO Add support for arrays of objects.
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
				else
				{
					return InResolver.Resolve(static_cast<ContainerType*>(InContainer), InPropertyPath);
				}
			}
			else
			{
				// Only allow functions as the final link in the chain.
				if ( SegmentIndex == ( InPropertyPath.GetNumSegments() - 1 ) )
				{
					return InResolver.Resolve(static_cast<ContainerType*>(InContainer), InPropertyPath);
				}
			}
		}

		return false;
	}

	/** Non-UObject helper struct for GetPropertyValueAsString function calls */
	template<typename ContainerType>
	struct FCallGetterFunctionAsStringHelper
	{
		static bool CallGetterFunction(ContainerType* InContainer, UFunction* InFunction, FString& OutValue) 
		{
			// Cant call UFunctions on non-UObject containers
			return false;
		}
	};

	/** UObject partial specialization of FCallGetterFunctionHelper */
	template<>
	struct FCallGetterFunctionAsStringHelper<UObject>
	{
		static bool CallGetterFunction(UObject* InContainer, UFunction* InFunction, FString& OutValue) 
		{
			// We only support calling functions that return a single value and take no parameters.
			if ( InFunction->NumParms == 1 )
			{
				// Verify there's a return property.
				if ( UProperty* ReturnProperty = InFunction->GetReturnProperty() )
				{
					if ( !InContainer->IsUnreachable() )
					{
						// Create and init a buffer for the function to write to
						TArray<uint8> TempBuffer;
						TempBuffer.AddUninitialized(ReturnProperty->ElementSize);
						ReturnProperty->InitializeValue(TempBuffer.GetData());

						InContainer->ProcessEvent(InFunction, TempBuffer.GetData());
						ReturnProperty->ExportTextItem(OutValue, TempBuffer.GetData(), nullptr, nullptr, 0);
						return true;
					}
				}
			}

			return false;
		}
	};

	template<typename ContainerType>
	static bool GetPropertyValueAsString(ContainerType* InContainer, const FCachedPropertyPath& InPropertyPath, UProperty*& OutProperty, FString& OutValue)
	{
		const FPropertyPathSegment& LastSegment = InPropertyPath.GetLastSegment();
		int32 ArrayIndex = LastSegment.GetArrayIndex();
		UField* Field = LastSegment.GetField();

		// We're on the final property in the path, it may be an array property, so check that first.
		if ( UArrayProperty* ArrayProp = Cast<UArrayProperty>(Field) )
		{
			// if it's an array property, we need to see if we parsed an index as part of the segment
			// as a user may have baked the index directly into the property path.
			if(ArrayIndex != INDEX_NONE)
			{
				// Property is an array property, so make sure we have a valid index specified
				FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);
				if ( ArrayHelper.IsValidIndex(ArrayIndex) )
				{
					OutProperty = ArrayProp->Inner;
					OutProperty->ExportTextItem(OutValue, static_cast<void*>(ArrayHelper.GetRawPtr(ArrayIndex)), nullptr, nullptr, 0);
					return true;
				}
			}
			else
			{
				// No index, so assume we want the array property itself
				if ( void* ValuePtr = ArrayProp->ContainerPtrToValuePtr<void>(InContainer) )
				{
					OutProperty = ArrayProp;
					OutProperty->ExportTextItem(OutValue, ValuePtr, nullptr, nullptr, 0);
					return true;
				}
			}
		}
		else if(UFunction* Function = Cast<UFunction>(Field))
		{
			return FCallGetterFunctionAsStringHelper<ContainerType>::CallGetterFunction(InContainer, Function, OutValue);
		}
		else if(UProperty* Property = Cast<UProperty>(Field))
		{
			ArrayIndex = ArrayIndex == INDEX_NONE ? 0 : ArrayIndex;
			if( ArrayIndex < Property->ArrayDim )
			{
				if ( void* ValuePtr = Property->ContainerPtrToValuePtr<void>(InContainer, ArrayIndex) )
				{
					OutProperty = Property;
					OutProperty->ExportTextItem(OutValue, ValuePtr, nullptr, nullptr, 0);
					return true;
				}
			}
		}

		return false;
	}

	/** Non-UObject helper struct for SetPropertyValueFromString function calls */
	template<typename ContainerType>
	struct FCallSetterFunctionFromStringHelper
	{
		static bool CallSetterFunction(ContainerType* InContainer, UFunction* InFunction, const FString& InValue)
		{
			// Cant call UFunctions on non-UObject containers
			return false;
		}
	};

	/** UObject partial specialization of FCallSetterFunctionFromStringHelper */
	template<>
	struct FCallSetterFunctionFromStringHelper<UObject>
	{
		static bool CallSetterFunction(UObject* InContainer, UFunction* InFunction, const FString& InValue)
		{
			// We only support calling functions that take a single value and return no parameters
			if ( InFunction->NumParms == 1 && InFunction->GetReturnProperty() == nullptr )
			{
				// Verify there's a single param
				if ( UProperty* ParamProperty = PropertyPathHelpersInternal::GetFirstParamProperty(InFunction) )
				{
					if ( !InContainer->IsUnreachable() )
					{
						// Create and init a buffer for the function to read from
						TArray<uint8> TempBuffer;
						TempBuffer.AddUninitialized(ParamProperty->ElementSize);
						ParamProperty->InitializeValue(TempBuffer.GetData());

						ParamProperty->ImportText(*InValue, TempBuffer.GetData(), 0, nullptr);
						InContainer->ProcessEvent(InFunction, TempBuffer.GetData());
						return true;
					}
				}
			}

			return false;
		}
	};

	template<typename ContainerType>
	static bool SetPropertyValueFromString(ContainerType* InContainer, const FCachedPropertyPath& InPropertyPath, const FString& InValue)
	{
		const FPropertyPathSegment& LastSegment = InPropertyPath.GetLastSegment();
		int32 ArrayIndex = LastSegment.GetArrayIndex();
		UField* Field = LastSegment.GetField();

		// We're on the final property in the path, it may be an array property, so check that first.
		if ( UArrayProperty* ArrayProp = Cast<UArrayProperty>(Field) )
		{
			// if it's an array property, we need to see if we parsed an index as part of the segment
			// as a user may have baked the index directly into the property path.
			if(ArrayIndex != INDEX_NONE)
			{
				// Property is an array property, so make sure we have a valid index specified
				FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);
				if ( ArrayHelper.IsValidIndex(ArrayIndex) )
				{
					ArrayProp->Inner->ImportText(*InValue, static_cast<void*>(ArrayHelper.GetRawPtr(ArrayIndex)), 0, nullptr);
					return true;
				}
			}
			else
			{
				// No index, so assume we want the array property itself
				if ( void* ValuePtr = ArrayProp->ContainerPtrToValuePtr<void>(InContainer) )
				{
					ArrayProp->ImportText(*InValue, ValuePtr, 0, nullptr);
					return true;
				}
			}
		}
		else if(UFunction* Function = Cast<UFunction>(Field))
		{
			return FCallSetterFunctionFromStringHelper<ContainerType>::CallSetterFunction(InContainer, Function, InValue);
		}
		else if(UProperty* Property = Cast<UProperty>(Field))
		{
			ArrayIndex = ArrayIndex == INDEX_NONE ? 0 : ArrayIndex;
			if(ArrayIndex < Property->ArrayDim)
			{
				if ( void* ValuePtr = Property->ContainerPtrToValuePtr<void>(InContainer, ArrayIndex) )
				{
					Property->ImportText(*InValue, ValuePtr, 0, nullptr);
					return true;
				}
			}
		}

		return false;
	}

	template<typename ContainerType>
	static bool PerformArrayOperation(ContainerType* InContainer, const FCachedPropertyPath& InPropertyPath, TFunctionRef<bool(FScriptArrayHelper&,int32)> InOperation)
	{
		const FPropertyPathSegment& LastSegment = InPropertyPath.GetLastSegment();
		int32 ArrayIndex = LastSegment.GetArrayIndex();
		UField* Field = LastSegment.GetField();

		// We only support array properties right now
		if ( UArrayProperty* ArrayProp = Cast<UArrayProperty>(Field) )
		{
			FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);
			return InOperation(ArrayHelper, ArrayIndex);
		}
		return false;
	}

	/** Caches resolve addresses in property paths for later use */
	template<typename ContainerType>
	static bool CacheResolveAddress(ContainerType* InContainer, const FCachedPropertyPath& InPropertyPath)
	{
		const FPropertyPathSegment& LastSegment = InPropertyPath.GetLastSegment();
		int32 ArrayIndex = LastSegment.GetArrayIndex();
		UField* Field = LastSegment.GetField();

		// We're on the final property in the path, it may be an array property, so check that first.
		if ( UArrayProperty* ArrayProp = Cast<UArrayProperty>(Field) )
		{
			// if it's an array property, we need to see if we parsed an index as part of the segment
			// as a user may have baked the index directly into the property path.
			if(ArrayIndex != INDEX_NONE)
			{
				// Property is an array property, so make sure we have a valid index specified
				FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);
				if ( ArrayHelper.IsValidIndex(ArrayIndex) )
				{
					if(void* Address = static_cast<void*>(ArrayHelper.GetRawPtr(ArrayIndex)))
					{
						InPropertyPath.ResolveLeaf(Address);
						return true;
					}
				}
			}
			else
			{
				// No index, so assume we want the array property itself
				if(void* Address = ArrayProp->ContainerPtrToValuePtr<void>(InContainer))
				{
					InPropertyPath.ResolveLeaf(Address);
					return true;
				}
			}
		}
		else if(UFunction* Function = Cast<UFunction>(Field))
		{
			InPropertyPath.ResolveLeaf(Function);
			return true;
		}
		else if(UProperty* Property = Cast<UProperty>(Field))
		{
			ArrayIndex = ArrayIndex == INDEX_NONE ? 0 : ArrayIndex;
			if ( ArrayIndex < Property->ArrayDim )
			{
				if(void* Address = Property->ContainerPtrToValuePtr<void>(InContainer, ArrayIndex))
				{
					InPropertyPath.ResolveLeaf(Address);
					return true;
				}
			}
		}

		return false;
	}

	/** helper function. Copy the values between two resolved paths. It is assumed that CanCopyProperties has been previously called and returned true. */
	static bool CopyResolvedPaths(const FCachedPropertyPath& InDestPropertyPath, const FCachedPropertyPath& InSrcPropertyPath)
	{
		// check we have valid addresses/functions that match
		if(InDestPropertyPath.GetCachedFunction() != nullptr && InSrcPropertyPath.GetCachedFunction() != nullptr)
		{
			// copying via functions is not supported yet
			return false;
		}
		else if(InDestPropertyPath.GetCachedAddress() != nullptr && InSrcPropertyPath.GetCachedAddress() != nullptr)
		{
			const FPropertyPathSegment& DestLastSegment = InDestPropertyPath.GetLastSegment();
			UProperty* DestProperty = CastChecked<UProperty>(DestLastSegment.GetField());
			UArrayProperty* DestArrayProp = Cast<UArrayProperty>(DestProperty);
			if ( DestArrayProp && DestLastSegment.GetArrayIndex() != INDEX_NONE )
			{
				DestArrayProp->Inner->CopySingleValue(InDestPropertyPath.GetCachedAddress(), InSrcPropertyPath.GetCachedAddress());
			}
			else if(DestProperty->ArrayDim > 1)
			{
				DestProperty->CopyCompleteValue(InDestPropertyPath.GetCachedAddress(), InSrcPropertyPath.GetCachedAddress());
			}
			else if(UBoolProperty* DestBoolProperty = Cast<UBoolProperty>(DestProperty))
			{
				UBoolProperty* SrcBoolProperty = Cast<UBoolProperty>(InSrcPropertyPath.GetLastSegment().GetField());
				const bool bValue = SrcBoolProperty->GetPropertyValue(InSrcPropertyPath.GetCachedAddress());
				DestBoolProperty->SetPropertyValue(InDestPropertyPath.GetCachedAddress(), bValue);
			}
			else
			{
				DestProperty->CopySingleValue(InDestPropertyPath.GetCachedAddress(), InSrcPropertyPath.GetCachedAddress());
			}
			return true;
		}

		return false;
	}

	/** Checks if two fully resolved paths can have their values copied between them. Checks the leaf property class to see if they match */
	static bool CanCopyProperties(const FCachedPropertyPath& InDestPropertyPath, const FCachedPropertyPath& InSrcPropertyPath)
	{
		const FPropertyPathSegment& DestLastSegment = InDestPropertyPath.GetLastSegment();
		const FPropertyPathSegment& SrcLastSegment = InSrcPropertyPath.GetLastSegment();

		UProperty* DestProperty = Cast<UProperty>(DestLastSegment.GetField());
		UProperty* SrcProperty = Cast<UProperty>(SrcLastSegment.GetField());

		if(SrcProperty && DestProperty)
		{
			UArrayProperty* DestArrayProperty = Cast<UArrayProperty>(DestProperty);
			UArrayProperty* SrcArrayProperty = Cast<UArrayProperty>(SrcProperty);

			// If we have a valid index and an array property then we should use the inner property
			UClass* DestClass = (DestArrayProperty != nullptr && DestLastSegment.GetArrayIndex() != INDEX_NONE) ? DestArrayProperty->Inner->GetClass() : DestProperty->GetClass();
			UClass* SrcClass = (SrcArrayProperty != nullptr && SrcLastSegment.GetArrayIndex() != INDEX_NONE) ? SrcArrayProperty->Inner->GetClass() : SrcProperty->GetClass();

			return DestClass == SrcClass && SrcProperty->ArrayDim == DestProperty->ArrayDim;
		}

		return false;
	}

	bool ResolvePropertyPath(UObject* InContainer, const FString& InPropertyPath, FPropertyPathResolver& InResolver)
	{
		FCachedPropertyPath InternalPropertyPath(InPropertyPath);
		return IteratePropertyPathRecursive<UObject>(InContainer->GetClass(), InContainer, 0, InternalPropertyPath, InResolver);
	}

	bool ResolvePropertyPath(UObject* InContainer, const FCachedPropertyPath& InPropertyPath, FPropertyPathResolver& InResolver)
	{
		return IteratePropertyPathRecursive<UObject>(InContainer->GetClass(), InContainer, 0, InPropertyPath, InResolver);
	}

	bool ResolvePropertyPath(void* InContainer, UStruct* InStruct, const FString& InPropertyPath, FPropertyPathResolver& InResolver)
	{
		FCachedPropertyPath InternalPropertyPath(InPropertyPath);
		return IteratePropertyPathRecursive<void>(InStruct, InContainer, 0, InternalPropertyPath, InResolver);
	}

	bool ResolvePropertyPath(void* InContainer, UStruct* InStruct, const FCachedPropertyPath& InPropertyPath, FPropertyPathResolver& InResolver)
	{
		return IteratePropertyPathRecursive<void>(InStruct, InContainer, 0, InPropertyPath, InResolver);
	}

	UProperty* GetFirstParamProperty(UFunction* InFunction)
	{
		for( TFieldIterator<UProperty> It(InFunction); It && (It->PropertyFlags & CPF_Parm); ++It )
		{
			if( (It->PropertyFlags & CPF_ReturnParm) == 0 )
			{
				return *It;
			}
		}
		return nullptr;
	}
}

FPropertyPathSegment::FPropertyPathSegment()
	: Name(NAME_None)
	, ArrayIndex(INDEX_NONE)
	, Struct(nullptr)
	, Field(nullptr)
{

}

FPropertyPathSegment::FPropertyPathSegment(const FString& SegmentName)
	: ArrayIndex(INDEX_NONE)
	, Struct(nullptr)
	, Field(nullptr)
{
	FString PropertyName;
	PropertyPathHelpers::FindFieldNameAndArrayIndex(SegmentName, PropertyName, ArrayIndex);
	Name = FName(*PropertyName);
}

UField* FPropertyPathSegment::Resolve(UStruct* InStruct) const
{
	if ( InStruct )
	{
		// only perform the find field work if the structure where this property would resolve to
		// has changed.  If it hasn't changed, the just return the UProperty we found last time.
		if ( InStruct != Struct )
		{
			Struct = InStruct;
			Field = FindField<UField>(InStruct, Name);
		}

		return Field;
	}

	return nullptr;
}

FName FPropertyPathSegment::GetName() const
{
	return Name;
}

int32 FPropertyPathSegment::GetArrayIndex() const
{
	return ArrayIndex;
}

UField* FPropertyPathSegment::GetField() const
{
	return Field;
}

UStruct* FPropertyPathSegment::GetStruct() const
{
	return Struct;
}

FCachedPropertyPath::FCachedPropertyPath()
	: CachedAddress(nullptr)
	, CachedFunction(nullptr)
#if DO_CHECK
	, CachedContainer(nullptr)
#endif
	, bCanSafelyUsedCachedAddress(false)
{
}

FCachedPropertyPath::FCachedPropertyPath(const FString& Path)
	: CachedAddress(nullptr)
	, CachedFunction(nullptr)
#if DO_CHECK
	, CachedContainer(nullptr)
#endif
	, bCanSafelyUsedCachedAddress(false)
{
	MakeFromString(Path);
}

FCachedPropertyPath::FCachedPropertyPath(const TArray<FString>& PropertyChain)
	: CachedAddress(nullptr)
	, CachedFunction(nullptr)
#if DO_CHECK
	, CachedContainer(nullptr)
#endif
	, bCanSafelyUsedCachedAddress(false)
{
	MakeFromStringArray(PropertyChain);
}

void FCachedPropertyPath::MakeFromString(const FString& InPropertyPath)
{
	TArray<FString> PropertyPathArray;
	InPropertyPath.ParseIntoArray(PropertyPathArray, TEXT("."));
	MakeFromStringArray(PropertyPathArray);
}

void FCachedPropertyPath::MakeFromStringArray(const TArray<FString>& InPropertyPathArray)
{
	for ( const FString& Segment : InPropertyPathArray )
	{
		Segments.Add(FPropertyPathSegment(Segment));
	}
}

int32 FCachedPropertyPath::GetNumSegments() const
{
	return Segments.Num();
}

const FPropertyPathSegment& FCachedPropertyPath::GetSegment(int32 InSegmentIndex) const
{
	return Segments[InSegmentIndex];
}

const FPropertyPathSegment& FCachedPropertyPath::GetLastSegment() const
{
	return Segments.Last();
}

/** Helper for cache/copy resolver */
struct FInternalCacheResolver : public PropertyPathHelpersInternal::TPropertyPathResolver<FInternalCacheResolver>
{
	template<typename ContainerType>
	bool Resolve_Impl(ContainerType* InContainer, const FCachedPropertyPath& InPropertyPath)
	{
		return PropertyPathHelpersInternal::CacheResolveAddress(InContainer, InPropertyPath);
	}
};

bool FCachedPropertyPath::Resolve(UObject* InContainer) const
{
	FInternalCacheResolver Resolver;
	return PropertyPathHelpersInternal::ResolvePropertyPath(InContainer, *this, Resolver);
}

void FCachedPropertyPath::ResolveLeaf(void* InAddress) const
{
	check(CachedFunction == nullptr);
	CachedAddress = InAddress;
}

void FCachedPropertyPath::ResolveLeaf(UFunction* InFunction) const
{
	check(CachedAddress == nullptr);
	CachedFunction = InFunction;
}

void FCachedPropertyPath::SetCanSafelyUsedCachedAddress(bool bInCanSafelyUsedCachedAddress) const
{
	bCanSafelyUsedCachedAddress = bInCanSafelyUsedCachedAddress;
}

bool FCachedPropertyPath::IsResolved() const
{
	return (CachedFunction != nullptr || CachedAddress != nullptr);
}

bool FCachedPropertyPath::IsFullyResolved() const
{
#if DO_CHECK
	bool bCachedContainer = CachedContainer != nullptr;
#else
	bool bCachedContainer = true;
#endif
	return bCanSafelyUsedCachedAddress && bCachedContainer && IsResolved();
}

void* FCachedPropertyPath::GetCachedAddress() const
{
	return CachedAddress;
}

UFunction* FCachedPropertyPath::GetCachedFunction() const
{
	return CachedFunction;
}

FPropertyChangedEvent FCachedPropertyPath::ToPropertyChangedEvent(EPropertyChangeType::Type InChangeType) const
{
	// Path must be resolved
	check(IsResolved());

	// Note: path must not be a to a UFunction
	FPropertyChangedEvent PropertyChangedEvent(CastChecked<UProperty>(GetLastSegment().GetField()), InChangeType);

	// Set a containing 'struct' if we need to
	if(Segments.Num() > 1)
	{
		PropertyChangedEvent.SetActiveMemberProperty(CastChecked<UProperty>(Segments[Segments.Num() - 2].GetField()));
	}

	return PropertyChangedEvent;
}

void FCachedPropertyPath::ToEditPropertyChain(FEditPropertyChain& OutPropertyChain) const
{
	// Path must be resolved
	check(IsResolved());

	for (const FPropertyPathSegment& Segment : Segments)
	{
		// Note: path must not be a to a UFunction
		OutPropertyChain.AddTail(CastChecked<UProperty>(Segment.GetField()));
	}

	OutPropertyChain.SetActivePropertyNode(CastChecked<UProperty>(GetLastSegment().GetField()));
	if (Segments.Num() > 1)
	{
		OutPropertyChain.SetActiveMemberPropertyNode(CastChecked<UProperty>(Segments[0].GetField()));
	}
}

FString FCachedPropertyPath::ToString() const
{
	FString OutString;
	for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		const FPropertyPathSegment& Segment = Segments[SegmentIndex];

		// Add property name
		OutString += Segment.GetName().ToString();

		// Add array index
		if(Segment.GetArrayIndex() != INDEX_NONE)
		{
			OutString += FString::Printf(TEXT("[%d]"), Segment.GetArrayIndex());
		}

		// Add separator
		if(SegmentIndex < Segments.Num() - 1)
		{
			OutString += TEXT(".");
		}
	}

	return OutString;
}

#if DO_CHECK
void* FCachedPropertyPath::GetCachedContainer() const
{
	return CachedContainer;
}

void FCachedPropertyPath::SetCachedContainer(void* InContainer) const
{
	CachedContainer = InContainer;
}
#endif

void FCachedPropertyPath::RemoveFromEnd(int32 InNumSegments)
{
	if(InNumSegments <= Segments.Num())
	{
		Segments.RemoveAt(Segments.Num() - 1, InNumSegments);

		// Clear cached data - the path is not the same as the previous Resolve() call
		for (const FPropertyPathSegment& Segment : Segments)
		{
			Segment.Struct = nullptr;
			Segment.Field = nullptr;
		}
		CachedAddress = nullptr;
		CachedFunction = nullptr;
#if DO_CHECK
		CachedContainer = nullptr;
#endif// DO_CHECK
		bCanSafelyUsedCachedAddress = false;
	}
}

void FCachedPropertyPath::RemoveFromStart(int32 InNumSegments)
{
	if(InNumSegments <= Segments.Num())
	{
		Segments.RemoveAt(0, InNumSegments);

		// Clear cached data - the path is not the same as the previous Resolve() call
		for (const FPropertyPathSegment& Segment : Segments)
		{
			Segment.Struct = nullptr;
			Segment.Field = nullptr;
		}
		CachedAddress = nullptr;
		CachedFunction = nullptr;
#if DO_CHECK
		CachedContainer = nullptr;
#endif // DO_CHECK
		bCanSafelyUsedCachedAddress = false;
	}
}

namespace PropertyPathHelpers
{
	void FindFieldNameAndArrayIndex(const FString& InSegmentName, FString& OutFieldName, int32& OutArrayIndex)
	{
		OutFieldName = InSegmentName;

		// Parse the property name and (optional) array index
		int32 ArrayPos = OutFieldName.Find(TEXT("["));
		if ( ArrayPos != INDEX_NONE )
		{
			FString IndexToken = OutFieldName.RightChop(ArrayPos + 1).LeftChop(1);
			LexFromString(OutArrayIndex, *IndexToken);

			OutFieldName = OutFieldName.Left(ArrayPos);
		}
		else
		{
			OutArrayIndex = INDEX_NONE;
		}
	}

	bool GetPropertyValueAsString(UObject* InContainer, const FString& InPropertyPath, FString& OutValue)
	{
		UProperty* Property;
		return GetPropertyValueAsString(InContainer, InPropertyPath, OutValue, Property);
	}

	/** Helper for string-based getters */
	struct FInternalStringGetterResolver : public PropertyPathHelpersInternal::TPropertyPathResolver<FInternalStringGetterResolver>
	{
		FInternalStringGetterResolver(FString& InOutValue, UProperty*& InOutProperty)
			: Value(InOutValue)
			, Property(InOutProperty)
		{
		}

		template<typename ContainerType>
		bool Resolve_Impl(ContainerType* InContainer, const FCachedPropertyPath& InPropertyPath)
		{
			return PropertyPathHelpersInternal::GetPropertyValueAsString<ContainerType>(InContainer, InPropertyPath, Property, Value);
		}

		FString& Value;
		UProperty*& Property;
	};

	bool GetPropertyValueAsString(UObject* InContainer, const FString& InPropertyPath, FString& OutValue, UProperty*& OutProperty)
	{
		check(InContainer);

		FInternalStringGetterResolver Resolver(OutValue, OutProperty);
		return ResolvePropertyPath(InContainer, InPropertyPath, Resolver);
	}

	bool GetPropertyValueAsString(void* InContainer, UStruct* InStruct, const FString& InPropertyPath, FString& OutValue)
	{
		UProperty* Property;
		return GetPropertyValueAsString(InContainer, InStruct, InPropertyPath, OutValue, Property);
	}

	bool GetPropertyValueAsString(void* InContainer, UStruct* InStruct, const FString& InPropertyPath, FString& OutValue, UProperty*& OutProperty)
	{
		check(InContainer);
		check(InStruct);

		FInternalStringGetterResolver Resolver(OutValue, OutProperty);
		return ResolvePropertyPath(InContainer, InStruct, InPropertyPath, Resolver);
	}

	bool GetPropertyValueAsString(UObject* InContainer, const FCachedPropertyPath& InPropertyPath, FString& OutValue)
	{
		check(InContainer);

		UProperty* Property;
		FInternalStringGetterResolver Resolver(OutValue, Property);
		return ResolvePropertyPath(InContainer, InPropertyPath, Resolver);
	}

	bool GetPropertyValueAsString(void* InContainer, UStruct* InStruct, const FCachedPropertyPath& InPropertyPath, FString& OutValue)
	{
		check(InContainer);
		check(InStruct);

		UProperty* Property;
		FInternalStringGetterResolver Resolver(OutValue, Property);
		return ResolvePropertyPath(InContainer, InStruct, InPropertyPath, Resolver);
	}

	/** Helper for string-based setters */
	struct FInternalStringSetterResolver : public PropertyPathHelpersInternal::TPropertyPathResolver<FInternalStringSetterResolver>
	{
		FInternalStringSetterResolver(const FString& InValueAsString)
			: Value(InValueAsString)
		{
		}

		template<typename ContainerType>
		bool Resolve_Impl(ContainerType* InContainer, const FCachedPropertyPath& InPropertyPath)
		{
			return PropertyPathHelpersInternal::SetPropertyValueFromString<ContainerType>(InContainer, InPropertyPath, Value);
		}
	
		const FString& Value;
	};

	bool SetPropertyValueFromString(UObject* InContainer, const FString& InPropertyPath, const FString& InValue)
	{
		check(InContainer);

		FInternalStringSetterResolver Resolver(InValue);
		return ResolvePropertyPath(InContainer, InPropertyPath, Resolver);
	}

	bool SetPropertyValueFromString(UObject* InContainer, const FCachedPropertyPath& InPropertyPath, const FString& InValue)
	{
		check(InContainer);

		FInternalStringSetterResolver Resolver(InValue);
		return ResolvePropertyPath(InContainer, InPropertyPath, Resolver);
	}

	bool SetPropertyValueFromString(void* InContainer, UStruct* InStruct, const FString& InPropertyPath, const FString& InValue)
	{
		check(InContainer);
		check(InStruct);

		FInternalStringSetterResolver Resolver(InValue);
		return ResolvePropertyPath(InContainer, InStruct, InPropertyPath, Resolver);
	}

	bool SetPropertyValueFromString(void* InContainer, UStruct* InStruct, const FCachedPropertyPath& InPropertyPath, const FString& InValue)
	{
		check(InContainer);
		check(InStruct);

		FInternalStringSetterResolver Resolver(InValue);
		return ResolvePropertyPath(InContainer, InStruct, InPropertyPath, Resolver);
	}

	bool CopyPropertyValue(UObject* InContainer, const FCachedPropertyPath& InDestPropertyPath, const FCachedPropertyPath& InSrcPropertyPath)
	{
		if(InDestPropertyPath.IsFullyResolved() && InSrcPropertyPath.IsFullyResolved())
		{
			return PropertyPathHelpersInternal::CopyResolvedPaths(InDestPropertyPath, InSrcPropertyPath);
		}
		else
		{
			FInternalCacheResolver DestResolver;
			FInternalCacheResolver SrcResolver;
			if(ResolvePropertyPath(InContainer, InDestPropertyPath, DestResolver) && ResolvePropertyPath(InContainer, InSrcPropertyPath, SrcResolver))
			{
				if(InDestPropertyPath.IsResolved() && InSrcPropertyPath.IsResolved())
				{
					if(PropertyPathHelpersInternal::CanCopyProperties(InDestPropertyPath, InSrcPropertyPath))
					{
						return PropertyPathHelpersInternal::CopyResolvedPaths(InDestPropertyPath, InSrcPropertyPath);
					}
				}
			}
		}

		return false;
	}

	bool CopyPropertyValueFast(UObject* InContainer, const FCachedPropertyPath& InDestPropertyPath, const FCachedPropertyPath& InSrcPropertyPath)
	{
#if DO_CHECK
		check(InContainer == InDestPropertyPath.GetCachedContainer());
		check(InContainer == InSrcPropertyPath.GetCachedContainer());
#endif // DO_CHECK
		checkSlow(InDestPropertyPath.IsResolved());
		checkSlow(InSrcPropertyPath.IsResolved());
		checkSlow(PropertyPathHelpersInternal::CanCopyProperties(InDestPropertyPath, InSrcPropertyPath));

		return PropertyPathHelpersInternal::CopyResolvedPaths(InDestPropertyPath, InSrcPropertyPath);
	}

	/** Helper for array operations*/
	struct FInternalArrayOperationResolver : public PropertyPathHelpersInternal::TPropertyPathResolver<FInternalArrayOperationResolver>
	{
		FInternalArrayOperationResolver(TFunctionRef<bool(FScriptArrayHelper&,int32)> InOperation)
			: Operation(InOperation)
		{
		}

		template<typename ContainerType>
		bool Resolve_Impl(ContainerType* InContainer, const FCachedPropertyPath& InPropertyPath)
		{
			return PropertyPathHelpersInternal::PerformArrayOperation<ContainerType>(InContainer, InPropertyPath, Operation);
		}
	
		TFunctionRef<bool(FScriptArrayHelper&,int32)> Operation;
	};

	bool PerformArrayOperation(UObject* InContainer, const FString& InPropertyPath, TFunctionRef<bool(FScriptArrayHelper&,int32)> InOperation)
	{
		FInternalArrayOperationResolver Resolver(InOperation);
		return ResolvePropertyPath(InContainer, InPropertyPath, Resolver);
	}

	bool PerformArrayOperation(UObject* InContainer, const FCachedPropertyPath& InPropertyPath, TFunctionRef<bool(FScriptArrayHelper&,int32)> InOperation)
	{
		FInternalArrayOperationResolver Resolver(InOperation);
		return ResolvePropertyPath(InContainer, InPropertyPath, Resolver);
	}
}