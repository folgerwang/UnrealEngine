// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ISequencerModule.h"
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "GeometryCacheTrackEditor.h"

/**
 * The public interface to this module
 */
class FGeometryCacheSequencerModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{

		ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
		TrackEditorBindingHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FGeometryCacheTrackEditor::CreateTrackEditor));

	}

	virtual void ShutdownModule() override
	{

		ISequencerModule* SequencerModulePtr = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer");
		if (SequencerModulePtr)
		{
			SequencerModulePtr->UnRegisterTrackEditor(TrackEditorBindingHandle);
		}

	}

	FDelegateHandle TrackEditorBindingHandle;
};

