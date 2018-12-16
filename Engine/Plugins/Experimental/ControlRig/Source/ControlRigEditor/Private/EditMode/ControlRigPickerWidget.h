// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "ControlRigPickerWidget.generated.h"

class FControlRigEditMode;

UCLASS()
class UControlRigPickerWidget : public UUserWidget
{
	GENERATED_BODY()

	UControlRigPickerWidget(const FObjectInitializer& ObjectInitializer);

	/** 
	 * Selected/deselect the specified control by property path
	 * @param	ControlPropertyPath		The property path to the control unit in the rig
	 * @param	bSelected				Whether the control is selected or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Control Rig Picker")
	void SelectControl(const FString& ControlPropertyPath, bool bSelected);

	/** 
	 * Get whether a control is selected, specified by property path
	 * @param	ControlPropertyPath		The property path to the control unit in the rig
	 * @return whether the control is selected or not
	 */
	UFUNCTION(BlueprintPure, Category = "Control Rig Picker")
	bool IsControlSelected(const FString& ControlPropertyPath);

	/** 
	 * Enable/disable the specified control by property path
	 * @param	ControlPropertyPath		The property path to the control unit in the rig
	 * @param	bEnable					Whether the control is enabled or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Control Rig Picker")
	void EnableControl(const FString& ControlPropertyPath, bool bEnabled);

	/** 
	 * Get whether a control is enabled, specified by property path 
	 * @param	ControlPropertyPath		The property path to the control unit in the rig
	 * @return whether the control is enabled or not
	 */
	UFUNCTION(BlueprintPure, Category = "Control Rig Picker")
	bool IsControlEnabled(const FString& ControlPropertyPath);

private:
	friend class SEditorUserWidgetHost;

	/** Bind to an edit mode */
	void SetEditMode(FControlRigEditMode* InEditMode);

private:
	/** Our bound edit mode */
	FControlRigEditMode* EditMode;
};