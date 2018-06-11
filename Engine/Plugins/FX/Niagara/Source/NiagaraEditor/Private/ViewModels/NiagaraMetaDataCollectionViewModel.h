// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraTypes.h"
#include "TickableEditorObject.h"

class UNiagaraScript;
class UNiagaraGraph;
class FNiagaraMetaDataViewModel;

class FNiagaraMetaDataCollectionViewModel : public FTickableEditorObject
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnCollectionChanged);

public:
	FNiagaraMetaDataCollectionViewModel();

	~FNiagaraMetaDataCollectionViewModel();

	//~ FTickableEditorObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	/** Sets the view model to a new script. */
	void SetGraph(UNiagaraGraph* InGraph);

	/** Gets the metadata view models. */
	TArray<TSharedRef<FNiagaraMetaDataViewModel>>& GetVariableModels();
	
	/** Request refresh the data from the graph and builds the viewmodels next frame. */
	void RequestRefresh();

	/** returns the delegate to be called when the collection changes*/
	FOnCollectionChanged& OnCollectionChanged() { return OnCollectionChangedDelegate; }

private:
	void Refresh();
	void SortViewModels();
	/** Callback for when the graph changes and the collection viewmodel needs to react */
	void OnGraphChanged(const struct FEdGraphEditAction& InAction);
	void Cleanup();
	/** Removes metadata listeners and empties MetaDataViewModels array */
	void CleanupMetadata();
	/** Called when one of the metadata in the viewmodel array has changed */
	void ChildMetadataChanged();
	TSharedPtr<FNiagaraMetaDataViewModel> GetMetadataViewModelForVariable(FNiagaraVariable InVariable);

private:
	/** The variables */
	TArray <TSharedRef<FNiagaraMetaDataViewModel>> MetaDataViewModels;

	/** The actual graph of the module we're editing */
	UNiagaraGraph* ModuleGraph;

	/** The handle to the graph changed delegate. */
	FDelegateHandle OnGraphChangedHandle;
	FDelegateHandle OnRecompileHandle;

	/** A multicast delegate which is called whenever the parameter collection is changed. */
	FOnCollectionChanged OnCollectionChangedDelegate;
	
	/** Refresh the UI next frames. */
	bool bNeedsRefresh;

	/** Guard flag that goes up when the graph change was done internally within this object, so we need to avoid refreshing. */
	bool bInternalGraphChange;
};