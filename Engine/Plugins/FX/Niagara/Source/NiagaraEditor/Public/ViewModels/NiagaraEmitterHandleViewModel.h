// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Delegates/Delegate.h"
#include "Types/SlateEnums.h"
#include "Styling/SlateTypes.h"
#include "Layout/Visibility.h"

class UNiagaraSystem;
struct FNiagaraEmitterHandle;
struct FNiagaraEmitterInstance;
class FNiagaraEmitterViewModel;

/** The view model for the FNiagaraEmitterEditorWidget. */
class FNiagaraEmitterHandleViewModel : public TSharedFromThis<FNiagaraEmitterHandleViewModel>
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnPropertyChanged);
	DECLARE_MULTICAST_DELEGATE(FOnNameChanged);
public:
	/** Creates a new emitter editor view model with the supplied emitter handle and simulation. */
	FNiagaraEmitterHandleViewModel(FNiagaraEmitterHandle* InEmitterHandle, TWeakPtr<FNiagaraEmitterInstance> InSimulation, UNiagaraSystem& InOwningSystem);
	
	~FNiagaraEmitterHandleViewModel();

	/** Reuses a the emitter editor view model with the supplied emitter handle and simulation.*/
	bool Set(FNiagaraEmitterHandle* InEmitterHandle, TWeakPtr<FNiagaraEmitterInstance> InSimulation, UNiagaraSystem& InOwningSystem);

	/** Sets the emitter handle.*/
	void SetEmitterHandle(FNiagaraEmitterHandle* InEmitterHandle);

	/** Sets the simulation for the emitter this handle references. */
	void SetSimulation(TWeakPtr<FNiagaraEmitterInstance> InSimulation);

	/** Gets the id of the emitter handle. */
	FGuid GetId() const;
	FText GetIdText() const;

	/** Gets the name of the emitter handle. */
	FName GetName() const;

	/** Sets the name of the emitter handle. */
	void SetName(FName InName);

	/** Gets the text representation of the emitter handle name. */
	NIAGARAEDITOR_API FText GetNameText() const;

	/** Called when the contents of the name text control is committed. */
	NIAGARAEDITOR_API void OnNameTextComitted(const FText& InText, ETextCommit::Type CommitInfo);

	/** Prevent invalid name being set on emitter.*/
	NIAGARAEDITOR_API bool VerifyNameTextChanged(const FText& NewText, FText& OutErrorMessage);

	/** Called to get the error state of the emitter handle.*/
	FText GetErrorText() const;
	EVisibility GetErrorTextVisibility() const;
	FSlateColor GetErrorTextColor() const;

	/** Called to get the synch state of the emitter handle to its source.*/
	bool IsSynchronized() const;

	/** Gets whether or not this emitter handle is enabled. */
	NIAGARAEDITOR_API bool GetIsEnabled() const;

	/** Sets whether or not this emitter handle is enabled. */
	NIAGARAEDITOR_API void SetIsEnabled(bool bInIsEnabled);

	/** Gets the check state for the is enabled check box. */
	NIAGARAEDITOR_API ECheckBoxState GetIsEnabledCheckState() const;

	/** Called when the check state of the enabled check box changes. */
	NIAGARAEDITOR_API void OnIsEnabledCheckStateChanged(ECheckBoxState InCheckState);

	/** Gets the emitter handled being view and edited by this view model. */
	NIAGARAEDITOR_API FNiagaraEmitterHandle* GetEmitterHandle();

	/** Gets the view model for the emitter this handle references. */
	NIAGARAEDITOR_API TSharedPtr<FNiagaraEmitterViewModel> GetEmitterViewModel();


	/** Opens the source emitter in a stand alone asset editor. */
	void OpenSourceEmitter();

	/** Gets a multicast delegate which is called any time a property on the handle changes. */
	FOnPropertyChanged& OnPropertyChanged();

	/** Gets a multicast delegate which is called any time this handle is renamed. */
	FOnNameChanged& OnNameChanged();

	void Cleanup();

private:
	/** The emitter handle being displayed and edited by this view model. */
	FNiagaraEmitterHandle* EmitterHandle;

	/** The System which owns the handled being displayed and edited by this view model. */
	UNiagaraSystem& OwningSystem;

	/** The view model for emitter this handle references. */
	TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModel;

	/** A multicast delegate which is called any time a property on the handle changes. */
	FOnPropertyChanged OnPropertyChangedDelegate;

	/** A multicast delegate which is called any time this emitter handle is renamed. */
	FOnNameChanged OnNameChangedDelegate;
};
