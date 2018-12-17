// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SSequencerDebugVisualizer.h"
#include "CommonMovieSceneTools.h"
#include "EditorStyleSet.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/ArrangedChildren.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "MovieSceneTimeHelpers.h"

#define LOCTEXT_NAMESPACE "SSequencerDebugVisualizer"

void SSequencerDebugVisualizer::Construct(const FArguments& InArgs, TSharedRef<FSequencer> InSequencer)
{
	WeakSequencer = InSequencer;

	SetClipping(EWidgetClipping::ClipToBounds);

	ViewRange = InArgs._ViewRange;

	Refresh();
}

const FMovieSceneEvaluationTemplate* SSequencerDebugVisualizer::GetTemplate() const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	return Sequencer.IsValid() ? Sequencer->GetEvaluationTemplate().FindTemplate(Sequencer->GetFocusedTemplateID()) : nullptr;
}

void SSequencerDebugVisualizer::Refresh()
{
	Children.Empty();

	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	const UMovieSceneSequence*           ActiveSequence = Sequencer->GetFocusedMovieSceneSequence();
	const FMovieSceneEvaluationTemplate* ActiveTemplate = GetTemplate();
	if (!ActiveTemplate || !ActiveSequence)
	{
		return;
	}

	const FFrameRate                  SequenceResolution = ActiveSequence->GetMovieScene()->GetTickResolution();
	const FMovieSceneEvaluationField& EvaluationField    = ActiveTemplate->EvaluationField;

	CachedSignature = EvaluationField.GetSignature();

	TArray<int32> SegmentComplexity;
	SegmentComplexity.Reserve(EvaluationField.Size());

	// Heatmap complexity
	float AverageComplexity = 0;
	int32 MaxComplexity = 0;

	for (int32 Index = 0; Index < EvaluationField.Size(); ++Index)
	{
		const FMovieSceneEvaluationGroup& Group = EvaluationField.GetGroup(Index);

		int32 Complexity = 0;
		for (const FMovieSceneEvaluationGroupLUTIndex& LUTIndex : Group.LUTIndices)
		{
			// more groups is more complex
			Complexity += 1;
			// Add total init and eval complexity
			Complexity += LUTIndex.NumInitPtrs + LUTIndex.NumEvalPtrs;
		}

		SegmentComplexity.Add(Complexity);
		MaxComplexity = FMath::Max(MaxComplexity, Complexity);
		AverageComplexity += Complexity;
	}

	AverageComplexity /= SegmentComplexity.Num();

	static const FSlateBrush* SectionBackgroundBrush = FEditorStyle::GetBrush("Sequencer.Section.Background");
	static const FSlateBrush* SectionBackgroundTintBrush = FEditorStyle::GetBrush("Sequencer.Section.BackgroundTint");

	for (int32 Index = 0; Index < EvaluationField.Size(); ++Index)
	{
		TRange<FFrameNumber> Range = EvaluationField.GetRange(Index);

		const float Complexity = SegmentComplexity[Index];
		
		float Lerp = FMath::Clamp((Complexity - AverageComplexity) / (MaxComplexity - AverageComplexity), 0.f, 1.f) * 0.5f +
			FMath::Clamp(Complexity / AverageComplexity, 0.f, 1.f) * 0.5f;

		// Blend from blue (240deg) to red (0deg)
		FLinearColor ComplexityColor = FLinearColor(FMath::Lerp(240.f, 0.f, FMath::Clamp(Lerp, 0.f, 1.f)), 1.f, 1.f, 0.5f).HSVToLinearRGB();

		Children.Add(
			SNew(SSequencerDebugSlot, Index)
			.Visibility(this, &SSequencerDebugVisualizer::GetSegmentVisibility, Range / SequenceResolution)
			.ToolTip(
				SNew(SToolTip)
				[
					GetTooltipForSegment(Index)
				]
			)
			[
				SNew(SBorder)
				.BorderImage(SectionBackgroundBrush)
				.Padding(FMargin(1.f))
				.OnMouseButtonUp(this, &SSequencerDebugVisualizer::OnSlotMouseButtonUp, Index)
				[
					SNew(SBorder)
					.BorderImage(SectionBackgroundTintBrush)
					.BorderBackgroundColor(ComplexityColor)
					.ForegroundColor(FLinearColor::Black)
					[
						SNew(STextBlock)
						.Text(FText::AsNumber(Index))
					]
				]
			]
		);
	}
}

FVector2D SSequencerDebugVisualizer::ComputeDesiredSize(float) const
{
	// Note: X Size is not used
	return FVector2D(100, 20.f);
}

FGeometry SSequencerDebugVisualizer::GetSegmentGeometry(const FGeometry& AllottedGeometry, const SSequencerDebugSlot& Slot, const FTimeToPixel& TimeToPixelConverter) const
{
	const FMovieSceneEvaluationTemplate* ActiveTemplate = GetTemplate();
	if (!ActiveTemplate || ActiveTemplate->EvaluationField.GetSignature() != CachedSignature)
	{
		return AllottedGeometry.MakeChild(FVector2D(0,0), FVector2D(0,0));
	}

	TRange<FFrameNumber> SegmentRange = ActiveTemplate->EvaluationField.GetRange(Slot.GetSegmentIndex());

	float PixelStartX = SegmentRange.GetLowerBound().IsOpen() ? 0.f : TimeToPixelConverter.FrameToPixel(MovieScene::DiscreteInclusiveLower(SegmentRange));
	float PixelEndX   = SegmentRange.GetUpperBound().IsOpen() ? AllottedGeometry.GetLocalSize().X : TimeToPixelConverter.FrameToPixel(MovieScene::DiscreteExclusiveUpper(SegmentRange));

	const float MinSectionWidth = 0.f;
	float SectionLength = FMath::Max(MinSectionWidth, PixelEndX - PixelStartX);

	return AllottedGeometry.MakeChild(
		FVector2D(PixelStartX, 0),
		FVector2D(SectionLength, FMath::Max(Slot.GetDesiredSize().Y, 20.f))
		);
}

EVisibility SSequencerDebugVisualizer::GetSegmentVisibility(TRange<double> Range) const
{
	return ViewRange.Get().Overlaps(Range) ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SSequencerDebugVisualizer::GetTooltipForSegment(int32 SegmentIndex) const
{
	const FMovieSceneEvaluationTemplate* ActiveTemplate = GetTemplate();
	if (!ActiveTemplate)
	{
		return SNullWidget::NullWidget;
	}

	const FMovieSceneEvaluationGroup& Group = ActiveTemplate->EvaluationField.GetGroup(SegmentIndex);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	int32 NumInitEntities = 0;
	for (int32 Index = 0; Index < Group.LUTIndices.Num(); ++Index)
	{
		VerticalBox->AddSlot()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("EvalGroupFormat", "Evaluation Group {0}:"), Index))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::Format(
					LOCTEXT("EvalTrackFormat", "{0} initialization steps, {1} evaluation steps"),
					FText::AsNumber(Group.LUTIndices[Index].NumInitPtrs),
					FText::AsNumber(Group.LUTIndices[Index].NumEvalPtrs)
				))
			]
		];
	}

	return VerticalBox;
}

void SSequencerDebugVisualizer::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SPanel::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const FMovieSceneEvaluationTemplate* Template = GetTemplate();
	if (!Template)
	{
		Children.Empty();
	}
	else if (Template->EvaluationField.GetSignature() != CachedSignature)
	{
		Refresh();
	}
}

void SSequencerDebugVisualizer::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	TSharedPtr<FSequencer>     Sequencer      = WeakSequencer.Pin();
	const UMovieSceneSequence* ActiveSequence = Sequencer.IsValid() ? Sequencer->GetFocusedMovieSceneSequence() : nullptr;

	if (!ActiveSequence)
	{
		return;
	}

	FTimeToPixel TimeToPixelConverter = FTimeToPixel(AllottedGeometry, ViewRange.Get(), ActiveSequence->GetMovieScene()->GetTickResolution());

	for (int32 WidgetIndex = 0; WidgetIndex < Children.Num(); ++WidgetIndex)
	{
		const TSharedRef<SSequencerDebugSlot>& Child = Children[WidgetIndex];

		EVisibility WidgetVisibility = Child->GetVisibility();
		if( ArrangedChildren.Accepts( WidgetVisibility ) )
		{
			FGeometry SegmentGeometry = GetSegmentGeometry(AllottedGeometry, *Child, TimeToPixelConverter);
			if (SegmentGeometry.GetLocalSize().X >= 1)
			{
				ArrangedChildren.AddWidget( 
					WidgetVisibility, 
					AllottedGeometry.MakeChild(Child, SegmentGeometry.Position, SegmentGeometry.GetLocalSize())
					);
			}
		}
	}
}

FReply SSequencerDebugVisualizer::OnSlotMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 SlotIndex)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("InvalidateSegment", "Invalidate Segment"),
		FText(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SSequencerDebugVisualizer::InvalidateSegment, SlotIndex))
	);

	int32 IndexNone = INDEX_NONE;

	MenuBuilder.AddMenuEntry(
		LOCTEXT("InvalidateAll", "Invalidate All"),
		FText(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SSequencerDebugVisualizer::InvalidateSegment, IndexNone))
	);

	FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
	FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuBuilder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

	return FReply::Handled();
}

void SSequencerDebugVisualizer::InvalidateSegment(int32 SlotIndex)
{
	TSharedPtr<FSequencer> Sequencer = this->WeakSequencer.Pin();
	FMovieSceneEvaluationTemplate* Template = Sequencer.IsValid() ? Sequencer->GetEvaluationTemplate().FindTemplate(Sequencer->GetFocusedTemplateID()) : nullptr;

	if (Template)
	{
		if (SlotIndex == INDEX_NONE)
		{
			Template->EvaluationField.Invalidate(TRange<FFrameNumber>::All());
		}
		else if (SlotIndex < Template->EvaluationField.Size())
		{
			Template->EvaluationField.Invalidate(Template->EvaluationField.GetRange(SlotIndex));
		}
	}
}

#undef LOCTEXT_NAMESPACE
