// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Layout/Visibility.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "ISequencerSection.h"
#include "Framework/Commands/UICommandList.h"
#include "ISequencerTrackEditor.h"
#include "KeyframeTrackEditor.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"

class AActor;
struct FAssetData;
class SHorizontalBox;

/**
 * Tools for animatable transforms
 */
class F3DTransformTrackEditor
	: public FKeyframeTrackEditor<UMovieScene3DTransformTrack>
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer	The sequencer instance to be used by this tool
	 */
	F3DTransformTrackEditor( TSharedRef<ISequencer> InSequencer );

	/** Virtual destructor. */
	virtual ~F3DTransformTrackEditor();

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

public:

	// ISequencerTrackEditor interface

	virtual void BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings) override;
	virtual void BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding ) override;
	virtual void OnRelease() override;
	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override;
	virtual void BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) override;

private:

	/** Returns whether or not a transform track can be added for an actor with a specific handle. */
	bool CanAddTransformTrackForActorHandle(FGuid ActorHandle) const;

	/** Whether the object has an existing transform track */
	bool HasTransformTrack( UObject& InObject ) const;

	/**
	 * Called before an actor or component transform changes
	 *
	 * @param Object The object whose transform is about to change
	 */
	void OnPreTransformChanged( UObject& InObject );

	/**
	 * Called when an actor or component transform changes
	 *
	 * @param Object The object whose transform has changed
	 */
	void OnTransformChanged( UObject& InObject );

	/** 
	 * Called before an actor or component property changes.
	 * Forward to OnPreTransformChanged if the property is transform related.
	 *
	 * @param InObject The object whose property is about to change
	 * @param InPropertyChain the property that is about to change
	 */
	void OnPrePropertyChanged(UObject* InObject, const class FEditPropertyChain& InPropertyChain);

	/** 
	 * Called before an actor or component property changes.
	 * Forward to OnTransformChanged if the property is transform related.
	 *
	 * @param InObject The object whose property is about to change
	 * @param InPropertyChangedEvent the property that changed
	 */
	void OnPostPropertyChanged(UObject* InObject, struct FPropertyChangedEvent& InPropertyChangedEvent);


	/** Delegate for camera button visible state */
	EVisibility IsCameraVisible(FGuid ObjectGuid) const;

	/** Delegate for camera button lock state */
	ECheckBoxState IsCameraLocked(FGuid ObjectGuid) const; 

	/** Delegate for locked camera button */
	void OnLockCameraClicked(ECheckBoxState CheckBoxState, FGuid ObjectGuid);

	/** Clear locked cameras */
	void ClearLockedCameras(AActor* LockedActor);

	/** Delegate for camera button lock tooltip */
	FText GetLockCameraToolTip(FGuid ObjectGuid) const; 

	/** Generates transform keys based on the last transform, the current transform, and other options. 
		One transform key is generated for each individual key to be added to the section. */
	void GetTransformKeys( const TOptional<FTransformData>& LastTransform, const FTransformData& CurrentTransform, EMovieSceneTransformChannel ChannelsToKey, FGeneratedTrackKeys& OutGeneratedKeys );

	/**
	* Adds transform tracks and keys to the selected objects in the level.
	*
	* @param Channel The transform channel to add keys for.
	*/
	void OnAddTransformKeysForSelectedObjects( EMovieSceneTransformChannel Channel );

	/** 
	 * Adds transform keys to an object represented by a handle.

	 * @param ObjectHandle The handle to the object to add keys to.
	 * @param ChannelToKey The channels to add keys to.
	 * @param KeyParams Parameters which control how the keys are added. 
	 */
	void AddTransformKeysForHandle( FGuid ObjectHandle, EMovieSceneTransformChannel ChannelToKey, ESequencerKeyMode KeyMode );

	/**
	* Adds transform keys to a specific object.

	* @param Object The object to add keys to.
	* @param ChannelToKey The channels to add keys to.
	* @param KeyParams Parameters which control how the keys are added.
	*/
	void AddTransformKeysForObject( UObject* Object, EMovieSceneTransformChannel ChannelToKey, ESequencerKeyMode KeyMode );

	/**
	* Adds keys to a specific actor.

	* @param LastTransform The last known transform of the actor if any.
	* @param CurrentTransform The current transform of the actor.
	* @param ChannelToKey The channels to add keys to.
	* @param KeyParams Parameters which control how the keys are added.
	*/
	void AddTransformKeys( UObject* ObjectToKey, const TOptional<FTransformData>& LastTransform, const FTransformData& CurrentTransform, EMovieSceneTransformChannel ChannelsToKey, ESequencerKeyMode KeyMode );

private:

	/** Import an animation sequence's root transforms into a transform section */
	static void ImportAnimSequenceTransforms(const FAssetData& Asset, TSharedRef<class ISequencer> Sequencer, UMovieScene3DTransformTrack* TransformTrack);

	/** Import an animation sequence's root transforms into a transform section */
	static void ImportAnimSequenceTransformsEnterPressed(const TArray<FAssetData>& Asset, TSharedRef<class ISequencer> Sequencer, UMovieScene3DTransformTrack* TransformTrack);

private:

	static FName TransformPropertyName;

	/** Mapping of objects to their existing transform data (for comparing against new transform data) */
	TMap< TWeakObjectPtr<UObject>, FTransformData > ObjectToExistingTransform;

private:
	/** 
	 * Modify the passed in Generated Keys by the current tracks values and weight at the passed in time.

	 * @param Object The handle to the object modify
	 * @param Track The track we are modifying
	 * @param SectionToKey The Sections Channels we will be modifiying
     * @param Time The Time at which to evaluate
	 * @param InOutGeneratedTrackKeys The Keys we need to modify. We change these values.
     * @param Weight The weight we need to modify the values by.
	 */
	virtual bool ModifyGeneratedKeysByCurrentAndWeight(UObject* Object, UMovieSceneTrack *Track, UMovieSceneSection* SectionToKey, FFrameNumber Time, FGeneratedTrackKeys& InOutGeneratedTotalKeys, float Weight) const override;

};
