// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Widgets/STakeRecorderCockpit.h"
#include "Widgets/TakeRecorderWidgetConstants.h"
#include "Widgets/STakeRecorderTabContent.h"
#include "TakesCoreBlueprintLibrary.h"
#include "TakeMetaData.h"
#include "TakeRecorderCommands.h"
#include "TakeRecorderModule.h"
#include "TakeRecorderSettings.h"
#include "TakeRecorderStyle.h"
#include "TakeRecorderSource.h"
#include "TakeRecorderSources.h"
#include "Misc/Timecode.h"
#include "Misc/FrameRate.h"
#include "Recorder/TakeRecorder.h"
#include "Recorder/TakeRecorderBlueprintLibrary.h"
#include "LevelSequence.h"
#include "Algo/Find.h"

// AssetRegistry includes
#include "AssetRegistryModule.h"

// TimeManagement includes
#include "FrameNumberNumericInterface.h"

// Slate includes
#include "Widgets/SBoxPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SFrameRatePicker.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

// EditorStyle includes
#include "EditorStyleSet.h"
#include "EditorFontGlyphs.h"

// UnrealEd includes
#include "ScopedTransaction.h"
#include "Editor/EditorEngine.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"

extern UNREALED_API UEditorEngine* GEditor;

#define LOCTEXT_NAMESPACE "STakeRecorderCockpit"

STakeRecorderCockpit::~STakeRecorderCockpit()
{
	UTakeRecorder::OnRecordingInitialized().Remove(OnRecordingInitializedHandle);

	if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		AssetRegistryModule->Get().OnFilesLoaded().Remove(OnAssetRegistryFilesLoadedHandle);
	}

	if (!ensure(TransactionIndex == INDEX_NONE))
	{
		GEditor->CancelTransaction(TransactionIndex);
	}
}

void STakeRecorderCockpit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(TakeMetaData);
	Collector.AddReferencedObject(TransientTakeMetaData);
}

PRAGMA_DISABLE_OPTIMIZATION
void STakeRecorderCockpit::Construct(const FArguments& InArgs)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	OnAssetRegistryFilesLoadedHandle = AssetRegistryModule.Get().OnFilesLoaded().AddSP(this, &STakeRecorderCockpit::OnAssetRegistryFilesLoaded);
	
	OnRecordingInitializedHandle = UTakeRecorder::OnRecordingInitialized().AddSP(this, &STakeRecorderCockpit::OnRecordingInitialized);

	bAutoApplyTakeNumber = true;

	TakeMetaData = nullptr;
	TransientTakeMetaData = nullptr;

	LevelSequenceAttribute = InArgs._LevelSequence;

	CacheMetaData();

	if (TakeMetaData && !TakeMetaData->IsLocked())
	{
		int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TakeMetaData->GetSlate());
		if (NextTakeNumber != TakeMetaData->GetTakeNumber())
		{
			TakeMetaData->SetTakeNumber(NextTakeNumber);
		}
	}

	UpdateTakeError();
	UpdateRecordError();

	CommandList = MakeShareable(new FUICommandList);

	BindCommands();

	TransactionIndex = INDEX_NONE;

	int32 Column[] = { 0, 1, 2 };
	int32 Row[]    = { 0, 1, 2 };

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FTakeRecorderStyle::Get().GetBrush("TakeRecorder.Slate"))
		[

		SNew(SVerticalBox)

		// Slate, Take #, and Record Button 
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage_Lambda([this]{ return Reviewing() ? FTakeRecorderStyle::Get().GetBrush("TakeRecorder.TakeRecorderReviewBorder") : FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"); })
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(0.6)
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Left)
					.Padding(2.0f, 2.0f)
					[
						SNew(STextBlock)
						.TextStyle(FTakeRecorderStyle::Get(), "TakeRecorder.TextBox")
						.Text(LOCTEXT("SlateLabel", "SLATE"))
					]

					+ SOverlay::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SEditableTextBox)
						.IsEnabled(this, &STakeRecorderCockpit::EditingMetaData)
						.Style(FTakeRecorderStyle::Get(), "TakeRecorder.EditableTextBox")
						.Font(FTakeRecorderStyle::Get().GetFontStyle("TakeRecorder.Cockpit.LargeText"))
						.HintText(LOCTEXT("EnterSlate_Hint", "<slate>"))
						.Justification(ETextJustify::Center)
						.SelectAllTextWhenFocused(true)
						.Text(this, &STakeRecorderCockpit::GetSlateText)
						.OnTextCommitted(this, &STakeRecorderCockpit::SetSlateText)
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.4)
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Left)
					.Padding(2.0f, 2.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.TextStyle(FTakeRecorderStyle::Get(), "TakeRecorder.TextBox")
							.Text(LOCTEXT("TakeLabel", "TAKE"))
						]

						+ SHorizontalBox::Slot()
						.Padding(2.f, 0.f)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "NoBorder")
							.OnClicked(this, &STakeRecorderCockpit::OnSetNextTakeNumber)
							.ForegroundColor(FSlateColor::UseForeground())
							.Visibility(this, &STakeRecorderCockpit::GetTakeWarningVisibility)
							.Content()
							[
								SNew(STextBlock)
								.ToolTipText(this, &STakeRecorderCockpit::GetTakeWarningText)
								.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.8"))
								.Text(FEditorFontGlyphs::Exclamation_Triangle)
							]
						]
					]

					+ SOverlay::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SSpinBox<int32>)
						.IsEnabled(this, &STakeRecorderCockpit::EditingMetaData)
						.ContentPadding(FMargin(8.f, 0.f))
						.Style(FTakeRecorderStyle::Get(), "TakeRecorder.TakeInput")
						.Font(FTakeRecorderStyle::Get().GetFontStyle("TakeRecorder.Cockpit.GiantText"))
						.Justification(ETextJustify::Center)
						.Value(this, &STakeRecorderCockpit::GetTakeNumber)
						.Delta(1)
						.MinValue(1)
						.MaxValue(TOptional<int32>())
						.OnBeginSliderMovement(this, &STakeRecorderCockpit::OnBeginSetTakeNumber)
						.OnValueChanged(this, &STakeRecorderCockpit::SetTakeNumber)
						.OnValueCommitted(this, &STakeRecorderCockpit::SetTakeNumber_FromCommit)
						.OnEndSliderMovement(this, &STakeRecorderCockpit::OnEndSetTakeNumber)
					]
				]

				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					[
						SNew(SBox)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.MaxAspectRatio(1)
						.Padding(FMargin(8.0f, 8.0f, 8.0f, 8.0f))
						.Visibility_Lambda([this]() { return Reviewing() ? EVisibility::Hidden : EVisibility::Visible; })
						[
							SNew(SCheckBox)
							.Style(FTakeRecorderStyle::Get(), "TakeRecorder.RecordButton")
							.OnCheckStateChanged(this, &STakeRecorderCockpit::OnToggleRecording)
							.IsChecked(this, &STakeRecorderCockpit::IsRecording)
							.IsEnabled(this, &STakeRecorderCockpit::CanRecord)
						]
					]

					+ SOverlay::Slot()
					[
						SNew(SBox)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.MaxAspectRatio(1)
						.Padding(FMargin(8.0f, 8.0f, 8.0f, 8.0f))
						.Visibility_Lambda([this]() { return Reviewing() ? EVisibility::Visible : EVisibility::Hidden; })
						[
							SNew(SButton)
							.ContentPadding(TakeRecorder::ButtonPadding)
							.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
							.ToolTipText(LOCTEXT("NewRecording", "Start a new recording using this Take as a base"))
							.ForegroundColor(FSlateColor::UseForeground())
							.OnClicked(this, &STakeRecorderCockpit::NewRecordingFromThis)
							[
								SNew(SImage)
								.Image(FTakeRecorderStyle::Get().GetBrush("TakeRecorder.StartNewRecordingButton"))
							]
						]
					]

					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.ToolTipText(this, &STakeRecorderCockpit::GetRecordErrorText)
						.Visibility(this, &STakeRecorderCockpit::GetRecordErrorVisibility)
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.9"))
						.Text(FEditorFontGlyphs::Exclamation_Triangle)
					]

					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FEditorStyle::Get().GetSlateColor("InvertedForeground"))
						.Visibility(this, &STakeRecorderCockpit::GetCountdownVisibility)
						.Text(this, &STakeRecorderCockpit::GetCountdownText)
					]
				]
			]
		]

		// Timestamp, Duration, Description and Remaining Metadata
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FTakeRecorderStyle::Get().GetBrush("TakeRecorder.Slate.BorderImage"))
			.BorderBackgroundColor(FTakeRecorderStyle::Get().GetColor("TakeRecorder.Slate.BorderColor"))
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.Padding(8, 4, 0, 4)
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FTakeRecorderStyle::Get().GetFontStyle("TakeRecorder.Cockpit.SmallText"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Text(this, &STakeRecorderCockpit::GetTimestampText)
					]

					+ SHorizontalBox::Slot()
					[
						SNew(SSpacer)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FTakeRecorderStyle::Get().GetFontStyle("TakeRecorder.Cockpit.MediumText"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Justification(ETextJustify::Right)
						.Text(this, &STakeRecorderCockpit::GetDurationText)
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.ToolTipText(LOCTEXT("AddMarkedFrame", "Click to add a marked frame while recording"))
						.IsEnabled_Lambda([this]() { return IsRecording() == ECheckBoxState::Checked; })
						.OnClicked(this, &STakeRecorderCockpit::OnAddMarkedFrame)
						.ForegroundColor(FSlateColor::UseForeground())
						[
							SNew(SImage)
							.Image(FTakeRecorderStyle::Get().GetBrush("TakeRecorder.MarkFrame"))
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8, 4)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					[
						SNew(SSpacer)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Font(FTakeRecorderStyle::Get().GetFontStyle("TakeRecorder.Cockpit.SmallText"))
						.Text(this, &STakeRecorderCockpit::GetFrameRateText)
					]
				]

				+SVerticalBox::Slot()
				.Padding(8, 0, 8, 8)
				.AutoHeight()
				[
					SNew(SEditableTextBox)
					.IsEnabled(this, &STakeRecorderCockpit::EditingMetaData)
					.Style(FTakeRecorderStyle::Get(), "TakeRecorder.EditableTextBox")
					.Font(FTakeRecorderStyle::Get().GetFontStyle("TakeRecorder.Cockpit.SmallText"))
					.SelectAllTextWhenFocused(true)
					.HintText(LOCTEXT("EnterSlateDescription_Hint", "<description>"))
					.Text(this, &STakeRecorderCockpit::GetUserDescriptionText)
					.OnTextCommitted(this, &STakeRecorderCockpit::SetUserDescriptionText)
				]
			]
		]
		]
	];
}
PRAGMA_ENABLE_OPTIMIZATION

bool STakeRecorderCockpit::CanStartRecording(FText* OutErrorText) const
{
	bool bCanRecord = CanRecord();
	if (!bCanRecord && OutErrorText)
	{
		*OutErrorText = RecordErrorText;
	}
	return bCanRecord;
}

FText STakeRecorderCockpit::GetTakeWarningText() const
{
	return TakeErrorText;
}

EVisibility STakeRecorderCockpit::GetTakeWarningVisibility() const
{
	return TakeErrorText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

FText STakeRecorderCockpit::GetRecordErrorText() const
{
	return RecordErrorText;
}

EVisibility STakeRecorderCockpit::GetRecordErrorVisibility() const
{
	return RecordErrorText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

void STakeRecorderCockpit::UpdateRecordError()
{
	RecordErrorText = FText();
	if (Reviewing())
	{
		// When take meta-data is locked, we cannot record until we hit the "Start a new recording using this Take as a base"
		// For this reason, we don't show any error information because we can always start a new recording from any take
		return;
	}

	ULevelSequence* Sequence = LevelSequenceAttribute.Get();
	if (!Sequence)
	{
		RecordErrorText = LOCTEXT("ErrorWidget_NoSequence", "There is no sequence to record from. Please re-open Take Recorder.");
		return;
	}

	if (!Sequence->HasAnyFlags(RF_Transient))
	{
		RecordErrorText = FText();
		return;
	}

	UTakeRecorderSources*                  SourcesContainer = Sequence->FindMetaData<UTakeRecorderSources>();
	TArrayView<UTakeRecorderSource* const> SourcesArray     = SourcesContainer ? SourcesContainer->GetSources() : TArrayView<UTakeRecorderSource* const>();
	if (!Algo::FindBy(SourcesArray, true, &UTakeRecorderSource::bEnabled))
	{
		RecordErrorText = LOCTEXT("ErrorWidget_NoSources", "There are no currently enabled sources to record from. Please add some above before recording.");
		return;
	}

	if (TakeMetaData->GetSlate().IsEmpty())
	{
		RecordErrorText = LOCTEXT("ErrorWidget_NoSlate", "You must enter a slate to begin recording.");
		return;
	}

	FString PackageName = TakeMetaData->GenerateAssetPath(GetDefault<UTakeRecorderProjectSettings>()->Settings.GetTakeAssetPath());
	FText OutReason;
	if (!FPackageName::IsValidLongPackageName(PackageName, false, &OutReason))
	{
		RecordErrorText = FText::Format(LOCTEXT("ErrorWidget_InvalidPath", "{0} is not a valid asset path. {1}"), FText::FromString(PackageName), OutReason);
		return;
	}
}

void STakeRecorderCockpit::UpdateTakeError()
{
	TakeErrorText = FText();

	TArray<FAssetData> DuplicateTakes = UTakesCoreBlueprintLibrary::FindTakes(TakeMetaData->GetSlate(), TakeMetaData->GetTakeNumber());

	// If there's only a single one, and it's the one that we're looking at directly, don't show the error
	if (DuplicateTakes.Num() == 1 && DuplicateTakes[0].IsValid())
	{
		ULevelSequence* AlreadyLoaded = FindObject<ULevelSequence>(nullptr, *DuplicateTakes[0].ObjectPath.ToString());
		if (AlreadyLoaded && AlreadyLoaded->FindMetaData<UTakeMetaData>() == TakeMetaData)
		{
			return;
		}
	}

	if (DuplicateTakes.Num() > 0)
	{
		FTextBuilder TextBuilder;
		TextBuilder.AppendLineFormat(
			LOCTEXT("DuplicateTakeNumber_1", "The following Level {0}|plural(one=Sequence, other=Sequences) {0}|plural(one=was, other=were) also recorded with take {1} of {2}"),
			DuplicateTakes.Num(),
			FText::AsNumber(TakeMetaData->GetTakeNumber()),
			FText::FromString(TakeMetaData->GetSlate())
		);

		for (const FAssetData& Asset : DuplicateTakes)
		{
			TextBuilder.AppendLine(FText::FromName(Asset.PackageName));
		}

		TextBuilder.AppendLine(LOCTEXT("GetNextAvailableTakeNumber", "Click to get the next available take number."));
		TakeErrorText = TextBuilder.ToText();
	}
}

EVisibility STakeRecorderCockpit::GetCountdownVisibility() const
{
	const UTakeRecorder* CurrentRecording = UTakeRecorder::GetActiveRecorder();
	const bool           bIsCountingDown  = CurrentRecording && CurrentRecording->GetState() == ETakeRecorderState::CountingDown;

	return bIsCountingDown ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FText STakeRecorderCockpit::GetCountdownText() const
{
	const UTakeRecorder* CurrentRecording = UTakeRecorder::GetActiveRecorder();
	const bool           bIsCountingDown  = CurrentRecording && CurrentRecording->GetState() == ETakeRecorderState::CountingDown;

	return bIsCountingDown ? FText::AsNumber(FMath::CeilToInt(CurrentRecording->GetCountdownSeconds())) : FText();
}

void STakeRecorderCockpit::CacheMetaData()
{
	UTakeMetaData* NewMetaDataThisTick = nullptr;

	if (ULevelSequence* LevelSequence = LevelSequenceAttribute.Get())
	{
		NewMetaDataThisTick = LevelSequence->FindMetaData<UTakeMetaData>();
	}

	// If it's null we use the transient meta-data
	if (!NewMetaDataThisTick)
	{
		// if the transient meta-data doesn't exist, create it now
		if (!TransientTakeMetaData)
		{
			TransientTakeMetaData = UTakeMetaData::CreateFromDefaults(GetTransientPackage(), NAME_None);
			TransientTakeMetaData->SetFlags(RF_Transactional | RF_Transient);

			TransientTakeMetaData->SetSlate(GetDefault<UTakeRecorderProjectSettings>()->Settings.DefaultSlate);

			// Compute the correct starting take number
			int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TransientTakeMetaData->GetSlate());
			TransientTakeMetaData->SetTakeNumber(NextTakeNumber);
		}

		NewMetaDataThisTick = TransientTakeMetaData;
	}

	check(NewMetaDataThisTick);
	if (NewMetaDataThisTick != TakeMetaData)
	{
		TakeMetaData = NewMetaDataThisTick;
		// Forcibly update any UI?
	}

	check(TakeMetaData);
}

void STakeRecorderCockpit::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	CacheMetaData();
	UpdateTakeError();
	UpdateRecordError();
}

FText STakeRecorderCockpit::GetSlateText() const
{
	return FText::FromString(TakeMetaData->GetSlate());
}

FText STakeRecorderCockpit::GetDurationText() const
{
	FFrameNumber TotalFrames;
	FFrameRate FrameRate = TakeMetaData->GetFrameRate();

	if (UTakeRecorderBlueprintLibrary::IsRecording())
	{
		FTimespan RecordingDuration =  FDateTime::UtcNow() - TakeMetaData->GetTimestamp();
		TotalFrames = FFrameNumber(static_cast<int32>(FrameRate.AsDecimal() * RecordingDuration.GetTotalSeconds()));
	}

	FTimecode Timecode = FTimecode::FromFrameNumber(TotalFrames, FrameRate, FTimecode::IsDropFormatTimecodeSupported(FrameRate));

	// FTimecode Timecode = FTimecode::FromFrameNumber(TakeMetaData->GetDuration().FrameNumber, TakeMetaData->GetFrameRate(), FTimecode::IsDropFormatTimecodeSupported(TakeMetaData->GetFrameRate()));
	return FText::FromString(Timecode.ToString());
}

FText STakeRecorderCockpit::GetUserDescriptionText() const
{
	return FText::FromString(TakeMetaData->GetDescription());
}

FText STakeRecorderCockpit::GetTimestampText() const
{
	// FDateTime Timestamp = TakeMetaData->GetTimestamp() == FDateTime(0) ? FDateTime::UtcNow() : TakeMetaData->GetTimestamp();
	FText TextTime = TakeMetaData->GetTimestamp() == FDateTime(0) ? FText::FromString(TEXT("--")) : FText::AsDateTime(TakeMetaData->GetTimestamp());
	return TextTime;
}

FText STakeRecorderCockpit::GetFrameRateText() const
{
	return GetFrameRate().ToPrettyText();
}

FFrameRate STakeRecorderCockpit::GetFrameRate() const
{
	return TakeMetaData->GetFrameRate();
}

bool STakeRecorderCockpit::IsFrameRateCompatible(FFrameRate InFrameRate) const
{
	ULevelSequence* Sequence   = LevelSequenceAttribute.Get();
	UMovieScene*    MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;

	return MovieScene && InFrameRate.IsMultipleOf(MovieScene->GetTickResolution());
}

void STakeRecorderCockpit::SetSlateText(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if (TakeMetaData->GetSlate() != InNewText.ToString())
	{
		FScopedTransaction Transaction(LOCTEXT("SetSlate_Transaction", "Set Take Slate"));
		TakeMetaData->Modify();

		TakeMetaData->SetSlate(InNewText.ToString());

		// Compute the correct starting take number
		int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TakeMetaData->GetSlate());
		TakeMetaData->SetTakeNumber(NextTakeNumber);
	}
}

void STakeRecorderCockpit::SetUserDescriptionText(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if (TakeMetaData->GetDescription() != InNewText.ToString())
	{
		FScopedTransaction Transaction(LOCTEXT("SetDescription_Transaction", "Set Description"));
		TakeMetaData->Modify();

		TakeMetaData->SetDescription(InNewText.ToString());
	}
}

void STakeRecorderCockpit::SetDurationText(const FText& InNewText, ETextCommit::Type)
{
	double CurrentFrameTime = TakeMetaData->GetDuration().AsDecimal();

	FFrameNumberInterface Interface(EFrameNumberDisplayFormats::DropFrameTimecode, 2, TakeMetaData->GetFrameRate(), TakeMetaData->GetFrameRate());
	TOptional<double> NewFrameTime = Interface.FromString(InNewText.ToString(), CurrentFrameTime);

	if (NewFrameTime.IsSet())
	{
		FScopedTransaction Transaction(LOCTEXT("SetDuration_Transaction", "Set Duration"));
		TakeMetaData->Modify();

		FFrameTime NewDuration = FFrameTime::FromDecimal(NewFrameTime.GetValue());
		TakeMetaData->SetDuration(NewDuration);

		ULevelSequence* Sequence   = LevelSequenceAttribute.Get();
		UMovieScene*    MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;

		if (MovieScene)
		{
			MovieScene->Modify();

			TRange<FFrameNumber> PlaybackRange = TRange<FFrameNumber>::Inclusive(0, ConvertFrameTime(NewDuration, TakeMetaData->GetFrameRate(), MovieScene->GetTickResolution()).CeilToFrame());
			MovieScene->SetPlaybackRange(PlaybackRange);
		}
	}
}

int32 STakeRecorderCockpit::GetTakeNumber() const
{
	return TakeMetaData->GetTakeNumber();
}

FReply STakeRecorderCockpit::OnSetNextTakeNumber()
{
	FScopedTransaction Transaction(LOCTEXT("SetNextTakeNumber_Transaction", "Set Next Take Number"));

	int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TakeMetaData->GetSlate());

	TakeMetaData->Modify();
	TakeMetaData->SetTakeNumber(NextTakeNumber);

	return FReply::Handled();
}

void STakeRecorderCockpit::OnBeginSetTakeNumber()
{
	const bool bIsInPIEOrSimulate = GEditor->PlayWorld != NULL || GEditor->bIsSimulatingInEditor;

	if (!bIsInPIEOrSimulate)
	{
		check(TransactionIndex == INDEX_NONE);
	}

	TransactionIndex = GEditor->BeginTransaction(nullptr, LOCTEXT("SetTakeNumber_Transaction", "Set Take Number"), nullptr);
	TakeMetaData->Modify();
}

void STakeRecorderCockpit::SetTakeNumber(int32 InNewTakeNumber)
{
	const bool bIsInPIEOrSimulate = GEditor->PlayWorld != NULL || GEditor->bIsSimulatingInEditor;

	if (TransactionIndex != INDEX_NONE || bIsInPIEOrSimulate)
	{
		TakeMetaData->SetTakeNumber(InNewTakeNumber);
		bAutoApplyTakeNumber = false;
	}
}

void STakeRecorderCockpit::SetTakeNumber_FromCommit(int32 InNewTakeNumber, ETextCommit::Type InCommitType)
{
	const bool bIsInPIEOrSimulate = GEditor->PlayWorld != NULL || GEditor->bIsSimulatingInEditor;

	if (TransactionIndex == INDEX_NONE && !bIsInPIEOrSimulate)
	{
		if (TakeMetaData->GetTakeNumber() != InNewTakeNumber)
		{
			OnBeginSetTakeNumber();
			OnEndSetTakeNumber(InNewTakeNumber);
		}
	}
	else
	{
		TakeMetaData->SetTakeNumber(InNewTakeNumber);
	}

	bAutoApplyTakeNumber = false;
}

void STakeRecorderCockpit::OnEndSetTakeNumber(int32 InFinalValue)
{
	const bool bIsInPIEOrSimulate = GEditor->PlayWorld != NULL || GEditor->bIsSimulatingInEditor;

	if (!bIsInPIEOrSimulate)
	{
		check(TransactionIndex != INDEX_NONE);
	}

	TakeMetaData->SetTakeNumber(InFinalValue);

	GEditor->EndTransaction();
	TransactionIndex = INDEX_NONE;
}

FReply STakeRecorderCockpit::OnAddMarkedFrame()
{
	if (UTakeRecorderBlueprintLibrary::IsRecording())
	{
		FFrameRate FrameRate = TakeMetaData->GetFrameRate();

		FTimespan RecordingDuration = FDateTime::UtcNow() - TakeMetaData->GetTimestamp();

		FFrameNumber ElapsedFrame = FFrameNumber(static_cast<int32>(FrameRate.AsDecimal() * RecordingDuration.GetTotalSeconds()));
		
		ULevelSequence* LevelSequence = LevelSequenceAttribute.Get();
		UMovieScene* MovieScene = LevelSequence->GetMovieScene();

		FMovieSceneMarkedFrame MarkedFrame;
		MarkedFrame.FrameNumber = ConvertFrameTime(ElapsedFrame, MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).CeilToFrame();

		MovieScene->AddMarkedFrame(MarkedFrame);
	}

	return FReply::Handled();
}

bool STakeRecorderCockpit::Reviewing() const 
{
	return bool(!Recording() && (TakeMetaData->Recorded()));
}

bool STakeRecorderCockpit::Recording() const
{
	return UTakeRecorderBlueprintLibrary::GetActiveRecorder() ? true : false;
}

ECheckBoxState STakeRecorderCockpit::IsRecording() const
{
	return Recording() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool STakeRecorderCockpit::CanRecord() const
{
	return RecordErrorText.IsEmpty();
}

bool STakeRecorderCockpit::IsLocked() const 
{
	if (TakeMetaData != nullptr)
	{
		return TakeMetaData->IsLocked();
	}
	return false;
}

void STakeRecorderCockpit::OnToggleRecording(ECheckBoxState)
{
	ULevelSequence*       LevelSequence = LevelSequenceAttribute.Get();
	UTakeRecorderSources* Sources = LevelSequence ? LevelSequence->FindMetaData<UTakeRecorderSources>() : nullptr;

	UTakeRecorder* CurrentRecording = UTakeRecorder::GetActiveRecorder();
	if (CurrentRecording)
	{
		StopRecording();
	}
	else if (LevelSequence && Sources)
	{
		StartRecording();
	}
}

void STakeRecorderCockpit::StopRecording()
{
	UTakeRecorder* CurrentRecording = UTakeRecorder::GetActiveRecorder();
	if (CurrentRecording)
	{
		CurrentRecording->Stop();
	}
}

void STakeRecorderCockpit::StartRecording()
{
	ULevelSequence*       LevelSequence = LevelSequenceAttribute.Get();
	UTakeRecorderSources* Sources = LevelSequence ? LevelSequence->FindMetaData<UTakeRecorderSources>() : nullptr;
	
	if (LevelSequence && Sources)
	{
		FTakeRecorderParameters Parameters;
		Parameters.User    = GetDefault<UTakeRecorderUserSettings>()->Settings;
		Parameters.Project = GetDefault<UTakeRecorderProjectSettings>()->Settings;

		FText ErrorText = LOCTEXT("UnknownError", "An unknown error occurred when trying to start recording");

		UTakeRecorder* NewRecorder = NewObject<UTakeRecorder>(GetTransientPackage(), NAME_None, RF_Transient);

		if (!NewRecorder->Initialize(LevelSequence, Sources, TakeMetaData, Parameters, &ErrorText))
		{
			if (ensure(!ErrorText.IsEmpty()))
			{
				FNotificationInfo Info(ErrorText);
				Info.ExpireDuration = 5.0f;
				FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
			}
		}
	}
}

FReply STakeRecorderCockpit::NewRecordingFromThis()
{
	ULevelSequence* Sequence = LevelSequenceAttribute.Get();
	if (!Sequence)
	{
		return FReply::Unhandled();
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedRef<SDockTab> DockTab = LevelEditorModule.GetLevelEditorTabManager()->InvokeTab(ITakeRecorderModule::TakeRecorderTabName);
	TSharedRef<STakeRecorderTabContent> TabContent = StaticCastSharedRef<STakeRecorderTabContent>(DockTab->GetContent());
	TabContent->SetupForRecording(Sequence);

	return FReply::Handled();
}

void STakeRecorderCockpit::OnAssetRegistryFilesLoaded()
{
	if (bAutoApplyTakeNumber && TransientTakeMetaData)
	{
		int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TransientTakeMetaData->GetSlate());
		TransientTakeMetaData->SetTakeNumber(NextTakeNumber);
	}
}

void STakeRecorderCockpit::OnRecordingInitialized(UTakeRecorder* Recorder)
{
	// Recache the meta-data here since we know that the sequence has probably changed as a result of the recording being started
	CacheMetaData();

	OnRecordingFinishedHandle = Recorder->OnRecordingFinished().AddSP(this, &STakeRecorderCockpit::OnRecordingFinished);
}

void STakeRecorderCockpit::OnRecordingFinished(UTakeRecorder* Recorder)
{
	if (TransientTakeMetaData)
	{
		// Increment the transient take meta data if necessary
		int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TransientTakeMetaData->GetSlate());
		TransientTakeMetaData->SetTakeNumber(NextTakeNumber);

		bAutoApplyTakeNumber = true;
	}

	Recorder->OnRecordingFinished().Remove(OnRecordingFinishedHandle);
}

void STakeRecorderCockpit::BindCommands()
{
	CommandList->MapAction(FTakeRecorderCommands::Get().StartRecording,
		FExecuteAction::CreateSP(this, &STakeRecorderCockpit::StartRecording));

	CommandList->MapAction(FTakeRecorderCommands::Get().StopRecording,
		FExecuteAction::CreateSP(this, &STakeRecorderCockpit::StopRecording));

	// Append to level editor module so that shortcuts are accessible in level editor
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	LevelEditorModule.GetGlobalLevelEditorActions()->Append(CommandList.ToSharedRef());
}

void STakeRecorderCockpit::OnToggleEditPreviousRecording(ECheckBoxState CheckState)
{
	if (Reviewing())
	{
		TakeMetaData->IsLocked() ? TakeMetaData->Unlock() : TakeMetaData->Lock();
	}	
}

bool STakeRecorderCockpit::EditingMetaData() const
{
	return (!Reviewing() || !TakeMetaData->IsLocked());
}

TSharedRef<SWidget> STakeRecorderCockpit::MakeLockButton()
{
	return SNew(SCheckBox)
	.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
	.Padding(TakeRecorder::ButtonPadding)
	.ToolTipText(LOCTEXT("Modify Slate", "Unlock to modify the slate information for this prior recording."))
	.IsChecked_Lambda([this]() { return TakeMetaData->IsLocked() ? ECheckBoxState::Unchecked: ECheckBoxState::Checked; } )
	.OnCheckStateChanged(this, &STakeRecorderCockpit::OnToggleEditPreviousRecording)
	.Visibility_Lambda([this]() { return Reviewing() ? EVisibility::Visible : EVisibility::Collapsed; })
	[
		SNew(STextBlock)
		.Justification(ETextJustify::Center)
		.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.14"))
		.Text_Lambda([this]() { return TakeMetaData->IsLocked() ? FEditorFontGlyphs::Lock : FEditorFontGlyphs::Unlock; } )
	];
}

#undef LOCTEXT_NAMESPACE