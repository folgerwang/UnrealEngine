// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"


class UObject;
struct FLevelSequenceActionExtender;


/**
 * Interface for the Level Sequence Editor module.
 */
class ILevelSequenceEditorModule
	: public IModuleInterface
{
public:
	DECLARE_EVENT_OneParam(ILevelSequenceEditorModule, FOnMasterSequenceCreated, UObject*);
	virtual FOnMasterSequenceCreated& OnMasterSequenceCreated() = 0;

	virtual void RegisterLevelSequenceActionExtender(TSharedRef<FLevelSequenceActionExtender> InExtender) = 0;
	virtual void UnregisterLevelSequenceActionExtender(TSharedRef<FLevelSequenceActionExtender> InExtender) = 0;
};
