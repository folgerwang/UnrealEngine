// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.



#pragma once

#include "IToolkit.h"
#include "LightmapResRatioAdjust.h"
#include "Developer/AssetTools/Public/IAssetTypeActions.h"

/**
 * Unreal level editor actions
 */
class LEVELEDITOR_API FLevelEditorCommands : public TCommands<FLevelEditorCommands>
{

public:
	FLevelEditorCommands() : TCommands<FLevelEditorCommands>
	(
		"LevelEditor", // Context name for fast lookup
		NSLOCTEXT("Contexts", "LevelEditor", "Level Editor"), // Localized context name for displaying
		"MainFrame", // Parent
		FEditorStyle::GetStyleSetName() // Icon Style Set
	)
	{
	}
	

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() OVERRIDE;

public:
	
	TSharedPtr< FUICommandInfo > BrowseDocumentation;
	TSharedPtr< FUICommandInfo > BrowseAPIReference;
	TSharedPtr< FUICommandInfo > BrowseViewportControls;

	/** Level file commands */
	TSharedPtr< FUICommandInfo > NewLevel;
	TSharedPtr< FUICommandInfo > OpenLevel;
	TSharedPtr< FUICommandInfo > LegacyOpenLevel;
	TSharedPtr< FUICommandInfo > Save;
	TSharedPtr< FUICommandInfo > SaveAs;
	TSharedPtr< FUICommandInfo > SaveAllLevels;

	static const int32 MaxRecentFiles = 10;
	TArray< TSharedPtr< FUICommandInfo > > OpenRecentFileCommands;

	static const int32 MaxFavoriteFiles = 10;
	TArray< TSharedPtr< FUICommandInfo > > OpenFavoriteFileCommands;

	TSharedPtr< FUICommandInfo > ToggleFavorite;
	TArray< TSharedPtr< FUICommandInfo > > RemoveFavoriteCommands;

	/** Import */
	TSharedPtr< FUICommandInfo > Import;

	/** Export All */
	TSharedPtr< FUICommandInfo > ExportAll;

	/** Export Selected */
	TSharedPtr< FUICommandInfo > ExportSelected;

	
	/** Build commands */
	TSharedPtr< FUICommandInfo > Build;
	TSharedPtr< FUICommandInfo > BuildAndSubmitToSourceControl;
	TSharedPtr< FUICommandInfo > BuildLightingOnly;
	TSharedPtr< FUICommandInfo > BuildReflectionCapturesOnly;
	TSharedPtr< FUICommandInfo > BuildLightingOnly_VisibilityOnly;
	TSharedPtr< FUICommandInfo > LightingBuildOptions_UseErrorColoring;
	TSharedPtr< FUICommandInfo > LightingBuildOptions_ShowLightingStats;
	TSharedPtr< FUICommandInfo > BuildGeometryOnly;
	TSharedPtr< FUICommandInfo > BuildGeometryOnly_OnlyCurrentLevel;
	TSharedPtr< FUICommandInfo > BuildPathsOnly;
	TSharedPtr< FUICommandInfo > LightingQuality_Production;
	TSharedPtr< FUICommandInfo > LightingQuality_High;
	TSharedPtr< FUICommandInfo > LightingQuality_Medium;
	TSharedPtr< FUICommandInfo > LightingQuality_Preview;
	TSharedPtr< FUICommandInfo > LightingTools_ShowBounds;
	TSharedPtr< FUICommandInfo > LightingTools_ShowTraces;
	TSharedPtr< FUICommandInfo > LightingTools_ShowDirectOnly;
	TSharedPtr< FUICommandInfo > LightingTools_ShowIndirectOnly;
	TSharedPtr< FUICommandInfo > LightingTools_ShowIndirectSamples;
	TSharedPtr< FUICommandInfo > LightingDensity_RenderGrayscale;
	TSharedPtr< FUICommandInfo > LightingResolution_CurrentLevel;
	TSharedPtr< FUICommandInfo > LightingResolution_SelectedLevels;
	TSharedPtr< FUICommandInfo > LightingResolution_AllLoadedLevels;
	TSharedPtr< FUICommandInfo > LightingResolution_SelectedObjectsOnly;
	TSharedPtr< FUICommandInfo > LightingStaticMeshInfo;
	TSharedPtr< FUICommandInfo > SceneStats;
	TSharedPtr< FUICommandInfo > TextureStats;
	TSharedPtr< FUICommandInfo > MapCheck;

	/** Recompile */
	TSharedPtr< FUICommandInfo > RecompileLevelEditor;
	TSharedPtr< FUICommandInfo > ReloadLevelEditor;
	TSharedPtr< FUICommandInfo > RecompileGameCode;

	/**
	 * Level context menu commands.  These are shared between all viewports
	 * and rely on GCurrentLevelEditingViewport
	 * @todo Slate: Do these belong in their own context?
	 */

	/** Edits associated asset(s), prompting for confirmation if there is more than one selected */
	TSharedPtr< FUICommandInfo > EditAsset;

	/** Edits associated asset(s) */
	TSharedPtr< FUICommandInfo > EditAssetNoConfirmMultiple;

	/** Snaps the camera to the selected actors. */
	TSharedPtr< FUICommandInfo > SnapCameraToActor;

	/** Goes to the source code for the selected actor's class. */
	TSharedPtr< FUICommandInfo > GoToCodeForActor;

	/** Paste actor at click location*/
	TSharedPtr< FUICommandInfo > PasteHere;

	/**
	 * Actor Transform Commands                   
	 */

	/** Snaps the actor to the grid at its pivot*/
	TSharedPtr< FUICommandInfo > SnapOriginToGrid;

	/** Snaps each selected actor separately to the grid at its pivot*/
	TSharedPtr< FUICommandInfo > SnapOriginToGridPerActor;

	/** Aligns the actor to the grid at its pivot*/
	TSharedPtr< FUICommandInfo > AlignOriginToGrid;

	/** Snaps the actor to the floor*/
	TSharedPtr< FUICommandInfo > SnapToFloor;

	/** Aligns the actor with the floor */
	TSharedPtr< FUICommandInfo > AlignToFloor;

	/** Snaps the actor to the floor at its pivot*/
	TSharedPtr< FUICommandInfo > SnapPivotToFloor;

	/** Aligns the actor to the floor at its pivot */
	TSharedPtr< FUICommandInfo > AlignPivotToFloor;

	/** Snaps the actor to the floor at its bottom center bounds position*/
	TSharedPtr< FUICommandInfo > SnapBottomCenterBoundsToFloor;

	/** Aligns the actor to the floor at its bottom center bounds position*/
	TSharedPtr< FUICommandInfo > AlignBottomCenterBoundsToFloor;

	/** Snaps the actor to another actor at its pivot*/
	TSharedPtr< FUICommandInfo > SnapOriginToActor;

	/** Aligns the actor to another actor at its pivot*/
	TSharedPtr< FUICommandInfo > AlignOriginToActor;

	/** Snaps the actor to another actor */
	TSharedPtr< FUICommandInfo > SnapToActor;

	/** Aligns the actor with another actor */
	TSharedPtr< FUICommandInfo > AlignToActor;

	/** Snaps the actor to another actor at its pivot */
	TSharedPtr< FUICommandInfo > SnapPivotToActor;

	/** Aligns the actor to another actor at its pivot */
	TSharedPtr< FUICommandInfo > AlignPivotToActor;

	/** Snaps the actor to the Actor at its bottom center bounds position*/
	TSharedPtr< FUICommandInfo > SnapBottomCenterBoundsToActor;

	/** Aligns the actor to the Actor at its bottom center bounds position*/
	TSharedPtr< FUICommandInfo > AlignBottomCenterBoundsToActor;

	/** Apply delta transform to selected actors */	
	TSharedPtr< FUICommandInfo > DeltaTransformToActors;

	/** Mirros the actor along the x axis */	
	TSharedPtr< FUICommandInfo > MirrorActorX;
	 
	/** Mirros the actor along the y axis */	
	TSharedPtr< FUICommandInfo > MirrorActorY;

	/** Mirros the actor along the z axis */	
	TSharedPtr< FUICommandInfo > MirrorActorZ;

	/** Locks the actor so it cannot be moved */
	TSharedPtr< FUICommandInfo > LockActorMovement;

	/** Saves the pivot to the pre-pivot */
	TSharedPtr< FUICommandInfo > SavePivotToPrePivot;

	/** Resets the pre-pivot */
	TSharedPtr< FUICommandInfo > ResetPrePivot;

	/** Resets the pivot */
	TSharedPtr< FUICommandInfo > ResetPivot;

	/** Moves the pivot to the click location */
	TSharedPtr< FUICommandInfo > MovePivotHere;

	/** Moves the pivot to the click location and snap it to the grid */
	TSharedPtr< FUICommandInfo > MovePivotHereSnapped;

	/** Moves the pivot to the center of the selection */
	TSharedPtr< FUICommandInfo > MovePivotToCenter;

	/** Detach selected actor(s) from any parent */
	TSharedPtr< FUICommandInfo > DetachFromParent;

	TSharedPtr< FUICommandInfo > AttachSelectedActors;

	TSharedPtr< FUICommandInfo > AttachActorIteractive;

	TSharedPtr< FUICommandInfo > CreateNewOutlinerFolder;

	TSharedPtr< FUICommandInfo > HoldToEnableVertexSnapping;

	/**
	 * Brush Commands                   
	 */

	/** Put the selected brushes first in the draw order */
	TSharedPtr< FUICommandInfo > OrderFirst;

	/** Put the selected brushes last in the draw order */
	TSharedPtr< FUICommandInfo > OrderLast;

	/** Converts the brush to an additive brush */
	TSharedPtr< FUICommandInfo > ConvertToAdditive;

	/** Converts the brush to a subtractive brush */
	TSharedPtr< FUICommandInfo > ConvertToSubtractive;
	
	/** Make the brush solid */
	TSharedPtr< FUICommandInfo > MakeSolid;

	/** Make the brush semi-solid */
	TSharedPtr< FUICommandInfo > MakeSemiSolid;

	/** Make the brush non-solid */
	TSharedPtr< FUICommandInfo > MakeNonSolid;

	/** Merge bsp polys into as few faces as possible*/
	TSharedPtr< FUICommandInfo > MergePolys;

	/** Reverse a merge */
	TSharedPtr< FUICommandInfo > SeparatePolys;


	/**
	 * Actor group commands
	 */

	/** Group or regroup the selected actors depending on context*/
	TSharedPtr< FUICommandInfo > RegroupActors;
	/** Groups selected actors */
	TSharedPtr< FUICommandInfo > GroupActors;
	/** Ungroups selected actors */
	TSharedPtr< FUICommandInfo > UngroupActors;
	/** Adds the selected actors to the selected group */
	TSharedPtr< FUICommandInfo > AddActorsToGroup;
	/** Removes selected actors from the group */
	TSharedPtr< FUICommandInfo > RemoveActorsFromGroup;
	/** Locks the selected group */
	TSharedPtr< FUICommandInfo > LockGroup;
	/** Unlocks the selected group */
	TSharedPtr< FUICommandInfo > UnlockGroup;
	/** Opens a dialog window for creating mesh proxies */
	TSharedPtr< FUICommandInfo > MergeActors;
	/** Merge selected actors grouping them by materials */
	TSharedPtr< FUICommandInfo > MergeActorsByMaterials;
		
	/**
	 * Visibility commands                   
	 */
	/** Shows all actors */
	TSharedPtr< FUICommandInfo > ShowAll;

	/** Shows only selected actors */
	TSharedPtr< FUICommandInfo > ShowSelectedOnly;

	/** Unhides selected actors */
	TSharedPtr< FUICommandInfo > ShowSelected;

	/** Hides selected actors */
	TSharedPtr< FUICommandInfo > HideSelected;

	/** Shows all actors at startup */
	TSharedPtr< FUICommandInfo > ShowAllStartup;

	/** Shows selected actors at startup */
	TSharedPtr< FUICommandInfo > ShowSelectedStartup;

	/** Hides selected actors at startup */
	TSharedPtr< FUICommandInfo > HideSelectedStartup;

	/** Cycles through all navigation data to show one at a time */
	TSharedPtr< FUICommandInfo > CycleNavigationDataDrawn;

	/**
	 * Selection commands                    
	 */

	/** Select nothing */
	TSharedPtr< FUICommandInfo > SelectNone;

	/** Invert the current selection */
	TSharedPtr< FUICommandInfo > InvertSelection;

	/** Selects all actors of the same class as the current selection */
	TSharedPtr< FUICommandInfo > SelectAllActorsOfSameClass;

	/** Selects all actors of the same class and archetype as the current selection */
	TSharedPtr< FUICommandInfo > SelectAllActorsOfSameClassWithArchetype;

	/** Selects all lights relevant to the current selection */
	TSharedPtr< FUICommandInfo > SelectRelevantLights;

	/** Selects all actors using the same static mesh(es) as the current selection */
	TSharedPtr< FUICommandInfo > SelectStaticMeshesOfSameClass;

	/** Selects all actors using the same static mesh(es) and same actor class as the current selection */
	TSharedPtr< FUICommandInfo > SelectStaticMeshesAllClasses;

	/** Selects all actors using the same skeletal mesh(es) as the current selection */
	TSharedPtr< FUICommandInfo > SelectSkeletalMeshesOfSameClass;

	/** Selects all actors using the same skeletal mesh(es) and same actor class as the current selection */
	TSharedPtr< FUICommandInfo > SelectSkeletalMeshesAllClasses;

	/** Selects all actors using the same material(s) as the current selection */
	TSharedPtr< FUICommandInfo > SelectAllWithSameMaterial;

	/** Selects all actors used by currently selected matinee actor */
	TSharedPtr< FUICommandInfo > SelectAllActorsControlledByMatinee;

	/** Selects all emitters using the same particle system as the current selection */
	TSharedPtr< FUICommandInfo > SelectMatchingEmitter;

	/** Selects all lights */
	TSharedPtr< FUICommandInfo > SelectAllLights;

	/** Selects all lights exceeding the overlap limit */
	TSharedPtr< FUICommandInfo > SelectStationaryLightsExceedingOverlap;

	/** Selects all additive brushes */
	TSharedPtr< FUICommandInfo > SelectAllAddditiveBrushes;

	/** Selects all subtractive brushes */
	TSharedPtr< FUICommandInfo > SelectAllSubtractiveBrushes;

	/** Selects all semi-solid brushes */
	TSharedPtr< FUICommandInfo > SelectAllSemiSolidBrushes;

	/** Selects all non-solid brushes */
	TSharedPtr< FUICommandInfo > SelectAllNonSolidBrushes;

	/**
	 * Surface commands                   
	 */
	TSharedPtr< FUICommandInfo > SelectAllSurfaces;

	/** Select all surfaces in the same brush as the current surface selection */
	TSharedPtr< FUICommandInfo > SurfSelectAllMatchingBrush;

	/** Select all surfaces using the same material as current surface selection */
	TSharedPtr< FUICommandInfo > SurfSelectAllMatchingTexture;

	/** Select all surfaces adjacent to current surface selection */
	TSharedPtr< FUICommandInfo > SurfSelectAllAdjacents;

	/** Select all surfaces adjacent and coplanar to current surface selection */
	TSharedPtr< FUICommandInfo > SurfSelectAllAdjacentCoplanars;

	/** Select all surfaces adjacent to to current surface selection that are walls*/
	TSharedPtr< FUICommandInfo > SurfSelectAllAdjacentWalls;

	/** Select all surfaces adjacent to to current surface selection that are floors(normals pointing up)*/
	TSharedPtr< FUICommandInfo > SurfSelectAllAdjacentFloors;

	/** Select all surfaces adjacent to to current surface selection that are slants*/
	TSharedPtr< FUICommandInfo > SurfSelectAllAdjacentSlants;

	/** Invert current surface selection */
	TSharedPtr< FUICommandInfo > SurfSelectReverse;

	/** Memorize current surface selection */
	TSharedPtr< FUICommandInfo > SurfSelectMemorize;

	/** Recall previously memorized selection */
	TSharedPtr< FUICommandInfo > SurfSelectRecall;

	/** Replace the current selection with only the surfaces which are both currently selected and contained within the saved selection in memory */
	TSharedPtr< FUICommandInfo > SurfSelectOr;
	
	/**	Add the selection of surfaces saved in memory to the current selection */
	TSharedPtr< FUICommandInfo > SurfSelectAnd;

	/** Replace the current selection with only the surfaces that are not in both the current selection and the selection saved in memory */
	TSharedPtr< FUICommandInfo > SurfSelectXor;

	/** Unalign surface texture */
	TSharedPtr< FUICommandInfo > SurfUnalign;

	/** Auto align surface texture */
	TSharedPtr< FUICommandInfo > SurfAlignPlanarAuto;

	/** Align surface texture like its a wall */
	TSharedPtr< FUICommandInfo > SurfAlignPlanarWall;

	/** Align surface texture like its a floor */
	TSharedPtr< FUICommandInfo > SurfAlignPlanarFloor;

	/** Align surface texture using box */
	TSharedPtr< FUICommandInfo > SurfAlignBox;

	/** Best fit surface texture alignment */
	TSharedPtr< FUICommandInfo > SurfAlignFit;

	/** Apply the currently selected material to the currently selected surfaces */
	TSharedPtr< FUICommandInfo > ApplyMaterialToSurface;

	/**
	 * Static mesh commands                   
	 */

	/** Create a blocking volume from the meshes bounding box */
	TSharedPtr< FUICommandInfo > CreateBoundingBoxVolume;

	/** Create a blocking volume from the meshes using a heavy convex shape */
	TSharedPtr< FUICommandInfo > CreateHeavyConvexVolume;

	/** Create a blocking volume from the meshes using a normal convex shape */
	TSharedPtr< FUICommandInfo > CreateNormalConvexVolume;

	/** Create a blocking volume from the meshes using a light convex shape */
	TSharedPtr< FUICommandInfo > CreateLightConvexVolume;

	/** Create a blocking volume from the meshes using a rough convex shape */
	TSharedPtr< FUICommandInfo > CreateRoughConvexVolume;

	/** Set the collision model on the static meshes to be the same shape as the builder brush */
	TSharedPtr< FUICommandInfo > SaveBrushAsCollision;

	/** Set the actors collision to block all */
	TSharedPtr< FUICommandInfo > SetCollisionBlockAll;

	/** Set the actors collision to block only weapons */
	TSharedPtr< FUICommandInfo > SetCollisionBlockWeapons;

	/** Set the actors collision to block nothing */
	TSharedPtr< FUICommandInfo > SetCollisionBlockNone;

	/**
	 * Simulation commands
	 */

	/** Pushes properties of the selected actor back to its EditorWorld counterpart */
	TSharedPtr< FUICommandInfo > KeepSimulationChanges;


	/**
	 * Level commands
	 */

	/** Makes the actor level the current level */
	TSharedPtr< FUICommandInfo > MakeActorLevelCurrent;

	/** Move all the selected actors to the current level */
	TSharedPtr< FUICommandInfo > MoveSelectedToCurrentLevel;

	/** Finds the levels of the selected actors in the level browser */
	TSharedPtr< FUICommandInfo > FindLevelsInLevelBrowser;

	/** Add levels of the selected actors to the level browser selection */
	TSharedPtr< FUICommandInfo > AddLevelsToSelection;

	/** Remove levels of the selected actors from the level browser selection */
	TSharedPtr< FUICommandInfo > RemoveLevelsFromSelection;

	/**
	 * Level Script Commands
	 */
	TSharedPtr< FUICommandInfo > FindActorInLevelScript;

	/**
	 * Level Menu
	 */

	TSharedPtr< FUICommandInfo > WorldProperties;
	TSharedPtr< FUICommandInfo > OpenContentBrowser;
	TSharedPtr< FUICommandInfo > OpenMarketplace;
	TSharedPtr< FUICommandInfo > EditMatinee;

	/**
	 * Blueprints commands
	 */
	TSharedPtr< FUICommandInfo > OpenLevelBlueprint;
	TSharedPtr< FUICommandInfo > OpenGameModeBlueprint;
	TSharedPtr< FUICommandInfo > OpenGameStateBlueprint;
	TSharedPtr< FUICommandInfo > OpenDefaultPawnBlueprint;
	TSharedPtr< FUICommandInfo > OpenHUDBlueprint;
	TSharedPtr< FUICommandInfo > OpenPlayerControllerBlueprint;
	TSharedPtr< FUICommandInfo > CreateClassBlueprint;

	/** Editor mode commands */
	TArray< TSharedPtr< FUICommandInfo > > EditorModeCommands;

	/**
	 * View commands
	 */
	TSharedPtr< FUICommandInfo > ShowTransformWidget;
	TSharedPtr< FUICommandInfo > AllowTranslucentSelection;
	TSharedPtr< FUICommandInfo > AllowGroupSelection;

	TSharedPtr< FUICommandInfo > StrictBoxSelect;
	TSharedPtr< FUICommandInfo > DrawBrushMarkerPolys;
	TSharedPtr< FUICommandInfo > OnlyLoadVisibleInPIE;

	TSharedPtr< FUICommandInfo > ToggleSocketSnapping; 
	TSharedPtr< FUICommandInfo > ToggleParticleSystemLOD;
	TSharedPtr< FUICommandInfo > ToggleParticleSystemHelpers;
	TSharedPtr< FUICommandInfo > ToggleFreezeParticleSimulation;
	TSharedPtr< FUICommandInfo > ToggleLODViewLocking;
	TSharedPtr< FUICommandInfo > LevelStreamingVolumePrevis;

	TSharedPtr< FUICommandInfo > EnableActorSnap;
	TSharedPtr< FUICommandInfo > EnableVertexSnap;

	TSharedPtr< FUICommandInfo > ToggleHideViewportUI;

	TSharedPtr< FUICommandInfo > AddMatinee;

	TSharedPtr< FUICommandInfo > MaterialQualityLevel_Low;
	TSharedPtr< FUICommandInfo > MaterialQualityLevel_High;

	TSharedPtr< FUICommandInfo > FeatureLevelPreview[ERHIFeatureLevel::Num];
	
	///**
	// * Mode Commands                   
	// */
	//TSharedPtr< FUICommandInfo > BspMode;
	//TSharedPtr< FUICommandInfo > MeshPaintMode;
	//TSharedPtr< FUICommandInfo > LandscapeMode;
	//TSharedPtr< FUICommandInfo > FoliageMode;

	/**
	 * Misc Commands
	 */
	TSharedPtr< FUICommandInfo > ShowSelectedDetails;
	TSharedPtr< FUICommandInfo > RecompileShaders;
	TSharedPtr< FUICommandInfo > ProfileGPU;

	TSharedPtr< FUICommandInfo > ResetAllParticleSystems;
	TSharedPtr< FUICommandInfo > ResetSelectedParticleSystem;
	TSharedPtr< FUICommandInfo > SelectActorsInLayers;

	TSharedPtr< FUICommandInfo > FocusAllViewportsToSelection;
};

/**
 * Implementation of various level editor action callback functions
 */
class LEVELEDITOR_API FLevelEditorActionCallbacks
{
public:

	/**
	 * The default can execute action for all commands unless they override it
	 * By default commands cannot be executed if the application is in K2 debug mode.
	 */
	static bool DefaultCanExecuteAction();

	/** Opens the global documentation homepage */
	static void BrowseDocumentation();

	/** Opens the API documentation CHM */
	static void BrowseAPIReference();

	/** Opens the viewport controls page*/
	static void BrowseViewportControls();

	/** Creates a new level */
	static void NewLevel();
	static bool NewLevel_CanExecute();

	/** Opens an existing level */
	static void OpenLevel();
	static bool OpenLevel_CanExecute();
	static struct FAssetPickerConfig CreateLevelAssetPickerConfig();
	static void OpenLevelPickingDialog();
	static void OpenLevelFromAssetPicker(const TArray<class FAssetData>& SelectedAssets, EAssetTypeActivationMethod::Type ActivationType);
	

	/** Opens delta transform */
	static void DeltaTransform();

	/**
	 * Opens a recent file
	 *
	 * @param	RecentFileIndex		Index into our MRU list of recent files that can be opened
	 */
	static void OpenRecentFile( int32 RecentFileIndex );

	/**
	 * Opens a favorite file
	 *
	 * @param	FavoriteFileIndex		Index into our list of favorite files that can be opened
	 */
	static void OpenFavoriteFile( int32 FavoriteFileIndex );

	static void ToggleFavorite();

	/**
	 * Remove a favorite file from the favorites list
	 *
	 * @param	FavoriteFileIndex		Index into our list of favorite files to be removed
	 */
	static void RemoveFavorite( int32 FavoriteFileIndex );

	static bool ToggleFavorite_CanExecute();
	static bool ToggleFavorite_IsChecked();

	/** Save the current level as... */
	static void SaveAs();

	/** Saves the current map */
	static void Save();

	/** Saves all unsaved maps (but not packages) */
	static void SaveAllLevels();


	/**
	 * Called when import is selected
	 */
	static void Import_Clicked();


	/**
	 * Called when export all is selected
	 */
	static void ExportAll_Clicked();


	/**
	 * Called when export selected is clicked
	 */
	static void ExportSelected_Clicked();


	/**
	 * @return	True if the export selected option is available to execute
	 */
	static bool ExportSelected_CanExecute();


	static void ConfigureLightingBuildOptions( const FLightingBuildOptions& Options );

	/**
	 * Build callbacks
	 */
	static void Build_Execute();
	static void BuildAndSubmitToSourceControl_Execute();
	static void BuildLightingOnly_Execute();
	static bool BuildLighting_CanExecute();
	static void BuildReflectionCapturesOnly_Execute();
	static void BuildLightingOnly_VisibilityOnly_Execute();
	static bool LightingBuildOptions_UseErrorColoring_IsChecked();
	static void LightingBuildOptions_UseErrorColoring_Toggled();
	static bool LightingBuildOptions_ShowLightingStats_IsChecked();
	static void LightingBuildOptions_ShowLightingStats_Toggled();
	static void BuildGeometryOnly_Execute();
	static void BuildGeometryOnly_OnlyCurrentLevel_Execute();
	static void BuildPathsOnly_Execute();
	static void SetLightingQuality( ELightingBuildQuality NewQuality );
	static bool IsLightingQualityChecked( ELightingBuildQuality TestQuality );
	static void SetLightingToolShowBounds();
	static bool IsLightingToolShowBoundsChecked();
	static void SetLightingToolShowTraces();
	static bool IsLightingToolShowTracesChecked();
	static void SetLightingToolShowDirectOnly();
	static bool IsLightingToolShowDirectOnlyChecked();
	static void SetLightingToolShowIndirectOnly();
	static bool IsLightingToolShowIndirectOnlyChecked();
	static void SetLightingToolShowIndirectSamples();
	static bool IsLightingToolShowIndirectSamplesChecked();
	static float GetLightingDensityIdeal();
	static void SetLightingDensityIdeal( float Value );
	static float GetLightingDensityMaximum();
	static void SetLightingDensityMaximum( float Value );
	static float GetLightingDensityColorScale();
	static void SetLightingDensityColorScale( float Value );
	static float GetLightingDensityGrayscaleScale();
	static void SetLightingDensityGrayscaleScale( float Value );
	static void SetLightingDensityRenderGrayscale();
	static bool IsLightingDensityRenderGrayscaleChecked();
	static void SetLightingResolutionStaticMeshes( ESlateCheckBoxState::Type NewCheckedState );
	static ESlateCheckBoxState::Type IsLightingResolutionStaticMeshesChecked();
	static void SetLightingResolutionBSPSurfaces( ESlateCheckBoxState::Type NewCheckedState );
	static ESlateCheckBoxState::Type IsLightingResolutionBSPSurfacesChecked();
	static void SetLightingResolutionLevel( FLightmapResRatioAdjustSettings::AdjustLevels NewLevel );
	static bool IsLightingResolutionLevelChecked( FLightmapResRatioAdjustSettings::AdjustLevels TestLevel );
	static void SetLightingResolutionSelectedObjectsOnly();
	static bool IsLightingResolutionSelectedObjectsOnlyChecked();
	static float GetLightingResolutionMinSMs();
	static void SetLightingResolutionMinSMs( float Value );
	static float GetLightingResolutionMaxSMs();
	static void SetLightingResolutionMaxSMs( float Value );
	static float GetLightingResolutionMinBSPs();
	static void SetLightingResolutionMinBSPs( float Value );
	static float GetLightingResolutionMaxBSPs();
	static void SetLightingResolutionMaxBSPs( float Value );
	static int32 GetLightingResolutionRatio();
	static void SetLightingResolutionRatio( int32 Value );
	static void SetLightingResolutionRatioCommit( int32 Value, ETextCommit::Type CommitInfo);
	static void ShowLightingStaticMeshInfo();
	static void AttachToActor(AActor* ParentActorPtr );
	static void AttachToSocketSelection(FName SocketName, AActor* ParentActorPtr);
	static void SetMaterialQualityLevel( EMaterialQualityLevel::Type NewQualityLevel );
	static bool IsMaterialQualityLevelChecked( EMaterialQualityLevel::Type TestQualityLevel );
	static void SetFeatureLevelPreview(ERHIFeatureLevel::Type InFeatureLevel);
	static bool IsFeatureLevelPreviewChecked(ERHIFeatureLevel::Type InFeatureLevel);
	
	/**
	 * Called when the Scene Stats button is clicked.  Invokes the Primitive Stats dialog.
	 */
	static void ShowSceneStats();

	/**
	 * Called when the Texture Stats button is clicked.  Invokes the Texture Stats dialog.
	 */
	static void ShowTextureStats();

	/**
	 * Called when the Map Check button is clicked.  Invokes the Map Check dialog.
	 */
	static void MapCheck_Execute();

	/** @return True if actions that should only be visible when source code is thought to be available */
	static bool CanShowSourceCodeActions();

	/**
	 * Called when the recompile buttons are clicked.
	 */
	static void RecompileLevelEditor_Clicked();
	static void ReloadLevelEditor_Clicked();
	static void RecompileGameCode_Clicked();
	static bool Recompile_CanExecute();
	static bool Reload_CanExecute();

	/**
	 * Called when the FindInContentBrowser command is executed
	 */
	static void FindInContentBrowser_Clicked();

	/** Called to when "Edit Asset" is clicked */
	static void EditAsset_Clicked( const EToolkitMode::Type ToolkitMode, TWeakPtr< class SLevelEditor > LevelEditor, bool bAskMultiple );

	/** Called when 'detach' is clicked */
	static void DetachActor_Clicked();

	/** Called when attach selected actors is pressed */
	static void AttachSelectedActors();

	/** Called when the actor picker needs to be used to select a new parent actor */
	static void AttachActorIteractive();

	/** @return true if the selected actor can be attached to the given parent actor */
	static bool IsAttachableActor( const AActor* const ParentActor );

	/** Called when create new outliner folder is clicked */
	static void CreateNewOutlinerFolder_Clicked();

	/** Called when 'Go to Code for Actor' is clicked */
	static void GoToCodeForActor_Clicked();


	/**
	 * Called when the LockActorMovement command is executed
	 */
	static void LockActorMovement_Clicked();

		
	/**
	 * @return true if the lock actor menu option should appear checked
	 */
	static bool LockActorMovement_IsChecked();

	/**
	 * Called when the AddActor command is executed
	 *
	 * @param ActorFactory		The actor factory to use when adding the actor
	 * @param bUsePlacement		Whether to use the placement editor. If not, the actor will be placed at the last click.
	 * @param ActorLocation		[opt] If NULL, positions the actor at the mouse location, otherwise the location specified. Default is true.
	 */
	static void AddActor_Clicked( UActorFactory* ActorFactory, FAssetData AssetData, bool bUsePlacement );
	static AActor* AddActor( UActorFactory* ActorFactory, const FAssetData& AssetData, const FVector* ActorLocation );

	/**
	 * Called when the AddActor command is executed and a class is selected in the actor browser
	 *
	 * @param ActorClass		The class of the actor to add
	 * @param ActorLocation		[opt] If NULL, positions the actor at the mouse location, otherwise the location specified. Default is true.
	 */
	static void AddActorFromClass_Clicked( UClass* ActorClass );
	static AActor* AddActorFromClass( UClass* ActorClass, const FVector* ActorLocation );

	/**
	 * Replaces currently selected actors with an actor from the given actor factory
	 *
	 * @param ActorFactory	The actor factory to use in replacement
	 */
	static void ReplaceActors_Clicked( UActorFactory* ActorFactory, FAssetData AssetData );
	static AActor* ReplaceActors( UActorFactory* ActorFactory, const FAssetData& AssetData );

	/**
	 * Called when the ReplaceActor command is executed and a class is selected in the actor browser
	 *
	 * @param ActorClass	The class of the actor to replace
	 */
	static void ReplaceActorsFromClass_Clicked( UClass* ActorClass );

	/**
	 * Called to check to see if the Edit commands can be executed
	 *
	 * @return true, if the operation can be performed
	 */
	static bool Duplicate_CanExecute();
	static bool Delete_CanExecute();
	static void Rename_Execute();
	static bool Rename_CanExecute();
	static bool Cut_CanExecute();
	static bool Copy_CanExecute();
	static bool Paste_CanExecute();
	static bool PasteHere_CanExecute();

	/**
	 * Called when many of the menu items in the level editor context menu are clicked
	 *
	 * @param Command	The command to execute
	 */
	static void ExecuteExecCommand( FString Command );
	
	/**
	 * Called when selecting all actors of the same class that is selected
	 *
	 * @param bArchetype	true to also check that the archetype is the same
	 */
	static void OnSelectAllActorsOfClass( bool bArchetype );

	/**
	 * Called to select all lights
	 */
	static void OnSelectAllLights();

	/** Selects stationary lights that are exceeding the overlap limit. */
	static void OnSelectStationaryLightsExceedingOverlap();

	/**
	 * Selects the MatineeActor - used by Matinee Selection
	 */
	static void OnSelectMatineeActor( AMatineeActor * ActorToSelect );

	/**
	 * Selects the Matinee InterpGroup
	 */
	static void OnSelectMatineeGroup( AActor * Actor );

	/**
	 * Called when selecting all actors that's controlled by currently selected matinee actor
	 */
	static void OnSelectAllActorsControlledByMatinee();
	
	/**
	 * Called to change bsp surface alignment
	 *
	 * @param AlignmentMode	The new alignment mode
	 */
	static void OnSurfaceAlignment( ETexAlign AligmentMode );

	/**
	 * Called to apply a material to selected surfaces
	 */
	static void OnApplyMaterialToSurface();
	
	/**
	 * Called when the RegroupActor command is executed
	 */
	static void RegroupActor_Clicked();
		
	/**
	 * Called when the UngroupActor command is executed
	 */
	static void UngroupActor_Clicked();
		
	/**
	 * Called when the LockGroup command is executed
	 */
	static void LockGroup_Clicked();
	
	/**
	 * Called when the UnlockGroup command is executed
	 */
	static void UnlockGroup_Clicked();
	
	/**
	 * Called when the AddActorsToGroup command is executed
	 */
	static void AddActorsToGroup_Clicked();
	
	/**
	 * Called when the RemoveActorsFromGroup command is executed
	 */
	static void RemoveActorsFromGroup_Clicked();

	/**
	 * Called when the MergeActors command is executed
	 */
	static void MergeActors_Clicked();

	/** @return Returns true if 'Merge Actors' can be used right now */
	static bool CanExecuteMergeActors();

	/**
	 * Called when the MergeActorsByMaterials command is executed
	 */
	static void MergeActorsByMaterials_Clicked();

	/** @return Returns true if 'Merge Actors y Materials' can be used right now */
	static bool CanExecuteMergeActorsByMaterials();

	/**
	 * Called when the location grid snap is toggled off and on
	 */
	static void LocationGridSnap_Clicked();

	/**
	 * @return Returns whether or not location grid snap is enabled
	 */
	static bool LocationGridSnap_IsChecked();

	/**
	 * Called when the rotation grid snap is toggled off and on
	 */
	static void RotationGridSnap_Clicked();

	/**
	 * @return Returns whether or not rotation grid snap is enabled
	 */
	static bool RotationGridSnap_IsChecked();

	/**
	 * Called when the scale grid snap is toggled off and on
	 */
	static void ScaleGridSnap_Clicked();

	/**
	 * @return Returns whether or not scale grid snap is enabled
	 */
	static bool ScaleGridSnap_IsChecked();


	/** Called when "Keep Simulation Changes" is clicked in the viewport right click menu */
	static void OnKeepSimulationChanges();

	/** @return Returns true if 'Keep Simulation Changes' can be used right now */
	static bool CanExecuteKeepSimulationChanges();
		
		
	/**
	 * Makes the currently selected actors level the current level
	 * If multiple actors are selected they must all be in the same level
	 */
	static void OnMakeSelectedActorLevelCurrent();

	/**
	 * Moves the currently selected actors to the current level                   
	 */
	static void OnMoveSelectedToCurrentLevel();

	/**
	 * Selects the currently selected actor(s) levels in the level browser
	 * Deselecting everything else first
	 */
	static void OnFindLevelsInLevelBrowser();

	/**
	 * Selects the currently selected actor(s) levels in the level browser
	 */
	static void OnSelectLevelInLevelBrowser();

	/**
	 * Deselects the currently selected actor(s) levels in the level browser
	 */
	static void OnDeselectLevelInLevelBrowser();

	/**
	 * Finds references to the currently selected actor(s) in level scripts
	 */
	static void OnFindActorInLevelScript();

	/**
	* Take the currently selected static mesh, and save the builder brush as its low poly collision model.
	*/
	static void OnSaveBrushAsCollision();

	/** Select the world info actor and show the properties */
	static void OnShowWorldProperties( TWeakPtr< SLevelEditor > LevelEditor );

	/** Open the Content Browser */
	static void OpenContentBrowser();

	/** Open the Marketplace */
	static void OpenMarketplace();

	/** Open the level's blueprint in Kismet2 */
	static void OpenLevelBlueprint( TWeakPtr< SLevelEditor > LevelEditor );

	/** Open the world's game mode blueprint or help the user create one. */
	static void OpenGameModeBlueprint( TWeakPtr< SLevelEditor > LevelEditor );

	/** Open the current game state blueprint or help the user create one */
	static void OpenGameStateBlueprint( TWeakPtr< SLevelEditor > LevelEditor );
	
	/** Open the current default pawn blueprint or help the user create one */
	static void OpenDefaultPawnBlueprint( TWeakPtr< SLevelEditor > LevelEditor );
	
	/** Open the current HUD blueprint or help the user create one */
	static void OpenHUDBlueprint( TWeakPtr< SLevelEditor > LevelEditor );
	
	/** Open the current player controller blueprint or help the user create one */
	static void OpenPlayerControllerBlueprint( TWeakPtr< SLevelEditor > LevelEditor );

	/** Returns TRUE if the user can edit game info Blueprints, this requires an active Blueprint based game mode to be set */
	static bool CanEditGameInfoBlueprints( TWeakPtr< SLevelEditor > LevelEditor);

	/** Helps the user create a class Blueprint */
	static void CreateClassBlueprint();

	/** Shows only selected actors, hiding any unselected actors and unhiding any selected hidden actors. */
	static void OnShowOnlySelectedActors();

	/**
	 * View callbacks
	 */ 
	static void OnToggleTransformWidgetVisibility();
	static bool OnGetTransformWidgetVisibility();
	static void OnAllowTranslucentSelection();
	static bool OnIsAllowTranslucentSelectionEnabled();	
	static void OnAllowGroupSelection();
	static bool OnIsAllowGroupSelectionEnabled(); 
	static void OnToggleStrictBoxSelect();
	static bool OnIsStrictBoxSelectEnabled(); 
	static void OnDrawBrushMarkerPolys();
	static bool OnIsDrawBrushMarkerPolysEnabled();
	static void OnToggleOnlyLoadVisibleInPIE();
	static bool OnIsOnlyLoadVisibleInPIEEnabled(); 
	static void OnToggleSocketSnapping();
	static bool OnIsSocketSnappingEnabled(); 
	static void OnToggleParticleSystemLOD();
	static bool OnIsParticleSystemLODEnabled(); 
	static void OnToggleFreezeParticleSimulation();
	static bool OnIsParticleSimulationFrozen();
	static void OnToggleParticleSystemHelpers();
	static bool OnIsParticleSystemHelpersEnabled();
	static void OnToggleLODViewLocking();
	static bool OnIsLODViewLockingEnabled(); 
	static void OnToggleLevelStreamingVolumePrevis();
	static bool OnIsLevelStreamingVolumePrevisEnabled(); 
	
	static FString GetAudioVolumeToolTip();
	static float GetAudioVolume();
	static void OnAudioVolumeChanged(float Volume);
	static bool GetAudioMuted();
	static void OnAudioMutedChanged(bool bMuted); 

	static void OnEnableActorSnap();
	static bool OnIsActorSnapEnabled();
	static FString GetActorSnapTooltip();
	static float GetActorSnapSetting();
	static void SetActorSnapSetting(float Distance);
	static void OnEnableVertexSnap();
	static bool OnIsVertexSnapEnabled();

	static void OnToggleHideViewportUI();
	static bool IsViewportUIHidden();

	static bool IsEditorModeActive( FEditorModeID EditorMode );

	static void MakeBuilderBrush( UClass* BrushBuilderClass );

	static void OnAddVolume( UClass* VolumeClass );

	static void OnAddMatinee();

	static void SelectActorsInLayers();

	static void SetWidgetMode( FWidget::EWidgetMode WidgetMode );
	static bool IsWidgetModeActive( FWidget::EWidgetMode WidgetMode );
	static bool CanSetWidgetMode( FWidget::EWidgetMode WidgetMode );
	static bool IsTranslateRotateModeVisible();
	static void SetCoordinateSystem( ECoordSystem CoordSystem );
	static bool IsCoordinateSystemActive( ECoordSystem CoordSystem );

	/**
	 * Return a world
	 */
	static class UWorld* GetWorld();
public:
	/** 
	 * Moves an actor to the grid.
	 */
	static void MoveActorToGrid_Clicked( bool InAlign, bool bInPerActor );

	/** 
	 * Moves an actor to another actor.
	 */
	static void MoveActorToActor_Clicked( bool InAlign );

	/** 
	 * Snaps an actor to the floor.  Optionally will align with the trace normal.
	 * @param InAlign			Whether or not to rotate the actor to align with the trace normal.
	 * @param InUseLineTrace	Whether or not to only trace with a line through the world.
	 * @param InUseBounds		Whether or not to base the line trace off of the bounds.
	 * @param InUsePivot		Whether or not to use the pivot position.
	 */
	static void SnapActorToFloor_Clicked( bool InAlign, bool InUseLineTrace, bool InUseBounds, bool InUsePivot );

	/**
	 * Snaps an actor to another actor.  Optionally will align with the trace normal.
	 * @param InAlign			Whether or not to rotate the actor to align with the trace normal.
	 * @param InUseLineTrace	Whether or not to only trace with a line through the world.
	 * @param InUseBounds		Whether or not to base the line trace off of the bounds.
	 * @param InUsePivot		Whether or not to use the pivot position.
	 */
	static void SnapActorToActor_Clicked( bool InAlign, bool InUseLineTrace, bool InUseBounds, bool InUsePivot );

	/**
	 * Checks to see if multiple actors are selected
	 *	@return true if it can execute.
	 */
	static bool ActorsSelected_CanExecute();

	/**
	 * Checks to see if at least a single actor is selected
	 *	@return true if it can execute.
	 */
	static bool ActorSelected_CanExecute();

private:
	/** 
	 * Moves an actor...
	 * @param InDestination		The destination actor we want to move this actor to, NULL assumes we just want to use the grid
	 */
	static void MoveActorTo_Clicked( const bool InAlign, const AActor* InDestination = NULL, bool bInPerActor = false );

	/** 
	 * Snaps an actor...  Optionally will align with the trace normal.
	 * @param InAlign			Whether or not to rotate the actor to align with the trace normal.
	 * @param InUseLineTrace	Whether or not to only trace with a line through the world.
	 * @param InUseBounds		Whether or not to base the line trace off of the bounds.
	 * @param InUsePivot		Whether or not to use the pivot position.
	 * @param InDestination		The destination actor we want to move this actor to, NULL assumes we just want to go towards the floor
	 */
	static void SnapActorTo_Clicked( const bool InAlign, const bool InUseLineTrace, const bool InUseBounds, const bool InUsePivot, const AActor* InDestination = NULL );
};

