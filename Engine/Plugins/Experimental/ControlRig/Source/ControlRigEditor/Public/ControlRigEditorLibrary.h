// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ControlRigEditorLibrary.generated.h"

UCLASS(meta=(ScriptName="ControlRigEditorLibrary"))
class CONTROLRIGEDITOR_API UControlRigEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, Category = "ControlRig")
	static URigUnitEditor_Base* GetEditorObject( UControlRig* ControlRig, const FName& RigUnitName );
};

