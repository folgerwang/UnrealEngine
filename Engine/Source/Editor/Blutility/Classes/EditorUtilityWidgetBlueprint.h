// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
enum class EAssetEditorCloseReason : uint8;

UCLASS()
class UEditorUtilityWidgetBlueprint : public UWidgetBlueprint
{
	GENERATED_UCLASS_BODY()

public:
	virtual void BeginDestroy() override;

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

	void SetRegistrationName(FName InRegistrationName)
	{
		RegistrationName = InRegistrationName;
	}

	FName GetRegistrationName() const
	{
		return RegistrationName;
	}

private:
	FName RegistrationName;

	TWeakPtr<SDockTab> CreatedTab;

	UEditorUtilityWidget* CreatedUMGWidget;

};
