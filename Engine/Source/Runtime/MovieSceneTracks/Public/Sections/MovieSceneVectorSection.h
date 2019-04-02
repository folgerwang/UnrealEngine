// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "MovieSceneSection.h"
#include "MovieSceneKeyStruct.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneVectorSection.generated.h"

class FStructOnScope;
struct FPropertyChangedEvent;

/**
* Base Proxy structure for vector section key data.
*/
USTRUCT()
struct FMovieSceneVectorKeyStructBase
	: public FMovieSceneKeyStruct
{
	GENERATED_BODY();

	/** The key's time. */
	UPROPERTY(EditAnywhere, Category=Key)
	FFrameNumber Time;

	FMovieSceneKeyStructHelper KeyStructInterop;

	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent) override;

	/** Gets a ptr value of a channel by index, 0-3 = x-w */
	virtual float* GetPropertyChannelByIndex(int32 Index) PURE_VIRTUAL(FMovieSceneVectorKeyStructBase::GetPropertyChannelByIndex, return nullptr; );
};
template<> struct TStructOpsTypeTraits<FMovieSceneVectorKeyStructBase> : public TStructOpsTypeTraitsBase2<FMovieSceneVectorKeyStructBase> { enum { WithCopy = false }; };


/**
 * Proxy structure for 2D vector section key data.
 */
USTRUCT()
struct FMovieSceneVector2DKeyStruct
	: public FMovieSceneVectorKeyStructBase
{
	GENERATED_BODY()

	/** They key's vector value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FVector2D Vector;

	//~ FMovieSceneVectorKeyStructBase interface
	virtual float* GetPropertyChannelByIndex(int32 Index) override { return &Vector[Index]; }
};
template<> struct TStructOpsTypeTraits<FMovieSceneVector2DKeyStruct> : public TStructOpsTypeTraitsBase2<FMovieSceneVector2DKeyStruct> { enum { WithCopy = false }; };

/**
* Proxy structure for vector section key data.
*/
USTRUCT()
struct FMovieSceneVectorKeyStruct
	: public FMovieSceneVectorKeyStructBase
{
	GENERATED_BODY()

	/** They key's vector value. */
	UPROPERTY(EditAnywhere, Category = Key)
	FVector Vector;

	//~ FMovieSceneVectorKeyStructBase interface
	virtual float* GetPropertyChannelByIndex(int32 Index) override { return &Vector[Index]; }
};
template<> struct TStructOpsTypeTraits<FMovieSceneVectorKeyStruct> : public TStructOpsTypeTraitsBase2<FMovieSceneVectorKeyStruct> { enum { WithCopy = false }; };

/**
* Proxy structure for vector4 section key data.
*/
USTRUCT()
struct FMovieSceneVector4KeyStruct
	: public FMovieSceneVectorKeyStructBase
{
	GENERATED_BODY()

	/** They key's vector value. */
	UPROPERTY(EditAnywhere, Category = Key)
	FVector4 Vector;

	//~ FMovieSceneVectorKeyStructBase interface
	virtual float* GetPropertyChannelByIndex(int32 Index) override { return &Vector[Index]; }
};
template<> struct TStructOpsTypeTraits<FMovieSceneVector4KeyStruct> : public TStructOpsTypeTraitsBase2<FMovieSceneVector4KeyStruct> { enum { WithCopy = false }; };


/**
 * A vector section.
 */
UCLASS(MinimalAPI)
class UMovieSceneVectorSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/** Sets how many channels are to be used */
	void SetChannelsUsed(int32 InChannelsUsed) 
	{
		checkf(InChannelsUsed >= 2 && InChannelsUsed <= 4, TEXT("Only 2-4 channels are supported."));
		ChannelsUsed = InChannelsUsed;
		RecreateChannelProxy();
	}

	/** Gets the number of channels in use */
	int32 GetChannelsUsed() const { return ChannelsUsed; }

	/**
	 * Public access to this section's internal data function
	 */
	const FMovieSceneFloatChannel& GetChannel(int32 Index) const
	{
		check(Index >= 0 && Index < GetChannelsUsed());
		return Curves[Index];
	}

protected:

	//~ UMovieSceneSection interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditImport() override;
	virtual TSharedPtr<FStructOnScope> GetKeyStruct(TArrayView<const FKeyHandle> KeyHandles) override;

	MOVIESCENETRACKS_API void RecreateChannelProxy();

private:

	/** Float functions for the X,Y,Z,W components of the vector */
	UPROPERTY()
	FMovieSceneFloatChannel Curves[4];

	/** How many curves are actually used */
	UPROPERTY()
	int32 ChannelsUsed;
};
