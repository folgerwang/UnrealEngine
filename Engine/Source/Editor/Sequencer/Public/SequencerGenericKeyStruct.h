// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneKeyStruct.h"
#include "SequencerGenericKeyStruct.generated.h"

class IDetailLayoutBuilder;

struct IMovieSceneKeyStructCustomization
{
	virtual ~IMovieSceneKeyStructCustomization() {}

	/** Extend the specified (empty) details customization with the specified key handle */
	virtual void Extend(IDetailLayoutBuilder& DetailBuilder) = 0;
	virtual void Apply(FFrameNumber Time) = 0;
};

template<typename ChannelType>
struct TMovieSceneKeyStructCustomization : IMovieSceneKeyStructCustomization
{
	TMovieSceneKeyStructCustomization(TMovieSceneChannelHandle<ChannelType> InChannel, FKeyHandle InHandle)
		: KeyHandle(InHandle), ChannelHandle(InChannel)
	{}

	TSharedPtr<FStructOnScope> GetValueStruct();

	/** Extend the specified (empty) details customization with the specified key handle */
	virtual void Extend(IDetailLayoutBuilder& DetailBuilder) override;

	virtual void Apply(FFrameNumber NewTime) override;

private:

	FKeyHandle KeyHandle;
	TMovieSceneChannelHandle<ChannelType> ChannelHandle;
	TSharedPtr<FStructOnScope> KeyStruct;
};

USTRUCT()
struct SEQUENCER_API FSequencerGenericKeyStruct : public FMovieSceneKeyStruct
{
	GENERATED_BODY()

	/** Cusomtization implementation that adds the key value data */
	TSharedPtr<IMovieSceneKeyStructCustomization> CustomizationImpl;

	/** This key's time */
	UPROPERTY(EditAnywhere, Category="Key")
	FFrameNumber Time;

	/**
	 * Propagate changes from this key structure to the corresponding key values.
	 *
	 * @param ChangeEvent The property change event.
	 */
	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent)
	{
		if (CustomizationImpl.IsValid())
		{
			CustomizationImpl->Apply(Time);
		}
	}
};