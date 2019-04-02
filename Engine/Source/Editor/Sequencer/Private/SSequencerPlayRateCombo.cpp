// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SSequencerPlayRateCombo.h"
#include "Sequencer.h"
#include "SSequencer.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "ScopedTransaction.h"
#include "Widgets/SFrameRateEntryBox.h"
#include "EditorFontGlyphs.h"
#include "Misc/Timecode.h"

#define LOCTEXT_NAMESPACE "SSequencerPlayRateCombo"

void SSequencerPlayRateCombo::Construct(const FArguments& InArgs, TWeakPtr<FSequencer> InWeakSequencer, TWeakPtr<SSequencer> InWeakSequencerWidget)
{
	WeakSequencer = InWeakSequencer;
	WeakSequencerWidget = InWeakSequencerWidget;

	FName BlockStyle = EMultiBlockLocation::ToName(ISlateStyle::Join( InArgs._StyleName, ".Button" ), InArgs._BlockLocation);
	FName ColorStyle = ISlateStyle::Join( InArgs._StyleName, ".SToolBarComboButtonBlock.ComboButton.Color" );

	SetToolTipText(MakeAttributeSP(this, &SSequencerPlayRateCombo::GetToolTipText));

	ChildSlot
	.VAlign(VAlign_Fill)
	[
		SNew(SComboButton)
		.ContentPadding(FMargin(2.f, 0.f))
		.VAlign(VAlign_Fill)
		.ButtonStyle( InArgs._StyleSet, BlockStyle )
		.ForegroundColor( InArgs._StyleSet->GetSlateColor( ColorStyle ) )
		.OnGetMenuContent(this, &SSequencerPlayRateCombo::OnCreateMenu)
		.ButtonContent()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Fill)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(this, &SSequencerPlayRateCombo::GetFrameRateText)
				.TextStyle( InArgs._StyleSet, ISlateStyle::Join( InArgs._StyleName, ".Label" ) )
			]

			+ SHorizontalBox::Slot()
			.Padding(3.f, 0.f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Visibility(this, &SSequencerPlayRateCombo::GetFrameLockedVisibility)
				.TextStyle( InArgs._StyleSet, ISlateStyle::Join( InArgs._StyleName, ".Label" ) )
				.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
				.Text(FEditorFontGlyphs::Lock)
			]

			+ SHorizontalBox::Slot()
			.Padding(0.f, 0.f, 3.f, 0.f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.ToolTipText(this, &SSequencerPlayRateCombo::GetFrameRateErrorDescription)
				.Visibility(this, &SSequencerPlayRateCombo::GetFrameRateErrorVisibility)
				.TextStyle( InArgs._StyleSet, ISlateStyle::Join( InArgs._StyleName, ".Label" ) )
				.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
				.Text(FEditorFontGlyphs::Exclamation_Triangle)
			]
		]
	];

	ChildSlot.Padding(InArgs._StyleSet->GetMargin(ISlateStyle::Join( InArgs._StyleName, ".SToolBarComboButtonBlock.Padding" )));
}

EVisibility SSequencerPlayRateCombo::GetFrameLockedVisibility() const
{
	TSharedPtr<FSequencer> Sequencer  = WeakSequencer.Pin();
	UMovieSceneSequence*   Sequence   = Sequencer.IsValid() ? Sequencer->GetFocusedMovieSceneSequence() : nullptr;
	return Sequence && Sequence->GetMovieScene()->GetEvaluationType() == EMovieSceneEvaluationType::FrameLocked ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

EVisibility SSequencerPlayRateCombo::GetFrameRateErrorVisibility() const
{
	TSharedPtr<FSequencer> Sequencer  = WeakSequencer.Pin();
	if (!Sequencer.IsValid() || Sequencer->GetFocusedDisplayRate().IsMultipleOf(Sequencer->GetFocusedTickResolution()))
	{
		return EVisibility::Collapsed;
	}
	return EVisibility::HitTestInvisible;
}

FText SSequencerPlayRateCombo::GetFrameRateErrorDescription() const
{
	TSharedPtr<FSequencer> Sequencer  = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		FFrameRate DisplayRate = Sequencer->GetFocusedDisplayRate();
		FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();

		const FCommonFrameRateInfo* DisplayRateInfo     = FCommonFrameRates::Find(DisplayRate);
		const FCommonFrameRateInfo* TickResolutionInfo  = FCommonFrameRates::Find(TickResolution);

		FText DisplayRateText     = DisplayRateInfo     ? DisplayRateInfo->DisplayName    : FText::Format(LOCTEXT("DisplayRateFormat", "{0} fps"), DisplayRate.AsDecimal());
		FText TickResolutionText  = TickResolutionInfo  ? TickResolutionInfo->DisplayName : FText::Format(LOCTEXT("TickResolutionFormat", "{0} ticks every second"), TickResolution.AsDecimal());

		return FText::Format(LOCTEXT("FrameRateErrorDescription", "The current display rate of {0} is incompatible with this sequence's tick resolution of {1} ticks per second."), DisplayRateText, TickResolutionText);
	}

	return FText();
}

TSharedRef<SWidget> SSequencerPlayRateCombo::OnCreateMenu()
{
	TSharedPtr<FSequencer> Sequencer       = WeakSequencer.Pin();
	TSharedPtr<SSequencer> SequencerWidget = WeakSequencerWidget.Pin();
	if (!Sequencer.IsValid() || !SequencerWidget.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	FMenuBuilder MenuBuilder(true, nullptr);

	FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();

	TArray<FCommonFrameRateInfo> CompatibleRates;
	for (const FCommonFrameRateInfo& Info : FCommonFrameRates::GetAll())
	{
		if (Info.FrameRate.IsMultipleOf(TickResolution))
		{
			CompatibleRates.Add(Info);
		}
	}

	CompatibleRates.Sort(
		[=](const FCommonFrameRateInfo& A, const FCommonFrameRateInfo& B)
		{
			return A.FrameRate.AsDecimal() < B.FrameRate.AsDecimal();
		}
	);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("RecommendedRates", "Sequence Display Rate"));
	{
		for (const FCommonFrameRateInfo& Info : CompatibleRates)
		{
			AddMenuEntry(MenuBuilder, Info);
		}

		MenuBuilder.AddWidget(
			SNew(SBox)
			.HAlign(HAlign_Right)
			.MaxDesiredWidth(100.f)
			[
				SNew(SFrameRateEntryBox)
				.Value(this, &SSequencerPlayRateCombo::GetDisplayRate)
				.OnValueChanged(this, &SSequencerPlayRateCombo::SetDisplayRate)
			],
			LOCTEXT("CustomFramerateDisplayLabel", "Custom")
		);

		if (CompatibleRates.Num() != FCommonFrameRates::GetAll().Num())
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("IncompatibleRates", "Incompatible Rates"),
				FText::Format(LOCTEXT("IncompatibleRates_Description", "Choose from a list of display rates that are incompatible with a resolution of {0} ticks per second"), TickResolution.AsDecimal()),
				FNewMenuDelegate::CreateSP(this, &SSequencerPlayRateCombo::PopulateIncompatibleRatesMenu)
				);
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddSubMenu(
		LOCTEXT("ShowTimesAs", "Show Time As"),
		LOCTEXT("ShowTimesAs_Description", "Change how to display times in Sequencer"),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& InMenuBuilder) {
			if (WeakSequencerWidget.IsValid())
			{
				WeakSequencerWidget.Pin()->FillTimeDisplayFormatMenu(InMenuBuilder);
			}
		})
	);

	if (Sequencer->GetRootMovieSceneSequence() == Sequencer->GetFocusedMovieSceneSequence())
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("ClockSource", "Clock Source"),
			LOCTEXT("ClockSource_Description", "Change which clock should be used when playing back this sequence"),
			FNewMenuDelegate::CreateSP(this, &SSequencerPlayRateCombo::PopulateClockSourceMenu)
		);
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("LockPlayback", "Lock to Display Rate at Runtime"),
		LOCTEXT("LockPlayback_Description", "When enabled, causes all runtime evaluation and the engine FPS to be locked to the current display frame rate"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SSequencerPlayRateCombo::OnToggleFrameLocked),
			FCanExecuteAction(),
			FGetActionCheckState::CreateSP(this, &SSequencerPlayRateCombo::OnGetFrameLockedCheckState)
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AdvancedOptions", "Advanced Options"),
		LOCTEXT("AdvancedOptions_Description", "Open advanced time-related properties for this sequence"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(SequencerWidget.Get(), &SSequencer::ShowTickResolutionOverlay)
		)
	);

	return MenuBuilder.MakeWidget();
}

void SSequencerPlayRateCombo::PopulateIncompatibleRatesMenu(FMenuBuilder& MenuBuilder)
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();

		TArray<FCommonFrameRateInfo> IncompatibleRates;
		for (const FCommonFrameRateInfo& Info : FCommonFrameRates::GetAll())
		{
			if (!Info.FrameRate.IsMultipleOf(TickResolution))
			{
				IncompatibleRates.Add(Info);
			}
		}

		IncompatibleRates.Sort(
			[=](const FCommonFrameRateInfo& A, const FCommonFrameRateInfo& B)
			{
				return A.FrameRate.AsDecimal() < B.FrameRate.AsDecimal();
			}
		);

		for (const FCommonFrameRateInfo& Info : IncompatibleRates)
		{
			AddMenuEntry(MenuBuilder, Info);
		}
	}
}

void SSequencerPlayRateCombo::PopulateClockSourceMenu(FMenuBuilder& MenuBuilder)
{
	TSharedPtr<FSequencer> Sequencer    = WeakSequencer.Pin();
	UMovieSceneSequence*   RootSequence = Sequencer.IsValid() ? Sequencer->GetRootMovieSceneSequence() : nullptr;

	const UEnum* ClockSourceEnum = StaticEnum<EUpdateClockSource>();

	check(ClockSourceEnum);

	if (RootSequence)
	{
		for (int32 Index = 0; Index < ClockSourceEnum->NumEnums() - 1; Index++)
		{
			if (!ClockSourceEnum->HasMetaData(TEXT("Hidden"), Index))
			{
				EUpdateClockSource Value = (EUpdateClockSource)ClockSourceEnum->GetValueByIndex(Index);

				MenuBuilder.AddMenuEntry(
					ClockSourceEnum->GetDisplayNameTextByIndex(Index),
					ClockSourceEnum->GetToolTipTextByIndex(Index),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(this, &SSequencerPlayRateCombo::SetClockSource, Value),
						FCanExecuteAction(),
						FIsActionChecked::CreateLambda([=]{ return RootSequence->GetMovieScene()->GetClockSource() == Value; })
					),
					NAME_None,
					EUserInterfaceActionType::RadioButton
				);
			}
		}
	}
}

void SSequencerPlayRateCombo::AddMenuEntry(FMenuBuilder& MenuBuilder, const FCommonFrameRateInfo& Info)
{
	MenuBuilder.AddMenuEntry(
		Info.DisplayName,
		Info.Description,
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SSequencerPlayRateCombo::SetDisplayRate, Info.FrameRate),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SSequencerPlayRateCombo::IsSameDisplayRate, Info.FrameRate)
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);
}

void SSequencerPlayRateCombo::SetClockSource(EUpdateClockSource NewClockSource)
{
	TSharedPtr<FSequencer> Sequencer    = WeakSequencer.Pin();
	UMovieSceneSequence*   RootSequence = Sequencer.IsValid() ? Sequencer->GetRootMovieSceneSequence() : nullptr;
	if (RootSequence)
	{
		UMovieScene* MovieScene = RootSequence->GetMovieScene();

		if (MovieScene->IsReadOnly())
		{
			return;
		}

		FScopedTransaction ScopedTransaction(LOCTEXT("SetClockSource", "Set Clock Source"));

		MovieScene->Modify();
		MovieScene->SetClockSource(NewClockSource);

		Sequencer->ResetTimeController();
	}
}

void SSequencerPlayRateCombo::SetDisplayRate(FFrameRate InFrameRate)
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	UMovieSceneSequence* FocusedSequence = Sequencer.IsValid() ? Sequencer->GetFocusedMovieSceneSequence() : nullptr;
	if (FocusedSequence)
	{
		UMovieScene* MovieScene = FocusedSequence->GetMovieScene();
		if (MovieScene->IsReadOnly())
		{
			return;
		}

		FScopedTransaction ScopedTransaction(FText::Format(LOCTEXT("SetDisplayRate", "Set Display Rate to {0}"), InFrameRate.ToPrettyText()));

		MovieScene->Modify();
		MovieScene->SetDisplayRate(InFrameRate);

		TArray<UMovieScene*> DescendantMovieScenes;
		MovieSceneHelpers::GetDescendantMovieScenes(FocusedSequence, DescendantMovieScenes);

		for (UMovieScene* DescendantMovieScene : DescendantMovieScenes)
		{
			if (DescendantMovieScene && InFrameRate != DescendantMovieScene->GetDisplayRate())
			{
				if (!DescendantMovieScene->IsReadOnly())
				{
					DescendantMovieScene->Modify();
					DescendantMovieScene->SetDisplayRate(InFrameRate);
				}
			}
		}
	}

	// Snap the local time to the new display rate
	Sequencer->SetLocalTime(Sequencer->GetLocalTime().Time, ESnapTimeMode::STM_Interval);
}

FFrameRate SSequencerPlayRateCombo::GetDisplayRate() const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	return Sequencer.IsValid() ? Sequencer->GetFocusedDisplayRate() : FFrameRate();
}

bool SSequencerPlayRateCombo::IsSameDisplayRate(FFrameRate InFrameRate) const
{
	return GetDisplayRate() == InFrameRate;
}

FText SSequencerPlayRateCombo::GetFrameRateText() const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();

	return Sequencer.IsValid() ? Sequencer->GetFocusedDisplayRate().ToPrettyText() : FText();
}

FText SSequencerPlayRateCombo::GetToolTipText() const
{
	TSharedPtr<FSequencer> Sequencer         = WeakSequencer.Pin();
	UMovieSceneSequence*   FocusedSequence   = Sequencer.IsValid() ? Sequencer->GetFocusedMovieSceneSequence() : nullptr;
	UMovieScene*           FocusedMovieScene = FocusedSequence ? FocusedSequence->GetMovieScene() : nullptr;

	if (FocusedMovieScene)
	{
		FFrameRate DisplayRate     = FocusedMovieScene->GetDisplayRate();
		FFrameRate TickResolution  = FocusedMovieScene->GetTickResolution();

		const FCommonFrameRateInfo* DisplayRateInfo    = FCommonFrameRates::Find(DisplayRate);
		const FCommonFrameRateInfo* TickResolutionInfo = FCommonFrameRates::Find(TickResolution);

		FText DisplayRateText     = DisplayRateInfo     ? DisplayRateInfo->DisplayName    : FText::Format(LOCTEXT("DisplayRateFormat", "{0} fps"), DisplayRate.AsDecimal());
		FText TickResolutionText  = TickResolutionInfo  ? TickResolutionInfo->DisplayName : FText::Format(LOCTEXT("TickResolutionFormat", "{0} ticks every second"), TickResolution.AsDecimal());

		return FocusedMovieScene->GetEvaluationType() == EMovieSceneEvaluationType::FrameLocked
			? FText::Format(LOCTEXT("ToolTip_Format_FrameLocked", "This sequence is locked at runtime to {0} and uses an underlying tick resolution of {1}."), DisplayRateText, TickResolutionText)
			: FText::Format(LOCTEXT("ToolTip_Format", "This sequence is being presented as {0} and uses an underlying tick resolution of {1}."), DisplayRateText, TickResolutionText);
	}

	return FText();
}


void SSequencerPlayRateCombo::OnToggleFrameLocked()
{
	TSharedPtr<FSequencer> Sequencer         = WeakSequencer.Pin();
	UMovieSceneSequence*   FocusedSequence   = Sequencer.IsValid() ? Sequencer->GetFocusedMovieSceneSequence() : nullptr;
	UMovieScene*           FocusedMovieScene = FocusedSequence ? FocusedSequence->GetMovieScene() : nullptr;

	if (FocusedMovieScene)
	{
		if (FocusedMovieScene->IsReadOnly())
		{
			return;
		}

		EMovieSceneEvaluationType NewType = FocusedMovieScene->GetEvaluationType() == EMovieSceneEvaluationType::WithSubFrames
			? EMovieSceneEvaluationType::FrameLocked
			: EMovieSceneEvaluationType::WithSubFrames;

		FScopedTransaction ScopedTransaction(NewType == EMovieSceneEvaluationType::FrameLocked
			? LOCTEXT("FrameLockedTransaction",   "Lock to Display Rate at Runtime")
			: LOCTEXT("WithSubFramesTransaction", "Unlock to runtime frame rate"));

		FocusedMovieScene->Modify();
		FocusedMovieScene->SetEvaluationType(NewType);
	}
}

ECheckBoxState SSequencerPlayRateCombo::OnGetFrameLockedCheckState() const
{
	TSharedPtr<FSequencer> Sequencer         = WeakSequencer.Pin();
	UMovieSceneSequence*   FocusedSequence   = Sequencer.IsValid() ? Sequencer->GetFocusedMovieSceneSequence() : nullptr;
	UMovieScene*           FocusedMovieScene = FocusedSequence ? FocusedSequence->GetMovieScene() : nullptr;

	return FocusedMovieScene && FocusedMovieScene->GetEvaluationType() == EMovieSceneEvaluationType::FrameLocked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

#undef LOCTEXT_NAMESPACE