// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Editor/UnrealEdEngine.h"
#include "DisplayClusterEditorEngine.generated.h"

class IPDisplayCluster;


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
