// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneActorReferenceSection.h"
#include "Channels/MovieSceneChannelProxy.h"

uint32 FMovieSceneActorReferenceData::GetChannelID()
{
	static uint32 ID = FMovieSceneChannelEntry::RegisterNewID();
	return ID;
}

FMovieSceneActorReferenceKey FMovieSceneActorReferenceData::Evaluate(FFrameTime InTime) const
{
	if (KeyTimes.Num())
	{
		const int32 Index = FMath::Max(0, Algo::UpperBound(KeyTimes, InTime.FrameNumber)-1);
		return KeyValues[Index];
	}

	return DefaultValue;
}

UMovieSceneActorReferenceSection::UMovieSceneActorReferenceSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
#if WITH_EDITOR

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(ActorReferenceData, FMovieSceneChannelEditorData());

#else

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(ActorReferenceData);

#endif
}

void UMovieSceneActorReferenceSection::PostLoad()
{
	Super::PostLoad();

	if (ActorGuidStrings_DEPRECATED.Num())
	{
		TArray<FGuid> Guids;

		for (const FString& ActorGuidString : ActorGuidStrings_DEPRECATED)
		{
			FGuid& ActorGuid = Guids[Guids.Emplace()];
			FGuid::Parse( ActorGuidString,  ActorGuid);
		}

		if (Guids.IsValidIndex(ActorGuidIndexCurve_DEPRECATED.GetDefaultValue()))
		{
			FMovieSceneObjectBindingID DefaultValue(Guids[ActorGuidIndexCurve_DEPRECATED.GetDefaultValue()], MovieSceneSequenceID::Root, EMovieSceneObjectBindingSpace::Local);
			ActorReferenceData.SetDefault(DefaultValue);
		}

		for (auto It = ActorGuidIndexCurve_DEPRECATED.GetKeyIterator(); It; ++It)
		{
			if (ensure(Guids.IsValidIndex(It->Value)))
			{
				FMovieSceneObjectBindingID BindingID(Guids[It->Value], MovieSceneSequenceID::Root, EMovieSceneObjectBindingSpace::Local);
				ActorReferenceData.UpgradeLegacyTime(this, It->Time, BindingID);
			}
		}
	}
}
