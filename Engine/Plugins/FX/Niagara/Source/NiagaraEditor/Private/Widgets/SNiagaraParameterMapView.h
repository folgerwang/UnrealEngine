// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Delegates/Delegate.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "PropertyEditorDelegates.h"
#include "EdGraph/EdGraphSchema.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "NiagaraActions.h"
#include "EditorStyleSet.h"

class SGraphActionMenu;
class SEditableTextBox;
class SExpanderArrow;
class SSearchBox;
class SComboButton;
class SNiagaraGraphPinAdd;
class FNiagaraObjectSelection;
class UNiagaraGraph;
struct FEdGraphSchemaAction;

/* Enums to use when grouping the blueprint members in the list panel. The order here will determine the order in the list */
namespace NiagaraParameterMapSectionID
{
	enum Type
	{
		NONE = 0,
		MODULE,
		ENGINE,
		PARAMETERCOLLECTION,
		USER,
		SYSTEM,
		EMITTER,
		PARTICLE,
		OTHER,
	};

	static FText OnGetSectionTitle(const NiagaraParameterMapSectionID::Type InSection);
	static NiagaraParameterMapSectionID::Type OnGetSectionFromVariable(const FNiagaraVariable& InVar, FNiagaraParameterHandle& OutParameterHandle, const NiagaraParameterMapSectionID::Type DefaultType = NiagaraParameterMapSectionID::Type::NONE);
};

class FNiagaraParameterMapViewCommands : public TCommands<FNiagaraParameterMapViewCommands>
{
public:
	/** Constructor */
	FNiagaraParameterMapViewCommands()
		: TCommands<FNiagaraParameterMapViewCommands>(TEXT("NiagaraParameterMapViewCommands"), NSLOCTEXT("Contexts", "NiagaraParameterMap", "NiagaraParameterMap"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	// Basic operations
	TSharedPtr<FUICommandInfo> DeleteEntry;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};

/** A widget for viewing and editing a set of selected objects with a details panel. */
class SNiagaraParameterMapView : public SCompoundWidget
{
public:
	enum EToolkitType
	{
		SCRIPT,
		SYSTEM,
	};

	SLATE_BEGIN_ARGS(SNiagaraParameterMapView)
	{}
	SLATE_END_ARGS();

	virtual ~SNiagaraParameterMapView();

	void Construct(const FArguments& InArgs, const TSharedRef<FNiagaraObjectSelection>& InSelectedObjects, const EToolkitType InToolkitType, const TSharedPtr<FUICommandList>& InToolkitCommands);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Wheter the add parameter button should be enabled. */
	bool ParameterAddEnabled() const;

	/** Adds parameter to the graph parameter store and refreshes the menu. */
	void AddParameter(FNiagaraVariable NewVariable);

	/** Refreshes the graphs used for this menu. */
	void Refresh(bool bRefreshMenu = true);
	void RefreshEmitterHandles(const TArray<TSharedPtr<class FNiagaraEmitterHandleViewModel>>& EmitterHandles);

	static TSharedRef<SExpanderArrow> CreateCustomActionExpander(const struct FCustomExpanderData& ActionMenuData);

private:
	/** Callback when the filter is changed, forces the action tree(s) to filter */
	void OnFilterTextChanged(const FText& InFilterText);

	// SGraphActionMenu delegates
	FText GetFilterText() const;
	TSharedRef<SWidget> OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData);
	void CollectAllActions(FGraphActionListBuilderBase& OutAllActions);
	void CollectStaticSections(TArray<int32>& StaticSectionIDs);
	FReply OnActionDragged(const TArray<TSharedPtr<FEdGraphSchemaAction>>& InActions, const FPointerEvent& MouseEvent);
	void OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& InActions, ESelectInfo::Type InSelectionType);
	void OnActionDoubleClicked(const TArray<TSharedPtr<FEdGraphSchemaAction>>& InActions);
	TSharedPtr<SWidget> OnContextMenuOpening();
	FText OnGetSectionTitle(int32 InSectionID);
	TSharedRef<SWidget> OnGetSectionWidget(TSharedRef<SWidget> RowWidget, int32 InSectionID);
	TSharedRef<SWidget> CreateAddToSectionButton(const NiagaraParameterMapSectionID::Type InSection, TWeakPtr<SWidget> WeakRowWidget, FText AddNewText, FName MetaDataTag);
	
	/** Checks if the selected action has context menu */
	bool SelectionHasContextMenu() const;
	
	TSharedRef<SWidget> OnGetParameterMenu(const NiagaraParameterMapSectionID::Type InSection = NiagaraParameterMapSectionID::NONE);
	EVisibility OnAddButtonTextVisibility(TWeakPtr<SWidget> RowWidget, const NiagaraParameterMapSectionID::Type InSection) const;

	void SelectedObjectsChanged();
	void AddGraph(UNiagaraGraph* Graph);
	void AddGraph(class UNiagaraScriptSourceBase* SourceBase);
	void OnGraphChanged(const struct FEdGraphEditAction& InAction);

	//Callbacks
	void OnDeleteEntry();
	bool CanDeleteEntry() const;
	void OnRequestRenameOnActionNode();
	bool CanRequestRenameOnActionNode(TWeakPtr<struct FGraphActionNode> InSelectedNode) const;
	bool CanRequestRenameOnActionNode() const;
	void OnPostRenameActionNode(const FText& InText, FNiagaraParameterAction& InAction);

	bool IsSystemToolkit();
	bool IsScriptToolkit();

	/** Delegate handler used to match an FName to an action in the list, used for renaming keys */
	bool HandleActionMatchesName(struct FEdGraphSchemaAction* InAction, const FName& InName) const;

private:

	/** Sets bNeedsRefresh to true. Causing the list to be refreshed next tick. */
	void RefreshActions();

	/** Graph Action Menu for displaying all our variables and functions */
	TSharedPtr<SGraphActionMenu> GraphActionMenu;

	/** The filter box that handles filtering for both graph action menus. */
	TSharedPtr<SSearchBox> FilterBox;

	/** Add parameter buttons for all sections. */
	TArray<TSharedPtr<SComboButton>> AddParameterButtons;

	/** The selected objects being viewed and edited by this widget. */
	TSharedPtr<FNiagaraObjectSelection> SelectedObjects;

	TArray<TWeakObjectPtr<UNiagaraGraph>> Graphs;

	/** The handle to the graph changed delegate. */
	FDelegateHandle OnGraphChangedHandle;
	FDelegateHandle OnRecompileHandle;

	EToolkitType ToolkitType;
	TSharedPtr<FUICommandList> ToolkitCommands;

	bool bNeedsRefresh;
};

class SNiagaraAddParameterMenu : public SCompoundWidget
{
public:
	/** Delegate that can be used to create a widget for a particular action */
	DECLARE_DELEGATE_OneParam(FOnAddParameter, FNiagaraVariable);
	DECLARE_DELEGATE_TwoParams(FOnCollectCustomActions, FGraphActionListBuilderBase&, bool&);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnAllowMakeType, const FNiagaraTypeDefinition&);

	SLATE_BEGIN_ARGS(SNiagaraAddParameterMenu)
		: _Section(NiagaraParameterMapSectionID::NONE)
		, _AllowCreatingNew(true)
		, _ShowNamespaceCategory(true)
		, _ShowGraphParameters(true)
		, _AutoExpandMenu(false)
		, _IsParameterRead(true) {}
		SLATE_EVENT(FOnAddParameter, OnAddParameter)
		SLATE_EVENT(FOnCollectCustomActions, OnCollectCustomActions)
		SLATE_EVENT(FOnAllowMakeType, OnAllowMakeType)
		SLATE_ATTRIBUTE(NiagaraParameterMapSectionID::Type, Section)
		SLATE_ATTRIBUTE(bool, AllowCreatingNew)
		SLATE_ATTRIBUTE(bool, ShowNamespaceCategory)
		SLATE_ATTRIBUTE(bool, ShowGraphParameters)
		SLATE_ATTRIBUTE(bool, AutoExpandMenu)
		SLATE_ATTRIBUTE(bool, IsParameterRead)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TArray<TWeakObjectPtr<UNiagaraGraph>> InGraphs);

	TSharedRef<SEditableTextBox> GetSearchBox();

	void AddParameterGroup(FGraphActionListBuilderBase& OutActions, TArray<FNiagaraVariable>& Variables, const NiagaraParameterMapSectionID::Type InSection = NiagaraParameterMapSectionID::NONE, const FText& Category = FText::GetEmpty(), const FString& RootCategory = FString(), const bool bSort = true, const bool bCustomName = true);
	void CollectParameterCollectionsActions(FGraphActionListBuilderBase& OutActions);
	void CollectMakeNew(FGraphActionListBuilderBase& OutActions, const NiagaraParameterMapSectionID::Type InSection = NiagaraParameterMapSectionID::NONE);

private:
	void OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedActions, ESelectInfo::Type InSelectionType);
	void CollectAllActions(FGraphActionListBuilderBase& OutAllActions);
	void AddParameterSelected(FNiagaraVariable NewVariable, const bool bCreateCustomName = true, const NiagaraParameterMapSectionID::Type InSection = NiagaraParameterMapSectionID::NONE);
	
	TSharedPtr<SGraphActionMenu> GraphMenu;

	/** Delegate that gets fired when a parameter was added. */
	FOnAddParameter OnAddParameter;
	FOnCollectCustomActions OnCollectCustomActions;
	FOnAllowMakeType OnAllowMakeType;

	TArray<TWeakObjectPtr<UNiagaraGraph>> Graphs;

	TAttribute<NiagaraParameterMapSectionID::Type> Section;
	TAttribute<bool> AllowCreatingNew;
	TAttribute<bool> ShowNamespaceCategory;
	TAttribute<bool> ShowGraphParameters;
	TAttribute<bool> AutoExpandMenu;
	TAttribute<bool> IsParameterRead;
};
