// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TrackRecorders/MovieScenePropertyTrackRecorder.h"
#include "UObject/UnrealType.h"
#include "MovieSceneCommonHelpers.h"

bool FMovieScenePropertyTrackRecorderFactory::CanRecordProperty(UObject* InObjectToRecord, UProperty* InPropertyToRecord) const
{
	if (InPropertyToRecord->IsA<UBoolProperty>())
	{
		return true;
	}
	else if (InPropertyToRecord->IsA<UByteProperty>())
	{
		return true;
	}
	else if (InPropertyToRecord->IsA<UEnumProperty>())
	{
		return true;
	}
	else if (InPropertyToRecord->IsA<UIntProperty>())
	{
		return true;
	}
	else if (InPropertyToRecord->IsA<UStrProperty>())
	{
		return true;
	}
	else if (InPropertyToRecord->IsA<UFloatProperty>())
	{
		return true;
	}
	else if (UStructProperty* StructProperty = Cast<UStructProperty>(InPropertyToRecord))
	{
		if (StructProperty->Struct->GetFName() == NAME_Vector)
		{
			return true;
		}
		else if (StructProperty->Struct->GetFName() == NAME_Color)
		{
			return true;
		}
	}

	// We only know how to make generic tracks for the types above
	return false;
}

UMovieSceneTrackRecorder* FMovieScenePropertyTrackRecorderFactory::CreateTrackRecorderForProperty(UObject* InObjectToRecord, const FName& InPropertyToRecord) const
{
	UMovieScenePropertyTrackRecorder* TrackRecorder = NewObject<UMovieScenePropertyTrackRecorder>();
	TrackRecorder->PropertyToRecord = InPropertyToRecord;
	return TrackRecorder;
}

UMovieSceneTrackRecorder* FMovieScenePropertyTrackRecorderFactory::CreateTrackRecorderForPropertyEnum(ESerializedPropertyType PropertyType, const FName& InPropertyToRecord) const
{
	UMovieScenePropertyTrackRecorder* TrackRecorder = NewObject<UMovieScenePropertyTrackRecorder>();
	TrackRecorder->PropertyToRecord = InPropertyToRecord;

	FTrackInstancePropertyBindings Binding(InPropertyToRecord, InPropertyToRecord.ToString());
	switch (PropertyType)
	{
	case ESerializedPropertyType::BoolType:
		TrackRecorder->PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<bool>(Binding));
		break;
	case ESerializedPropertyType::ByteType:
		TrackRecorder->PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<uint8>(Binding));
		break;
	case ESerializedPropertyType::EnumType:
		TrackRecorder->PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorderEnum(Binding));
		break;
	case ESerializedPropertyType::IntegerType:
		TrackRecorder->PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<int32>(Binding));
		break;
	case ESerializedPropertyType::StringType:
		TrackRecorder->PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<FString>(Binding));
		break;
	case ESerializedPropertyType::FloatType:
		TrackRecorder->PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<float>(Binding));
		break;
	case ESerializedPropertyType::VectorType:
		TrackRecorder->PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<FVector>(Binding));
		break;
	case ESerializedPropertyType::ColorType:
		TrackRecorder->PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<FColor>(Binding));
		break;

	}
	return TrackRecorder;
}

void UMovieScenePropertyTrackRecorder::CreateTrackImpl()
{
 	FTrackInstancePropertyBindings Binding(PropertyToRecord, PropertyToRecord.ToString());
 	UProperty* Property = Binding.GetProperty(*ObjectToRecord);
 	if (Property != nullptr)
 	{
 		if (Property->IsA<UBoolProperty>())
 		{
 			PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<bool>(Binding));
 		}
 		else if (Property->IsA<UByteProperty>())
 		{
 			PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<uint8>(Binding));
 		}
		else if (Property->IsA<UEnumProperty>())
		{
			PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorderEnum(Binding));
		}
		else if (Property->IsA<UIntProperty>())
		{
			PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<int32>(Binding));
		}
		else if (Property->IsA<UStrProperty>())
		{
			PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<FString>(Binding));
		}
		else if (Property->IsA<UFloatProperty>())
 		{
			PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<float>(Binding));
 		}
 		else if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
 		{
 			if (StructProperty->Struct->GetFName() == NAME_Vector)
 			{
 				PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<FVector>(Binding));
 			}
 			else if (StructProperty->Struct->GetFName() == NAME_Color)
 			{
				PropertyRecorder = MakeShareable(new FMovieSceneTrackPropertyRecorder<FColor>(Binding));
 			}
 		} 
		ensure(PropertyRecorder);
		PropertyRecorder->SetSavedRecordingDirectory(Directory);
 		PropertyRecorder->Create(OwningTakeRecorderSource, ObjectToRecord.Get(), MovieScene.Get(), ObjectGuid, true);
 	}
}

void UMovieScenePropertyTrackRecorder::SetSectionStartTimecodeImpl(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame)
{
	PropertyRecorder->SetSectionStartTimecode(InSectionStartTimecode, InSectionFirstFrame);
}

void UMovieScenePropertyTrackRecorder::FinalizeTrackImpl()
{
	PropertyRecorder->Finalize(ObjectToRecord.Get());
}

void UMovieScenePropertyTrackRecorder::RecordSampleImpl(const FQualifiedFrameTime& CurrentTime)
{
	PropertyRecorder->Record(ObjectToRecord.Get(), CurrentTime);
}

bool UMovieScenePropertyTrackRecorder::LoadRecordedFile(const FString& InFileName, UMovieScene *InMovieScene, TMap<FGuid, AActor*>& ActorGuidToActorMap, TFunction<void()> InCompletionCallback)
{
	if (PropertyRecorder)
	{
		return PropertyRecorder->LoadRecordedFile(InFileName, InMovieScene, ActorGuidToActorMap, InCompletionCallback);
	}
	return false;
}