// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"

extern const FName UMGEditorAppIdentifier;

class FUMGEditor;
class FWidgetBlueprintCompiler;

/** The public interface of the UMG editor module. */
class IUMGEditorModule : public IModuleInterface, public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	virtual FWidgetBlueprintCompiler* GetRegisteredCompiler() = 0;
};
