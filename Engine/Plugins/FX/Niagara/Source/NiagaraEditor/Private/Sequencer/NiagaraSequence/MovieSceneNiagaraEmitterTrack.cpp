// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNiagaraEmitterTrack.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "MovieSceneNiagaraEmitterTrackInstance.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScriptSource.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraEditorStyle.h"

#include "ISequencerSection.h"
#include "SequencerSectionPainter.h"

#define LOCTEXT_NAMESPACE "MovieSceneNiagaraEmitterTrack"

class FNiagaraInvalidSequencerSection : public ISequencerSection
{
public:
	virtual UMovieSceneSection* GetSectionObject()  override
	{
		unimplemented();
		return nullptr;
	}

	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override
	{
		unimplemented();
		return InPainter.LayerId;
	}
};

void UMovieSceneNiagaraEmitterSectionBase::Initialize(FNiagaraSystemViewModel& InSystemViewModel, TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel)
{
	ClearFlags(RF_Transactional);
	SystemViewModel = &InSystemViewModel;
	EmitterHandleViewModel = InEmitterHandleViewModel;
}

FNiagaraSystemViewModel& UMovieSceneNiagaraEmitterSectionBase::GetSystemViewModel() const
{
	return *SystemViewModel;
}

TSharedPtr<FNiagaraEmitterHandleViewModel> UMovieSceneNiagaraEmitterSectionBase::GetEmitterHandleViewModel() const
{
	return EmitterHandleViewModel.Pin();
}

FName UMovieSceneNiagaraEmitterSectionBase::GetInstanceName() const
{
	return InstanceName;
}

void UMovieSceneNiagaraEmitterSectionBase::SetInstanceName(FName InInstanceName)
{
	InstanceName = InInstanceName;
}

TSharedRef<ISequencerSection> UMovieSceneNiagaraEmitterSectionBase::MakeInvalidSectionInterface()
{
	checkf(false, TEXT("Invalid section interface can't be used."));
	return MakeShared<FNiagaraInvalidSequencerSection>();
}

UMovieSceneNiagaraEmitterTrack::UMovieSceneNiagaraEmitterTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMovieSceneNiagaraEmitterTrack::Initialize(FNiagaraSystemViewModel& InSystemViewModel, TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel, const FFrameRate& InFrameResolution)
{
	ClearFlags(RF_Transactional);
	SystemViewModel = &InSystemViewModel;
	EmitterHandleViewModel = InEmitterHandleViewModel;
	SetDisplayName(InEmitterHandleViewModel->GetNameText());
	EmitterHandleId = InEmitterHandleViewModel->GetId();
	SetColorTint(FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.NiagaraSequence.DefaultTrackColor").ToFColor(true));
	CreateSections(InFrameResolution);
}

bool UMovieSceneNiagaraEmitterTrack::CanRename() const
{
	if (SystemViewModel && SystemViewModel->GetEditMode() == ENiagaraSystemViewModelEditMode::SystemAsset)
	{
		return true;
	}
	return false;
}

FNiagaraSystemViewModel& UMovieSceneNiagaraEmitterTrack::GetSystemViewModel() const
{
	return *SystemViewModel;
}

TSharedPtr<FNiagaraEmitterHandleViewModel> UMovieSceneNiagaraEmitterTrack::GetEmitterHandleViewModel() const
{
	return EmitterHandleViewModel.Pin();
}

void UMovieSceneNiagaraEmitterTrack::UpdateTrackFromEmitterGraphChange(const FFrameRate& InFrameResolution)
{
	SetDisplayName(EmitterHandleViewModel.Pin()->GetNameText());
	Sections.Empty();
	CreateSections(InFrameResolution);
}

void UMovieSceneNiagaraEmitterTrack::UpdateTrackFromEmitterParameterChange(const FFrameRate& InFrameResolution)
{
	for (UMovieSceneSection* Section : Sections)
	{
		CastChecked<UMovieSceneNiagaraEmitterSectionBase>(Section)->UpdateSectionFromModules(InFrameResolution);
		Section->SetIsActive(EmitterHandleViewModel.Pin()->GetIsEnabled());
	}
}

void UMovieSceneNiagaraEmitterTrack::UpdateEmitterHandleFromTrackChange(const FFrameRate& InFrameResolution)
{
	if (Sections.Num() > 0)
	{
		TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModelPinned = EmitterHandleViewModel.Pin();
		checkf(EmitterHandleViewModelPinned.IsValid(), TEXT("Emitter handle view model is invalid while it's track is still active."));

		TOptional<bool> SectionsActive = Sections[0]->IsActive();
		for (UMovieSceneSection* Section : Sections)
		{
			CastChecked<UMovieSceneNiagaraEmitterSectionBase>(Section)->UpdateModulesFromSection(InFrameResolution);
			if (SectionsActive.IsSet() && Section->IsActive() != SectionsActive.GetValue())
			{
				SectionsActive.Reset();
			}
		}

		if (SectionsActive.IsSet())
		{
			EmitterHandleViewModelPinned->SetIsEnabled(SectionsActive.GetValue());
		}
		else
		{
			// If there were mixed active states on the sections then one was toggled from the emitter handle state so we need
			// to update the emitter is active and the other sections.
			EmitterHandleViewModelPinned->SetIsEnabled(!EmitterHandleViewModelPinned->GetIsEnabled());
			for (UMovieSceneSection* Section : Sections)
			{
				Section->SetIsActive(EmitterHandleViewModelPinned->GetIsEnabled());
			}
		}
	}
}

bool UMovieSceneNiagaraEmitterTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}

void UMovieSceneNiagaraEmitterTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}

bool UMovieSceneNiagaraEmitterTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}

const TArray<UMovieSceneSection*>& UMovieSceneNiagaraEmitterTrack::GetAllSections() const
{
	return Sections;
}

bool UMovieSceneNiagaraEmitterTrack::SupportsMultipleRows() const
{
	return true;
}

FGuid UMovieSceneNiagaraEmitterTrack::GetEmitterHandleId() const
{
	return EmitterHandleId;
}

const TArray<FText>& UMovieSceneNiagaraEmitterTrack::GetSectionInitializationErrors() const
{
	return SectionInitializationErrors;
}

void UMovieSceneNiagaraEmitterTrack::CreateSections(const FFrameRate& InFrameResolution)
{
	SectionInitializationErrors.Empty();

	UNiagaraScript* EmitterUpdateScript = GetEmitterHandleViewModel()->GetEmitterViewModel()->GetEmitter()->GetScript(ENiagaraScriptUsage::EmitterUpdateScript, FGuid());
	UNiagaraScriptSource* ScriptSource = CastChecked<UNiagaraScriptSource>(EmitterUpdateScript->GetSource());
	UNiagaraNodeOutput* OutputNode = ScriptSource->NodeGraph->FindOutputNode(ENiagaraScriptUsage::EmitterUpdateScript);

	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackGroups;
	FNiagaraStackGraphUtilities::GetStackNodeGroups(*OutputNode, StackGroups);
	for (FNiagaraStackGraphUtilities::FStackNodeGroup& StackGroup : StackGroups)
	{
		UNiagaraNodeFunctionCall* FunctionNode = Cast<UNiagaraNodeFunctionCall>(StackGroup.EndNode);
		if (FunctionNode != nullptr && FunctionNode->FunctionScript != nullptr)
		{
			FString* SectionClassName = FunctionNode->FunctionScript->ScriptMetaData.Find("NiagaraTimelineSectionClass");
			if (SectionClassName != nullptr)
			{
				UClass* SectionClass = FindObject<UClass>(ANY_PACKAGE, **SectionClassName);
				if (SectionClass != nullptr)
				{
					UMovieSceneNiagaraEmitterSectionBase* EmitterSection = nullptr;
					bool bSectionCreated = false;

					UMovieSceneSection** SharedEmitterSection = Sections.FindByPredicate([=](UMovieSceneSection* Section) {	return Section->IsA(SectionClass); });
					if (SharedEmitterSection != nullptr)
					{
						EmitterSection = CastChecked<UMovieSceneNiagaraEmitterSectionBase>(*SharedEmitterSection);
					}

					if(EmitterSection == nullptr)
					{
						EmitterSection = NewObject<UMovieSceneNiagaraEmitterSectionBase>(this, SectionClass);
						EmitterSection->Initialize(*SystemViewModel, EmitterHandleViewModel.Pin().ToSharedRef());
						bSectionCreated = true;
					}

					FText AddError;
					if (EmitterSection->TryAddModule(*FunctionNode, AddError))
					{
						if (bSectionCreated)
						{
							EmitterSection->SetRowIndex(Sections.Num());
							Sections.Add(EmitterSection);
						}
					}
					else
					{
						FText SectionInitializationError = FText::Format(LOCTEXT("AddModuleErrorFormat", "Failed to add module {0} to section of type {1}.\nMessage: {2}"),
							FText::FromString(FunctionNode->GetFunctionName()), FText::FromString(*SectionClassName), AddError);
						SectionInitializationErrors.Add(SectionInitializationError);
					}
				}
				else
				{
					SectionInitializationErrors.Add(
						FText::Format(LOCTEXT("SectionClassNotFoundErrorFormat", "Module script {0} tried to use {1} as it's timeline section class, but it could not be found."),
							FText::FromString(FunctionNode->FunctionScript->GetPathName()),
							FText::FromString(*SectionClassName)));
				}
			}
		}
	}

	UpdateTrackFromEmitterParameterChange(InFrameResolution);
}

#undef LOCTEXT_NAMESPACE

