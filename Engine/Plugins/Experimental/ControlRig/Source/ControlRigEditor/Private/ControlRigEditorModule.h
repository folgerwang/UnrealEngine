// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "ControlRigBlueprintCompiler.h"
#include "UObject/WeakObjectPtr.h"
#include "IControlRigEditorModule.h"
#include "IControlRigModule.h"

class UBlueprint;
class IAssetTypeActions;
class UControlRigSequence;
class ISequencer;
class UMaterial;
class UAnimSequence;
class USkeletalMesh;
class FToolBarBuilder;
class FExtender;
class FUICommandList;
class UMovieSceneTrack;
class FControlRigGraphPanelNodeFactory;
class FControlRigGraphPanelPinFactory;

class FControlRigEditorModule : public IControlRigEditorModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** IControlRigEditorModule interface */
	virtual TSharedRef<IControlRigEditor> CreateControlRigEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UControlRigBlueprint* Blueprint) override;
	DECLARE_DELEGATE_RetVal_TwoParams(TSharedRef<FExtender>, FControlRigEditorToolbarExtender, const TSharedRef<FUICommandList> /*InCommandList*/, TSharedRef<IControlRigEditor> /*InControlRigEditor*/);
	virtual TArray<FControlRigEditorToolbarExtender>& GetAllControlRigEditorToolbarExtenders() override { return ControlRigEditorToolbarExtenders; }

	/** IHasMenuExtensibility interface */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }

	/** IHasToolBarExtensibility interface */
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

	UMaterial* GetTrajectoryMaterial() const { return TrajectoryMaterial.Get(); }

	virtual void RegisterRigUnitEditorClass(FName RigUnitClassName, TSubclassOf<URigUnitEditor_Base> Class);
	virtual void UnregisterRigUnitEditorClass(FName RigUnitClassName);
	static TSubclassOf<URigUnitEditor_Base> GetEditorObjectByRigUnit(const FName& RigUnitClassName);

private:
	/** Handle a new animation controller blueprint being created */
	void HandleNewBlueprintCreated(UBlueprint* InBlueprint);

	/** Handle a new sequencer instance being created */
	void HandleSequencerCreated(TSharedRef<ISequencer> InSequencer);

	/** Handle an asset being opened */
	void HandleAssetEditorOpened(UObject* InAsset);

	/** Called to setup a new sequence's defaults */
	static void OnInitializeSequence(UControlRigSequence* Sequence);

	/** Whether we can export the current control rig sequence as an anim sequence */
	bool CanExportAnimSequenceFromSequencer() const;

	/** Export the current control rig sequence as an anim sequence */
	void ExportAnimSequenceFromSequencer();

	/** Export control rig sequence(s) to anim sequence(s) */
	void ExportToAnimSequence(TArray<FAssetData> InAssetData);

	/** Re-export control rig sequence(s) to anim sequence(s) using the previous export settings */
	void ReExportToAnimSequence(TArray<FAssetData> InAssetData);

	/** Import animation sequence(s) from a source rig sequence */
	void ImportFromRigSequence(TArray<FAssetData> InAssetData);

	/** Re-import animation sequence(s) from their source rig sequence(s) */
	void ReImportFromRigSequence(TArray<FAssetData> InAssetData);

	/** Bind our module-level commands */
	void BindCommands();

	/** Whether the track is visible in the sequencer node tree */
	bool IsTrackVisible(const UMovieSceneTrack* InTrack);

	/** Compiler customization for animation controllers */
	FControlRigBlueprintCompiler ControlRigBlueprintCompiler;

	/** Handle for our sequencer track editor */
	FDelegateHandle ControlRigTrackCreateEditorHandle;

	/** Handle for our sequencer binding track editor */
	FDelegateHandle ControlRigBindingTrackCreateEditorHandle;

	/** Handle for our sequencer object binding */
	FDelegateHandle ControlRigEditorObjectBindingHandle;

	/** Handle for our level sequence spawner */
	FDelegateHandle LevelSequenceSpawnerDelegateHandle;

	/** Handle for tracking ISequencer creation */
	FDelegateHandle SequencerCreatedHandle;

	/** Handle for tracking asset editors opening */
	FDelegateHandle AssetEditorOpenedHandle;

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;

	/** Material used for trajectories */
	TWeakObjectPtr<UMaterial> TrajectoryMaterial;

	/** Toolbar extender for Sequencer */
	TSharedPtr<FExtender> SequencerToolbarExtender;

	/** Command bindings for keyboard shortcuts */
	TSharedPtr<FUICommandList> CommandBindings;

	/** Weak pointer to the last sequencer that was opened */
	TWeakPtr<ISequencer> WeakSequencer;

	/** Delegate handle used to extend the content browser asset menu */
	FDelegateHandle ContentBrowserMenuExtenderHandle;

	/** StaticClass is not safe on shutdown, so we cache the name, and use this to unregister on shut down */
	TArray<FName> ClassesToUnregisterOnShutdown;
	TArray<FName> PropertiesToUnregisterOnShutdown;

	/** Extensibility managers */
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
	TArray<FControlRigEditorToolbarExtender> ControlRigEditorToolbarExtenders;

	/** Node factory for the control rig graph */
	TSharedPtr<FControlRigGraphPanelNodeFactory> ControlRigGraphPanelNodeFactory;

	/** Pin factory for the control rig graph */
	TSharedPtr<FControlRigGraphPanelPinFactory> ControlRigGraphPanelPinFactory;

	/** Delegate handles for blueprint utils */
	FDelegateHandle RefreshAllNodesDelegateHandle;
	FDelegateHandle ReconstructAllNodesDelegateHandle;
	FDelegateHandle RenameVariableReferencesDelegateHandle;

	/** Delegate handler for BP Compiler Getter */
	static TSharedPtr<FKismetCompilerContext> GetControlRigCompiler(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions);

	/** Rig Unit Editor Classes Handler */
	static TMap<FName, TSubclassOf<URigUnitEditor_Base>> RigUnitEditorClasses;
};