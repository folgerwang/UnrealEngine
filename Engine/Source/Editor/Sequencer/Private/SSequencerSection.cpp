// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SSequencerSection.h"
#include "Rendering/DrawElements.h"
#include "EditorStyleSet.h"
#include "SequencerSelectionPreview.h"
#include "SequencerSettings.h"
#include "Editor.h"
#include "Sequencer.h"
#include "SequencerSectionPainter.h"
#include "MovieSceneSequence.h"
#include "CommonMovieSceneTools.h"
#include "ISequencerEditTool.h"
#include "ISequencerSection.h"
#include "ISequencerHotspot.h"
#include "SequencerHotspots.h"
#include "Widgets/SOverlay.h"
#include "SequencerObjectBindingNode.h"
#include "MovieScene.h"
#include "Fonts/FontCache.h"
#include "Framework/Application/SlateApplication.h"
#include "KeyDrawParams.h"
#include "MovieSceneTimeHelpers.h"
#include "Tracks/MovieScenePropertyTrack.h"

double SSequencerSection::SectionSelectionThrobEndTime = 0;
double SSequencerSection::KeySelectionThrobEndTime = 0;

const FSectionLayoutElement& SSequencerSection::FLayoutElementKeyFuncs::GetSetKey(const TPair<FSectionLayoutElement, TArray<FSequencerCachedKeys, TInlineAllocator<1>>>& Element)
{
	return Element.Key;
}

bool SSequencerSection::FLayoutElementKeyFuncs::Matches(const FSectionLayoutElement& A, const FSectionLayoutElement& B)
{
	if (A.GetDisplayNode() != B.GetDisplayNode())
	{
		return false;
	}
	TArrayView<const TSharedRef<IKeyArea>> KeyAreasA = A.GetKeyAreas(), KeyAreasB = B.GetKeyAreas();
	if (KeyAreasA.Num() != KeyAreasB.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < KeyAreasA.Num(); ++Index)
	{
		if (KeyAreasA[Index] != KeyAreasB[Index])
		{
			return false;
		}
	}
	return true;
}

uint32 SSequencerSection::FLayoutElementKeyFuncs::GetKeyHash(const FSectionLayoutElement& Key)
{
	uint32 Hash = GetTypeHash(Key.GetDisplayNode()) ;
	for (TSharedRef<IKeyArea> KeyArea : Key.GetKeyAreas())
	{
		Hash = HashCombine(GetTypeHash(KeyArea), Hash);
	}
	return Hash;
}

/** A point on an easing curve used for rendering */
struct FEasingCurvePoint
{
	FEasingCurvePoint(FVector2D InLocation, const FLinearColor& InPointColor) : Location(InLocation), Color(InPointColor) {}

	/** The location of the point (x=time, y=easing value [0-1]) */
	FVector2D Location;
	/** The color of the point */
	FLinearColor Color;
};

FTimeToPixel ConstructTimeConverterForSection(const FGeometry& InSectionGeometry, const UMovieSceneSection& InSection, FSequencer& InSequencer)
{
	TRange<double> ViewRange       = InSequencer.GetViewRange();

	FFrameRate     TickResolution  = InSection.GetTypedOuter<UMovieScene>()->GetTickResolution();
	double         LowerTime       = InSection.HasStartFrame() ? InSection.GetInclusiveStartFrame() / TickResolution : ViewRange.GetLowerBoundValue();
	double         UpperTime       = InSection.HasEndFrame()   ? InSection.GetExclusiveEndFrame()   / TickResolution : ViewRange.GetUpperBoundValue();

	return FTimeToPixel(InSectionGeometry, TRange<double>(LowerTime, UpperTime), TickResolution);
}


struct FSequencerSectionPainterImpl : FSequencerSectionPainter
{
	FSequencerSectionPainterImpl(FSequencer& InSequencer, UMovieSceneSection& InSection, FSlateWindowElementList& _OutDrawElements, const FGeometry& InSectionGeometry, const SSequencerSection& InSectionWidget)
		: FSequencerSectionPainter(_OutDrawElements, InSectionGeometry, InSection)
		, Sequencer(InSequencer)
		, SectionWidget(InSectionWidget)
		, TimeToPixelConverter(ConstructTimeConverterForSection(SectionGeometry, InSection, Sequencer))
	{
		CalculateSelectionColor();

		const ISequencerEditTool* EditTool = InSequencer.GetEditTool();
		Hotspot = EditTool ? EditTool->GetDragHotspot() : nullptr;
		if (!Hotspot)
		{
			Hotspot = Sequencer.GetHotspot().Get();
		}
	}

	FLinearColor GetFinalTintColor(const FLinearColor& Tint) const
	{
		FLinearColor FinalTint = FSequencerSectionPainter::BlendColor(Tint);
		if (bIsHighlighted && Section.GetRange() != TRange<FFrameNumber>::All())
		{
			float Lum = FinalTint.ComputeLuminance() * 0.2f;
			FinalTint = FinalTint + FLinearColor(Lum, Lum, Lum, 0.f);
		}
		return FinalTint;
	}

	virtual int32 PaintSectionBackground(const FLinearColor& Tint) override
	{
		const ESlateDrawEffect DrawEffects = bParentEnabled
			? ESlateDrawEffect::None
			: ESlateDrawEffect::DisabledEffect;

		static const FSlateBrush* SectionBackgroundBrush = FEditorStyle::GetBrush("Sequencer.Section.Background");
		static const FSlateBrush* SectionBackgroundTintBrush = FEditorStyle::GetBrush("Sequencer.Section.BackgroundTint");
		static const FSlateBrush* SelectedSectionOverlay = FEditorStyle::GetBrush("Sequencer.Section.SelectedSectionOverlay");

		FLinearColor FinalTint = GetFinalTintColor(Tint);

		// Offset lower bounds and size for infinte sections so we don't draw the rounded border on the visible area
		const float InfiniteLowerOffset = Section.HasStartFrame() ? 0.f : 100.f;
		const float InfiniteSizeOffset  = InfiniteLowerOffset + (Section.HasEndFrame() ? 0.f : 100.f);

		FPaintGeometry PaintGeometry = SectionGeometry.ToPaintGeometry(
			SectionGeometry.GetLocalSize() + FVector2D(InfiniteSizeOffset, 0.f),
			FSlateLayoutTransform(FVector2D(-InfiniteLowerOffset, 0.f))
			);

		if (Sequencer.GetSequencerSettings()->ShouldShowPrePostRoll())
		{
			TOptional<FSlateClippingState> PreviousClipState = DrawElements.GetClippingState();
			DrawElements.PopClip();

			static const FSlateBrush* PreRollBrush = FEditorStyle::GetBrush("Sequencer.Section.PreRoll");
			float BrushHeight = 16.f, BrushWidth = 10.f;

			if (Section.HasStartFrame())
			{
				FFrameNumber SectionStartTime = Section.GetInclusiveStartFrame();
				FFrameNumber PreRollStartTime = SectionStartTime - Section.GetPreRollFrames();

				const float PreRollPx = TimeToPixelConverter.FrameToPixel(SectionStartTime) - TimeToPixelConverter.FrameToPixel(PreRollStartTime);
				if (PreRollPx > 0)
				{
					const float RoundedPreRollPx = (int(PreRollPx / BrushWidth)+1) * BrushWidth;

					// Round up to the nearest BrushWidth size
					FGeometry PreRollArea = SectionGeometry.MakeChild(
						FVector2D(RoundedPreRollPx, BrushHeight),
						FSlateLayoutTransform(FVector2D(-PreRollPx, (SectionGeometry.GetLocalSize().Y - BrushHeight)*.5f))
						);

					FSlateDrawElement::MakeBox(
						DrawElements,
						LayerId,
						PreRollArea.ToPaintGeometry(),
						PreRollBrush,
						DrawEffects
					);
				}
			}

			if (Section.HasEndFrame())
			{
				FFrameNumber SectionEndTime  = Section.GetExclusiveEndFrame();
				FFrameNumber PostRollEndTime = SectionEndTime + Section.GetPostRollFrames();

				const float PostRollPx = TimeToPixelConverter.FrameToPixel(PostRollEndTime) - TimeToPixelConverter.FrameToPixel(SectionEndTime);
				if (PostRollPx > 0)
				{
					const float RoundedPostRollPx = (int(PostRollPx / BrushWidth)+1) * BrushWidth;
					const float Difference = RoundedPostRollPx - PostRollPx;

					// Slate border brushes tile UVs along +ve X, so we round the arrows to a multiple of the brush width, and offset, to ensure we don't have a partial tile visible at the end
					FGeometry PostRollArea = SectionGeometry.MakeChild(
						FVector2D(RoundedPostRollPx, BrushHeight),
						FSlateLayoutTransform(FVector2D(SectionGeometry.GetLocalSize().X - Difference, (SectionGeometry.GetLocalSize().Y - BrushHeight)*.5f))
						);

					FSlateDrawElement::MakeBox(
						DrawElements,
						LayerId,
						PostRollArea.ToPaintGeometry(),
						PreRollBrush,
						DrawEffects
					);
				}
			}

			if (PreviousClipState.IsSet())
			{
				DrawElements.GetClippingManager().PushClippingState(PreviousClipState.GetValue());
			}
		}


		{
			TOptional<FSlateClippingState> PreviousClipState = DrawElements.GetClippingState();
			DrawElements.PopClip();

			// Draw the section background
			FSlateDrawElement::MakeBox(
				DrawElements,
				LayerId,
				PaintGeometry,
				SectionBackgroundBrush,
				DrawEffects
			);
			++LayerId;

			if (PreviousClipState.IsSet())
			{
				DrawElements.GetClippingManager().PushClippingState(PreviousClipState.GetValue());
			}
		}

		// Draw the section background tint over the background
		FSlateDrawElement::MakeBox(
			DrawElements,
			LayerId,
			PaintGeometry,
			SectionBackgroundTintBrush,
			DrawEffects,
			FinalTint
		);
		++LayerId;

		// Draw underlapping sections
		DrawOverlaps(FinalTint);

		// Draw empty space
		DrawEmptySpace();

		// Draw the blend type text
		DrawBlendType();

		// Draw easing curves
		DrawEasing(FinalTint);

		// Draw the selection hash
		if (SelectionColor.IsSet())
		{
			FSlateDrawElement::MakeBox(
				DrawElements,
				LayerId,
				SectionGeometry.ToPaintGeometry(FVector2D(1, 1), SectionGeometry.GetLocalSize() - FVector2D(2,2)),
				SelectedSectionOverlay,
				DrawEffects,
				SelectionColor.GetValue().CopyWithNewOpacity(0.8f)
			);
		}

		return LayerId;
	}

	virtual const FTimeToPixel& GetTimeConverter() const
	{
		return TimeToPixelConverter;
	}

	void CalculateSelectionColor()
	{
		// Don't draw selected if infinite
		if (Section.GetRange() == TRange<FFrameNumber>::All())
		{
			return;
		}

		FSequencerSelection& Selection = Sequencer.GetSelection();
		FSequencerSelectionPreview& SelectionPreview = Sequencer.GetSelectionPreview();

		ESelectionPreviewState SelectionPreviewState = SelectionPreview.GetSelectionState(&Section);

		if (SelectionPreviewState == ESelectionPreviewState::NotSelected)
		{
			// Explicitly not selected in the preview selection
			return;
		}
		
		if (SelectionPreviewState == ESelectionPreviewState::Undefined && !Selection.IsSelected(&Section))
		{
			// No preview selection for this section, and it's not selected
			return;
		}

		SelectionColor = FEditorStyle::GetSlateColor(SequencerSectionConstants::SelectionColorName).GetColor(FWidgetStyle());

		// Use a muted selection color for selection previews
		if (SelectionPreviewState == ESelectionPreviewState::Selected)
		{
			SelectionColor.GetValue() = SelectionColor.GetValue().LinearRGBToHSV();
			SelectionColor.GetValue().R += 0.1f; // +10% hue
			SelectionColor.GetValue().G = 0.6f; // 60% saturation

			SelectionColor = SelectionColor.GetValue().HSVToLinearRGB();
		}
	}

	void DrawBlendType()
	{
		// Draw the blend type text if necessary
		UMovieSceneTrack* Track = GetTrack();
		if (!Track || Track->GetSupportedBlendTypes().Num() <= 1 || !Section.GetBlendType().IsValid() || !bIsHighlighted || Section.GetBlendType().Get() == EMovieSceneBlendType::Absolute)
		{
			return;
		}

		TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();

		UEnum* Enum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMovieSceneBlendType"), true);
		FText DisplayText = Enum->GetDisplayNameTextByValue((int64)Section.GetBlendType().Get());

		FSlateFontInfo FontInfo = FEditorStyle::GetFontStyle("Sequencer.Section.BackgroundText");
		FontInfo.Size = 24;

		auto GetFontHeight = [&]
		{
			return FontCache->GetMaxCharacterHeight(FontInfo, 1.f) + FontCache->GetBaseline(FontInfo, 1.f);
		};
		while( GetFontHeight() > SectionGeometry.Size.Y && FontInfo.Size > 11 )
		{
			FontInfo.Size = FMath::Max(FMath::FloorToInt(FontInfo.Size - 6.f), 11);
		}

		FVector2D TextOffset = Section.GetRange() == TRange<FFrameNumber>::All() ? FVector2D(0.f, -1.f) : FVector2D(1.f, -1.f);
		FVector2D BottomLeft = SectionGeometry.AbsoluteToLocal(SectionClippingRect.GetBottomLeft()) + TextOffset;

		FSlateDrawElement::MakeText(
			DrawElements,
			LayerId,
			SectionGeometry.MakeChild(
				FVector2D(SectionGeometry.Size.X, GetFontHeight()),
				FSlateLayoutTransform(BottomLeft - FVector2D(0.f, GetFontHeight()+1.f))
			).ToPaintGeometry(),
			DisplayText,
			FontInfo,
			bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
			FLinearColor(1.f,1.f,1.f,.2f)
		);
	}

	float GetEaseHighlightAmount(FSectionHandle Handle, float EaseInInterp, float EaseOutInterp) const
	{
		if (!Hotspot)
		{
			return 0.f;
		}

		const bool bEaseInHandle = Hotspot->GetType() == ESequencerHotspot::EaseInHandle;
		const bool bEaseOutHandle = Hotspot->GetType() == ESequencerHotspot::EaseOutHandle;

		float EaseInScale = 0.f, EaseOutScale = 0.f;
		if (bEaseInHandle || bEaseOutHandle)
		{
			if (static_cast<const FSectionEasingHandleHotspot*>(Hotspot)->Section == Handle)
			{
				if (bEaseInHandle)
				{
					EaseInScale = 1.f;
				}
				else
				{
					EaseOutScale = 1.f;
				}
			}
		}
		else if (Hotspot->GetType() == ESequencerHotspot::EasingArea)
		{
			for (const FEasingAreaHandle& Easing : static_cast<const FSectionEasingAreaHotspot*>(Hotspot)->Easings)
			{
				if (Easing.Section == Handle)
				{
					if (Easing.EasingType == ESequencerEasingType::In)
					{
						EaseInScale = 1.f;
					}
					else
					{
						EaseOutScale = 1.f;
					}
				}
			}
		}

		const float TotalScale = EaseInScale + EaseOutScale;
		return TotalScale > 0.f ? EaseInInterp * (EaseInScale/TotalScale) + ((1.f-EaseOutInterp) * (EaseOutScale/TotalScale)) : 0.f;
	}

	FEasingCurvePoint MakeCurvePoint(FSectionHandle SectionHandle, FFrameTime Time, const FLinearColor& FinalTint, const FLinearColor& EaseSelectionColor) const
	{
		TOptional<float> EaseInValue, EaseOutValue;
		float EaseInInterp = 0.f, EaseOutInterp = 1.f;
		SectionHandle.GetSectionObject()->EvaluateEasing(Time, EaseInValue, EaseOutValue, &EaseInInterp, &EaseOutInterp);

		return FEasingCurvePoint(
			FVector2D(Time / TimeToPixelConverter.GetTickResolution(), EaseInValue.Get(1.f) * EaseOutValue.Get(1.f)),
			FMath::Lerp(FinalTint, EaseSelectionColor, GetEaseHighlightAmount(SectionHandle, EaseInInterp, EaseOutInterp))
		);
	}

	/** Adds intermediate control points for the specified section's easing up to a given threshold */
	void RefineCurvePoints(FSectionHandle SectionHandle, const FLinearColor& FinalTint, const FLinearColor& EaseSelectionColor, TArray<FEasingCurvePoint>& InOutPoints)
	{
		static float GradientThreshold = .05f;
		static float ValueThreshold = .05f;

		float MinTimeSize = FMath::Max(0.0001, TimeToPixelConverter.PixelToSeconds(2.5) - TimeToPixelConverter.PixelToSeconds(0));

		UMovieSceneSection* SectionObject = SectionHandle.GetSectionObject();

		for (int32 Index = 0; Index < InOutPoints.Num() - 1; ++Index)
		{
			const FEasingCurvePoint& Lower = InOutPoints[Index];
			const FEasingCurvePoint& Upper = InOutPoints[Index + 1];

			if ((Upper.Location.X - Lower.Location.X)*.5f > MinTimeSize)
			{
				float      NewPointTime  = (Upper.Location.X + Lower.Location.X)*.5f;
				FFrameTime FrameTime     = NewPointTime * TimeToPixelConverter.GetTickResolution();
				float      NewPointValue = SectionObject->EvaluateEasing(FrameTime);

				// Check that the gradient is changing significantly
				float LinearValue = (Upper.Location.Y + Lower.Location.Y) * .5f;
				float PointGradient = NewPointValue - SectionObject->EvaluateEasing(FMath::Lerp(Lower.Location.X, NewPointTime, 0.9f) * TimeToPixelConverter.GetTickResolution());
				float OuterGradient = Upper.Location.Y - Lower.Location.Y;
				if (!FMath::IsNearlyEqual(OuterGradient, PointGradient, GradientThreshold) ||
					!FMath::IsNearlyEqual(LinearValue, NewPointValue, ValueThreshold))
				{
					// Add the point
					InOutPoints.Insert(MakeCurvePoint(SectionHandle, FrameTime, FinalTint, EaseSelectionColor), Index+1);
					--Index;
				}
			}
		}
	}

	void DrawEasingForSegment(const FSequencerOverlapRange& Segment, const FGeometry& InnerSectionGeometry, const FLinearColor& FinalTint)
	{
		// @todo: sequencer-timecode: Test that start offset is not required here
		const float RangeStartPixel = TimeToPixelConverter.FrameToPixel(MovieScene::DiscreteInclusiveLower(Segment.Range));
		const float RangeEndPixel = TimeToPixelConverter.FrameToPixel(MovieScene::DiscreteExclusiveUpper(Segment.Range));
		const float RangeSizePixel = RangeEndPixel - RangeStartPixel;

		FGeometry RangeGeometry = InnerSectionGeometry.MakeChild(FVector2D(RangeSizePixel, InnerSectionGeometry.Size.Y), FSlateLayoutTransform(FVector2D(RangeStartPixel, 0.f)));
		if (!FSlateRect::DoRectanglesIntersect(RangeGeometry.GetLayoutBoundingRect(), ParentClippingRect))
		{
			return;
		}

		UMovieSceneTrack* Track = Section.GetTypedOuter<UMovieSceneTrack>();
		if (!Track)
		{
			return;
		}

		const FSlateBrush* MyBrush = FEditorStyle::Get().GetBrush("Sequencer.Timeline.EaseInOut");
		FSlateShaderResourceProxy *ResourceProxy = FSlateDataPayload::ResourceManager->GetShaderResource(*MyBrush);
		FSlateResourceHandle ResourceHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*MyBrush);

		FVector2D AtlasOffset = ResourceProxy ? ResourceProxy->StartUV : FVector2D(0.f, 0.f);
		FVector2D AtlasUVSize = ResourceProxy ? ResourceProxy->SizeUV : FVector2D(1.f, 1.f);

		FSlateRenderTransform RenderTransform;

		const FVector2D Pos = RangeGeometry.GetAbsolutePosition();
		const FVector2D Size = RangeGeometry.GetLocalSize();

		FLinearColor EaseSelectionColor = FEditorStyle::GetSlateColor(SequencerSectionConstants::SelectionColorName).GetColor(FWidgetStyle());

		FColor FillColor(0,0,0,51);

		TArray<FEasingCurvePoint> CurvePoints;

		// Segment.Impls are already sorted bottom to top
		for (int32 CurveIndex = 0; CurveIndex < Segment.Sections.Num(); ++CurveIndex)
		{
			FSectionHandle Handle = Segment.Sections[CurveIndex];

			// Make the points for the curve
			CurvePoints.Reset(20);
			{
				CurvePoints.Add(MakeCurvePoint(Handle, Segment.Range.GetLowerBoundValue(), FinalTint, EaseSelectionColor));
				CurvePoints.Add(MakeCurvePoint(Handle, Segment.Range.GetUpperBoundValue(), FinalTint, EaseSelectionColor));

				// Refine the control points
				int32 LastNumPoints;
				do
				{
					LastNumPoints = CurvePoints.Num();
					RefineCurvePoints(Handle, FinalTint, EaseSelectionColor, CurvePoints);
				} while(LastNumPoints != CurvePoints.Num());
			}

			TArray<SlateIndex> Indices;
			TArray<FSlateVertex> Verts;
			TArray<FVector2D> BorderPoints;
			TArray<FLinearColor> BorderPointColors;

			Indices.Reserve(CurvePoints.Num()*6);
			Verts.Reserve(CurvePoints.Num()*2);

			for (const FEasingCurvePoint& Point : CurvePoints)
			{
				float SegmentStartTime = MovieScene::DiscreteInclusiveLower(Segment.Range) / TimeToPixelConverter.GetTickResolution();
				float U = (Point.Location.X - SegmentStartTime) / ( FFrameNumber(MovieScene::DiscreteSize(Segment.Range)) / TimeToPixelConverter.GetTickResolution() );

				// Add verts top->bottom
				FVector2D UV(U, 0.f);
				Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RenderTransform, (Pos + UV*Size*RangeGeometry.Scale), AtlasOffset + UV*AtlasUVSize, FillColor));

				UV.Y = 1.f - Point.Location.Y;
				BorderPoints.Add(UV*Size);
				BorderPointColors.Add(Point.Color);
				Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RenderTransform, (Pos + UV*Size*RangeGeometry.Scale), AtlasOffset + FVector2D(UV.X, 0.5f)*AtlasUVSize, FillColor));

				if (Verts.Num() >= 4)
				{
					int32 Index0 = Verts.Num()-4, Index1 = Verts.Num()-3, Index2 = Verts.Num()-2, Index3 = Verts.Num()-1;
					Indices.Add(Index0);
					Indices.Add(Index1);
					Indices.Add(Index2);

					Indices.Add(Index1);
					Indices.Add(Index2);
					Indices.Add(Index3);
				}
			}

			if (Indices.Num())
			{
				FSlateDrawElement::MakeCustomVerts(
					DrawElements,
					LayerId,
					ResourceHandle,
					Verts,
					Indices,
					nullptr,
					0,
					0, ESlateDrawEffect::PreMultipliedAlpha);

				const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
				FSlateDrawElement::MakeLines(
					DrawElements,
					LayerId + 1,
					RangeGeometry.ToPaintGeometry(),
					BorderPoints,
					BorderPointColors,
					DrawEffects | ESlateDrawEffect::PreMultipliedAlpha,
					FLinearColor::White,
					true);
			}
		}

		++LayerId;
	}

	void DrawEasing(const FLinearColor& FinalTint)
	{
		if (!Section.GetBlendType().IsValid())
		{
			return;
		}

		// Compute easing geometry by insetting from the current section geometry by 1px
		FGeometry InnerSectionGeometry = SectionGeometry.MakeChild(SectionGeometry.Size - FVector2D(2.f, 2.f), FSlateLayoutTransform(FVector2D(1.f, 1.f)));
		for (const FSequencerOverlapRange& Segment : SectionWidget.UnderlappingEasingSegments)
		{
			DrawEasingForSegment(Segment, InnerSectionGeometry, FinalTint);
		}

		++LayerId;
	}

	void DrawOverlaps(const FLinearColor& FinalTint)
	{
		FGeometry InnerSectionGeometry = SectionGeometry.MakeChild(SectionGeometry.Size - FVector2D(2.f, 2.f), FSlateLayoutTransform(FVector2D(1.f, 1.f)));

		UMovieSceneTrack* Track = Section.GetTypedOuter<UMovieSceneTrack>();
		if (!Track)
		{
			return;
		}

		static const FSlateBrush* PinCusionBrush = FEditorStyle::GetBrush("Sequencer.Section.PinCusion");
		static const FSlateBrush* OverlapBorderBrush = FEditorStyle::GetBrush("Sequencer.Section.OverlapBorder");

		const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		const float StartTimePixel = Section.HasStartFrame() ? TimeToPixelConverter.FrameToPixel(Section.GetInclusiveStartFrame()) : 0.f;

		for (int32 SegmentIndex = 0; SegmentIndex < SectionWidget.UnderlappingSegments.Num(); ++SegmentIndex)
		{
			const FSequencerOverlapRange& Segment = SectionWidget.UnderlappingSegments[SegmentIndex];

			const float RangeStartPixel	= Segment.Range.GetLowerBound().IsOpen() ? 0.f							: TimeToPixelConverter.FrameToPixel(MovieScene::DiscreteInclusiveLower(Segment.Range));
			const float RangeEndPixel	= Segment.Range.GetUpperBound().IsOpen() ? InnerSectionGeometry.Size.X	: TimeToPixelConverter.FrameToPixel(MovieScene::DiscreteExclusiveUpper(Segment.Range));
			const float RangeSizePixel	= RangeEndPixel - RangeStartPixel;

			FGeometry RangeGeometry = InnerSectionGeometry.MakeChild(FVector2D(RangeSizePixel, InnerSectionGeometry.Size.Y), FSlateLayoutTransform(FVector2D(RangeStartPixel - StartTimePixel, 0.f)));
			if (!FSlateRect::DoRectanglesIntersect(RangeGeometry.GetLayoutBoundingRect(), ParentClippingRect))
			{
				continue;
			}

			const FSequencerOverlapRange* NextSegment = SegmentIndex < SectionWidget.UnderlappingSegments.Num() - 1 ? &SectionWidget.UnderlappingSegments[SegmentIndex+1] : nullptr;
			const bool bDrawRightMostBound = !NextSegment || !Segment.Range.Adjoins(NextSegment->Range);

			FSlateDrawElement::MakeBox(
				DrawElements,
				LayerId,
				RangeGeometry.ToPaintGeometry(),
				PinCusionBrush,
				DrawEffects,
				FinalTint
			);

			FPaintGeometry PaintGeometry = bDrawRightMostBound ? RangeGeometry.ToPaintGeometry() : RangeGeometry.ToPaintGeometry(FVector2D(RangeGeometry.Size) + FVector2D(10.f, 0.f), FSlateLayoutTransform(FVector2D::ZeroVector));
			FSlateDrawElement::MakeBox(
				DrawElements,
				LayerId,
				PaintGeometry,
				OverlapBorderBrush,
				DrawEffects,
				FLinearColor(1.f,1.f,1.f,.3f)
			);
		}

		++LayerId;
	}

	void DrawEmptySpace()
	{
		const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
		static const FSlateBrush* EmptySpaceBrush = FEditorStyle::GetBrush("Sequencer.Section.EmptySpace");

		// Attach contiguous regions together
		TOptional<FSlateRect> CurrentArea;

		for (const FSectionLayoutElement& Element : SectionWidget.Layout->GetElements())
		{
			const bool bIsEmptySpace = Element.GetDisplayNode()->GetType() == ESequencerNode::KeyArea && Element.GetKeyAreas().Num() == 0;
			const bool bExistingEmptySpace = CurrentArea.IsSet();

			if (bIsEmptySpace && bExistingEmptySpace && FMath::IsNearlyEqual(CurrentArea->Bottom, Element.GetOffset()))
			{
				CurrentArea->Bottom = Element.GetOffset() + Element.GetHeight();
				continue;
			}

			if (bExistingEmptySpace)
			{
				FPaintGeometry PaintGeom = SectionGeometry.MakeChild(CurrentArea->GetSize(), FSlateLayoutTransform(CurrentArea->GetTopLeft())).ToPaintGeometry();
				FSlateDrawElement::MakeBox(DrawElements, LayerId, PaintGeom, EmptySpaceBrush, DrawEffects);
				CurrentArea.Reset();
			}

			if (bIsEmptySpace)
			{
				CurrentArea = FSlateRect::FromPointAndExtent(FVector2D(0.f, Element.GetOffset()), FVector2D(SectionGeometry.Size.X, Element.GetHeight()));
			}
		}

		if (CurrentArea.IsSet())
		{
			FPaintGeometry PaintGeom = SectionGeometry.MakeChild(CurrentArea->GetSize(), FSlateLayoutTransform(CurrentArea->GetTopLeft())).ToPaintGeometry();
			FSlateDrawElement::MakeBox(DrawElements, LayerId, PaintGeom, EmptySpaceBrush, DrawEffects);
		}

		++LayerId;
	}

	TOptional<FLinearColor> SelectionColor;
	FSequencer& Sequencer;
	const SSequencerSection& SectionWidget;
	FTimeToPixel TimeToPixelConverter;
	const ISequencerHotspot* Hotspot;

	/** The clipping rectangle of the parent widget */
	FSlateRect ParentClippingRect;
};

void SSequencerSection::Construct( const FArguments& InArgs, TSharedRef<FSequencerTrackNode> SectionNode, int32 InSectionIndex )
{
	SectionIndex = InSectionIndex;
	ParentSectionArea = SectionNode;
	SectionInterface = SectionNode->GetSections()[InSectionIndex];
	Layout = FSectionLayout(*SectionNode, InSectionIndex);
	HandleOffsetPx = 0.f;

	ChildSlot
	[
		SectionInterface->GenerateSectionWidget()
	];
}


FVector2D SSequencerSection::ComputeDesiredSize(float) const
{
	return FVector2D(100, Layout->GetTotalHeight());
}


FGeometry SSequencerSection::GetKeyAreaGeometry( const FSectionLayoutElement& LayoutElement, const FGeometry& SectionGeometry ) const
{
	// Compute the geometry for the key area
	return SectionGeometry.MakeChild( FVector2D( 0, LayoutElement.GetOffset() ), FVector2D( SectionGeometry.GetLocalSize().X, LayoutElement.GetHeight()  ) );
}


void SSequencerSection::GetKeysUnderMouse( const FVector2D& MousePosition, const FGeometry& AllottedGeometry, TArray<FSequencerSelectedKey>& OutKeys ) const
{
	FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );

	UMovieSceneSection& Section = *SectionInterface->GetSectionObject();

	FTimeToPixel TimeToPixelConverter = ConstructTimeConverterForSection(SectionGeometry, Section, GetSequencer());
	const FVector2D MousePixel           = SectionGeometry.AbsoluteToLocal( MousePosition );

	// HitTest 
	const FFrameTime HalfKeySizeFrames = TimeToPixelConverter.PixelDeltaToFrame( SequencerSectionConstants::KeySize.X*.5f );
	const FFrameTime MouseFrameTime    = TimeToPixelConverter.PixelToFrame( MousePixel.X );

	TRange<FFrameNumber> HitTestRange = TRange<FFrameNumber>( (MouseFrameTime-HalfKeySizeFrames).CeilToFrame(), (MouseFrameTime+HalfKeySizeFrames).CeilToFrame());
	
	if (HitTestRange.IsEmpty())
	{
		return;
	}

	// Search every key area until we find the one under the mouse
	for (const FSectionLayoutElement& Element : Layout->GetElements())
	{
		const FGeometry KeyAreaGeometry = GetKeyAreaGeometry(Element, AllottedGeometry);
		const FVector2D LocalMousePixel = KeyAreaGeometry.AbsoluteToLocal(MousePosition);
		const float LocalKeyPosY = KeyAreaGeometry.GetLocalSize().Y * .5f;

		// Check that this section is under our mouse, and discard it from potential selection if the mouse is higher than the key's height. We have to
		// check keys on a per-section basis (and not for the overall SectionGeometry) because keys are offset on tracks that have expandable 
		// ranges (ie: Audio, Animation) which otherwise makes them fail the height-threshold check.
		if (!KeyAreaGeometry.IsUnderLocation(MousePosition) || FMath::Abs(LocalKeyPosY - LocalMousePixel.Y) > SequencerSectionConstants::KeySize.Y *.5f)
		{
			continue;
		}

		for (TSharedPtr<IKeyArea> KeyArea : Element.GetKeyAreas())
		{
			if (KeyArea.IsValid())
			{
				TArray<FKeyHandle> KeyHandles;
				KeyArea->GetKeyHandles(KeyHandles, HitTestRange);

				// Only ever select one key from any given key area
				if (KeyHandles.Num())
				{
					OutKeys.Add( FSequencerSelectedKey( Section, KeyArea, KeyHandles[0] ) );
				}
			}
		}

		// The mouse is in this key area so it cannot possibly be in any other key area
		return;
	}
}


void SSequencerSection::CreateKeysUnderMouse( const FVector2D& MousePosition, const FGeometry& AllottedGeometry, TArrayView<const FSequencerSelectedKey> InPressedKeys, TArray<FSequencerSelectedKey>& OutKeys )
{
	UMovieSceneSection& Section = *SectionInterface->GetSectionObject();

	if (Section.IsReadOnly())
	{
		return;
	}

	// If the pressed key exists, offset the new key and look for it in the newly laid out key areas
	if (InPressedKeys.Num())
	{
		Section.Modify();

		// Offset by 1 pixel worth of time if possible
		const FFrameTime TimeFuzz = (GetSequencer().GetViewRange().Size<double>() / ParentGeometry.GetLocalSize().X) * Section.GetTypedOuter<UMovieScene>()->GetTickResolution();

		for (const FSequencerSelectedKey& PressedKey : InPressedKeys)
		{
			const FFrameNumber CurrentTime = PressedKey.KeyArea->GetKeyTime(PressedKey.KeyHandle.GetValue());
			const FKeyHandle NewHandle = PressedKey.KeyArea->DuplicateKey(PressedKey.KeyHandle.GetValue());

			PressedKey.KeyArea->SetKeyTime(NewHandle, CurrentTime + TimeFuzz.FrameNumber);
			OutKeys.Add(FSequencerSelectedKey(Section, PressedKey.KeyArea, NewHandle));
		}
	}
	else
	{
		TSharedPtr<FSequencerObjectBindingNode> ObjectBindingNode = ParentSectionArea.IsValid() ? ParentSectionArea->FindParentObjectBindingNode() : nullptr;
		FGuid ObjectBinding = ObjectBindingNode.IsValid() ? ObjectBindingNode->GetObjectBinding() : FGuid();

		FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );

		// Search every key area until we find the one under the mouse
		for (const FSectionLayoutElement& Element : Layout->GetElements())
		{
			// Compute the current key area geometry
			FGeometry KeyAreaGeometryPadded = GetKeyAreaGeometry( Element, AllottedGeometry );

			// Is the key area under the mouse
			if( !KeyAreaGeometryPadded.IsUnderLocation( MousePosition ) )
			{
				continue;
			}

			FTimeToPixel TimeToPixelConverter = ConstructTimeConverterForSection(SectionGeometry, Section, GetSequencer());

			FVector2D LocalSpaceMousePosition = SectionGeometry.AbsoluteToLocal( MousePosition );
			const FFrameTime KeyTime = TimeToPixelConverter.PixelToFrame(LocalSpaceMousePosition.X);

			Section.Modify();

			for (TSharedPtr<IKeyArea> KeyArea : Element.GetKeyAreas())
			{
				if (KeyArea.IsValid())
				{
					FKeyHandle NewHandle = KeyArea->AddOrUpdateKey(KeyTime.FrameNumber, ObjectBinding, GetSequencer());
					OutKeys.Add(FSequencerSelectedKey(Section, KeyArea, NewHandle));
				}
			}
		}
	}

	if (OutKeys.Num())
	{
		Layout = FSectionLayout(*ParentSectionArea, SectionIndex);
	}
}


bool SSequencerSection::CheckForEasingHandleInteraction( const FPointerEvent& MouseEvent, const FGeometry& SectionGeometry )
{
	UMovieSceneSection* ThisSection = SectionInterface->GetSectionObject();
	if (!ThisSection)
	{
		return false;
	}

	FTimeToPixel TimeToPixelConverter = ConstructTimeConverterForSection(MakeSectionGeometryWithoutHandles(SectionGeometry, SectionInterface), *ThisSection, GetSequencer());

	const double MouseTime = TimeToPixelConverter.PixelToSeconds(SectionGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X);
	// We intentionally give the handles a little more hit-test area than is visible as they are quite small
	const double HalfHandleSizeX = TimeToPixelConverter.PixelToSeconds(8.f) - TimeToPixelConverter.PixelToSeconds(0.f);

	// Now test individual easing handles if we're at the correct vertical position
	float LocalMouseY = SectionGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).Y;
	if (LocalMouseY < 0.f || LocalMouseY > 5.f)
	{
		return false;
	}

	// Gather all underlapping sections
	TArray<FSectionHandle> AllUnderlappingSections;
	AllUnderlappingSections.Add(FSectionHandle(ParentSectionArea, SectionIndex));
	for (const FSequencerOverlapRange& Segment : UnderlappingSegments)
	{
		for (FSectionHandle Section : Segment.Sections)
		{
			AllUnderlappingSections.AddUnique(Section);
		}
	}

	for (FSectionHandle Handle : AllUnderlappingSections)
	{
		TSharedRef<ISequencerSection> EasingSection    =  Handle.TrackNode->GetSections()[Handle.SectionIndex];
		UMovieSceneSection*           EasingSectionObj = EasingSection->GetSectionObject();

		if (EasingSectionObj->HasStartFrame())
		{
			TRange<FFrameNumber> EaseInRange      = EasingSectionObj->GetEaseInRange();
			double               HandlePositionIn = ( EaseInRange.IsEmpty() ? EasingSectionObj->GetInclusiveStartFrame() : EaseInRange.GetUpperBoundValue() ) / TimeToPixelConverter.GetTickResolution();

			if (FMath::IsNearlyEqual(MouseTime, HandlePositionIn, HalfHandleSizeX))
			{
				GetSequencer().SetHotspot(MakeShared<FSectionEasingHandleHotspot>(ESequencerEasingType::In, Handle));
				return true;
			}
		}

		if (EasingSectionObj->HasEndFrame())
		{
			TRange<FFrameNumber> EaseOutRange      = EasingSectionObj->GetEaseOutRange();
			double               HandlePositionOut = (EaseOutRange.IsEmpty() ? EasingSectionObj->GetExclusiveEndFrame() : EaseOutRange.GetLowerBoundValue() ) / TimeToPixelConverter.GetTickResolution();

			if (FMath::IsNearlyEqual(MouseTime, HandlePositionOut, HalfHandleSizeX))
			{
				GetSequencer().SetHotspot(MakeShared<FSectionEasingHandleHotspot>(ESequencerEasingType::Out, Handle));
				return true;
			}
		}
	}

	return false;
}


bool SSequencerSection::CheckForEdgeInteraction( const FPointerEvent& MouseEvent, const FGeometry& SectionGeometry )
{
	UMovieSceneSection* ThisSection = SectionInterface->GetSectionObject();
	if (!ThisSection)
	{
		return false;
	}

	TArray<FSectionHandle> AllUnderlappingSections;
	AllUnderlappingSections.Add(FSectionHandle(ParentSectionArea, SectionIndex));
	for (const FSequencerOverlapRange& Segment : UnderlappingSegments)
	{
		for (FSectionHandle Section : Segment.Sections)
		{
			AllUnderlappingSections.AddUnique(Section);
		}
	}

	FGeometry    SectionGeometryWithoutHandles = MakeSectionGeometryWithoutHandles(SectionGeometry, SectionInterface);
	FTimeToPixel TimeToPixelConverter          = ConstructTimeConverterForSection(SectionGeometryWithoutHandles, *ThisSection, GetSequencer());

	for (FSectionHandle Handle : AllUnderlappingSections)
	{
		TSharedRef<ISequencerSection> UnderlappingSection =  Handle.TrackNode->GetSections()[Handle.SectionIndex];
		UMovieSceneSection* UnderlappingSectionObj = UnderlappingSection->GetSectionObject();
		if (!UnderlappingSection->SectionIsResizable())
		{
			continue;
		}

		const float ThisHandleOffset = UnderlappingSectionObj == ThisSection ? HandleOffsetPx : 0.f;
		FVector2D GripSize( UnderlappingSection->GetSectionGripSize(), SectionGeometry.Size.Y );

		if (UnderlappingSectionObj->HasStartFrame())
		{
			// Make areas to the left and right of the geometry.  We will use these areas to determine if someone dragged the left or right edge of a section
			FGeometry SectionRectLeft = SectionGeometryWithoutHandles.MakeChild(
				FVector2D( TimeToPixelConverter.FrameToPixel(UnderlappingSectionObj->GetInclusiveStartFrame()) - ThisHandleOffset, 0.f ),
				GripSize
			);

			if( SectionRectLeft.IsUnderLocation( MouseEvent.GetScreenSpacePosition() ) )
			{
				GetSequencer().SetHotspot(MakeShareable( new FSectionResizeHotspot(FSectionResizeHotspot::Left, Handle)) );
				return true;
			}
		}

		if (UnderlappingSectionObj->HasEndFrame())
		{
			FGeometry SectionRectRight = SectionGeometryWithoutHandles.MakeChild(
				FVector2D( TimeToPixelConverter.FrameToPixel(UnderlappingSectionObj->GetExclusiveEndFrame()) - UnderlappingSection->GetSectionGripSize() + ThisHandleOffset, 0 ), 
				GripSize
			);

			if( SectionRectRight.IsUnderLocation( MouseEvent.GetScreenSpacePosition() ) )
			{
				GetSequencer().SetHotspot(MakeShareable( new FSectionResizeHotspot(FSectionResizeHotspot::Right, Handle)) );
				return true;
			}
		}
	}
	return false;
}

bool SSequencerSection::CheckForEasingAreaInteraction( const FPointerEvent& MouseEvent, const FGeometry& SectionGeometry )
{
	UMovieSceneSection* ThisSection = SectionInterface->GetSectionObject();
	if (!ThisSection)
	{
		return false;
	}

	FTimeToPixel TimeToPixelConverter = ConstructTimeConverterForSection(MakeSectionGeometryWithoutHandles(SectionGeometry, SectionInterface), *ThisSection, GetSequencer());
	const FFrameNumber MouseTime = TimeToPixelConverter.PixelToFrame(SectionGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X).FrameNumber;

	// First off, set the hotspot to an easing area if necessary
	for (const FSequencerOverlapRange& Segment : UnderlappingEasingSegments)
	{
		if (!Segment.Range.Contains(MouseTime))
		{
			continue;
		}

		TArray<FEasingAreaHandle> EasingAreas;
		for (FSectionHandle Handle : Segment.Sections)
		{
			UMovieSceneSection* Section = Handle.GetSectionObject();
			if (Section->GetEaseInRange().Contains(MouseTime))
			{
				EasingAreas.Add(FEasingAreaHandle{ Handle, ESequencerEasingType::In });
			}
			if (Section->GetEaseOutRange().Contains(MouseTime))
			{
				EasingAreas.Add(FEasingAreaHandle{ Handle, ESequencerEasingType::Out });
			}
		}

		if (EasingAreas.Num())
		{
			GetSequencer().SetHotspot(MakeShared<FSectionEasingAreaHotspot>(EasingAreas, FSectionHandle(ParentSectionArea, SectionIndex)));
			return true;
		}
	}
	return false;
}

FSequencer& SSequencerSection::GetSequencer() const
{
	return ParentSectionArea->GetSequencer();
}


int32 SSequencerSection::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	UMovieSceneSection& SectionObject = *SectionInterface->GetSectionObject();

	const ISequencerEditTool* EditTool = GetSequencer().GetEditTool();
	const ISequencerHotspot* Hotspot = EditTool ? EditTool->GetDragHotspot() : nullptr;
	if (!Hotspot)
	{
		Hotspot = GetSequencer().GetHotspot().Get();
	}

	const bool bEnabled = bParentEnabled && SectionObject.IsActive();
	const bool bLocked = SectionObject.IsLocked();
	UMovieScenePropertyTrack* Track = SectionObject.GetTypedOuter<UMovieScenePropertyTrack>();
	bool bSetSectionToKey = false;
	if (Track)
	{
		if (Track->GetSectionToKey() == &SectionObject)
		{
			bSetSectionToKey = true;
		}
	}


	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );

	FSequencerSectionPainterImpl Painter(ParentSectionArea->GetSequencer(), SectionObject, OutDrawElements, SectionGeometry, *this);

	FGeometry PaintSpaceParentGeometry = ParentGeometry;
	PaintSpaceParentGeometry.AppendTransform(FSlateLayoutTransform(Inverse(Args.GetWindowToDesktopTransform())));

	Painter.ParentClippingRect = PaintSpaceParentGeometry.GetLayoutBoundingRect();

	// Clip vertically
	Painter.ParentClippingRect.Top = FMath::Max(Painter.ParentClippingRect.Top, MyCullingRect.Top);
	Painter.ParentClippingRect.Bottom = FMath::Min(Painter.ParentClippingRect.Bottom, MyCullingRect.Bottom);

	Painter.SectionClippingRect = Painter.SectionGeometry.GetLayoutBoundingRect().InsetBy(FMargin(1.f)).IntersectionWith(Painter.ParentClippingRect);

	Painter.LayerId = LayerId;
	Painter.bParentEnabled = bEnabled;
	Painter.bIsHighlighted = IsSectionHighlighted(FSectionHandle(ParentSectionArea, SectionIndex), Hotspot);
	auto& Selection = ParentSectionArea->GetSequencer().GetSelection();
	Painter.bIsSelected = Selection.IsSelected(&SectionObject);

	FSlateClippingZone ClippingZone(Painter.SectionClippingRect);
	OutDrawElements.PushClip(ClippingZone);

	// Ask the interface to draw the section
	LayerId = SectionInterface->OnPaintSection(Painter);

	LayerId = SCompoundWidget::OnPaint( Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bEnabled );

	FLinearColor SelectionColor = FEditorStyle::GetSlateColor(SequencerSectionConstants::SelectionColorName).GetColor(FWidgetStyle());
	DrawSectionHandles(AllottedGeometry, OutDrawElements, LayerId, DrawEffects, SelectionColor, Hotspot);

	Painter.LayerId = LayerId;
	PaintEasingHandles( Painter, SelectionColor, Hotspot );
	PaintKeys( Painter, InWidgetStyle );

	LayerId = Painter.LayerId;
	if (bLocked)
	{
		static const FName SelectionBorder("Sequencer.Section.LockedBorder");

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FEditorStyle::GetBrush(SelectionBorder),
			DrawEffects,
			FLinearColor::Red
		);
	}
	else if (bSetSectionToKey)
	{
		static const FName SelectionBorder("Sequencer.Section.LockedBorder");

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FEditorStyle::GetBrush(SelectionBorder),
			DrawEffects,
			FLinearColor::Green
		);
	}

	// Section name with drop shadow
	FText SectionTitle = SectionInterface->GetSectionTitle();
	FMargin ContentPadding = SectionInterface->GetContentPadding();

	const int32 EaseInAmount = SectionObject.Easing.GetEaseInDuration();
	if (EaseInAmount > 0)
	{
		ContentPadding.Left += Painter.GetTimeConverter().FrameToPixel(EaseInAmount) - Painter.GetTimeConverter().FrameToPixel(0);
	}

	if (!SectionTitle.IsEmpty())
	{
		FVector2D TopLeft = SectionGeometry.AbsoluteToLocal(Painter.SectionClippingRect.GetTopLeft()) + FVector2D(1.f, -1.f);

		FSlateFontInfo FontInfo = FEditorStyle::GetFontStyle("NormalFont");

		TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();

		auto GetFontHeight = [&]
		{
			return FontCache->GetMaxCharacterHeight(FontInfo, 1.f) + FontCache->GetBaseline(FontInfo, 1.f);
		};
		while (GetFontHeight() > SectionGeometry.Size.Y && FontInfo.Size > 11)
		{
			FontInfo.Size = FMath::Max(FMath::FloorToInt(FontInfo.Size - 6.f), 11);
		}

		// Drop shadow
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			SectionGeometry.MakeChild(
				FVector2D(SectionGeometry.Size.X, GetFontHeight()),
				FSlateLayoutTransform(TopLeft + FVector2D(ContentPadding.Left, ContentPadding.Top) + FVector2D(1.f, 1.f))
			).ToPaintGeometry(),
			SectionTitle,
			FontInfo,
			DrawEffects,
			FLinearColor(0,0,0,.5f)
		);

		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			SectionGeometry.MakeChild(
				FVector2D(SectionGeometry.Size.X, GetFontHeight()),
				FSlateLayoutTransform(TopLeft + FVector2D(ContentPadding.Left, ContentPadding.Top))
			).ToPaintGeometry(),
			SectionTitle,
			FontInfo,
			DrawEffects,
			FColor(200, 200, 200)
		);
	}

	OutDrawElements.PopClip();
	return LayerId + 1;
}


void SSequencerSection::PaintKeys( FSequencerSectionPainter& InPainter, const FWidgetStyle& InWidgetStyle ) const
{
	static const FName HighlightBrushName("Sequencer.AnimationOutliner.DefaultBorder");
	static const FName StripeOverlayBrushName("Sequencer.Section.StripeOverlay");

	static const FName SelectionColorName("SelectionColor");
	static const FName SelectionInactiveColorName("SelectionColorInactive");
	static const FName SelectionColorPressedName("SelectionColor_Pressed");

	static const float BrushBorderWidth = 2.0f;

	const FLinearColor PressedKeyColor = FEditorStyle::GetSlateColor(SelectionColorPressedName).GetColor( InWidgetStyle );
	FLinearColor SelectionColor = FEditorStyle::GetSlateColor(SelectionColorName).GetColor( InWidgetStyle );
	FLinearColor SelectedKeyColor = SelectionColor;
	FSequencer& Sequencer = ParentSectionArea->GetSequencer();
	TSharedPtr<ISequencerHotspot> Hotspot = Sequencer.GetHotspot();

	const FSlateBrush* StripeOverlayBrush = FEditorStyle::GetBrush(StripeOverlayBrushName);
	const FSlateBrush* HighlightBrush = FEditorStyle::GetBrush(HighlightBrushName);

	// get hovered key
	TArrayView<const FSequencerSelectedKey> HoveredKeys;

	if (Hotspot.IsValid() && Hotspot->GetType() == ESequencerHotspot::Key)
	{
		HoveredKeys = static_cast<FKeyHotspot*>(Hotspot.Get())->Keys;
	}

	auto& Selection = Sequencer.GetSelection();
	auto& SelectionPreview = Sequencer.GetSelectionPreview();

	const float KeyThrobScaleValue = GetKeySelectionThrobValue();
	const float SectionThrobScaleValue = GetSectionSelectionThrobValue();

	// draw all keys in each key area
	UMovieSceneSection& SectionObject = *SectionInterface->GetSectionObject();

	// Use the sub sequence range to draw valid keys, else use the current sequence's playback range
	TRange<FFrameNumber> ValidKeyRange = Sequencer.GetSubSequenceRange().Get(Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange());

	const ESlateDrawEffect DrawEffects = InPainter.bParentEnabled
		? ESlateDrawEffect::None
		: ESlateDrawEffect::DisabledEffect;

	const FTimeToPixel& TimeToPixelConverter = InPainter.GetTimeConverter();

	for (const FSectionLayoutElement& LayoutElement : Layout->GetElements())
	{
		// get key handles
		TArrayView<const TSharedRef<IKeyArea>> KeyAreas = LayoutElement.GetKeyAreas();

		FGeometry KeyAreaGeometry = GetKeyAreaGeometry(LayoutElement, InPainter.SectionGeometry);

		TOptional<FLinearColor> KeyAreaColor = KeyAreas.Num() == 1 ? KeyAreas[0]->GetColor() : TOptional<FLinearColor>();

		// draw a box for the key area 
		if (KeyAreaColor.IsSet() && Sequencer.GetSequencerSettings()->GetShowChannelColors())
		{
			static float BoxThickness = 5.f;
			FVector2D KeyAreaSize = KeyAreaGeometry.GetLocalSize();
			FSlateDrawElement::MakeBox( 
				InPainter.DrawElements,
				InPainter.LayerId,
				KeyAreaGeometry.ToPaintGeometry(FVector2D(KeyAreaSize.X, BoxThickness), FSlateLayoutTransform(FVector2D(0.f, KeyAreaSize.Y*.5f - BoxThickness*.5f))),
				StripeOverlayBrush,
				DrawEffects,
				KeyAreaColor.GetValue()
			); 
		}

		if (LayoutElement.GetDisplayNode().IsValid())
		{
			FLinearColor HighlightColor;
			bool bDrawHighlight = false;
			if (Sequencer.GetSelection().NodeHasSelectedKeysOrSections(LayoutElement.GetDisplayNode().ToSharedRef()))
			{
				bDrawHighlight = true;
				HighlightColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.15f);
			}
			else if (LayoutElement.GetDisplayNode()->IsHovered())
			{
				bDrawHighlight = true;
				HighlightColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.05f);
			}

			if (bDrawHighlight)
			{
				FSlateDrawElement::MakeBox(
					InPainter.DrawElements,
					InPainter.LayerId,
					KeyAreaGeometry.ToPaintGeometry(),
					HighlightBrush,
					DrawEffects,
					HighlightColor
				);
			}
		}

		if (Selection.IsSelected(LayoutElement.GetDisplayNode().ToSharedRef()))
		{
			static const FName SelectedTrackTint("Sequencer.Section.SelectedTrackTint");

			FLinearColor KeyAreaOutlineColor = SelectionColor;

			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				InPainter.LayerId,
				KeyAreaGeometry.ToPaintGeometry(),
				FEditorStyle::GetBrush(SelectedTrackTint),
				DrawEffects,
				KeyAreaOutlineColor
			);
		}

		bool bSectionSelected = Selection.IsSelected(&SectionObject);
		if (bSectionSelected && SectionThrobScaleValue != 0.f)
		{
			static const FName SelectedTrackTint("Sequencer.Section.BackgroundTint");

			FLinearColor KeyAreaOutlineColor = SelectionColor;
			KeyAreaOutlineColor.A = SectionThrobScaleValue;

			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				InPainter.LayerId,
				KeyAreaGeometry.ToPaintGeometry(),
				FEditorStyle::GetBrush(SelectedTrackTint),
				DrawEffects,
				KeyAreaOutlineColor
			);
		}

		// Gather keys for a region larger than the view range to ensure we draw keys that are only just offscreen.
		TRange<double> PaddedViewRange;
		{
			// Compute visible range taking into account a half-frame offset for keys, plus half a key width for keys that are partially offscreen
			TRange<FFrameNumber> SectionRange   = SectionObject.GetRange();
			const double         HalfKeyWidth   = 0.5f * (TimeToPixelConverter.PixelToSeconds(SequencerSectionConstants::KeySize.X) - TimeToPixelConverter.PixelToSeconds(0));
			TRange<double>       VisibleRange   = MovieScene::DilateRange(GetSequencer().GetViewRange(), -HalfKeyWidth, HalfKeyWidth);

			PaddedViewRange = TRange<double>::Intersection(SectionRange / Sequencer.GetFocusedTickResolution(), VisibleRange);
		}

		// For each key area, we gather draw params for the visible time range, then combine
		
		auto& Found = CachedKeyAreaPositions.FindChecked(LayoutElement);
		TArrayView<const FSequencerCachedKeys> CachedKeys = Found;

		// Can't do any of the rest if there are no keys, or there's no range to draw within
		if (CachedKeys.Num() == 0 || PaddedViewRange.IsEmpty())
		{
			continue;
		}

		const int32 KeyLayer = InPainter.LayerId;

		TOptional<FSlateClippingState> PreviousClipState = InPainter.DrawElements.GetClippingState();
		InPainter.DrawElements.PopClip();

		struct FKeyDrawInformation
		{
			FKeyDrawInformation()
				: NextIndex(0)
			{}

			int32 NextIndex;
			TArrayView<const double> TimesInRange;
			TArrayView<const FFrameNumber> FramesInRange;
			TArrayView<const FKeyHandle> HandlesInRange;
			TArray<FKeyDrawParams> DrawParams;
		};
		TArray<FKeyDrawInformation> DrawInfoPerCache;
		DrawInfoPerCache.Reserve(CachedKeys.Num());

		for (const FSequencerCachedKeys& Cache : CachedKeys)
		{
			// Gather all the key handles in this view range
			FKeyDrawInformation KeyDrawInfo;

			Cache.GetKeysInRange(PaddedViewRange, &KeyDrawInfo.TimesInRange, &KeyDrawInfo.FramesInRange, &KeyDrawInfo.HandlesInRange);

			// Generate draw data for this key area
			if (KeyDrawInfo.TimesInRange.Num())
			{
				KeyDrawInfo.DrawParams.SetNum(KeyDrawInfo.TimesInRange.Num());
				Cache.GetKeyArea()->DrawKeys(KeyDrawInfo.HandlesInRange, KeyDrawInfo.DrawParams);
			}

			check(KeyDrawInfo.DrawParams.Num() == KeyDrawInfo.TimesInRange.Num() && KeyDrawInfo.TimesInRange.Num() == KeyDrawInfo.HandlesInRange.Num());

			// Add it to the array
			DrawInfoPerCache.Add(MoveTemp(KeyDrawInfo));
		}

		static float PixelOverlapThreshold = 3.f;
		const double TimeOverlapThreshold = TimeToPixelConverter.PixelToSeconds(PixelOverlapThreshold) - TimeToPixelConverter.PixelToSeconds(0.f);

		auto AnythingLeftToDraw = [](const FKeyDrawInformation& In)
		{
			return In.NextIndex < In.TimesInRange.Num();
		};

		TArray<int32> KeyParamUpperBounds;
		KeyParamUpperBounds.SetNum(DrawInfoPerCache.Num());

		while (DrawInfoPerCache.ContainsByPredicate(AnythingLeftToDraw))
		{
			// Determine the next key position to draw
			double CardinalKeyTime = TNumericLimits<double>::Max();
			for (const FKeyDrawInformation& Info : DrawInfoPerCache)
			{
				if (Info.NextIndex < Info.TimesInRange.Num())
				{
					CardinalKeyTime = FMath::Min(CardinalKeyTime, Info.TimesInRange[Info.NextIndex]);
				}
			}

			// Start grouping keys at the current key time plus 99% of the threshold to ensure that we group at the center of keys
			// and that we avoid floating point precision issues where there is only one key [(KeyTime + TimeOverlapThreshold) - KeyTime != TimeOverlapThreshold] for some floats
			CardinalKeyTime += TimeOverlapThreshold*0.9994f;

			// Generate the hull of frame numbers that contribute to this key so we can draw it enabled/disabled depending on whether it is outside of the valid range or not
			TRange<FFrameNumber> KeyRange = TRange<FFrameNumber>::Empty();

			float AverageKeyTime = 0.f;
			int32 NumKeyTimes = 0;
			int32 NumOverlaps = 0;

			// Determine the ranges of keys considered to reside at this position
			for (int32 DrawIndex = 0; DrawIndex < DrawInfoPerCache.Num(); ++DrawIndex)
			{
				const FKeyDrawInformation& Info = DrawInfoPerCache[DrawIndex];
				if (Info.NextIndex >= Info.TimesInRange.Num())
				{
					continue;
				}

				if (FMath::IsNearlyEqual(Info.TimesInRange[Info.NextIndex], CardinalKeyTime, TimeOverlapThreshold))
				{
					int32 FinalIndexInThreshold = Info.NextIndex + 1;

					KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(Info.FramesInRange[Info.NextIndex]));
					AverageKeyTime += Info.TimesInRange[Info.NextIndex];
					++NumKeyTimes;

					// Count the number of overlapping keys
					while (FinalIndexInThreshold < Info.TimesInRange.Num() && FMath::IsNearlyEqual(Info.TimesInRange[FinalIndexInThreshold], CardinalKeyTime, TimeOverlapThreshold))
					{
						KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(Info.FramesInRange[FinalIndexInThreshold]));
						AverageKeyTime += Info.TimesInRange[FinalIndexInThreshold];
						++NumKeyTimes;

						++FinalIndexInThreshold;
						++NumOverlaps;
					}

					KeyParamUpperBounds[DrawIndex] = FinalIndexInThreshold;
				}
				else
				{
					KeyParamUpperBounds[DrawIndex] = Info.NextIndex;
				}
			}

			const float FinalKeyPosition = TimeToPixelConverter.SecondsToPixel(AverageKeyTime / NumKeyTimes);

			static const FSlateBrush* PartialKeyBrush = FEditorStyle::GetBrush("Sequencer.PartialKey");

			TOptional<FKeyDrawParams> KeyDrawParam;

			bool bPartialKey = false;

			int32 NumPreviewSelected = 0;
			int32 NumPreviewNotSelected = 0;
			int32 NumSelected = 0;
			int32 NumHovered = 0;
			int32 TotalNumKeys = 0;

			for (int32 DrawIndex = 0; DrawIndex < DrawInfoPerCache.Num(); ++DrawIndex)
			{
				FKeyDrawInformation& Info = DrawInfoPerCache[DrawIndex];
				int32 UpperBound = KeyParamUpperBounds[DrawIndex];

				if (Info.NextIndex >= Info.TimesInRange.Num() || UpperBound - Info.NextIndex == 0)
				{
					bPartialKey = true;
					continue;
				}

				for (int32 KeyIndex = Info.NextIndex; KeyIndex < UpperBound; ++KeyIndex)
				{
					if (!KeyDrawParam.IsSet())
					{
						KeyDrawParam = Info.DrawParams[KeyIndex];
					}
					else if (Info.DrawParams[KeyIndex] != KeyDrawParam.GetValue())
					{
						bPartialKey = true;
					}

					FSequencerSelectedKey TestKey(SectionObject, CachedKeys[DrawIndex].GetKeyArea(), Info.HandlesInRange[KeyIndex]);

					ESelectionPreviewState SelectionPreviewState = SelectionPreview.GetSelectionState(TestKey);
					NumPreviewSelected += int32(SelectionPreviewState == ESelectionPreviewState::Selected);
					NumPreviewNotSelected += int32(SelectionPreviewState == ESelectionPreviewState::NotSelected);
					NumSelected += int32(Selection.IsSelected(TestKey));
					NumHovered += int32(HoveredKeys.Contains(TestKey));
					++TotalNumKeys;
				}

				Info.NextIndex = UpperBound;
			}

			if (bPartialKey)
			{
				KeyDrawParam->FillOffset = FVector2D(0.f, 0.f);
				KeyDrawParam->FillTint   = KeyDrawParam->BorderTint = FLinearColor::White;
				KeyDrawParam->FillBrush  = KeyDrawParam->BorderBrush = PartialKeyBrush;
			}

			const bool bSelected = NumSelected == TotalNumKeys;
			ensure(KeyDrawParam.IsSet());

			// Determine the key color based on its selection/hover states
			if (NumPreviewSelected == TotalNumKeys)
			{
				FLinearColor PreviewSelectionColor = SelectionColor.LinearRGBToHSV();
				PreviewSelectionColor.R += 0.1f; // +10% hue
				PreviewSelectionColor.G = 0.6f; // 60% saturation
				KeyDrawParam->BorderTint = KeyDrawParam->FillTint = PreviewSelectionColor.HSVToLinearRGB();
			}
			else if (NumPreviewNotSelected == TotalNumKeys)
			{
				KeyDrawParam->BorderTint = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
			}
			else if (bSelected)
			{
				KeyDrawParam->BorderTint = SelectedKeyColor;
				KeyDrawParam->FillTint = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
			}
			else if (NumSelected != 0)
			{
				// partially selected
				KeyDrawParam->BorderTint = SelectedKeyColor.CopyWithNewOpacity(0.5f);
				KeyDrawParam->FillTint = FLinearColor(0.05f, 0.05f, 0.05f, 0.5f);
			}
			else if (NumHovered == TotalNumKeys)
			{
				KeyDrawParam->BorderTint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
				KeyDrawParam->FillTint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
			else
			{
				KeyDrawParam->BorderTint = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
			}

			// Color keys with overlaps with a red border
			if (NumOverlaps > 0)
			{
				KeyDrawParam->BorderTint = FLinearColor(0.83f, 0.12f, 0.12f, 1.0f); // Red
			}

			const ESlateDrawEffect KeyDrawEffects = ValidKeyRange.Contains(KeyRange) ? DrawEffects : ESlateDrawEffect::DisabledEffect;

			// draw border
			static FVector2D ThrobAmount(12.f, 12.f);
			const FVector2D KeySize = bSelected ? SequencerSectionConstants::KeySize + ThrobAmount * KeyThrobScaleValue : SequencerSectionConstants::KeySize;

			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				// always draw selected keys on top of other keys
				bSelected ? KeyLayer + 1 : KeyLayer,
				// Center the key along Y.  Ensure the middle of the key is at the actual key time
				KeyAreaGeometry.ToPaintGeometry(
					FVector2D(
						FinalKeyPosition - FMath::CeilToFloat(KeySize.X / 2.0f),
						((KeyAreaGeometry.GetLocalSize().Y / 2.0f) - (KeySize.Y / 2.0f))
					),
					KeySize
				),
				KeyDrawParam->BorderBrush,
				KeyDrawEffects,
				KeyDrawParam->BorderTint
			);

			// draw fill
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				// always draw selected keys on top of other keys
				bSelected ? KeyLayer + 2 : KeyLayer + 1,
				// Center the key along Y.  Ensure the middle of the key is at the actual key time
				KeyAreaGeometry.ToPaintGeometry(
					KeyDrawParam->FillOffset + 
					FVector2D(
						(FinalKeyPosition - FMath::CeilToFloat((KeySize.X / 2.0f) - BrushBorderWidth)),
						((KeyAreaGeometry.GetLocalSize().Y / 2.0f) - ((KeySize.Y / 2.0f) - BrushBorderWidth))
					),
					KeySize - 2.0f * BrushBorderWidth
				),
				KeyDrawParam->FillBrush,
				KeyDrawEffects,
				KeyDrawParam->FillTint
			);
		}

		if (PreviousClipState.IsSet())
		{
			InPainter.DrawElements.GetClippingManager().PushClippingState(PreviousClipState.GetValue());
		}

		InPainter.LayerId = KeyLayer + 2;
	}
}


void SSequencerSection::PaintEasingHandles( FSequencerSectionPainter& InPainter, FLinearColor SelectionColor, const ISequencerHotspot* Hotspot ) const
{
	if (!SectionInterface->GetSectionObject()->GetBlendType().IsValid())
	{
		return;
	}

	TArray<FSectionHandle> AllUnderlappingSections;
	if (IsSectionHighlighted(FSectionHandle(ParentSectionArea, SectionIndex), Hotspot))
	{
		AllUnderlappingSections.Add(FSectionHandle(ParentSectionArea, SectionIndex));
	}

	for (const FSequencerOverlapRange& Segment : UnderlappingSegments)
	{
		for (FSectionHandle Section : Segment.Sections)
		{
			if (IsSectionHighlighted(Section, Hotspot) && !AllUnderlappingSections.Contains(Section))
			{
				AllUnderlappingSections.Add(Section);
			}
		}
	}

	FTimeToPixel TimeToPixelConverter = InPainter.GetTimeConverter();
	for (FSectionHandle Handle : AllUnderlappingSections)
	{
		UMovieSceneSection* UnderlappingSectionObj = Handle.GetSectionInterface()->GetSectionObject();
		if (UnderlappingSectionObj->GetRange() == TRange<FFrameNumber>::All())
		{
			continue;
		}

		bool bDrawThisSectionsHandles = true;
		bool bLeftHandleActive = false;
		bool bRightHandleActive = false;

		// Get the hovered/selected state for the section handles from the hotspot
		if (Hotspot)
		{
			if (Hotspot->GetType() == ESequencerHotspot::EaseInHandle || Hotspot->GetType() == ESequencerHotspot::EaseOutHandle)
			{
				const FSectionEasingHandleHotspot* EasingHotspot = static_cast<const FSectionEasingHandleHotspot*>(Hotspot);

				bDrawThisSectionsHandles = EasingHotspot->Section == Handle;
				bLeftHandleActive = Hotspot->GetType() == ESequencerHotspot::EaseInHandle;
				bRightHandleActive = Hotspot->GetType() == ESequencerHotspot::EaseOutHandle;
			}
			else if (Hotspot->GetType() == ESequencerHotspot::EasingArea)
			{
				const FSectionEasingAreaHotspot* EasingAreaHotspot = static_cast<const FSectionEasingAreaHotspot*>(Hotspot);
				for (const FEasingAreaHandle& Easing : EasingAreaHotspot->Easings)
				{
					if (Easing.Section == Handle)
					{
						if (Easing.EasingType == ESequencerEasingType::In)
						{
							bLeftHandleActive = true;
						}
						else
						{
							bRightHandleActive = true;
						}

						if (bLeftHandleActive && bRightHandleActive)
						{
							break;
						}
					}
				}
			}
		}

		if (!bDrawThisSectionsHandles)
		{
			continue;
		}

		const ESlateDrawEffect DrawEffects = InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		const FSlateBrush* EasingHandle = FEditorStyle::GetBrush("Sequencer.Section.EasingHandle");
		FVector2D HandleSize(10.f, 10.f);

		if (UnderlappingSectionObj->HasStartFrame())
		{
			TRange<FFrameNumber> EaseInRange = UnderlappingSectionObj->GetEaseInRange();
			// Always draw handles if the section is highlighted, even if there is no range (to allow manual adjustment)
			FFrameNumber HandleFrame = EaseInRange.IsEmpty() ? UnderlappingSectionObj->GetInclusiveStartFrame() : MovieScene::DiscreteExclusiveUpper(EaseInRange);
			FVector2D HandlePos(TimeToPixelConverter.FrameToPixel(HandleFrame), 0.f);
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				// always draw selected keys on top of other keys
				InPainter.LayerId,
				// Center the key along X.  Ensure the middle of the key is at the actual key time
				InPainter.SectionGeometry.ToPaintGeometry(
					HandlePos - FVector2D(HandleSize.X*0.5f, 0.f),
					HandleSize
				),
				EasingHandle,
				DrawEffects,
				(bLeftHandleActive ? SelectionColor : EasingHandle->GetTint(FWidgetStyle()))
			);
		}

		if (UnderlappingSectionObj->HasEndFrame())
		{
			TRange<FFrameNumber> EaseOutRange = UnderlappingSectionObj->GetEaseOutRange();

			// Always draw handles if the section is highlighted, even if there is no range (to allow manual adjustment)
			FFrameNumber HandleFrame = EaseOutRange.IsEmpty() ? UnderlappingSectionObj->GetExclusiveEndFrame() : MovieScene::DiscreteInclusiveLower(EaseOutRange);
			FVector2D    HandlePos   = FVector2D(TimeToPixelConverter.FrameToPixel(HandleFrame), 0.f);

			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				// always draw selected keys on top of other keys
				InPainter.LayerId,
				// Center the key along X.  Ensure the middle of the key is at the actual key time
				InPainter.SectionGeometry.ToPaintGeometry(
					HandlePos - FVector2D(HandleSize.X*0.5f, 0.f),
					HandleSize
				),
				EasingHandle,
				DrawEffects,
				(bRightHandleActive ? SelectionColor : EasingHandle->GetTint(FWidgetStyle()))
			);
		}
	}
}


void SSequencerSection::DrawSectionHandles( const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, ESlateDrawEffect DrawEffects, FLinearColor SelectionColor, const ISequencerHotspot* Hotspot ) const
{
	UMovieSceneSection* ThisSection = SectionInterface->GetSectionObject();
	if (!ThisSection)
	{
		return;
	}

	TOptional<FSlateClippingState> PreviousClipState = OutDrawElements.GetClippingState();
	OutDrawElements.PopClip();

	OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry.GetLayoutBoundingRect()));

	TArray<FSectionHandle> AllUnderlappingSections;
	AllUnderlappingSections.Add(FSectionHandle(ParentSectionArea, SectionIndex));
	for (const FSequencerOverlapRange& Segment : UnderlappingSegments)
	{
		for (FSectionHandle Section : Segment.Sections)
		{
			AllUnderlappingSections.AddUnique(Section);
		}
	}

	FGeometry SectionGeometryWithoutHandles = MakeSectionGeometryWithoutHandles(AllottedGeometry, SectionInterface);
	FTimeToPixel TimeToPixelConverter = ConstructTimeConverterForSection(SectionGeometryWithoutHandles, *ThisSection, GetSequencer());

	for (FSectionHandle Handle : AllUnderlappingSections)
	{
		TSharedRef<ISequencerSection> UnderlappingSection =  Handle.TrackNode->GetSections()[Handle.SectionIndex];
		UMovieSceneSection* UnderlappingSectionObj = UnderlappingSection->GetSectionObject();
		if (!UnderlappingSection->SectionIsResizable() || UnderlappingSectionObj->GetRange() == TRange<FFrameNumber>::All())
		{
			continue;
		}

		bool bDrawThisSectionsHandles = (UnderlappingSectionObj == ThisSection && HandleOffsetPx != 0) || IsSectionHighlighted(Handle, Hotspot);
		bool bLeftHandleActive = false;
		bool bRightHandleActive = false;

		// Get the hovered/selected state for the section handles from the hotspot
		if (Hotspot && (
			Hotspot->GetType() == ESequencerHotspot::SectionResize_L ||
			Hotspot->GetType() == ESequencerHotspot::SectionResize_R))
		{
			const FSectionResizeHotspot* ResizeHotspot = static_cast<const FSectionResizeHotspot*>(Hotspot);
			if (ResizeHotspot->Section == Handle)
			{
				bDrawThisSectionsHandles = true;
				bLeftHandleActive = Hotspot->GetType() == ESequencerHotspot::SectionResize_L;
				bRightHandleActive = Hotspot->GetType() == ESequencerHotspot::SectionResize_R;
			}
			else
			{
				bDrawThisSectionsHandles = false;
			}
		}

		if (!bDrawThisSectionsHandles)
		{
			continue;
		}

		const float ThisHandleOffset = UnderlappingSectionObj == ThisSection ? HandleOffsetPx : 0.f;
		FVector2D GripSize( UnderlappingSection->GetSectionGripSize(), AllottedGeometry.Size.Y );

		float Opacity = 0.5f;
		if (ThisHandleOffset != 0)
		{
			Opacity = FMath::Clamp(.5f + ThisHandleOffset / GripSize.X * .5f, .5f, 1.f);
		}

		const FSlateBrush* LeftGripBrush = FEditorStyle::GetBrush("Sequencer.Section.GripLeft");
		const FSlateBrush* RightGripBrush = FEditorStyle::GetBrush("Sequencer.Section.GripRight");

		// Left Grip
		if (UnderlappingSectionObj->HasStartFrame())
		{
			FGeometry SectionRectLeft = SectionGeometryWithoutHandles.MakeChild(
				FVector2D( TimeToPixelConverter.FrameToPixel(UnderlappingSectionObj->GetInclusiveStartFrame()) - ThisHandleOffset, 0.f ),
				GripSize
			);
			FSlateDrawElement::MakeBox
			(
				OutDrawElements,
				LayerId,
				SectionRectLeft.ToPaintGeometry(),
				LeftGripBrush,
				DrawEffects,
				(bLeftHandleActive ? SelectionColor : LeftGripBrush->GetTint(FWidgetStyle())).CopyWithNewOpacity(Opacity)
			);
		}

		// Right Grip
		if (UnderlappingSectionObj->HasEndFrame())
		{
			FGeometry SectionRectRight = SectionGeometryWithoutHandles.MakeChild(
				FVector2D( TimeToPixelConverter.FrameToPixel(UnderlappingSectionObj->GetExclusiveEndFrame()) - UnderlappingSection->GetSectionGripSize() + ThisHandleOffset, 0 ), 
				GripSize
			);
			FSlateDrawElement::MakeBox
			(
				OutDrawElements,
				LayerId,
				SectionRectRight.ToPaintGeometry(),
				RightGripBrush,
				DrawEffects,
				(bRightHandleActive ? SelectionColor : RightGripBrush->GetTint(FWidgetStyle())).CopyWithNewOpacity(Opacity)
			);
		}
	}

	OutDrawElements.PopClip();
	if (PreviousClipState.IsSet())
	{
		OutDrawElements.GetClippingManager().PushClippingState(PreviousClipState.GetValue());
	}
}

void SSequencerSection::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{	
	if( GetVisibility() == EVisibility::Visible )
	{
		Layout = FSectionLayout(*ParentSectionArea, SectionIndex);

		// Update cached key area key positions by retaining existing caches where possible
		TMap<FSectionLayoutElement, TArray<FSequencerCachedKeys, TInlineAllocator<1>>, FDefaultSetAllocator, FLayoutElementKeyFuncs> OldCachedKeyAreaPositions;
		Swap(OldCachedKeyAreaPositions, CachedKeyAreaPositions);

		FFrameRate TickResolution = GetSequencer().GetFocusedTickResolution();

		// Move over any existing still valid caches 
		for (const FSectionLayoutElement& LayoutElement : Layout->GetElements())
		{
			TArray<FSequencerCachedKeys, TInlineAllocator<1>>* CacheArray = CachedKeyAreaPositions.Find(LayoutElement);
			if (!CacheArray)
			{
				// A new cache needs to be created
				TArray<FSequencerCachedKeys, TInlineAllocator<1>> NewCachedKeys;
				for (TSharedRef<IKeyArea> KeyArea : LayoutElement.GetKeyAreas())
				{
					NewCachedKeys.Emplace();
					NewCachedKeys.Last().Update(KeyArea, TickResolution);
				}
				CachedKeyAreaPositions.Add(LayoutElement, MoveTemp(NewCachedKeys));
			}
			else
			{
				// We can reuse this one
				for (FSequencerCachedKeys& CachedKeys : *CacheArray)
				{
					CachedKeys.Update(CachedKeys.GetKeyArea().ToSharedRef(), TickResolution);
				}
				CachedKeyAreaPositions.Add(LayoutElement, MoveTemp(*CacheArray));
			}
		}

		UMovieSceneSection* Section = SectionInterface->GetSectionObject();
		if (Section && Section->HasStartFrame() && Section->HasEndFrame())
		{
			FTimeToPixel TimeToPixelConverter(ParentGeometry, GetSequencer().GetViewRange(), TickResolution);

			const int32 SectionLengthPx = FMath::Max(0,
				FMath::RoundToInt(
					TimeToPixelConverter.FrameToPixel(Section->GetExclusiveEndFrame())) - FMath::RoundToInt(TimeToPixelConverter.FrameToPixel(Section->GetInclusiveStartFrame())
				)
			);

			const float SectionGripSize = SectionInterface->GetSectionGripSize();
			HandleOffsetPx = FMath::Max(FMath::RoundToFloat((2*SectionGripSize - SectionLengthPx) * .5f), 0.f);
		}
		else
		{
			HandleOffsetPx = 0;
		}

		FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );
		SectionInterface->Tick(SectionGeometry, ParentGeometry, InCurrentTime, InDeltaTime);

		UpdateUnderlappingSegments();
	}
}

FReply SSequencerSection::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FSequencer& Sequencer = GetSequencer();

	TArrayView<const FSequencerSelectedKey> HoveredKeys;
	
	// The hovered key is defined from the sequencer hotspot
	TSharedPtr<ISequencerHotspot> Hotspot = GetSequencer().GetHotspot();
	if (Hotspot.IsValid() && Hotspot->GetType() == ESequencerHotspot::Key)
	{
		HoveredKeys = static_cast<FKeyHotspot*>(Hotspot.Get())->Keys;
	}

	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		GEditor->BeginTransaction(NSLOCTEXT("Sequencer", "CreateKey_Transaction", "Create Key"));

		// Generate a key and set it as the PressedKey
		TArray<FSequencerSelectedKey> NewKeys;
		CreateKeysUnderMouse(MouseEvent.GetScreenSpacePosition(), MyGeometry, HoveredKeys, NewKeys);

		if (NewKeys.Num())
		{
			Sequencer.GetSelection().EmptySelectedKeys();
			for (const FSequencerSelectedKey& NewKey : NewKeys)
			{
				Sequencer.GetSelection().AddToSelection(NewKey);
			}

			// Pass the event to the tool to copy the hovered key and move it
			GetSequencer().SetHotspot( MakeShared<FKeyHotspot>(MoveTemp(NewKeys)) );

			// Return unhandled so that the EditTool can handle the mouse down based on the newly created keyframe and prepare to move it
			return FReply::Unhandled();
		}
	}

	return FReply::Unhandled();
}


FGeometry SSequencerSection::MakeSectionGeometryWithoutHandles( const FGeometry& AllottedGeometry, const TSharedPtr<ISequencerSection>& InSectionInterface ) const
{
	return AllottedGeometry.MakeChild(
		AllottedGeometry.GetLocalSize() - FVector2D( HandleOffsetPx*2, 0.0f ),
		FSlateLayoutTransform(FVector2D(HandleOffsetPx, 0 ))
	);
}

void SSequencerSection::UpdateUnderlappingSegments()
{
	UMovieSceneSection* ThisSection = SectionInterface->GetSectionObject();
	UMovieSceneTrack* Track = ThisSection ? ThisSection->GetTypedOuter<UMovieSceneTrack>() : nullptr;
	if (!Track)
	{
		UnderlappingSegments.Reset();
		UnderlappingEasingSegments.Reset();
	}
	else if (Track->GetSignature() != CachedTrackSignature)
	{
		UnderlappingSegments = ParentSectionArea->GetUnderlappingSections(ThisSection);
		UnderlappingEasingSegments = ParentSectionArea->GetEasingSegmentsForSection(ThisSection);
		CachedTrackSignature = Track->GetSignature();
	}
}

FReply SSequencerSection::OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		TArray<FSequencerSelectedKey> Keys;
		GetKeysUnderMouse(MouseEvent.GetScreenSpacePosition(), MyGeometry, Keys);
		if (Keys.Num() == 1 && Keys[0].KeyHandle.IsSet())
		{
			return SectionInterface->OnKeyDoubleClicked(Keys[0].KeyHandle.GetValue());
		}

		FReply Reply = SectionInterface->OnSectionDoubleClicked( MyGeometry, MouseEvent );
		if (!Reply.IsEventHandled())
		{
			// Find the object binding this node is underneath
			FGuid ObjectBinding;
			if (ParentSectionArea.IsValid())
			{
				TSharedPtr<FSequencerObjectBindingNode> ObjectBindingNode = ParentSectionArea->FindParentObjectBindingNode();
				if (ObjectBindingNode.IsValid())
				{
					ObjectBinding = ObjectBindingNode->GetObjectBinding();
				}
			}

			Reply = SectionInterface->OnSectionDoubleClicked(MyGeometry, MouseEvent, ObjectBinding);
		}

		if (Reply.IsEventHandled())
		{
			return Reply;
		}

		GetSequencer().ZoomToSelectedSections();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}


FReply SSequencerSection::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Checked for hovered key
	TArray<FSequencerSelectedKey> KeysUnderMouse;
	GetKeysUnderMouse( MouseEvent.GetScreenSpacePosition(), MyGeometry, KeysUnderMouse );
	if ( KeysUnderMouse.Num() )
	{
		GetSequencer().SetHotspot( MakeShared<FKeyHotspot>( MoveTemp(KeysUnderMouse) ) );
	}
	// Check other interaction points in order of importance
	else if (
		!CheckForEasingHandleInteraction(MouseEvent, MyGeometry) &&
		!CheckForEdgeInteraction(MouseEvent, MyGeometry) &&
		!CheckForEasingAreaInteraction(MouseEvent, MyGeometry))
	{
		// If nothing was hit, we just hit the section
		GetSequencer().SetHotspot( MakeShareable( new FSectionHotspot(FSectionHandle(ParentSectionArea, SectionIndex))) );
	}

	return FReply::Unhandled();
}
	
FReply SSequencerSection::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		GEditor->EndTransaction();

		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SSequencerSection::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	SCompoundWidget::OnMouseLeave( MouseEvent );
	GetSequencer().SetHotspot(nullptr);
}

static float SectionThrobDurationSeconds = 1.f;
void SSequencerSection::ThrobSectionSelection(int32 ThrobCount)
{
	SectionSelectionThrobEndTime = FPlatformTime::Seconds() + ThrobCount*SectionThrobDurationSeconds;
}

static float KeyThrobDurationSeconds = .5f;
void SSequencerSection::ThrobKeySelection(int32 ThrobCount)
{
	KeySelectionThrobEndTime = FPlatformTime::Seconds() + ThrobCount*KeyThrobDurationSeconds;
}

float EvaluateThrob(float Alpha)
{
	return .5f - FMath::Cos(FMath::Pow(Alpha, 0.5f) * 2 * PI) * .5f;
}

float SSequencerSection::GetSectionSelectionThrobValue()
{
	double CurrentTime = FPlatformTime::Seconds();

	if (SectionSelectionThrobEndTime > CurrentTime)
	{
		float Difference = SectionSelectionThrobEndTime - CurrentTime;
		return EvaluateThrob(1.f - FMath::Fmod(Difference, SectionThrobDurationSeconds));
	}

	return 0.f;
}

float SSequencerSection::GetKeySelectionThrobValue()
{
	double CurrentTime = FPlatformTime::Seconds();

	if (KeySelectionThrobEndTime > CurrentTime)
	{
		float Difference = KeySelectionThrobEndTime - CurrentTime;
		return EvaluateThrob(1.f - FMath::Fmod(Difference, KeyThrobDurationSeconds));
	}

	return 0.f;
}

bool SSequencerSection::IsSectionHighlighted(FSectionHandle InSectionHandle, const ISequencerHotspot* Hotspot)
{
	if (!Hotspot)
	{
		return false;
	}

	switch(Hotspot->GetType())
	{
	case ESequencerHotspot::Key:
		return static_cast<const FKeyHotspot*>(Hotspot)->Keys.ContainsByPredicate([InSectionHandle](const FSequencerSelectedKey& Key){ return Key.Section == InSectionHandle.GetSectionObject(); });

	case ESequencerHotspot::Section:
		return static_cast<const FSectionHotspot*>(Hotspot)->Section == InSectionHandle;

	case ESequencerHotspot::SectionResize_L:
	case ESequencerHotspot::SectionResize_R:
		return static_cast<const FSectionResizeHotspot*>(Hotspot)->Section == InSectionHandle;

	case ESequencerHotspot::EaseInHandle:
	case ESequencerHotspot::EaseOutHandle:
		return static_cast<const FSectionEasingHandleHotspot*>(Hotspot)->Section == InSectionHandle;

	case ESequencerHotspot::EasingArea:
		return static_cast<const FSectionEasingAreaHotspot*>(Hotspot)->Contains(InSectionHandle);

	default:
		return false;
	}
}
