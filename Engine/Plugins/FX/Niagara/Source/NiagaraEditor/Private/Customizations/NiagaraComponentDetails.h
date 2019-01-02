// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "EditorUndoClient.h"
#include "TickableEditorObject.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Misc/NotifyHook.h"
#include "NiagaraComponent.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class FNiagaraParameterViewModelCustomDetails;
class INiagaraParameterViewModel;
class SNiagaraParameterEditor;
class FNiagaraParameterViewModelCustomDetails;
class UNiagaraSystem;

class FNiagaraComponentDetails : public IDetailCustomization
{
public:
	virtual ~FNiagaraComponentDetails();

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

protected:
	void OnWorldDestroyed(class UWorld* InWorld);
	void OnPiEEnd();

	FNiagaraComponentDetails();

private:
	TWeakObjectPtr<UNiagaraComponent> Component;
	IDetailLayoutBuilder* Builder;
};
