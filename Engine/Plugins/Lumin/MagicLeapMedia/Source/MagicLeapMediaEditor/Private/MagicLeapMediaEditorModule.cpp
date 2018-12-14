// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * Implements the MagicLeapMediaEditor module.
 */
class FMagicLeapMediaEditorModule : public IModuleInterface
{
public:
  /** IModuleInterface interface */
  virtual void StartupModule() override { }
  virtual void ShutdownModule() override { }
};

IMPLEMENT_MODULE(FMagicLeapMediaEditorModule, MagicLeapMediaEditor);
