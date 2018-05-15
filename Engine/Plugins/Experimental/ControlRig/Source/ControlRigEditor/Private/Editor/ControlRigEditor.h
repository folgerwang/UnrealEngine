// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IControlRigEditor.h"
#include "ControlRigEditorEditMode.h"
#include "AssetEditorModeManager.h"

class UControlRigBlueprint;
class IPersonaToolkit;
class SWidget;
class SBorder;
class USkeletalMesh;
class FStructOnScope;

/** Delegate fired when node selection has changed */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGraphNodeSelectionChanged, const TSet<UObject*>& /*Selection*/);

struct FControlRigEditorModes
{
	// Mode constants
	static const FName ControlRigEditorMode;
	static FText GetLocalizedMode(const FName InMode)
	{
		static TMap< FName, FText > LocModes;

		if (LocModes.Num() == 0)
		{
			LocModes.Add(ControlRigEditorMode, NSLOCTEXT("ControlRigEditorModes", "ControlRigEditorMode", "Rigging"));
		}

		check(InMode != NAME_None);
		const FText* OutDesc = LocModes.Find(InMode);
		check(OutDesc);
		return *OutDesc;
	}
private:
	FControlRigEditorModes() {}
};

class FControlRigEditor : public IControlRigEditor
{
public:
	/**
	 * Edits the specified character asset(s)
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	InControlRigBlueprint	The blueprint object to start editing.
	 */
	void InitControlRigEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UControlRigBlueprint* InControlRigBlueprint);

public:
	FControlRigEditor();
	virtual ~FControlRigEditor();

	// FBlueprintEditor interface
	virtual UBlueprint* GetBlueprintObj() const override;

public:
	// IToolkit Interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;	
	virtual FString GetDocumentationLink() const override
	{
		return FString(TEXT("Engine/Animation/ControlRig"));
	}
	
	// IToolkitHost Interface
	virtual void OnToolkitHostingStarted(const TSharedRef<class IToolkit>& Toolkit) override;
	virtual void OnToolkitHostingFinished(const TSharedRef<class IToolkit>& Toolkit) override;

	//  FTickableEditorObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// Gets the Control Rig Blueprint being edited/viewed
	UControlRigBlueprint* GetControlRigBlueprint() const;

	void SetDetailObjects(const TArray<UObject*>& InObjects);

	void SetDetailObject(UObject* Obj);

	void SetDetailStruct(TSharedPtr<FStructOnScope> StructToDisplay);

	void ClearDetailObject();

	FOnGraphNodeSelectionChanged& OnGraphNodeSelectionChanged() { return OnGraphNodeSelectionChangedDelegate; }

	/** Get the persona toolkit */
	TSharedRef<IPersonaToolkit> GetPersonaToolkit() const { return PersonaToolkit.ToSharedRef(); }

	/** Get the toolbox hosting widget */
	TSharedRef<SBorder> GetToolbox() { return Toolbox.ToSharedRef(); }

	/** Get the edit mode */
	FControlRigEditorEditMode& GetEditMode() { return *static_cast<FControlRigEditorEditMode*>(GetAssetEditorModeManager()->GetActiveMode(FControlRigEditorEditMode::ModeName)); }

	/** Try to set the selected nodes form some external source */
	void SetSelectedNodes(const TArray<FString>& InSelectedPropertyPaths);
	void SelectJoint(const FName& InJoint);
	// this changes everytime you compile, so don't cache it expecting it will last. 
	UControlRig* GetInstanceRig() const { return ControlRig;  }
protected:
	// FBlueprintEditor Interface
	virtual void CreateDefaultCommands() override;
	virtual void OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList);
	virtual void Compile() override;
	virtual bool IsInAScriptingMode() const override { return true; }
	virtual void CreateDefaultTabContents(const TArray<UBlueprint*>& InBlueprints) override;
	virtual FGraphAppearanceInfo GetGraphAppearance(class UEdGraph* InGraph) const override;
	virtual bool IsEditable(UEdGraph* InGraph) const override;
	virtual FText GetGraphDecorationString(UEdGraph* InGraph) const override;
	virtual void OnActiveTabChanged( TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated ) override;
	virtual void OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection) override;
	virtual void OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled) override;
	virtual void SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents) override;
	virtual void PostPasteNode(TSet<UEdGraphNode*>& PastedNodes) override;

	// FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	// FNotifyHook Interface
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged) override;

	// FGCObject Interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	void BindCommands();

protected:
	/** Undo Action**/
	void UndoAction();

	/** Redo Action **/
	void RedoAction();

private:
	/** Extend menu */
	void ExtendMenu();

	/** Extend toolbar */
	void ExtendToolbar();

	/** Handle hiding items in the graph */
	void HandleHideItem();
	bool CanHideItem() const;

	/** Handle preview scene setup */
	void HandlePreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene);
	void HandleViewportCreated(const TSharedRef<class IPersonaViewport>& InViewport);

	/** Handle switching skeletal meshes */
	void HandlePreviewMeshChanged(USkeletalMesh* InOldSkeletalMesh, USkeletalMesh* InNewSkeletalMesh);

	/** Push a newly compiled/opened control rig to the edit mode */
	void UpdateControlRig();

	/** Rebind our anim instance to the preview's skeletal mesh component */
	void RebindToSkeletalMeshComponent();

	/** Wraps the normal blueprint editor's action menu creation callback */
	FActionMenuContent HandleCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed);

protected:

	/** Toolbox hosting widget */
	TSharedPtr<SBorder> Toolbox;

	/** Persona toolkit used to support skeletal mesh preview */
	TSharedPtr<IPersonaToolkit> PersonaToolkit;

	/** The extender to pass to the level editor to extend it's window menu */
	TSharedPtr<FExtender> MenuExtender;

	/** Toolbar extender */
	TSharedPtr<FExtender> ToolbarExtender;

	/** Preview instance inspector widget */
	TSharedPtr<SWidget> PreviewEditor;

	FOnGraphNodeSelectionChanged OnGraphNodeSelectionChangedDelegate;

	/** Our currently running control rig instance */
	UControlRig* ControlRig;

	/** Recursion guard for selection */
	bool bSelecting;

	/** Joint Selection related */
	FTransform GetJointTransform(const FName& InJoint, bool bLocal) const;
	void SetJointTransform(const FName& InJoint, const FTransform& InTransform);

	/** delegate for changing property */
	void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);

	/** Selected Joint from hierarchy tree */
	FName SelectedJoint;

	friend class FControlRigEditorMode;
};