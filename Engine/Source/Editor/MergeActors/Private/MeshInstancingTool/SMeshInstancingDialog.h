// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

class FMeshInstancingTool;
class IDetailsView;
class UMeshInstancingSettingsObject;

/** Data structure used to keep track of the selected mesh components, and whether or not they should be incorporated in the merge */
struct FInstanceComponentData
{
	FInstanceComponentData(UPrimitiveComponent* InPrimComponent)
		: PrimComponent(InPrimComponent)
		, bShouldIncorporate( true )
	{}

	/** Component extracted from selected actors */
	TWeakObjectPtr<UPrimitiveComponent> PrimComponent;
	/** Flag determining whether or not this component should be incorporated into the merge */
	bool bShouldIncorporate;
};

/*-----------------------------------------------------------------------------
   SMeshInstancingDialog
-----------------------------------------------------------------------------*/
class SMeshInstancingDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMeshInstancingDialog)
	{
	}

	SLATE_END_ARGS()

public:
	/** **/
	SMeshInstancingDialog();
	~SMeshInstancingDialog();

	/** SWidget functions */
	void Construct(const FArguments& InArgs, FMeshInstancingTool* InTool);	
	
	/** Getter functionality */
	const TArray<TSharedPtr<FInstanceComponentData>>& GetSelectedComponents() const { return SelectedComponents; }
	/** Get number of selected meshes */
	const int32 GetNumSelectedMeshComponents() const { return NumSelectedMeshComponents; }

	/** Resets the state of the UI and flags it for refreshing */
	void Reset();
private:	
	/** Begin override SCompoundWidget */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	/** End override SCompoundWidget */

	/** Creates and sets up the settings view element*/
	void CreateSettingsView();	

	/** Delegate for the creation of the list view item's widget */
	TSharedRef<ITableRow> MakeComponentListItemWidget(TSharedPtr<FInstanceComponentData> ComponentData, const TSharedRef<STableViewBase>& OwnerTable);

	/** Delegate to determine whether or not the UI elements should be enabled (determined by number of selected actors / mesh components) */
	bool GetContentEnabledState() const;

	/** Editor delgates for map and selection changes */
	void OnLevelSelectionChanged(UObject* Obj);
	void OnMapChange(uint32 MapFlags);
	void OnNewCurrentLevel();
	void OnSettingChanged(const FPropertyChangedEvent& InPropertyChangedEvent);

	/** Updates SelectedMeshComponent array according to retrieved mesh components from editor selection*/
	void UpdateSelectedStaticMeshComponents();	
	/** Stores the individual check box states for the currently selected mesh components */
	void StoreCheckBoxState();

	/** Refresh the predicted results text */;
	void RefreshPredictedResultsText();

	/** Get the predicted results text */
	FText GetPredictedResultsText() const;

private:
	/** Owning mesh instancing tool */
	FMeshInstancingTool* Tool;
	/** List of mesh components extracted from editor selection */
	TArray<TSharedPtr<FInstanceComponentData>> SelectedComponents;
	/** List view ui element */
	TSharedPtr<SListView<TSharedPtr<FInstanceComponentData>>> ComponentsListView;
	/** Map of keeping track of checkbox states for each selected component (used to restore state when listview is refreshed) */
	TMap<UPrimitiveComponent*, ECheckBoxState> StoredCheckBoxStates;
	/** Settings view ui element ptr */
	TSharedPtr<IDetailsView> SettingsView;
	/** Cached pointer to mesh instancing setting singleton object */
	UMeshInstancingSettingsObject* InstancingSettings;
	/** List view state tracking data */
	bool bRefreshListView;

	/** Number of selected static mesh components */
	int32 NumSelectedMeshComponents;

	/** Predicted results of the merge given the current settings */
	FText PredictedResultsText;
};
