// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterEditorEngine.h"
#include "DisplayClusterEditorLog.h"

#include "DisplayCluster/Private/IPDisplayCluster.h"


void UDisplayClusterEditorEngine::Init(IEngineLoop* InEngineLoop)
{
	UE_LOG(LogDisplayClusterEditorEngine, VeryVerbose, TEXT("UDisplayClusterEditorEngine::Init"));

	// Initialize DisplayCluster module for editor mode
	DisplayClusterModule = static_cast<IPDisplayCluster*>(&IDisplayCluster::Get());
	if (DisplayClusterModule)
	{
		const bool bResult = DisplayClusterModule->Init(EDisplayClusterOperationMode::Editor);
		if (bResult)
		{
			UE_LOG(LogDisplayClusterEditorEngine, Log, TEXT("DisplayCluster module has been initialized"));
		}
		else
		{
			UE_LOG(LogDisplayClusterEditorEngine, Error, TEXT("An error occured during DisplayCluster initialization"));
		}
	}
	else
	{
		UE_LOG(LogDisplayClusterEditorEngine, Error, TEXT("Couldn't initialize DisplayCluster module"));
	}

	return Super::Init(InEngineLoop);
}

void UDisplayClusterEditorEngine::PreExit()
{
	UE_LOG(LogDisplayClusterEditorEngine, VeryVerbose, TEXT("UDisplayClusterEditorEngine::PreExit"));

	Super::PreExit();
}

void UDisplayClusterEditorEngine::PlayInEditor(UWorld* InWorld, bool bInSimulateInEditor, FPlayInEditorOverrides Overrides)
{
	UE_LOG(LogDisplayClusterEditorEngine, VeryVerbose, TEXT("UDisplayClusterEditorEngine::PlayInEditor"));

	Super::PlayInEditor(InWorld, bInSimulateInEditor, Overrides);
}
