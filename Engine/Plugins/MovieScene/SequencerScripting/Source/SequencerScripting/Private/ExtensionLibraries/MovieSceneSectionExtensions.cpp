// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ExtensionLibraries/MovieSceneSectionExtensions.h"
#include "SequencerScriptingRange.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "Channels/MovieSceneBoolChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneChannelHandle.h"
#include "KeysAndChannels/MovieSceneScriptingBool.h"
#include "KeysAndChannels/MovieSceneScriptingByte.h"
#include "KeysAndChannels/MovieSceneScriptingInteger.h"
#include "KeysAndChannels/MovieSceneScriptingFloat.h"
#include "KeysAndChannels/MovieSceneScriptingString.h"
#include "KeysAndChannels/MovieSceneScriptingEvent.h"
#include "KeysAndChannels/MovieSceneScriptingActorReference.h"


FSequencerScriptingRange UMovieSceneSectionExtensions::GetRange(UMovieSceneSection* Section)
{
	UMovieScene* MovieScene = Section->GetTypedOuter<UMovieScene>();
	return FSequencerScriptingRange::FromNative(Section->GetRange(), MovieScene->GetTickResolution());
}

void UMovieSceneSectionExtensions::SetRange(UMovieSceneSection* Section, const FSequencerScriptingRange& InRange)
{
	UMovieScene* MovieScene = Section->GetTypedOuter<UMovieScene>();
	TRange<FFrameNumber> NewRange = InRange.ToNative(MovieScene->GetTickResolution());

	if (NewRange.GetLowerBound().IsOpen() || NewRange.GetUpperBound().IsOpen() || NewRange.GetLowerBoundValue() <= NewRange.GetUpperBoundValue())
	{
		Section->SetRange(NewRange);
	}
	else
	{
		UE_LOG(LogMovieScene, Error, TEXT("Invalid range specified"));
	}
}

template<typename ChannelType, typename ScriptingChannelType>
void GetScriptingChannelsForChannel(FMovieSceneChannelProxy& ChannelProxy, TWeakObjectPtr<UMovieSceneSequence> Sequence, TArray<UMovieSceneScriptingChannel*>& OutChannels)
{
	for (int32 i = 0; i < ChannelProxy.GetChannels<ChannelType>().Num(); i++)
	{
		TMovieSceneChannelHandle<ChannelType> ChannelHandle = ChannelProxy.MakeHandle<ChannelType>(i);

		const FName ChannelName = ChannelHandle.GetMetaData()->Name;
		ScriptingChannelType* ScriptingChannel = NewObject<ScriptingChannelType>(GetTransientPackage(), ChannelName);
		ScriptingChannel->ChannelHandle = ChannelHandle;
		ScriptingChannel->OwningSequence = Sequence;

		OutChannels.Add(ScriptingChannel);
	}
}

TArray<UMovieSceneScriptingChannel*> UMovieSceneSectionExtensions::GetChannels(UMovieSceneSection* Section)
{
	TArray<UMovieSceneScriptingChannel*> Channels;
	if (!Section)
	{
		UE_LOG(LogMovieScene, Error, TEXT("Cannot get channels for null section"));
		return Channels;
	}
	FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();

	// ToDo: This isn't a great way of collecting all of the channel types and instantiating the correct UObject for it
	// but given that we're already hard-coding the UObject implementations... We currently support the following channel
	// types: Bool, Byte, Float, Integer, String, Event, ActorReference
	TWeakObjectPtr<UMovieSceneSequence> Sequence = Section->GetTypedOuter<UMovieSceneSequence>();
	GetScriptingChannelsForChannel<FMovieSceneBoolChannel, UMovieSceneScriptingBoolChannel>(ChannelProxy, Sequence, Channels);
	GetScriptingChannelsForChannel<FMovieSceneByteChannel, UMovieSceneScriptingByteChannel>(ChannelProxy, Sequence, Channels);
	GetScriptingChannelsForChannel<FMovieSceneFloatChannel, UMovieSceneScriptingFloatChannel>(ChannelProxy, Sequence, Channels);
	GetScriptingChannelsForChannel<FMovieSceneIntegerChannel, UMovieSceneScriptingIntegerChannel>(ChannelProxy, Sequence, Channels);
	GetScriptingChannelsForChannel<FMovieSceneStringChannel, UMovieSceneScriptingStringChannel>(ChannelProxy, Sequence, Channels);
	GetScriptingChannelsForChannel<FMovieSceneEventSectionData, UMovieSceneScriptingEventChannel>(ChannelProxy, Sequence, Channels);
	GetScriptingChannelsForChannel<FMovieSceneActorReferenceData, UMovieSceneScriptingActorReferenceChannel>(ChannelProxy, Sequence, Channels);

	return Channels;
}

TArray<UMovieSceneScriptingChannel*> UMovieSceneSectionExtensions::FindChannelsByType(UMovieSceneSection* Section, TSubclassOf<UMovieSceneScriptingChannel> ChannelType)
{
	TArray<UMovieSceneScriptingChannel*> Channels;
	if (!Section)
	{
 		UE_LOG(LogMovieScene, Error, TEXT("Cannot get channels for null section"));
 		return Channels;
	}
 
	FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
 
	// ToDo: This needs to be done dynamically in the future but there's not a good way to do that right now with all of the SFINAE-based Templating driving these channels
	TWeakObjectPtr<UMovieSceneSequence> Sequence = Section->GetTypedOuter<UMovieSceneSequence>();

	if (ChannelType == UMovieSceneScriptingBoolChannel::StaticClass())					{ GetScriptingChannelsForChannel<FMovieSceneBoolChannel, UMovieSceneScriptingBoolChannel>(ChannelProxy, Sequence, Channels); }
	else if (ChannelType == UMovieSceneScriptingByteChannel::StaticClass())				{ GetScriptingChannelsForChannel<FMovieSceneByteChannel, UMovieSceneScriptingByteChannel>(ChannelProxy, Sequence, Channels); }
	else if (ChannelType == UMovieSceneScriptingFloatChannel::StaticClass())			{ GetScriptingChannelsForChannel<FMovieSceneFloatChannel, UMovieSceneScriptingFloatChannel>(ChannelProxy, Sequence, Channels); }
	else if (ChannelType == UMovieSceneScriptingIntegerChannel::StaticClass())			{ GetScriptingChannelsForChannel<FMovieSceneIntegerChannel, UMovieSceneScriptingIntegerChannel>(ChannelProxy, Sequence, Channels); }
	else if (ChannelType == UMovieSceneScriptingStringChannel::StaticClass())			{ GetScriptingChannelsForChannel<FMovieSceneStringChannel, UMovieSceneScriptingStringChannel>(ChannelProxy, Sequence, Channels); }
	else if (ChannelType == UMovieSceneScriptingEventChannel::StaticClass())			{ GetScriptingChannelsForChannel<FMovieSceneEventSectionData, UMovieSceneScriptingEventChannel>(ChannelProxy, Sequence, Channels); }
	else if (ChannelType == UMovieSceneScriptingActorReferenceChannel::StaticClass())	{ GetScriptingChannelsForChannel<FMovieSceneActorReferenceData, UMovieSceneScriptingActorReferenceChannel>(ChannelProxy, Sequence, Channels); }
	else
	{
		UE_LOG(LogMovieScene, Error, TEXT("Unsupported ChannelType for FindChannelsByType!"));
	}


	return Channels;
}
