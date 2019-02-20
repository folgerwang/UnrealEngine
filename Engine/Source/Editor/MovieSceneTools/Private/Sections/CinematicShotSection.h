// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Layout/Margin.h"
#include "Sections/ThumbnailSection.h"

class FCinematicShotTrackEditor;
class FMenuBuilder;
class FSequencerSectionPainter;
class FTrackEditorThumbnailPool;
class UMovieSceneCinematicShotSection;

/**
 * CinematicShot section, which paints and ticks the appropriate section.
 */
class FCinematicShotSection
	: public FViewportThumbnailSection
{
public:

	/** Create and initialize a new instance. */
	FCinematicShotSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection, TSharedPtr<FCinematicShotTrackEditor> InCinematicShotTrackEditor);

	/** Virtual destructor. */
	virtual ~FCinematicShotSection();

public:

	// ISequencerSection interface

	virtual void Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaintSection( FSequencerSectionPainter& Painter ) const override;
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding) override;
	virtual FReply OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent) override;
	virtual FText GetSectionTitle() const override;
	virtual float GetSectionHeight() const override;
	virtual FMargin GetContentPadding() const override;
	virtual void BeginResizeSection() override;
	virtual void ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime) override;
	virtual void BeginSlipSection() override;
	virtual void SlipSection(FFrameNumber SlipTime) override;
	virtual bool IsReadOnly() const override;

	// FThumbnail interface
	virtual void SetSingleTime(double GlobalTime) override;
	virtual FText HandleThumbnailTextBlockText() const override;
	virtual void HandleThumbnailTextBlockTextCommitted(const FText& NewThumbnailName, ETextCommit::Type CommitType) override;
	virtual UCameraComponent* GetViewCamera() override;

private:

	/** Add shot takes menu */
	void AddTakesMenu(FMenuBuilder& MenuBuilder);

private:

	/** The section we are visualizing */
	UMovieSceneCinematicShotSection& SectionObject;

	/** The cinematic shot track editor that contains this section */
	TWeakPtr<FCinematicShotTrackEditor> CinematicShotTrackEditor;

	/** Cached start offset value valid only during resize */
	FFrameNumber InitialStartOffsetDuringResize;

	/** Cached start time valid only during resize */
	FFrameNumber InitialStartTimeDuringResize;

	struct FCinematicSectionCache
	{
		FCinematicSectionCache(UMovieSceneCinematicShotSection* Section = nullptr);

		bool operator!=(const FCinematicSectionCache& RHS) const
		{
			return InnerFrameRate != RHS.InnerFrameRate || InnerFrameOffset != RHS.InnerFrameOffset || SectionStartFrame != RHS.SectionStartFrame || TimeScale != RHS.TimeScale;
		}

		FFrameRate   InnerFrameRate;
		FFrameNumber InnerFrameOffset;
		FFrameNumber SectionStartFrame;
		float        TimeScale;
	};

	/** Cached section thumbnail data */
	FCinematicSectionCache ThumbnailCacheData;
};
