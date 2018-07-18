// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneKeyStructHelper.h"
#include "UObject/StructOnScope.h"
#include "MovieSceneKeyStruct.generated.h"

struct FPropertyChangedEvent;

/**
 * Base class for movie scene section key structs that need to manually
 * have their changes propagated to key values.
 */
USTRUCT()
struct FMovieSceneKeyStruct
{
	GENERATED_BODY()

	/**
	 * Propagate changes from this key structure to the corresponding key values.
	 *
	 * @param ChangeEvent The property change event.
	 */
	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent) { }

	virtual ~FMovieSceneKeyStruct() {}
};


USTRUCT()
struct FMovieSceneKeyTimeStruct : public FMovieSceneKeyStruct
{
	GENERATED_BODY()

	FMovieSceneKeyTimeStruct(){}

	UPROPERTY(EditAnywhere, Category="Key", meta=(Units=s))
	FFrameNumber Time;

	FMovieSceneKeyStructHelper KeyStructInterop;

	/**
	 * Propagate changes from this key structure to the corresponding key values.
	 *
	 * @param ChangeEvent The property change event.
	 */
	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent)
	{
		KeyStructInterop.Apply(Time);
	}
};
template<> struct TStructOpsTypeTraits<FMovieSceneKeyTimeStruct> : public TStructOpsTypeTraitsBase2<FMovieSceneKeyTimeStruct> { enum { WithCopy = false }; };

/**
 * Templated helper to aid in the creation of key structs
 */
template<typename KeyStructType, typename ChannelType>
TSharedPtr<FStructOnScope> CreateKeyStruct(TMovieSceneChannelHandle<ChannelType> ChannelHandle, FKeyHandle InHandle)
{
	TSharedPtr<FStructOnScope> KeyStruct;

	ChannelType* Channel = ChannelHandle.Get();
	if (Channel)
	{
		auto ChannelData = Channel->GetData();
		const int32 KeyIndex = ChannelData.GetIndex(InHandle);

		if (KeyIndex != INDEX_NONE)
		{
			KeyStruct = MakeShared<FStructOnScope>(KeyStructType::StaticStruct());
			KeyStructType* Struct = reinterpret_cast<KeyStructType*>(KeyStruct->GetStructMemory());

			Struct->Time  = ChannelData.GetTimes()[KeyIndex];
			Struct->Value = ChannelData.GetValues()[KeyIndex];

			Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelHandle, &Struct->Value, MakeTuple(InHandle, Struct->Time)));
		}
	}
	return KeyStruct;
}