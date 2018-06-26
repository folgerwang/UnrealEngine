// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Editor/UnrealEdEngine.h"
#include "DisplayClusterEditorEngine.generated.h"

struct IPDisplayCluster;


/**
 * Extended editor engine
 */
UCLASS()
class UDisplayClusterEditorEngine
	: public UUnrealEdEngine
{
	GENERATED_BODY()

public:
	virtual void Init(IEngineLoop* InEngineLoop) override;
	virtual void PreExit() override;
	virtual void PlayInEditor(UWorld* InWorld, bool bInSimulateInEditor, FPlayInEditorOverrides Overrides = FPlayInEditorOverrides()) override;

private:
	
	IPDisplayCluster* DisplayClusterModule = nullptr;
};
