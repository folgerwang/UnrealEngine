// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "ControlRigControl.generated.h"

struct FRigUnit_Control;

/** An actor used to represent a rig control */
UCLASS(Abstract)
class CONTROLRIG_API AControlRigControl : public AActor
{
	GENERATED_BODY()

public:
	AControlRigControl(const FObjectInitializer& ObjectInitializer);

	/** Set the property path to rig property we are controlling */
	void SetPropertyPath(const FString& InPropertyPath);

	/** Get the property path to rig property we are controlling */
	const FString& GetPropertyPath() const;

	/** Set the transform for this control */
	virtual void SetTransform(const FTransform& InTransform);

	/** Set the transform for this control */
	virtual const FTransform& GetTransform() const;

	/** Set the control to be enabled/disabled */
	virtual void SetEnabled(bool bInEnabled);

	/** Get whether the control is enabled/disabled */
	virtual bool IsEnabled() const;

	/** Set the control to be selected/unselected */
	virtual void SetSelected(bool bInSelected);

	/** Get whether the control is selected/unselected */
	virtual bool IsSelected() const;

	/** Set the control to be hovered */
	virtual void SetHovered(bool bInHovered);

	/** Get whether the control is hovered */
	virtual bool IsHovered() const;

	/** Set whether the control is being manipulated */
	virtual void SetManipulating(bool bInManipulating);

	/** Get whether the control is being manipulated */
	virtual bool IsManipulating() const;

	/** Called from the edit mode each tick */
	virtual void TickControl(float InDeltaSeconds, FRigUnit_Control& InRigUnit, UScriptStruct* InRigUnitStruct);

	/** Event called when the transform of this control has changed */
	UFUNCTION(BlueprintImplementableEvent)
	void OnTransformChanged(const FTransform& NewTransform);

	/** Event called when the enabled state of this control has changed */
	UFUNCTION(BlueprintImplementableEvent)
	void OnEnabledChanged(bool bIsEnabled);

	/** Event called when the selection state of this control has changed */
	UFUNCTION(BlueprintImplementableEvent)
	void OnSelectionChanged(bool bIsSelected);

	/** Event called when the hovered state of this control has changed */
	UFUNCTION(BlueprintImplementableEvent)
	void OnHoveredChanged(bool bIsSelected);

	/** Event called when the manipulating state of this control has changed */
	UFUNCTION(BlueprintImplementableEvent)
	void OnManipulatingChanged(bool bIsManipulating);

	/** Property path to rig property we are controlling */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control")
	FString PropertyPath;

	/** The transform (in world space) used by this control */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control")
	FTransform Transform;

	/** Whether this control is enabled */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control")
	uint8 bEnabled : 1;

	/** Whether this control is selected */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control")
	uint8 bSelected : 1;

	/** Whether this control is hovered */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control")
	uint8 bHovered : 1;

	/** Whether this control is being manipulated */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control")
	uint8 bManipulating : 1;
};