// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Containers/Array.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"

#include "MovieSceneKeyStruct.h"
#include "SequencerChannelTraits.h"
#include "Channels/MovieSceneChannelHandle.h"

#include "Channels/MovieSceneBoolChannel.h"
#include "Channels/MovieSceneByteChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "Channels/MovieSceneEventChannel.h"
#include "Sections/MovieSceneStringSection.h"
#include "Sections/MovieSceneParticleSection.h"
#include "Sections/MovieSceneActorReferenceSection.h"

#include "BuiltInChannelEditors.generated.h"

struct FKeyHandle;
struct FKeyDrawParams;

class SWidget;
class ISequencer;
class FMenuBuilder;
class FStructOnScope;
class ISectionLayoutBuilder;

template<typename> class  TArrayView;

/** Overrides for adding or updating a key for non-standard channels */
FKeyHandle AddOrUpdateKey(FMovieSceneFloatChannel* Channel, const TMovieSceneExternalValue<float>& EditorData, FFrameNumber InTime, ISequencer& Sequencer, const FGuid& InObjectBindingID, FTrackInstancePropertyBindings* PropertyBindings);
FKeyHandle AddOrUpdateKey(FMovieSceneActorReferenceData* Channel, FFrameNumber InTime, ISequencer& Sequencer, const FGuid& InObjectBindingID, FTrackInstancePropertyBindings* PropertyBindings);

/** Key editor overrides */
bool CanCreateKeyEditor(const FMovieSceneBoolChannel*    Channel);
bool CanCreateKeyEditor(const FMovieSceneByteChannel*    Channel);
bool CanCreateKeyEditor(const FMovieSceneIntegerChannel* Channel);
bool CanCreateKeyEditor(const FMovieSceneFloatChannel*   Channel);
bool CanCreateKeyEditor(const FMovieSceneStringChannel*  Channel);

TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneBoolChannel>&    Channel, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> InSequencer);
TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneByteChannel>&    Channel, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> InSequencer);
TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneIntegerChannel>& Channel, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> InSequencer);
TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneFloatChannel>&   Channel, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> InSequencer);
TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneStringChannel>&  Channel, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> InSequencer);

/** KeyStruct overrides */
TSharedPtr<FStructOnScope> GetKeyStruct(const TMovieSceneChannelHandle<FMovieSceneBoolChannel>&     Channel, FKeyHandle InHandle);
TSharedPtr<FStructOnScope> GetKeyStruct(const TMovieSceneChannelHandle<FMovieSceneByteChannel>&     Channel, FKeyHandle InHandle);
TSharedPtr<FStructOnScope> GetKeyStruct(const TMovieSceneChannelHandle<FMovieSceneIntegerChannel>&  Channel, FKeyHandle InHandle);
TSharedPtr<FStructOnScope> GetKeyStruct(const TMovieSceneChannelHandle<FMovieSceneStringChannel>&   Channel, FKeyHandle InHandle);
TSharedPtr<FStructOnScope> GetKeyStruct(const TMovieSceneChannelHandle<FMovieSceneParticleChannel>& Channel, FKeyHandle InHandle);

/** Key drawing overrides */
void DrawKeys(FMovieSceneFloatChannel*    Channel, TArrayView<const FKeyHandle> InKeyHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams);
void DrawKeys(FMovieSceneParticleChannel* Channel, TArrayView<const FKeyHandle> InKeyHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams);
void DrawKeys(FMovieSceneEventChannel*    Channel, TArrayView<const FKeyHandle> InKeyHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams);

/** Context menu overrides */
void ExtendSectionMenu(FMenuBuilder& OuterMenuBuilder, TArray<TMovieSceneChannelHandle<FMovieSceneFloatChannel>>&& Channels, TArrayView<UMovieSceneSection* const> Sections, TWeakPtr<ISequencer> InSequencer);
void ExtendKeyMenu(FMenuBuilder& OuterMenuBuilder, TArray<TExtendKeyMenuParams<FMovieSceneFloatChannel>>&& Channels, TWeakPtr<ISequencer> InSequencer);

/** Curve editor models */
TUniquePtr<FCurveModel> CreateCurveEditorModel(const TMovieSceneChannelHandle<FMovieSceneFloatChannel>& FloatChannel, UMovieSceneSection* OwningSection, TSharedRef<ISequencer> InSequencer);

USTRUCT()
struct FMovieSceneIntegerKeyStruct : public FMovieSceneKeyTimeStruct
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, Category="Key")
	int32 Value;
};
template<> struct TStructOpsTypeTraits<FMovieSceneIntegerKeyStruct> : public TStructOpsTypeTraitsBase2<FMovieSceneIntegerKeyStruct> { enum { WithCopy = false }; };


USTRUCT()
struct FMovieSceneByteKeyStruct : public FMovieSceneKeyTimeStruct
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, Category="Key")
	uint8 Value;
};
template<> struct TStructOpsTypeTraits<FMovieSceneByteKeyStruct> : public TStructOpsTypeTraitsBase2<FMovieSceneByteKeyStruct> { enum { WithCopy = false }; };


USTRUCT()
struct FMovieSceneBoolKeyStruct : public FMovieSceneKeyTimeStruct
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, Category="Key")
	bool Value;
};
template<> struct TStructOpsTypeTraits<FMovieSceneBoolKeyStruct> : public TStructOpsTypeTraitsBase2<FMovieSceneBoolKeyStruct> { enum { WithCopy = false }; };


USTRUCT()
struct FMovieSceneStringKeyStruct : public FMovieSceneKeyTimeStruct
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, Category="Key")
	FString Value;
};
template<> struct TStructOpsTypeTraits<FMovieSceneStringKeyStruct> : public TStructOpsTypeTraitsBase2<FMovieSceneStringKeyStruct> { enum { WithCopy = false }; };


USTRUCT()
struct FMovieSceneParticleKeyStruct : public FMovieSceneKeyTimeStruct
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, Category="Key")
	EParticleKey Value;
};
template<> struct TStructOpsTypeTraits<FMovieSceneParticleKeyStruct> : public TStructOpsTypeTraitsBase2<FMovieSceneParticleKeyStruct> { enum { WithCopy = false }; };

