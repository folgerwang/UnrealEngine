// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"

#include "MovieSceneMediaSection.generated.h"

class UMediaPlayer;
class UMediaSoundComponent;
class UMediaSource;
class UMediaTexture;


/**
 * Implements a movie scene section for media playback.
 */
UCLASS(MinimalAPI)
class UMovieSceneMediaSection
	: public UMovieSceneSection
{
public:

	GENERATED_BODY()


public:
	/** The source to play with this video track. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media")
	UMediaSource* MediaSource;

	/** Should the media player be set to loop? This can be helpful for media formats that can use this information (such as exr sequences) to pre-cache the starting data when nearing the end of playback. Does not cause the media to continue playing after the end of the section is reached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
	bool bLooping;

	/** Offset into the source media. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media")
	FFrameNumber StartFrameOffset;

	/** The media texture that receives the track's video output. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media", meta = (EditCondition = "!bUseExternalMediaPlayer"))
	UMediaTexture* MediaTexture;

	/** The media sound component that receives the track's audio output. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media")
	UMediaSoundComponent* MediaSoundComponent;

	/** If true, this track will control a previously created media player instead of automatically creating one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media", AdvancedDisplay)
	bool bUseExternalMediaPlayer;

	/** The external media player this track should control. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media", meta = (EditCondition = "bUseExternalMediaPlayer"), AdvancedDisplay)
	UMediaPlayer* ExternalMediaPlayer;

public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param ObjectInitializer The object initializer.
	 */
	UMovieSceneMediaSection(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitProperties() override;

public:

	/**
	 * Get this section's video source.
	 *
	 * @return The media source object.
	 * @see SetMediaSource
	 */
	UMediaSource* GetMediaSource() const
	{
		return MediaSource;
	}

	/**
	 * Set this section's video source.
	 *
	 * @param InMediaSource The media source object to set.
	 * @see GetMediaSource
	 */
	void SetMediaSource(UMediaSource* InMediaSource)
	{
		MediaSource = InMediaSource;
	}

#if WITH_EDITORONLY_DATA

	/** @return The thumbnail reference frame offset from the start of this section */
	float GetThumbnailReferenceOffset() const
	{
		return ThumbnailReferenceOffset;
	}

	/** Set the thumbnail reference offset */
	void SetThumbnailReferenceOffset(float InNewOffset)
	{
		Modify();
		ThumbnailReferenceOffset = InNewOffset;
	}

#endif //WITH_EDITORONLY_DATA

#if WITH_EDITORONLY_DATA

	/** The reference frame offset for single thumbnail rendering */
	UPROPERTY()
	float ThumbnailReferenceOffset;

#endif //WITH_EDITORONLY_DATA
};
