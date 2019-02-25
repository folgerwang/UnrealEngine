// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/**
 * Widget for editor utilities
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Blueprint/UserWidget.h"
#include "EditorUtilityWidget.generated.h"

class AActor;
class UEditorPerProjectUserSettings;

UCLASS(Abstract, config = Editor)
class BLUTILITY_API UEditorUtilityWidget : public UUserWidget
{
	GENERATED_UCLASS_BODY()

public:
	// The default action called when the widget is invoked if bAutoRunDefaultAction=true (it is never called otherwise)
	UFUNCTION(BlueprintImplementableEvent)
	void OnDefaultActionClicked();

	// Run the default action
	void ExecuteDefaultAction();

	bool ShouldAlwaysReregisterWithWindowsMenu() const
	{
		return bAlwaysReregisterWithWindowsMenu;
	}

	bool ShouldAutoRunDefaultAction() const
	{
		return bAutoRunDefaultAction;
	}

protected:
	UPROPERTY(Category = Config, EditDefaultsOnly, BlueprintReadWrite, AssetRegistrySearchable)
	FString HelpText;

	// Should this widget always be re-added to the windows menu once it's opened
	UPROPERTY(Config, Category = Settings, EditDefaultsOnly)
	bool bAlwaysReregisterWithWindowsMenu;

	// Should this blueprint automatically run OnDefaultActionClicked, or should it open up a details panel to edit properties and/or offer multiple buttons
	UPROPERTY(Category = Settings, EditDefaultsOnly, BlueprintReadOnly)
	bool bAutoRunDefaultAction;

};
