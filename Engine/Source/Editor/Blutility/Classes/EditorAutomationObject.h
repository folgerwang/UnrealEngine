// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/**
 * Base class of any editor-only automation test objects
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "EditorAutomationObject.generated.h"


UCLASS(Abstract, Blueprintable)
class BLUTILITY_API UEditorAutomationObject : public UObject
{
	GENERATED_UCLASS_BODY()

	
};
