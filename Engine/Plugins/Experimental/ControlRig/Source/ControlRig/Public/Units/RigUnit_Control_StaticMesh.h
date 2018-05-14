// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_Control.h"
#include "RigUnit_Control_StaticMesh.generated.h"

/** A control unit used to drive a transform from an external source */
USTRUCT(meta=(DisplayName="Static Mesh Control", Category="Controls", ShowVariableNameInTitle))
struct CONTROLRIG_API FRigUnit_Control_StaticMesh : public FRigUnit_Control
{
	GENERATED_BODY()

	FRigUnit_Control_StaticMesh();

#if WITH_EDITORONLY_DATA
	/** The static mesh to use to display this control */
	UPROPERTY(meta=(Input))
	UStaticMesh* StaticMesh;

	/** The override materials we use to display this control */
	UPROPERTY(meta=(Input))
	TArray<UMaterialInterface*> Materials;

	/** The the transform the mesh will be rendered with (applied on top of the control's transform in the viewport) */
	UPROPERTY(meta=(Input))
	FTransform MeshTransform;
#endif
};