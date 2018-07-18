// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRigControl.h"
#include "ControlRigStaticMeshControl.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** An actor used to represent a rig control with a static mesh component */
UCLASS()
class AControlRigStaticMeshControl : public AControlRigControl
{
	GENERATED_BODY()

public:
	AControlRigStaticMeshControl(const FObjectInitializer& ObjectInitializer);

	/** AControlRigControl interface */
	virtual void SetTransform(const FTransform& InTransform) override;
	virtual void TickControl(float InDeltaSeconds, FRigUnit_Control& InRigUnit, UScriptStruct* InRigUnitStruct) override;

private:
	UPROPERTY(Category = Components, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USceneComponent* Scene;

	UPROPERTY(Category = Components, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Mesh;
};