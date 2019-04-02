// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "IControlRigModule.h"
#include "Materials/Material.h"

class FControlRigModule : public IControlRigModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
#if WITH_EDITOR
	UMaterial* ManipulatorMaterial;
#endif
	
private:
	FDelegateHandle OnCreateMovieSceneObjectSpawnerHandle;
};
