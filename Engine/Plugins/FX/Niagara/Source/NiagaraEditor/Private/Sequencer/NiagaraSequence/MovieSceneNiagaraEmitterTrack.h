// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneNameableTrack.h"
#include "MovieSceneSection.h"
#include "MovieSceneNiagaraEmitterTrack.generated.h"

class UNiagaraSystem;
class FNiagaraSystemViewModel;
class FNiagaraEmitterHandleViewModel;
class UNiagaraNodeFunctionCall;
class ISequencerSection;

UCLASS(MinimalAPI)
class UMovieSceneNiagaraEmitterSectionBase : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	void Initialize(FNiagaraSystemViewModel& InSystemViewModel, TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel);

	FNiagaraSystemViewModel& GetSystemViewModel() const;

	TSharedPtr<FNiagaraEmitterHandleViewModel> GetEmitterHandleViewModel() const;

	FName GetInstanceName() const;

	void SetInstanceName(FName InInstanceName);

	virtual bool TryAddModule(UNiagaraNodeFunctionCall& InModule, FText& OutErrorMessage) PURE_VIRTUAL(UMovieSceneNiagaraEmitterSectionBase::TryAddModule, return false;);

	virtual void UpdateSectionFromModules(const FFrameRate& InFrameResolution) PURE_VIRTUAL(UMovieSceneNiagaraEmitterSectionBase::UpdateSectionFromModules, );

	virtual void UpdateModulesFromSection(const FFrameRate& InFrameResolution) PURE_VIRTUAL(UMovieSceneNiagaraEmitterSectionBase::UpdateModulesFromSection, );

	virtual TSharedRef<ISequencerSection> MakeSectionInterface() PURE_VIRTUAL(UMovieSceneNiagaraEmitterSectionBase::UpdateModulesFromSection, return MakeInvalidSectionInterface(););

private:
	TSharedRef<ISequencerSection> MakeInvalidSectionInterface();

	FNiagaraSystemViewModel* SystemViewModel;

	TWeakPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel;

	FName InstanceName;
};

/**
*	A track representing an emitter in the niagara effect editor timeline.
*/
UCLASS(MinimalAPI)
class UMovieSceneNiagaraEmitterTrack
	: public UMovieSceneNameableTrack
{
	GENERATED_UCLASS_BODY()

public:
	void Initialize(FNiagaraSystemViewModel& SystemViewModel, TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel, const FFrameRate& InFrameResolution);

	virtual bool CanRename() const override;

	FNiagaraSystemViewModel& GetSystemViewModel() const;

	TSharedPtr<FNiagaraEmitterHandleViewModel> GetEmitterHandleViewModel() const;

	void UpdateTrackFromEmitterGraphChange(const FFrameRate& InFrameResolution);

	void UpdateTrackFromEmitterParameterChange(const FFrameRate& InFrameResolution);

	void UpdateEmitterHandleFromTrackChange(const FFrameRate& InFrameResolution);

	//~ UMovieSceneTrack interface
	virtual void RemoveAllAnimationData() override { }
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual bool SupportsMultipleRows() const override;

	/** Gets the unique id for the emitter handle that was associated with this track; used for copy/paste detection */
	FGuid GetEmitterHandleId() const;

	const TArray<FText>& GetSectionInitializationErrors() const;

private:
	void CreateSections(const FFrameRate& InFrameResolution);

private:
	FNiagaraSystemViewModel* SystemViewModel;

	TWeakPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel;

	UPROPERTY()
	TArray<UMovieSceneSection*> Sections;

	// Used for detecting copy/paste 
	UPROPERTY()
	FGuid EmitterHandleId;

	TArray<FText> SectionInitializationErrors;
};