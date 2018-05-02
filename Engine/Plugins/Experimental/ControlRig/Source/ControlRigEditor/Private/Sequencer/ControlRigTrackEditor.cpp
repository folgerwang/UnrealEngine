// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigTrackEditor.h"
#include "Sequencer/MovieSceneControlRigSection.h"
#include "Sequencer/MovieSceneControlRigTrack.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "ISectionLayoutBuilder.h"
#include "SequencerSectionPainter.h"
#include "ControlRig.h"
#include "SequencerUtilities.h"
#include "Sequencer/ControlRigSequence.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "LevelEditor.h"
#include "EditorStyleSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "MovieSceneTimeHelpers.h"

namespace ControlRigEditorConstants
{
	// @todo Sequencer Allow this to be customizable
	const uint32 AnimationTrackHeight = 20;
}

#define LOCTEXT_NAMESPACE "FControlRigTrackEditor"

/** Class for animation sections */
class FControlRigSection
	: public ISequencerSection
	, public TSharedFromThis<FControlRigSection>
{
public:

	/** Constructor. */
	FControlRigSection(UMovieSceneSection& InSection, TSharedRef<ISequencer> InSequencer)
		: Sequencer(InSequencer)
		, Section(*CastChecked<UMovieSceneControlRigSection>(&InSection))
	{
	}

	/** Virtual destructor. */
	virtual ~FControlRigSection() { }

public:

	// ISequencerSection interface

	virtual UMovieSceneSection* GetSectionObject() override
	{
		return &Section;
	}

	virtual FText GetSectionTitle() const override
	{
		if (Section.GetSequence() != nullptr)
		{
			return Section.GetSequence()->GetDisplayName();
		}
		return LOCTEXT("NoSequenceSection", "No Sequence");
	}

	virtual float GetSectionHeight() const override
	{
		return (float)ControlRigEditorConstants::AnimationTrackHeight;
	}

	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override
	{
		int32 LayerId = InPainter.PaintSectionBackground();

		ESlateDrawEffect DrawEffects = InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		TRange<FFrameNumber> SectionRange = Section.GetRange();
		if (SectionRange.GetLowerBound().IsOpen() || SectionRange.GetUpperBound().IsOpen())
		{
			return InPainter.LayerId;
		}
		const FFrameNumber SectionStartFrame = Section.GetInclusiveStartFrame();
		const FFrameNumber SectionEndFrame   = Section.GetExclusiveEndFrame();
		const int32        SectionSize       = MovieScene::DiscreteSize(SectionRange);

		if (SectionSize <= 0)
		{
			return InPainter.LayerId;
		}

		const float        PixelsPerFrame    = InPainter.SectionGeometry.Size.X / float(SectionSize);

		UMovieSceneSequence* InnerSequence = Section.GetSequence();
		if (InnerSequence)
		{
			UMovieScene*         MovieScene    = InnerSequence->GetMovieScene();
			TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();

			FMovieSceneSequenceTransform InnerToOuterTransform = Section.OuterToInnerTransform().Inverse();

			const FFrameNumber StartOffset = (FFrameTime(Section.Parameters.GetStartFrameOffset()) * InnerToOuterTransform).FloorToFrame();
			if (StartOffset < 0)
			{
				// add dark tint for left out-of-bounds
				FSlateDrawElement::MakeBox(
					InPainter.DrawElements,
					InPainter.LayerId++,
					InPainter.SectionGeometry.ToPaintGeometry(
						FVector2D(0.0f, 0.f),
						FVector2D(-StartOffset.Value * PixelsPerFrame, InPainter.SectionGeometry.Size.Y)
					),
					FEditorStyle::GetBrush("WhiteBrush"),
					DrawEffects,
					FLinearColor::Black.CopyWithNewOpacity(0.5f)
				);

				// add green line for playback start
				FSlateDrawElement::MakeBox(
					InPainter.DrawElements,
					InPainter.LayerId++,
					InPainter.SectionGeometry.ToPaintGeometry(
						FVector2D(-StartOffset.Value * PixelsPerFrame, 0.f),
						FVector2D(1.0f, InPainter.SectionGeometry.Size.Y)
					),
					FEditorStyle::GetBrush("WhiteBrush"),
					DrawEffects,
					FColor(32, 128, 32)	// 120, 75, 50 (HSV)
				);
			}

			const FFrameNumber InnerEndFrame = PlaybackRange.GetUpperBound().IsInclusive() ? PlaybackRange.GetUpperBoundValue()+1 : PlaybackRange.GetUpperBoundValue();
			const FFrameTime   PlaybackEnd   = FFrameTime(InnerEndFrame) * InnerToOuterTransform;

			if (SectionRange.Contains(PlaybackEnd.FrameNumber))
			{
				// add dark tint for right out-of-bounds
				const double EndFrameRelativeToStart = (PlaybackEnd-SectionStartFrame).AsDecimal();
				FSlateDrawElement::MakeBox(
					InPainter.DrawElements,
					InPainter.LayerId++,
					InPainter.SectionGeometry.ToPaintGeometry(
						FVector2D(EndFrameRelativeToStart * PixelsPerFrame, 0.f),
						FVector2D((SectionSize - EndFrameRelativeToStart) * PixelsPerFrame, InPainter.SectionGeometry.Size.Y)
					),
					FEditorStyle::GetBrush("WhiteBrush"),
					DrawEffects,
					FLinearColor::Black.CopyWithNewOpacity(0.5f)
				);


				// add red line for playback end
				FSlateDrawElement::MakeBox(
					InPainter.DrawElements,
					InPainter.LayerId++,
					InPainter.SectionGeometry.ToPaintGeometry(
						FVector2D(EndFrameRelativeToStart * PixelsPerFrame, 0.f),
						FVector2D(1.0f, InPainter.SectionGeometry.Size.Y)
					),
					FEditorStyle::GetBrush("WhiteBrush"),
					DrawEffects,
					FColor(128, 32, 32)	// 0, 75, 50 (HSV)
				);
			}
		}

		return LayerId;
	}

	virtual FReply OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent, const FGuid& ObjectBinding) override
	{
		Sequencer.Pin()->FocusSequenceInstance(Section);

		return FReply::Handled();
	}

private:

	/** The sequencer we are editing in */
	TWeakPtr<ISequencer> Sequencer;

	/** The section we are visualizing */
	UMovieSceneControlRigSection& Section;
};

FControlRigTrackEditor::FControlRigTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FSubTrackEditor( InSequencer ) 
{ 
}

TSharedRef<ISequencerTrackEditor> FControlRigTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FControlRigTrackEditor( InSequencer ) );
}

bool FControlRigTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return (InSequence != nullptr) && (InSequence->GetClass()->GetName() == TEXT("ControlRigSequence"));
}

bool FControlRigTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneControlRigTrack::StaticClass();
}

TSharedRef<ISequencerSection> FControlRigTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );
	
	return MakeShareable( new FControlRigSection(SectionObject, GetSequencer().ToSharedRef()) );
}

void FControlRigTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	// do nothing
}

void FControlRigTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(USkeletalMeshComponent::StaticClass()) || ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

		UMovieSceneTrack* Track = nullptr;

		MenuBuilder.AddSubMenu(
			LOCTEXT("AddControlRig", "Animation ControlRig"), NSLOCTEXT("Sequencer", "AddControlRigTooltip", "Adds an animation ControlRig track."),
			FNewMenuDelegate::CreateRaw(this, &FControlRigTrackEditor::AddControlRigSubMenu, ObjectBinding, Track)
		);
	}
}

TSharedRef<SWidget> FControlRigTrackEditor::BuildControlRigSubMenu(FGuid ObjectBinding, UMovieSceneTrack* Track)
{
	FMenuBuilder MenuBuilder(true, nullptr);
	
	AddControlRigSubMenu(MenuBuilder, ObjectBinding, Track);

	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> FControlRigTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	if (ObjectBinding.IsValid())
	{
		// Create a container edit box
		return SNew(SHorizontalBox)

			// Add the sub sequence combo box
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				FSequencerUtilities::MakeAddButton(LOCTEXT("SubText", "Sequence"), FOnGetContent::CreateSP(this, &FControlRigTrackEditor::HandleAddSubSequenceComboButtonGetMenuContent, ObjectBinding, Track), Params.NodeIsHovered)
			];
	}
	else
	{
		return TSharedPtr<SWidget>();
	}
}

void FControlRigTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	
}

void FControlRigTrackEditor::AddControlRigSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, UMovieSceneTrack* Track)
{
	MenuBuilder.BeginSection(TEXT("ChooseSequence"), LOCTEXT("ChooseSequence", "Choose Sequence"));
	{
		FAssetPickerConfig AssetPickerConfig;
		{
			AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FControlRigTrackEditor::OnSequencerAssetSelected, ObjectBinding, Track);
			AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateRaw(this, &FControlRigTrackEditor::OnSequencerAssetEnterPressed, ObjectBinding, Track);
			AssetPickerConfig.bAllowNullSelection = false;
			AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
			AssetPickerConfig.Filter.bRecursiveClasses = true;
			AssetPickerConfig.Filter.ClassNames.Add(UControlRigSequence::StaticClass()->GetFName());
		}

		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		TSharedPtr<SBox> MenuEntry = SNew(SBox)
			.WidthOverride(300.0f)
			.HeightOverride(300.0f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			];

		MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();
}

void FControlRigTrackEditor::OnSequencerAssetSelected(const FAssetData& AssetData, FGuid ObjectBinding, UMovieSceneTrack* Track)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();

	if (SelectedObject && SelectedObject->IsA(UControlRigSequence::StaticClass()))
	{
		UControlRigSequence* Sequence = CastChecked<UControlRigSequence>(AssetData.GetAsset());
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FControlRigTrackEditor::AddKeyInternal, ObjectBinding, Sequence, Track));
	}
}

void FControlRigTrackEditor::OnSequencerAssetEnterPressed(const TArray<FAssetData>& AssetData, FGuid ObjectBinding, UMovieSceneTrack* Track)
{
	if (AssetData.Num() > 0)
	{
		OnSequencerAssetSelected(AssetData[0].GetAsset(), ObjectBinding, Track);
	}
}

FKeyPropertyResult FControlRigTrackEditor::AddKeyInternal(FFrameNumber KeyTime, FGuid ObjectBinding, UControlRigSequence* Sequence, UMovieSceneTrack* Track)
{
	FKeyPropertyResult KeyPropertyResult;
	bool bHandleCreated = false;
	bool bTrackCreated = false;
	bool bTrackModified = false;

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (SequencerPtr.IsValid())
	{
		UObject* Object = SequencerPtr->FindSpawnedObjectOrTemplate(ObjectBinding);

		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(Object);
		FGuid ObjectHandle = HandleResult.Handle;
		KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;
		if (ObjectBinding.IsValid())
		{
			if (!Track)
			{
				Track = AddTrack(GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene(), ObjectBinding, UMovieSceneControlRigTrack::StaticClass(), NAME_None);
				KeyPropertyResult.bTrackCreated = true;
			}

			if (ensure(Track))
			{
				Cast<UMovieSceneControlRigTrack>(Track)->AddNewControlRig(KeyTime, Sequence);
				KeyPropertyResult.bTrackModified = true;
			}
		}
	}

	return KeyPropertyResult;
}


TSharedRef<SWidget> FControlRigTrackEditor::HandleAddSubSequenceComboButtonGetMenuContent(FGuid ObjectBinding, UMovieSceneTrack* InTrack)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	AddControlRigSubMenu(MenuBuilder, ObjectBinding, InTrack);

	return MenuBuilder.MakeWidget();
}

const FSlateBrush* FControlRigTrackEditor::GetIconBrush() const
{
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
