// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterTrackEditor.h"
#include "NiagaraEmitter.h"
#include "NiagaraRendererProperties.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "Sequencer/NiagaraSequence/NiagaraSequence.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "NiagaraEditorStyle.h"

#include "EditorStyleSet.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "NiagaraEmitterTrackEditor"

class SEmitterTrackWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEmitterTrackWidget) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UMovieSceneNiagaraEmitterTrack& InEmitterTrack)
	{
		EmitterTrack = &InEmitterTrack;

		TSharedRef<SHorizontalBox> TrackBox = SNew(SHorizontalBox)
			// Track initialization error icon.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3, 0, 0, 0)
			[
				SNew(SImage)
				.Visibility(this, &SEmitterTrackWidget::GetTrackErrorIconVisibility)
				.Image(FEditorStyle::GetBrush("Icons.Info"))
				.ToolTipText(this, &SEmitterTrackWidget::GetTrackErrorIconToolTip)
			]
			// Enabled checkbox.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3, 0, 0, 0)
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("EnabledTooltip", "Toggle whether or not this emitter is enabled."))
				.IsChecked(this, &SEmitterTrackWidget::GetEnabledCheckState)
				.OnCheckStateChanged(this, &SEmitterTrackWidget::OnEnabledCheckStateChanged)
				.Visibility(this, &SEmitterTrackWidget::GetEnableCheckboxVisibility)
			]
			// Isolate toggle
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3, 0, 0, 0)
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.HAlign(HAlign_Center)
				.ContentPadding(1)
				.ToolTipText(this, &SEmitterTrackWidget::GetToggleIsolateToolTip)
				.OnClicked(this, &SEmitterTrackWidget::OnToggleIsolateButtonClicked)
				.Visibility(this, &SEmitterTrackWidget::GetIsolateToggleVisibility)
				.Content()
				[
					SNew(SImage)
					.Image(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Isolate"))
					.ColorAndOpacity(this, &SEmitterTrackWidget::GetToggleIsolateImageColor)
				]
			];

		// Renderer buttons
		for (UNiagaraRendererProperties* Renderer : EmitterTrack->GetEmitterHandleViewModel()->GetEmitterViewModel()->GetEmitter()->GetRenderers())
		{
			TrackBox->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(3, 0, 0, 0)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.IsFocusable(false)
					.ToolTipText(FText::Format(LOCTEXT("RenderButtonToolTip", "{0} - Press to select."), FText::FromString(FName::NameToDisplayString(Renderer->GetName(), false))))
					.OnClicked(this, &SEmitterTrackWidget::OnRenderButtonClicked)
					[
						SNew(SImage)
						.Image(FSlateIconFinder::FindIconBrushForClass(Renderer->GetClass()))
					]
				];
		}

		ChildSlot
		[
			TrackBox
		];
	}

private:
	EVisibility GetTrackErrorIconVisibility() const 
	{
		return EmitterTrack.IsValid() && EmitterTrack.Get()->GetSectionInitializationErrors().Num() > 0
			? EVisibility::Visible
			: EVisibility::Collapsed;
	}

	FText GetTrackErrorIconToolTip() const
	{
		if(TrackErrorIconToolTip.IsSet() == false && EmitterTrack.IsValid())
		{
			FString TrackErrorIconToolTipBuilder;
			for (const FText& SectionInitializationError : EmitterTrack.Get()->GetSectionInitializationErrors())
			{
				if (TrackErrorIconToolTipBuilder.IsEmpty() == false)
				{
					TrackErrorIconToolTipBuilder.Append(TEXT("\n"));
				}
				TrackErrorIconToolTipBuilder.Append(SectionInitializationError.ToString());
			}
			TrackErrorIconToolTip = FText::FromString(TrackErrorIconToolTipBuilder);
		}
		return TrackErrorIconToolTip.GetValue();
	}

	ECheckBoxState GetEnabledCheckState() const
	{
		return EmitterTrack.IsValid() && EmitterTrack->GetEmitterHandleViewModel()->GetIsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void OnEnabledCheckStateChanged(ECheckBoxState InCheckState)
	{
		if (EmitterTrack.IsValid())
		{
			EmitterTrack->GetEmitterHandleViewModel()->SetIsEnabled(InCheckState == ECheckBoxState::Checked);
		}
	}

	FReply OnToggleIsolateButtonClicked()
	{
		TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> EmittersToIsolate;
		if (EmitterTrack.IsValid())
		{
			if (EmitterTrack->GetSystemViewModel().IsEmitterIsolated(EmitterTrack->GetEmitterHandleViewModel().ToSharedRef()) == false)
			{
				EmittersToIsolate.Add(EmitterTrack->GetEmitterHandleViewModel().ToSharedRef());
			}
			EmitterTrack->GetSystemViewModel().IsolateEmitters(EmittersToIsolate);
		}
		return FReply::Handled();
	}

	FText GetToggleIsolateToolTip() const
	{
		return EmitterTrack.IsValid() && EmitterTrack->GetSystemViewModel().IsEmitterIsolated(EmitterTrack->GetEmitterHandleViewModel().ToSharedRef())
			? LOCTEXT("TurnOffEmitterIsolation", "Disable emitter isolation.")
			: LOCTEXT("IsolateThisEmitter", "Enable isolation for this emitter.");
	}

	FSlateColor GetToggleIsolateImageColor() const
	{
		return EmitterTrack.IsValid() && EmitterTrack->GetSystemViewModel().IsEmitterIsolated(EmitterTrack->GetEmitterHandleViewModel().ToSharedRef())
			? FEditorStyle::GetSlateColor("SelectionColor")
			: FLinearColor::Gray;
	}

	FReply OnRenderButtonClicked()
	{
		return FReply::Handled();
	}

	EVisibility GetEnableCheckboxVisibility() const
	{
		return EmitterTrack.IsValid() && EmitterTrack->GetSystemViewModel().GetEditMode() == ENiagaraSystemViewModelEditMode::SystemAsset ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GetIsolateToggleVisibility() const
	{
		return EmitterTrack.IsValid() && EmitterTrack->GetSystemViewModel().GetEditMode() == ENiagaraSystemViewModelEditMode::SystemAsset ? EVisibility::Visible : EVisibility::Collapsed;
	}

private:
	TWeakObjectPtr<UMovieSceneNiagaraEmitterTrack> EmitterTrack;
	mutable TOptional<FText> TrackErrorIconToolTip;
};

FNiagaraEmitterTrackEditor::FNiagaraEmitterTrackEditor(TSharedPtr<ISequencer> Sequencer) 
	: FMovieSceneTrackEditor(Sequencer.ToSharedRef())
{
}

TSharedRef<ISequencerTrackEditor> FNiagaraEmitterTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FNiagaraEmitterTrackEditor(InSequencer));
}

bool FNiagaraEmitterTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const 
{
	if (TrackClass == UMovieSceneNiagaraEmitterTrack::StaticClass())
	{
		return true;
	}
	return false;
}

TSharedRef<ISequencerSection> FNiagaraEmitterTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieSceneNiagaraEmitterSectionBase* EmitterSection = CastChecked<UMovieSceneNiagaraEmitterSectionBase>(&SectionObject);
	return EmitterSection->MakeSectionInterface();
}

bool FNiagaraEmitterTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	UNiagaraEmitter* EmitterAsset = Cast<UNiagaraEmitter>(Asset);
	UNiagaraSequence* NiagaraSequence = Cast<UNiagaraSequence>(GetSequencer()->GetRootMovieSceneSequence());
	if (EmitterAsset != nullptr && NiagaraSequence != nullptr && NiagaraSequence->GetSystemViewModel().GetCanModifyEmittersFromTimeline())
	{
		NiagaraSequence->GetSystemViewModel().AddEmitter(*EmitterAsset);
	}
	return false;
}

void FNiagaraEmitterTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	UMovieSceneNiagaraEmitterTrack* EmitterTrack = CastChecked<UMovieSceneNiagaraEmitterTrack>(Track);
	FNiagaraSystemViewModel& SystemViewModel = EmitterTrack->GetSystemViewModel();

	if (SystemViewModel.GetEditMode() == ENiagaraSystemViewModelEditMode::SystemAsset)
	{
		MenuBuilder.BeginSection("Niagara", LOCTEXT("NiagaraContextMenuSectionName", "Niagara"));
		{
			MenuBuilder.AddMenuEntry(
				SystemViewModel.IsEmitterIsolated(EmitterTrack->GetEmitterHandleViewModel().ToSharedRef())
					? LOCTEXT("RemoveFromIsolation", "Remove this from isolation.")
					: LOCTEXT("AddToIsolation", "Add this to isolation"),
				SystemViewModel.IsEmitterIsolated(EmitterTrack->GetEmitterHandleViewModel().ToSharedRef())
					? LOCTEXT("RemoveFromIsolation_NoChangeOthers", "Remove this emitter from isolation, without changing other emitters.")
					: LOCTEXT("AddToIsolation_NoChangeOthers", "Add this emitter to isolation, without changing other emitters."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(&SystemViewModel, &FNiagaraSystemViewModel::ToggleEmitterIsolation, EmitterTrack->GetEmitterHandleViewModel().ToSharedRef())));
			
			TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitters;
			SystemViewModel.GetSelectedEmitterHandles(SelectedEmitters);
			MenuBuilder.AddMenuEntry(
				LOCTEXT("IsolateSelected", "Isolate all selected"),
				LOCTEXT("IsolateSelectedToolTip", "Add all of the selected emitters to isloation"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(&SystemViewModel, &FNiagaraSystemViewModel::IsolateEmitters, SelectedEmitters)));
		}
		MenuBuilder.EndSection();
	}
}

TSharedPtr<SWidget> FNiagaraEmitterTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	return SNew(SEmitterTrackWidget, *CastChecked<UMovieSceneNiagaraEmitterTrack>(Track));
}

#undef LOCTEXT_NAMESPACE