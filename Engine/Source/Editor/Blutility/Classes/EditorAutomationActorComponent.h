// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/**
 * Base class of any editor-only automation test actor components
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/ScriptMacros.h"
#include "EditorAutomationActorComponent.generated.h"


UCLASS(Abstract, Blueprintable)
class BLUTILITY_API UEditorAutomationActorComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	
};
