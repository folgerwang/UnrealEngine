// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MediaThumbnailSection.h"

#include "EditorStyleSet.h"
#include "IMediaCache.h"
#include "IMediaTracks.h"
#include "ISequencer.h"
#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "MovieScene.h"
#include "MovieSceneMediaSection.h"
#include "MovieSceneTimeHelpers.h"
#include "Rendering/DrawElements.h"
#include "SequencerSectionPainter.h"
#include "TrackEditorThumbnail/TrackEditorThumbnailPool.h"
#include "CommonMovieSceneTools.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#include "MovieSceneMediaData.h"


#define LOCTEXT_NAMESPACE "FMediaThumbnailSection"


/* FMediaThumbnailSection structors
 *****************************************************************************/

FMediaThumbnailSection::FMediaThumbnailSection(UMovieSceneMediaSection& InSection, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, TSharedPtr<ISequencer> InSequencer)
	: FThumbnailSection(InSequencer, InThumbnailPool, this, InSection)
//	, MediaPlayer(nullptr)
//	, MediaTexture(nullptr)
	, SectionPtr(&InSection)
	, SequencerPtr(InSequencer)
{
	TimeSpace = ETimeSpace::Local;
}


FMediaThumbnailSection::~FMediaThumbnailSection()
{
//	if (MediaPlayer != nullptr)
//	{
//		MediaPlayer->Close();
//	}
}


/* FGCObject interface
 *****************************************************************************/

void FMediaThumbnailSection::AddReferencedObjects(FReferenceCollector& Collector)
{
//	Collector.AddReferencedObject(MediaPlayer);
//	Collector.AddReferencedObject(MediaTexture);
}


/* FThumbnailSection interface
 *****************************************************************************/

FMargin FMediaThumbnailSection::GetContentPadding() const
{
	return FMargin(8.0f, 15.0f);
}


float FMediaThumbnailSection::GetSectionHeight() const
{
	return FThumbnailSection::GetSectionHeight() + 2 * 9.0f; // make space for the film border
}


FText FMediaThumbnailSection::GetSectionTitle() const
{
	UMovieSceneMediaSection* MediaSection = CastChecked<UMovieSceneMediaSection>(Section);
	UMediaSource* MediaSource = MediaSection->GetMediaSource();

	if (MediaSource == nullptr)
	{
		return LOCTEXT("NoSequence", "Empty");
	}

	return FText::FromString(MediaSource->GetFName().ToString());
}


int32 FMediaThumbnailSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	// draw background
	InPainter.LayerId = InPainter.PaintSectionBackground();

	FVector2D SectionSize = InPainter.SectionGeometry.GetLocalSize();
	FSlateClippingZone ClippingZone(InPainter.SectionClippingRect.InsetBy(FMargin(1.0f)));
	
	InPainter.DrawElements.PushClip(ClippingZone);
	{
		DrawFilmBorder(InPainter, SectionSize);
	}
	InPainter.DrawElements.PopClip();

	// draw thumbnails
	int32 LayerId = FThumbnailSection::OnPaintSection(InPainter) + 1;

	UMediaPlayer* MediaPlayer = GetTemplateMediaPlayer();

	if (MediaPlayer == nullptr)
	{
		return LayerId;
	}

	// draw overlays
	const FTimespan MediaDuration = MediaPlayer->GetDuration();

	if (MediaDuration.IsZero())
	{
		return LayerId;
	}

	TSharedRef<FMediaPlayerFacade, ESPMode::ThreadSafe> MediaPlayerFacade = MediaPlayer->GetPlayerFacade();

	InPainter.DrawElements.PushClip(ClippingZone);
	{
		TRangeSet<FTimespan> CacheRangeSet;

		MediaPlayerFacade->QueryCacheState(EMediaTrackType::Video, EMediaCacheState::Pending, CacheRangeSet);
		DrawSampleStates(InPainter, MediaDuration, SectionSize, CacheRangeSet, FLinearColor::Gray);

		CacheRangeSet.Empty();

		MediaPlayerFacade->QueryCacheState(EMediaTrackType::Video, EMediaCacheState::Loading, CacheRangeSet);
		DrawSampleStates(InPainter, MediaDuration, SectionSize, CacheRangeSet, FLinearColor::Yellow);

		CacheRangeSet.Empty();

		MediaPlayerFacade->QueryCacheState(EMediaTrackType::Video, EMediaCacheState::Loaded, CacheRangeSet);
		DrawSampleStates(InPainter, MediaDuration, SectionSize, CacheRangeSet, FLinearColor(0.10616, 0.48777, 0.10616));

		CacheRangeSet.Empty();

		MediaPlayerFacade->QueryCacheState(EMediaTrackType::Video, EMediaCacheState::Cached, CacheRangeSet);
		DrawSampleStates(InPainter, MediaDuration, SectionSize, CacheRangeSet, FLinearColor(0.07059, 0.32941, 0.07059));

		DrawLoopIndicators(InPainter, MediaDuration, SectionSize);
	}
	InPainter.DrawElements.PopClip();

	return LayerId;
}


void FMediaThumbnailSection::SetSingleTime(double GlobalTime)
{
	UMovieSceneMediaSection* MediaSection = CastChecked<UMovieSceneMediaSection>(Section);

	if (MediaSection != nullptr)
	{
		double StartTime = MediaSection->GetInclusiveStartFrame() / MediaSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
		MediaSection->SetThumbnailReferenceOffset(GlobalTime - StartTime);
	}
}


void FMediaThumbnailSection::Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);

	if (MediaSection != nullptr)
	{
		if (GetDefault<UMovieSceneUserThumbnailSettings>()->bDrawSingleThumbnails)
		{
			ThumbnailCache.SetSingleReferenceFrame(MediaSection->GetThumbnailReferenceOffset());
		}
		else
		{
			ThumbnailCache.SetSingleReferenceFrame(TOptional<double>());
		}
	}

	FThumbnailSection::Tick(AllottedGeometry, ClippedGeometry, InCurrentTime, InDeltaTime);
}

void FMediaThumbnailSection::BeginResizeSection()
{
	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);
	InitialStartOffsetDuringResize = MediaSection->StartFrameOffset;
	InitialStartTimeDuringResize = MediaSection->HasStartFrame() ? MediaSection->GetInclusiveStartFrame() : 0;
}

void FMediaThumbnailSection::ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime)
{
	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);

	if (ResizeMode == SSRM_LeadingEdge && MediaSection)
	{
		FFrameNumber StartOffset = ResizeTime - InitialStartTimeDuringResize;
		StartOffset += InitialStartOffsetDuringResize;

		// Ensure start offset is not less than 0
		if (StartOffset < 0)
		{
			ResizeTime = ResizeTime - StartOffset;

			StartOffset = FFrameNumber(0);
		}

		MediaSection->StartFrameOffset = StartOffset;
	}

	ISequencerSection::ResizeSection(ResizeMode, ResizeTime);
}

void FMediaThumbnailSection::BeginSlipSection()
{
	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);
	InitialStartOffsetDuringResize = MediaSection->StartFrameOffset;
	InitialStartTimeDuringResize = MediaSection->HasStartFrame() ? MediaSection->GetInclusiveStartFrame() : 0;
}

void FMediaThumbnailSection::SlipSection(FFrameNumber SlipTime)
{
	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);

	const FFrameRate FrameRate = MediaSection->GetTypedOuter<UMovieScene>()->GetTickResolution();

	FFrameNumber StartOffset = SlipTime - InitialStartTimeDuringResize;
	StartOffset += InitialStartOffsetDuringResize;

	// Ensure start offset is not less than 0
	if (StartOffset < 0)
	{
		SlipTime = SlipTime - StartOffset;

		StartOffset = FFrameNumber(0);
	}

	MediaSection->StartFrameOffset = StartOffset;

	ISequencerSection::SlipSection(SlipTime);
}

/* ICustomThumbnailClient interface
 *****************************************************************************/

void FMediaThumbnailSection::Draw(FTrackEditorThumbnail& TrackEditorThumbnail)
{/*
	check(MediaPlayer != nullptr);

	if (MediaPlayer->IsPreparing())
	{
		RedrawThumbnails();

		return;
	}

	check(MediaTexture != nullptr);
	check(MediaTexture->Resource != nullptr);
	check(MediaTexture->Resource->TextureRHI.IsValid());

	// get target texture resource
	FTexture2DRHIRef Texture2DRHI = MediaTexture->Resource->TextureRHI->GetTexture2D();

	if (!Texture2DRHI.IsValid())
	{
		return;
	}

	// seek media player to thumbnail position
	const float EvalPosition = FMath::Max(0.0f, TrackEditorThumbnail.GetEvalPosition());
	const FTimespan EvalTime = int64(EvalPosition * ETimespan::TicksPerSecond);
	const FTimespan MediaTime = EvalTime % MediaPlayer->GetDuration();

	if (!MediaPlayer->Seek(FTimespan(MediaTime)))
	{
		return;
	}

	// resolve media player texture to track editor thumbnail
	TrackEditorThumbnail.CopyTextureIn(Texture2DRHI);

	TSharedPtr<ISequencer> Sequencer = SequencerPtr.Pin();

	if (Sequencer.IsValid())
	{
		TrackEditorThumbnail.SetupFade(Sequencer->GetSequencerWidget());
	}*/
}


void FMediaThumbnailSection::Setup()
{/*
	UMovieSceneMediaSection* MediaSection = CastChecked<UMovieSceneMediaSection>(Section);
	UMediaSource* MediaSource = MediaSection->GetMediaSource();

	if (MediaSource == nullptr)
	{
		return;
	}

	// create internal player
	if (MediaPlayer == nullptr)
	{
		MediaPlayer = NewObject<UMediaPlayer>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UMediaPlayer::StaticClass()));
	}

	// create target texture
	if (MediaTexture == nullptr)
	{
		MediaTexture = NewObject<UMediaTexture>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UMediaTexture::StaticClass()));
		MediaTexture->SetMediaPlayer(MediaPlayer);
		MediaTexture->UpdateResource();
	}

	// open latest media source
	if (MediaPlayer->GetUrl() != MediaSource->GetUrl())
	{
		MediaPlayer->OpenSource(MediaSource);
	}

	MediaPlayer->Pause();*/
}


/* FMediaThumbnailSection implementation
 *****************************************************************************/

void FMediaThumbnailSection::DrawFilmBorder(FSequencerSectionPainter& InPainter, FVector2D SectionSize) const
{
	static const FSlateBrush* FilmBorder = FEditorStyle::GetBrush("Sequencer.Section.FilmBorder");

	// draw top film border
	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(SectionSize.X - 2.0f, 7.0f), FSlateLayoutTransform(FVector2D(1.0f, 4.0f))),
		FilmBorder,
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);

	// draw bottom film border
	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(SectionSize.X - 2.0f, 7.0f), FSlateLayoutTransform(FVector2D(1.0f, SectionSize.Y - 11.0f))),
		FilmBorder,
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);
}


void FMediaThumbnailSection::DrawLoopIndicators(FSequencerSectionPainter& InPainter, FTimespan MediaDuration, FVector2D SectionSize) const
{
	/*
	static const FSlateBrush* GenericBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");

	FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
	double SectionDuration = FFrameTime(MovieScene::DiscreteSize(Section->GetRange())) / TickResolution;
	const float MediaSizeX = MediaDuration.GetTotalSeconds() * SectionSize.X / SectionDuration;
	float DrawOffset = 0.0f;

	while (DrawOffset < SectionSize.X)
	{
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(FVector2D(DrawOffset, 0.0f), FVector2D(1.0f, SectionSize.Y)),
			GenericBrush,
			ESlateDrawEffect::None,
			FLinearColor::Gray
		);

		DrawOffset += MediaSizeX;
	}
	*/
	static const FSlateBrush* GenericBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");

	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);

	const FTimeToPixel& TimeToPixelConverter = InPainter.GetTimeConverter();

	FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
	double SectionDuration = FFrameTime(MovieScene::DiscreteSize(Section->GetRange())) / TickResolution;
	const float MediaSizeX = MediaDuration.GetTotalSeconds() * SectionSize.X / SectionDuration;
	float DrawOffset = MediaSizeX - TimeToPixelConverter.SecondsToPixel(TickResolution.AsSeconds(MediaSection->StartFrameOffset));

	while (DrawOffset < SectionSize.X)
	{
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(FVector2D(DrawOffset, 0.0f), FVector2D(1.0f, SectionSize.Y)),
			GenericBrush,
			ESlateDrawEffect::None,
			FLinearColor::Gray
		);

		DrawOffset += MediaSizeX;
	}
}


void FMediaThumbnailSection::DrawSampleStates(FSequencerSectionPainter& InPainter, FTimespan MediaDuration, FVector2D SectionSize, const TRangeSet<FTimespan>& RangeSet, const FLinearColor& Color) const
{
	static const FSlateBrush* GenericBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");

	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);

	const FTimeToPixel& TimeToPixelConverter = InPainter.GetTimeConverter();

	FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
	double SectionDuration = FFrameTime(MovieScene::DiscreteSize(Section->GetRange())) / TickResolution;
	const float MediaSizeX = MediaDuration.GetTotalSeconds() * SectionSize.X / SectionDuration;

	TArray<TRange<FTimespan>> Ranges;
	RangeSet.GetRanges(Ranges);

	for (auto& Range : Ranges)
	{
		const float DrawOffset = FMath::RoundToNegativeInfinity(FTimespan::Ratio(Range.GetLowerBoundValue(), MediaDuration) * MediaSizeX) - TimeToPixelConverter.SecondsToPixel(TickResolution.AsSeconds(MediaSection->StartFrameOffset));
		const float DrawSize = FMath::RoundToPositiveInfinity(FTimespan::Ratio(Range.Size<FTimespan>(), MediaDuration) * MediaSizeX);
		const float BarHeight = 4.0f;

		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(FVector2D(DrawOffset, SectionSize.Y - BarHeight - 1.0f), FVector2D(DrawSize, BarHeight)),
			GenericBrush,
			ESlateDrawEffect::None,
			Color
		);
	}
}


UMediaPlayer* FMediaThumbnailSection::GetTemplateMediaPlayer() const
{
	// locate the track that evaluates this section
	if (!SectionPtr.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<ISequencer> Sequencer = SequencerPtr.Pin();

	if (!Sequencer.IsValid())
	{
		return nullptr; // no movie scene player
	}

	const FMovieSceneSequenceID SequenceId = Sequencer->GetFocusedTemplateID();
	FMovieSceneEvaluationTemplate* Template = Sequencer->GetEvaluationTemplate().FindTemplate(SequenceId);

	if (Template == nullptr)
	{
		return nullptr; // section template not found
	}

	auto OwnerTrack = Cast<UMovieSceneTrack>(SectionPtr->GetOuter());

	if (OwnerTrack == nullptr)
	{
		return nullptr; // media track not found
	}

	const FMovieSceneTrackIdentifier TrackIdentifier = Template->GetLedger().FindTrack(OwnerTrack->GetSignature());
	FMovieSceneEvaluationTrack* EvaluationTrack = Template->FindTrack(TrackIdentifier);

	if (EvaluationTrack == nullptr)
	{
		return nullptr; // evaluation track not found
	}

	FMovieSceneMediaData* MediaData = nullptr;

	// find the persistent data of the section being drawn
	TArrayView<FMovieSceneEvalTemplatePtr> Children = EvaluationTrack->GetChildTemplates();
	FPersistentEvaluationData PersistentData(*Sequencer.Get());

	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		if (Children[ChildIndex]->GetSourceSection() == SectionPtr)
		{
			FMovieSceneEvaluationKey SectionKey(SequenceId, TrackIdentifier, ChildIndex);
			PersistentData.SetSectionKey(SectionKey);
			MediaData = PersistentData.FindSectionData<FMovieSceneMediaData>();

			break;
		}
	}

	// get the template's media player
	if (MediaData == nullptr)
	{
		return nullptr; // section persistent data not found
	}

	return MediaData->GetMediaPlayer();
}


#undef LOCTEXT_NAMESPACE
