// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/**
 * Widget for editor utilities
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "WidgetBlueprint.h"
#include "Widgets/Docking/SDockTab.h"
#include "EditorUtilityWidgetBlueprint.generated.h"

class UBlueprint;
class UEditorUtilityWidget;

UCLASS()
class UEditorUtilityWidgetBlueprint : public UWidgetBlueprint
{
	GENERATED_UCLASS_BODY()

public:
	TSharedRef<SDockTab> SpawnEditorUITab(const FSpawnTabArgs& SpawnTabArgs);

	/** Creates the slate widget from the UMG widget */
	TSharedRef<SWidget> CreateUtilityWidget();

	/** Recreate the tab's content on recompile */
	void RegenerateCreatedTab(UBlueprint* RecompiledBlueprint);
	
	void UpdateRespawnListIfNeeded(TSharedRef<SDockTab> TabBeingClosed);

	// UBlueprint interface
	virtual void GetReparentingRules(TSet< const UClass* >& AllowedChildrenOfClasses, TSet< const UClass* >& DisallowedChildrenOfClasses) const override;

	UEditorUtilityWidget* GetCreatedWidget() const
	{
		return CreatedUMGWidget;
	}

private:
	TWeakPtr<SDockTab> CreatedTab;

	UEditorUtilityWidget* CreatedUMGWidget;
};
