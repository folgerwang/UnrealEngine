// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/**
 * Base class of any editor-only automation test actors
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ScriptMacros.h"
#include "EditorAutomationActor.generated.h"


UCLASS(Abstract, Blueprintable)
class BLUTILITY_API AEditorAutomationActor : public AActor
{
	GENERATED_UCLASS_BODY()

	
};
