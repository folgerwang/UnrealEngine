// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ViewModels/Stack/NiagaraStackItem.h"
#include "NiagaraTypes.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "NiagaraParameterStore.h"
#include "NiagaraStackParameterStoreEntry.generated.h"

class UNiagaraNodeFunctionCall;
class UNiagaraNodeAssignment;
class UNiagaraNodeParameterMapSet;
class FStructOnScope;
class UNiagaraStackObject;
class FNiagaraSystemViewModel;
class UEdGraphPin;

/** Represents a single module input in the module stack view model. */
UCLASS()
class NIAGARAEDITOR_API UNiagaraStackParameterStoreEntry : public UNiagaraStackItemContent
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnValueChanged);
	DECLARE_MULTICAST_DELEGATE(FOnParameterDeleted);

public:
	UNiagaraStackParameterStoreEntry();

	/** 
	 * Sets the input data for this entry.
	 * @param InSystemViewModel The view model for the system which owns the stack containing this entry.
	 * @param InEmitterViewModel The view model for the emitter which owns the stack containing this entry.
	 * @param InStackEditorData The stack editor data for this input.
	 * @param InInputParameterHandle The input parameter handle for the function call.
	 * @param InInputType The type of this input.
	 * @param InOwnerStackItemEditorDataKey The editor data key for the item that owns this entry.
	 */
	void Initialize(
		FRequiredEntryData InRequiredEntryData,
		UObject* InOwner,
		FNiagaraParameterStore* InParameterStore,
		FString InInputParameterHandle,
		FNiagaraTypeDefinition InInputType,
		FString InOwnerStackItemEditorDataKey);

	/** Gets the type of this input. */
	const FNiagaraTypeDefinition& GetInputType() const;

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;

	/** Gets the current struct value of this input is there is one. */
	TSharedPtr<FStructOnScope> GetValueStruct();

	/** Gets the current object value of this input is there is one. */
	UNiagaraDataInterface* GetValueObject();

	/** Called to notify the input that an ongoing change to it's value has begun. */
	void NotifyBeginValueChange();

	/** Called to notify the input that an ongoing change to it's value has ended. */
	void NotifyEndValueChange();
	
	/** Called to notify the input that it's value has been changed. */
	void NotifyValueChanged();

	/** Returns whether or not the value or handle of this input has been overridden and can be reset. */
	bool CanReset() const;

	/** Resets the value and handle of this input to the value and handle defined in the module. */
	void Reset();

	/** Returns whether or not this input can be renamed. */
	bool CanRenameInput() const;

	/** Gets whether this input has a rename pending. */
	bool GetIsRenamePending() const;

	/** Sets whether this input has a rename pending. */
	void SetIsRenamePending(bool bIsRenamePending);

	/** Renames this input to the name specified. */
	void RenameInput(FString NewName);

	/** Checks if the chosen name is unique (not duplicate) */
	bool IsUniqueName(FString NewName);

	/** Gets a multicast delegate which is called whenever the value on this input changes. */
	FOnValueChanged& OnValueChanged();

	/** Gets a multicast delegate which is called when this parameter is deleted. */
	FOnParameterDeleted& OnParameterDeleted();

	/** Delete the parameter from the ParameterStore and notify that the store changed. */
	void Delete();

protected:
	//~ UNiagaraStackEntry interface
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	void RefreshValueAndHandle();
	TSharedPtr<FNiagaraVariable> GetCurrentValueVariable();
	UNiagaraDataInterface* GetCurrentValueObject();

private:
	void RemovePins(TArray<UEdGraphPin*> PinsToRemove);
	TArray<UEdGraphPin*> GetOwningPins(); 

private:
	/** */
	FName ParameterName;

	/** The nigara type definition for this input. */
	FNiagaraTypeDefinition InputType;

	/** The name of this input for display in the UI. */
	FText DisplayName;

	/** A local copy of the value of this input if one is available. */
	TSharedPtr<FStructOnScope> LocalValueStruct;

	/** A multicast delegate which is called when the value of this input is changed. */
	FOnValueChanged ValueChangedDelegate;

	/** A multicast delegate which is called when this parameter is deleted. */
	FOnParameterDeleted ParameterDeletedDelegate;

	/** A pointer to the data interface object for this input if one is available. */
	UPROPERTY()
	UNiagaraDataInterface* ValueObject;

	UPROPERTY()
	UObject* Owner;

	FNiagaraParameterStore* ParameterStore;

	/** The stack entry for displaying the value object. */
	UPROPERTY()
	UNiagaraStackObject* ValueObjectEntry;
};
