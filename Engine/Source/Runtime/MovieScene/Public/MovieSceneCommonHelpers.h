// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectKey.h"
#include "Curves/KeyHandle.h"
#include "Misc/FrameNumber.h"

class AActor;
class UCameraComponent;
class UMovieScene;
class UMovieSceneSection;
class UMovieSceneSequence;
class USceneComponent;
class USoundBase;
struct FRichCurve;
enum class EMovieSceneKeyInterpolation : uint8;

class MOVIESCENE_API MovieSceneHelpers
{
public:

	/**
	 * Finds a section that exists at a given time
	 *
	 * @param Time	The time to find a section at
	 * @return The found section or null
	 */
	static UMovieSceneSection* FindSectionAtTime( const TArray<UMovieSceneSection*>& Sections, FFrameNumber Time );

	/**
	 * Finds the nearest section to the given time
	 *
	 * @param Time	The time to find a section at
	 * @return The found section or null
	 */
	static UMovieSceneSection* FindNearestSectionAtTime( const TArray<UMovieSceneSection*>& Sections, FFrameNumber Time );

	/*
	 * Fix up consecutive sections so that there are no gaps
	 * 
	 * @param Sections All the sections
	 * @param Section The section that was modified 
	 * @param bDelete Was this a deletion?
	 */
	static void FixupConsecutiveSections(TArray<UMovieSceneSection*>& Sections, UMovieSceneSection& Section, bool bDelete);

	/*
 	 * Sort consecutive sections so that they are in order based on start time
 	 */
	static void SortConsecutiveSections(TArray<UMovieSceneSection*>& Sections);

	/*
	 * Gather up descendant movie scenes from the incoming sequence
	 */
	static void GetDescendantMovieScenes(UMovieSceneSequence* InSequence, TArray<UMovieScene*> & InMovieScenes);

	/**
	 * Get the scene component from the runtime object
	 *
	 * @param Object The object to get the scene component for
	 * @return The found scene component
	 */	
	static USceneComponent* SceneComponentFromRuntimeObject(UObject* Object);

	/**
	 * Get the active camera component from the actor 
	 *
	 * @param InActor The actor to look for the camera component on
	 * @return The active camera component
	 */
	static UCameraComponent* CameraComponentFromActor(const AActor* InActor);

	/**
	 * Find and return camera component from the runtime object
	 *
	 * @param Object The object to get the camera component for
	 * @return The found camera component
	 */	
	static UCameraComponent* CameraComponentFromRuntimeObject(UObject* RuntimeObject);

	/**
	 * Set the runtime object movable
	 *
	 * @param Object The object to set the mobility for
	 * @param Mobility The mobility of the runtime object
	 */
	static void SetRuntimeObjectMobility(UObject* Object, EComponentMobility::Type ComponentMobility = EComponentMobility::Movable);

	/*
	 * Get the duration for the given sound

	 * @param Sound The sound to get the duration for
	 * @return The duration in seconds
	 */
	static float GetSoundDuration(USoundBase* Sound);

	/**
	 * Sort predicate that sorts lower bounds of a range
	 */
	static bool SortLowerBounds(TRangeBound<FFrameNumber> A, TRangeBound<FFrameNumber> B)
	{
		return TRangeBound<FFrameNumber>::MinLower(A, B) == A && A != B;
	}

	/**
	 * Sort predicate that sorts upper bounds of a range
	 */
	static bool SortUpperBounds(TRangeBound<FFrameNumber> A, TRangeBound<FFrameNumber> B)
	{
		return TRangeBound<FFrameNumber>::MinUpper(A, B) == A && A != B;
	}

	/**
	 * Sort predicate that sorts overlapping sections by row primarily, then by overlap priority
	 */
	static bool SortOverlappingSections(const UMovieSceneSection* A, const UMovieSceneSection* B);

	/*
	* Get weight needed to modify the global difference in order to correctly key this section due to it possibly being blended by other sections.
	* @param Section The Section who's weight we are calculating.
	* @param  Time we are at.
	* @return Returns the weight that needs to be applied to the global difference to correctly key this section.
	*/
	static float CalculateWeightForBlending(UMovieSceneSection* SectionToKey, FFrameNumber Time);
};

/**
 * Manages bindings to keyed properties for a track instance. 
 * Calls UFunctions to set the value on runtime objects
 */
class MOVIESCENE_API FTrackInstancePropertyBindings
{
public:
	FTrackInstancePropertyBindings( FName InPropertyName, const FString& InPropertyPath, const FName& InFunctionName = FName(), const FName& InNotifyFunctionName = FName());

	/**
	 * Calls the setter function for a specific runtime object or if the setter function does not exist, the property is set directly
	 *
	 * @param InRuntimeObject The runtime object whose function to call
	 * @param PropertyValue The new value to assign to the property
	 */
	template <typename ValueType>
	void CallFunction( UObject& InRuntimeObject, typename TCallTraits<ValueType>::ParamType PropertyValue )
	{
		FPropertyAndFunction PropAndFunction = FindOrAdd(InRuntimeObject);
		if (UFunction* SetterFunction = PropAndFunction.SetterFunction.Get())
		{
			InvokeSetterFunction(&InRuntimeObject, SetterFunction, PropertyValue);
		}
		else if (ValueType* Val = PropAndFunction.GetPropertyAddress<ValueType>())
		{
			*Val = MoveTempIfPossible(PropertyValue);
		}

		if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
		{
			InRuntimeObject.ProcessEvent(NotifyFunction, nullptr);
		}
	}

	/**
	 * Calls the setter function for a specific runtime object or if the setter function does not exist, the property is set directly
	 *
	 * @param InRuntimeObject The runtime object whose function to call
	 * @param PropertyValue The new value to assign to the property
	 */
	void CallFunctionForEnum( UObject& InRuntimeObject, int64 PropertyValue );

	/**
	 * Rebuilds the property and function mappings for a single runtime object, and adds them to the cache
	 *
	 * @param InRuntimeObject	The object to cache mappings for
	 */
	void CacheBinding( const UObject& InRuntimeObject );

	/**
	 * Gets the UProperty that is bound to the track instance
	 *
	 * @param Object	The Object that owns the property
	 * @return			The property on the object if it exists
	 */
	UProperty* GetProperty(const UObject& Object) const;

	/**
	 * Gets the current value of a property on an object
	 *
	 * @param Object	The object to get the property from
	 * @return ValueType	The current value
	 */
	template <typename ValueType>
	ValueType GetCurrentValue(const UObject& Object)
	{
		FPropertyAndFunction PropAndFunction = FindOrAdd(Object);

		const ValueType* Val = PropAndFunction.GetPropertyAddress<ValueType>();
		return Val ? *Val : ValueType();
	}

	/**
	 * Optionally gets the current value of a property on an object
	 *
	 * @param Object	The object to get the property from
	 * @return (Optional) The current value of the property on the object
	 */
	template <typename ValueType>
	TOptional<ValueType> GetOptionalValue(const UObject& Object)
	{
		FPropertyAndFunction PropAndFunction = FindOrAdd(Object);

		const ValueType* Val = PropAndFunction.GetPropertyAddress<ValueType>();
		return Val ? *Val : TOptional<ValueType>();
	}

	/**
	 * Gets the current value of a property on an object
	 *
	 * @param Object	The object to get the property from
	 * @return ValueType	The current value
	 */
	int64 GetCurrentValueForEnum(const UObject& Object);

	/**
	 * Sets the current value of a property on an object
	 *
	 * @param Object	The object to set the property on
	 * @param InValue   The value to set
	 */
	template <typename ValueType>
	void SetCurrentValue(UObject& Object, typename TCallTraits<ValueType>::ParamType InValue)
	{
		FPropertyAndFunction PropAndFunction = FindOrAdd(Object);

		if(ValueType* Val = PropAndFunction.GetPropertyAddress<ValueType>())
		{
			*Val = InValue;

			if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
			{
				Object.ProcessEvent(NotifyFunction, nullptr);
			}
		}
	}

	/** @return the property path that this binding was initialized from */
	const FString& GetPropertyPath() const
	{
		return PropertyPath;
	}

	/** @return the property name that this binding was initialized from */
	const FName& GetPropertyName() const
	{
		return PropertyName;
	}

private:

	/**
	 * Wrapper for UObject::ProcessEvent that attempts to pass the new property value directly to the function as a parameter,
	 * but handles cases where multiple parameters or a return value exists. The setter parameter must be the first in the list,
	 * any other parameters will be default constructed.
	 */
	template<typename T>
	static void InvokeSetterFunction(UObject* InRuntimeObject, UFunction* Setter, T&& InPropertyValue);

	struct FPropertyAddress
	{
		TWeakObjectPtr<UProperty> Property;
		void* Address;

		UProperty* GetProperty() const
		{
			UProperty* PropertyPtr = Property.Get();
			if (PropertyPtr && Address && !PropertyPtr->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
			{
				return PropertyPtr;
			}
			return nullptr;
		}

		FPropertyAddress()
			: Property(nullptr)
			, Address(nullptr)
		{}
	};

	struct FPropertyAndFunction
	{
		FPropertyAddress PropertyAddress;
		TWeakObjectPtr<UFunction> SetterFunction;
		TWeakObjectPtr<UFunction> NotifyFunction;

		template<typename ValueType>
		ValueType* GetPropertyAddress() const
		{
			UProperty* PropertyPtr = PropertyAddress.GetProperty();
			return PropertyPtr ? PropertyPtr->ContainerPtrToValuePtr<ValueType>(PropertyAddress.Address) : nullptr;
		}

		FPropertyAndFunction()
			: PropertyAddress()
			, SetterFunction( nullptr )
			, NotifyFunction( nullptr )
		{}
	};

	static FPropertyAddress FindPropertyRecursive(void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames, uint32 Index);
	static FPropertyAddress FindProperty(const UObject& Object, const FString& InPropertyPath);

	/** Find or add the FPropertyAndFunction for the specified object */
	FPropertyAndFunction FindOrAdd(const UObject& InObject)
	{
		FObjectKey ObjectKey(&InObject);

		const FPropertyAndFunction* PropAndFunction = RuntimeObjectToFunctionMap.Find(ObjectKey);
		if (PropAndFunction && (PropAndFunction->SetterFunction.IsValid() || PropAndFunction->PropertyAddress.Property.IsValid()))
		{
			return *PropAndFunction;
		}

		CacheBinding(InObject);
		return RuntimeObjectToFunctionMap.FindRef(ObjectKey);
	}

private:
	/** Mapping of objects to bound functions that will be called to update data on the track */
	TMap< FObjectKey, FPropertyAndFunction > RuntimeObjectToFunctionMap;

	/** Path to the property we are bound to */
	FString PropertyPath;

	/** Name of the function to call to set values */
	FName FunctionName;

	/** Name of a function to call when a value has been set */
	FName NotifyFunctionName;

	/** Actual name of the property we are bound to */
	FName PropertyName;

};

/** Explicit specializations for bools */
template<> MOVIESCENE_API void FTrackInstancePropertyBindings::CallFunction<bool>(UObject& InRuntimeObject, TCallTraits<bool>::ParamType PropertyValue);
template<> MOVIESCENE_API bool FTrackInstancePropertyBindings::GetCurrentValue<bool>(const UObject& Object);
template<> MOVIESCENE_API void FTrackInstancePropertyBindings::SetCurrentValue<bool>(UObject& Object, TCallTraits<bool>::ParamType InValue);

template<> MOVIESCENE_API void FTrackInstancePropertyBindings::CallFunction<UObject*>(UObject& InRuntimeObject, UObject* PropertyValue);
template<> MOVIESCENE_API UObject* FTrackInstancePropertyBindings::GetCurrentValue<UObject*>(const UObject& InRuntimeObject);
template<> MOVIESCENE_API void FTrackInstancePropertyBindings::SetCurrentValue<UObject*>(UObject& InRuntimeObject, UObject* InValue);


template<typename T>
void FTrackInstancePropertyBindings::InvokeSetterFunction(UObject* InRuntimeObject, UFunction* Setter, T&& InPropertyValue)
{
	// CacheBinding already guarantees that the function has >= 1 parameters
	const int32 ParmsSize = Setter->ParmsSize;

	// This should all be const really, but ProcessEvent only takes a non-const void*
	void* InputParameter = const_cast<typename TDecay<T>::Type*>(&InPropertyValue);

	// By default we try and use the existing stack value
	uint8* Params = reinterpret_cast<uint8*>(InputParameter);

	check(InRuntimeObject && Setter);
	if (Setter->ReturnValueOffset != MAX_uint16 || Setter->NumParms > 1)
	{
		// Function has a return value or multiple parameters, we need to initialize memory for the entire parameter pack
		// We use alloca here (as in UObject::ProcessEvent) to avoid a heap allocation. Alloca memory survives the current function's stack frame.
		Params = reinterpret_cast<uint8*>(FMemory_Alloca(ParmsSize));

		bool bFirstProperty = true;
		for (UProperty* Property = Setter->PropertyLink; Property; Property = Property->PropertyLinkNext)
		{
			// Initialize the parameter pack with any param properties that reside in the container
			if (Property->IsInContainer(ParmsSize))
			{
				Property->InitializeValue_InContainer(Params);

				// The first encountered property is assumed to be the input value so initialize this with the user-specified value from InPropertyValue
				if (Property->HasAnyPropertyFlags(CPF_Parm) && !Property->HasAnyPropertyFlags(CPF_ReturnParm) && bFirstProperty)
				{
					const bool bIsValid = ensureMsgf(sizeof(T) == Property->ElementSize, TEXT("Property type does not match for Sequencer setter function %s::%s (%ibytes != %ibytes"), *InRuntimeObject->GetName(), *Setter->GetName(), sizeof(T), Property->ElementSize);
					if (bIsValid)
					{
						Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Params), &InPropertyValue);
					}
					else
					{
						return;
					}
				}
				bFirstProperty = false;
			}
		}
	}

	// Now we have the parameters set up correctly, call the function
	InRuntimeObject->ProcessEvent(Setter, Params);
}