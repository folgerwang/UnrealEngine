// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCommonHelpers.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "KeyParams.h"
#include "MovieSceneSection.h"
#include "Algo/Sort.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"

UMovieSceneSection* MovieSceneHelpers::FindSectionAtTime( const TArray<UMovieSceneSection*>& Sections, FFrameNumber Time )
{
	for( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
	{
		UMovieSceneSection* Section = Sections[SectionIndex];

		//@todo sequencer: There can be multiple sections overlapping in time. Returning instantly does not account for that.
		if( Section->IsTimeWithinSection( Time ) && Section->IsActive() )
		{
			return Section;
		}
	}

	return nullptr;
}


UMovieSceneSection* MovieSceneHelpers::FindNearestSectionAtTime( const TArray<UMovieSceneSection*>& Sections, FFrameNumber Time )
{
	TArray<UMovieSceneSection*> OverlappingSections, NonOverlappingSections;
	for (UMovieSceneSection* Section : Sections)
	{
		if (Section->GetRange().Contains(Time))
		{
			OverlappingSections.Add(Section);
		}
		else
		{
			NonOverlappingSections.Add(Section);
		}
	}

	if (OverlappingSections.Num())
	{
		Algo::Sort(OverlappingSections, SortOverlappingSections);
		return OverlappingSections[0];
	}

	if (NonOverlappingSections.Num())
	{
		Algo::SortBy(NonOverlappingSections, [](const UMovieSceneSection* A) { return A->GetRange().GetUpperBound(); }, SortUpperBounds);

		const int32 PreviousIndex = Algo::UpperBoundBy(NonOverlappingSections, TRangeBound<FFrameNumber>(Time), [](const UMovieSceneSection* A){ return A->GetRange().GetUpperBound(); }, SortUpperBounds)-1;
		if (NonOverlappingSections.IsValidIndex(PreviousIndex))
		{
			return NonOverlappingSections[PreviousIndex];
		}
		else
		{
			Algo::SortBy(NonOverlappingSections, [](const UMovieSceneSection* A) { return A->GetRange().GetLowerBound(); }, SortLowerBounds);
			return NonOverlappingSections[0];
		}
	}

	return nullptr;
}

bool MovieSceneHelpers::SortOverlappingSections(const UMovieSceneSection* A, const UMovieSceneSection* B)
{
	return A->GetRowIndex() == B->GetRowIndex()
		? A->GetOverlapPriority() < B->GetOverlapPriority()
		: A->GetRowIndex() < B->GetRowIndex();
}

void MovieSceneHelpers::SortConsecutiveSections(TArray<UMovieSceneSection*>& Sections)
{
	Sections.Sort([](const UMovieSceneSection& A, const UMovieSceneSection& B)
		{
			TRangeBound<FFrameNumber> LowerBoundA = A.GetRange().GetLowerBound();
			return TRangeBound<FFrameNumber>::MinLower(LowerBoundA, B.GetRange().GetLowerBound()) == LowerBoundA;
		}
	);
}

void MovieSceneHelpers::FixupConsecutiveSections(TArray<UMovieSceneSection*>& Sections, UMovieSceneSection& Section, bool bDelete)
{
	// Find the previous section and extend it to take the place of the section being deleted
	int32 SectionIndex = INDEX_NONE;

	TRange<FFrameNumber> SectionRange = Section.GetRange();

	if (Sections.Find(&Section, SectionIndex))
	{
		int32 PrevSectionIndex = SectionIndex - 1;
		if( Sections.IsValidIndex( PrevSectionIndex ) )
		{
			// Extend the previous section
			if (bDelete)
			{
				Sections[PrevSectionIndex]->SetEndFrame(SectionRange.GetUpperBound());
			}
			else
			{
				Sections[PrevSectionIndex]->SetEndFrame(TRangeBound<FFrameNumber>::FlipInclusion(SectionRange.GetLowerBound()));
			}
		}

		if( !bDelete )
		{
			int32 NextSectionIndex = SectionIndex + 1;
			if(Sections.IsValidIndex(NextSectionIndex))
			{
				// Shift the next CameraCut's start time so that it starts when the new CameraCut ends
				Sections[NextSectionIndex]->SetStartFrame(TRangeBound<FFrameNumber>::FlipInclusion(SectionRange.GetUpperBound()));
			}
		}
	}

	SortConsecutiveSections(Sections);
}



USceneComponent* MovieSceneHelpers::SceneComponentFromRuntimeObject(UObject* Object)
{
	AActor* Actor = Cast<AActor>(Object);

	USceneComponent* SceneComponent = nullptr;
	if (Actor && Actor->GetRootComponent())
	{
		// If there is an actor, modify its root component
		SceneComponent = Actor->GetRootComponent();
	}
	else
	{
		// No actor was found.  Attempt to get the object as a component in the case that we are editing them directly.
		SceneComponent = Cast<USceneComponent>(Object);
	}
	return SceneComponent;
}

UCameraComponent* MovieSceneHelpers::CameraComponentFromActor(const AActor* InActor)
{
	TArray<UCameraComponent*> CameraComponents;
	InActor->GetComponents<UCameraComponent>(CameraComponents);

	// If there's a camera component that's active, return that one
	for (UCameraComponent* CameraComponent : CameraComponents)
	{
		if (CameraComponent->bIsActive)
		{
			return CameraComponent;
		}
	}

	// Otherwise, return the first camera component
	for (UCameraComponent* CameraComponent : CameraComponents)
	{
		return CameraComponent;
	}

	// now see if any actors are attached to us, directly or indirectly, that have an active camera component we might want to use
	// we will just return the first one.
	// #note: assumption here that attachment cannot be circular
	TArray<AActor*> AttachedActors;
	InActor->GetAttachedActors(AttachedActors);
	for (AActor* AttachedActor : AttachedActors)
	{
		UCameraComponent* const Comp = CameraComponentFromActor(AttachedActor);
		if (Comp)
		{
			return Comp;
		}
	}

	return nullptr;
}

UCameraComponent* MovieSceneHelpers::CameraComponentFromRuntimeObject(UObject* RuntimeObject)
{
	if (RuntimeObject)
	{
		// find camera we want to control
		UCameraComponent* const CameraComponent = dynamic_cast<UCameraComponent*>(RuntimeObject);
		if (CameraComponent)
		{
			return CameraComponent;
		}

		// see if it's an actor that has a camera component
		AActor* const Actor = dynamic_cast<AActor*>(RuntimeObject);
		if (Actor)
		{
			return CameraComponentFromActor(Actor);
		}
	}

	return nullptr;
}

float MovieSceneHelpers::GetSoundDuration(USoundBase* Sound)
{
	USoundWave* SoundWave = nullptr;

	if (Sound->IsA<USoundWave>())
	{
		SoundWave = Cast<USoundWave>(Sound);
	}
	else if (Sound->IsA<USoundCue>())
	{
#if WITH_EDITORONLY_DATA
		USoundCue* SoundCue = Cast<USoundCue>(Sound);

		// @todo Sequencer - Right now for sound cues, we just use the first sound wave in the cue
		// In the future, it would be better to properly generate the sound cue's data after forcing determinism
		const TArray<USoundNode*>& AllNodes = SoundCue->AllNodes;
		for (int32 i = 0; i < AllNodes.Num() && SoundWave == nullptr; ++i)
		{
			if (AllNodes[i]->IsA<USoundNodeWavePlayer>())
			{
				SoundWave = Cast<USoundNodeWavePlayer>(AllNodes[i])->GetSoundWave();
			}
		}
#endif
	}

	const float Duration = (SoundWave ? SoundWave->GetDuration() : 0.f);
	return Duration == INDEFINITELY_LOOPING_DURATION ? SoundWave->Duration : Duration;
}

FTrackInstancePropertyBindings::FTrackInstancePropertyBindings( FName InPropertyName, const FString& InPropertyPath, const FName& InFunctionName, const FName& InNotifyFunctionName )
    : PropertyPath( InPropertyPath )
	, NotifyFunctionName(InNotifyFunctionName)
	, PropertyName( InPropertyName )
{
	if (InFunctionName != FName())
	{
		FunctionName = InFunctionName;
	}
	else
	{
		static const FString Set(TEXT("Set"));

		const FString FunctionString = Set + PropertyName.ToString();

		FunctionName = FName(*FunctionString);
	}
}

struct FPropertyAndIndex
{
	FPropertyAndIndex() : Property(nullptr), ArrayIndex(INDEX_NONE) {}

	UProperty* Property;
	int32 ArrayIndex;
};

FPropertyAndIndex FindPropertyAndArrayIndex(UStruct* InStruct, const FString& PropertyName)
{
	FPropertyAndIndex PropertyAndIndex;

	// Calculate the array index if possible
	int32 ArrayIndex = -1;
	if (PropertyName.Len() > 0 && PropertyName.GetCharArray()[PropertyName.Len() - 1] == ']')
	{
		int32 OpenIndex = 0;
		if (PropertyName.FindLastChar('[', OpenIndex))
		{
			FString TruncatedPropertyName(OpenIndex, *PropertyName);
			PropertyAndIndex.Property = FindField<UProperty>(InStruct, *TruncatedPropertyName);

			const int32 NumberLength = PropertyName.Len() - OpenIndex - 2;
			if (NumberLength > 0 && NumberLength <= 10)
			{
				TCHAR NumberBuffer[11];
				FMemory::Memcpy(NumberBuffer, &PropertyName[OpenIndex + 1], sizeof(TCHAR) * NumberLength);
				LexFromString(PropertyAndIndex.ArrayIndex, NumberBuffer);
			}

			return PropertyAndIndex;
		}
	}

	PropertyAndIndex.Property = FindField<UProperty>(InStruct, *PropertyName);
	return PropertyAndIndex;
}

FTrackInstancePropertyBindings::FPropertyAddress FTrackInstancePropertyBindings::FindPropertyRecursive( void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames, uint32 Index )
{
	FPropertyAndIndex PropertyAndIndex = FindPropertyAndArrayIndex(InStruct, *InPropertyNames[Index]);
	
	FTrackInstancePropertyBindings::FPropertyAddress NewAddress;

	if (PropertyAndIndex.ArrayIndex != INDEX_NONE)
	{
		UArrayProperty* ArrayProp = CastChecked<UArrayProperty>(PropertyAndIndex.Property);

		FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(BasePointer));
		if (ArrayHelper.IsValidIndex(PropertyAndIndex.ArrayIndex))
		{
			UStructProperty* InnerStructProp = Cast<UStructProperty>(ArrayProp->Inner);
			if (InnerStructProp && InPropertyNames.IsValidIndex(Index+1))
			{
				return FindPropertyRecursive(ArrayHelper.GetRawPtr(PropertyAndIndex.ArrayIndex), InnerStructProp->Struct, InPropertyNames, Index+1);
			}
			else
			{
				NewAddress.Property = ArrayProp->Inner;
				NewAddress.Address = ArrayHelper.GetRawPtr(PropertyAndIndex.ArrayIndex);
			}
		}
	}
	else if (UStructProperty* StructProp = Cast<UStructProperty>(PropertyAndIndex.Property))
	{
		NewAddress.Property = StructProp;
		NewAddress.Address = BasePointer;

		if( InPropertyNames.IsValidIndex(Index+1) )
		{
			void* StructContainer = StructProp->ContainerPtrToValuePtr<void>(BasePointer);
			return FindPropertyRecursive( StructContainer, StructProp->Struct, InPropertyNames, Index+1 );
		}
		else
		{
			check( StructProp->GetName() == InPropertyNames[Index] );
		}
	}
	else if(PropertyAndIndex.Property)
	{
		NewAddress.Property = PropertyAndIndex.Property;
		NewAddress.Address = BasePointer;
	}

	return NewAddress;

}


FTrackInstancePropertyBindings::FPropertyAddress FTrackInstancePropertyBindings::FindProperty( const UObject& InObject, const FString& InPropertyPath )
{
	TArray<FString> PropertyNames;

	InPropertyPath.ParseIntoArray(PropertyNames, TEXT("."), true);

	if( PropertyNames.Num() > 0 )
	{
		return FindPropertyRecursive( (void*)&InObject, InObject.GetClass(), PropertyNames, 0 );
	}
	else
	{
		return FTrackInstancePropertyBindings::FPropertyAddress();
	}
}

void FTrackInstancePropertyBindings::CallFunctionForEnum( UObject& InRuntimeObject, int64 PropertyValue )
{
	FPropertyAndFunction PropAndFunction = FindOrAdd(InRuntimeObject);
	if (UFunction* Setter = PropAndFunction.SetterFunction.Get())
	{
		// ProcessEvent should really be taking const void*
		InRuntimeObject.ProcessEvent(Setter, (void*)&PropertyValue);
	}
	else if (UProperty* Property = PropAndFunction.PropertyAddress.GetProperty())
	{
		if (UEnumProperty* EnumProperty = CastChecked<UEnumProperty>(Property))
		{
			UNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
			void* ValueAddr = EnumProperty->ContainerPtrToValuePtr<void>(PropAndFunction.PropertyAddress.Address);
			UnderlyingProperty->SetIntPropertyValue(ValueAddr, PropertyValue);
		}
	}

	if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
	{
		InRuntimeObject.ProcessEvent(NotifyFunction, nullptr);
	}
}

void FTrackInstancePropertyBindings::CacheBinding(const UObject& Object)
{
	FPropertyAndFunction PropAndFunction;
	{
		PropAndFunction.SetterFunction = Object.FindFunction(FunctionName);
		PropAndFunction.PropertyAddress = FindProperty(Object, PropertyPath);
		if (NotifyFunctionName != NAME_None)
		{
			PropAndFunction.NotifyFunction = Object.FindFunction(NotifyFunctionName);
		}
	}

	RuntimeObjectToFunctionMap.Add(FObjectKey(&Object), PropAndFunction);
}

UProperty* FTrackInstancePropertyBindings::GetProperty(const UObject& Object) const
{
	FPropertyAndFunction PropAndFunction = RuntimeObjectToFunctionMap.FindRef(&Object);
	if (UProperty* Property = PropAndFunction.PropertyAddress.GetProperty())
	{
		return Property;
	}

	return FindProperty(Object, PropertyPath).GetProperty();
}

int64 FTrackInstancePropertyBindings::GetCurrentValueForEnum(const UObject& Object)
{
	FPropertyAndFunction PropAndFunction = FindOrAdd(Object);

	if (UProperty* Property = PropAndFunction.PropertyAddress.GetProperty())
	{
		if(UEnumProperty* EnumProperty = CastChecked<UEnumProperty>(Property))
		{
			UNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
			void* ValueAddr = EnumProperty->ContainerPtrToValuePtr<void>(PropAndFunction.PropertyAddress.Address);
			int64 Result = UnderlyingProperty->GetSignedIntPropertyValue(ValueAddr);
			return Result;
		}
	}

	return 0;
}

template<> void FTrackInstancePropertyBindings::CallFunction<bool>(UObject& InRuntimeObject, TCallTraits<bool>::ParamType PropertyValue)
{
	FPropertyAndFunction PropAndFunction = FindOrAdd(InRuntimeObject);
	if (UFunction* SetterFunction = PropAndFunction.SetterFunction.Get())
	{
		// ProcessEvent should really be taking const void*
		InRuntimeObject.ProcessEvent(SetterFunction, (void*)&PropertyValue);
	}
	else if (UProperty* Property = PropAndFunction.PropertyAddress.GetProperty())
	{
		if (UBoolProperty* BoolProperty = CastChecked<UBoolProperty>(Property))
		{
			uint8* ValuePtr = BoolProperty->ContainerPtrToValuePtr<uint8>(PropAndFunction.PropertyAddress.Address);
			BoolProperty->SetPropertyValue(ValuePtr, PropertyValue);
		}
	}

	if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
	{
		InRuntimeObject.ProcessEvent(NotifyFunction, nullptr);
	}
}

template<> bool FTrackInstancePropertyBindings::GetCurrentValue<bool>(const UObject& Object)
{
	FPropertyAndFunction PropAndFunction = FindOrAdd(Object);
	if (UProperty* Property = PropAndFunction.PropertyAddress.GetProperty())
	{
		if (UBoolProperty* BoolProperty = CastChecked<UBoolProperty>(Property))
		{
			const uint8* ValuePtr = BoolProperty->ContainerPtrToValuePtr<uint8>(PropAndFunction.PropertyAddress.Address);
			return BoolProperty->GetPropertyValue(ValuePtr);
		}
	}

	return false;
}

template<> void FTrackInstancePropertyBindings::SetCurrentValue<bool>(UObject& Object, TCallTraits<bool>::ParamType InValue)
{
	FPropertyAndFunction PropAndFunction = FindOrAdd(Object);
	if (UProperty* Property = PropAndFunction.PropertyAddress.GetProperty())
	{
		if (UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property))
		{
			uint8* ValuePtr = BoolProperty->ContainerPtrToValuePtr<uint8>(PropAndFunction.PropertyAddress.Address);
			BoolProperty->SetPropertyValue(ValuePtr, InValue);
		}
	}

	if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
	{
		Object.ProcessEvent(NotifyFunction, nullptr);
	}
}
