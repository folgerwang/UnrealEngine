// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditableMeshTypes.h"
#include "EditableMesh.h"
#include "EdMode.h"
#include "MeshElement.h"
#include "ArrayView.h"
#include "LevelEditorViewport.h"	// For FTrackingTransaction
#include "UnrealEdMisc.h"
#include "IMeshEditorMode.h"
#include "MeshEditorMode.generated.h"


UCLASS()
class UMeshEditorModeProxyObject : public UObject
{
	GENERATED_BODY()

public:

	/** The mesh editor that owns us */
	class FMeshEditorMode* OwningMeshEditorMode;
};


/**
 * Mesh Editor Mode.  Extends editor viewports with the ability to edit meshes
 */
class FMeshEditorMode : public FEdMode, public IMeshEditorMode
{
public:

	/**
	 * The types of interactor shapes we support
	 */
	enum class EInteractorShape
	{
		/** Invalid shape (or none) */
		Invalid,

		/** Grabber sphere */
		GrabberSphere,

		/** Laser pointer shape */
		Laser,
	};

	/**
	 * Contains state for either a mouse cursor or a virtual hand (in VR), to be used to interact with a mesh
	 */
	struct FMeshEditorInteractorData
	{
		/** The viewport interactor that is this data's counterpart */
		TWeakObjectPtr<class UViewportInteractor> ViewportInteractor;

		/** True if we have a valid interaction grabber sphere right now */
		bool bGrabberSphereIsValid;

		/** The sphere for radial interactions */
		FSphere GrabberSphere;

		/** True if we have a valid interaction ray right now */
		bool bLaserIsValid;

		/** World space start location of the interaction ray the last time we were ticked */
		FVector LaserStart;

		/** World space end location of the interaction ray */
		FVector LaserEnd;

		/** What shape of interactor are we using to hover? */
		EInteractorShape HoverInteractorShape;

		/** Information about a mesh we're hovering over or editing */
		FMeshElement HoveredMeshElement;

		/** The element we were hovering over last frame */
		FMeshElement PreviouslyHoveredMeshElement;

		/** The hover point.  With a ray, this could be the impact point along the ray.  With grabber sphere interaction, this 
		    would be the point within the sphere radius where we've found a point on an object to interact with */
		FVector HoverLocation;


		/** Default constructor that initializes everything to safe values */
		FMeshEditorInteractorData();
	};


public:

	/** Default constructor for FMeshEditorMode */
	FMeshEditorMode();

	/** Cleans up this mode, called when the editor is shutting down */
	virtual ~FMeshEditorMode();

	// FEdMode interface
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void Tick( FEditorViewportClient* ViewportClient, float DeltaTime ) override;
	virtual bool InputKey( FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event ) override;
	virtual bool InputAxis( FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime ) override;
	virtual bool InputDelta( FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& Drag, FRotator& Rotation, FVector& Scale ) override;
	virtual bool IsCompatibleWith( FEditorModeID OtherModeID ) const override;
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual void Render( const FSceneView* SceneView, FViewport* Viewport, FPrimitiveDrawInterface* PDI ) override;	
	virtual void PostUndo() override;
	virtual bool FrustumSelect( const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect = true ) override;
	virtual bool ShouldDrawWidget() const override;

	// IMeshEditorMode Interface
	virtual EEditableMeshElementType GetMeshElementSelectionMode() const override{ return MeshElementSelectionMode; }
	virtual void SetMeshElementSelectionMode( EEditableMeshElementType ElementType ) override;
	virtual void RegisterWithExternalMenuSystem() override;
	virtual void UnregisterWithExternalMenuSystem() override;
	// End IMeshEditorMode Interface


	/** Returns the type of elements that are selected right now, or Invalid if nothing is selected */
	EEditableMeshElementType GetSelectedMeshElementType() const;

	/** Returns whether the specified element type is selected */
	bool IsMeshElementTypeSelected( EEditableMeshElementType ElementType ) const { return GetSelectedMeshElementType() == ElementType; }

	/** Returns whether either the specified element type is selected, or we're in the selection mode for that element type */
	bool IsMeshElementTypeSelectedOrIsActiveSelectionMode( EEditableMeshElementType ElementType ) const { return GetMeshElementSelectionMode() == ElementType || GetSelectedMeshElementType() == ElementType; }

	const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetVertexActions() const { return VertexActions; }
	const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetEdgeActions() const { return EdgeActions; }
	const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetPolygonActions() const { return PolygonActions; }

	/** Checks to see that the mesh element actually exists in the mesh */
	inline static bool IsElementIDValid( const FMeshElement& MeshElement, const UEditableMesh* EditableMesh )
	{
		bool bIsValid = false;

		if( MeshElement.ElementAddress.ElementID != FElementID::Invalid )
		{
			switch( MeshElement.ElementAddress.ElementType )
			{
				case EEditableMeshElementType::Vertex:
					bIsValid = EditableMesh->IsValidVertex( FVertexID( MeshElement.ElementAddress.ElementID ) );
					break;

				case EEditableMeshElementType::Edge:
					bIsValid = EditableMesh->IsValidEdge( FEdgeID( MeshElement.ElementAddress.ElementID ) );
					break;

				case EEditableMeshElementType::Polygon:
					bIsValid = EditableMesh->IsValidPolygon( FPolygonRef( MeshElement.ElementAddress.SectionID, FPolygonID( MeshElement.ElementAddress.ElementID ) ) );
					break;
			}
		}

		return bIsValid;
	}

	bool IsEditingPerInstance() const { return bPerInstanceEdits; }
	void SetEditingPerInstance( bool bPerInstance ) { bPerInstanceEdits = bPerInstance; }

	/** Propagates instance changes to the static mesh asset */
	void PropagateInstanceChanges();

	/** Whether there are instance changes which can be propagated */
	bool CanPropagateInstanceChanges() const;

	/** Gets the container of all the assets used in the mesh editor */
	const class UMeshEditorAssetContainer& GetAssetContainer() const;

protected:

	/** Gets an editable mesh from our cache of editable meshes for the specified sub-mesh address, or tries to create and cache a new 
	    editable mesh if we haven't seen this sub-mesh address before.  Can return nullptr if no mesh was possible for that address. */
	UEditableMesh* FindOrCreateEditableMesh( class UPrimitiveComponent& Component, const FEditableMeshSubMeshAddress& SubMeshAddress );

	/** Fills the specified dynamic mesh builder with primitives to render a mesh vertex */
	void AddVertexToDynamicMesh( const UEditableMesh& EditableMesh, const FTransform& CameraToWorld, const FMatrix& ComponentToWorldMatrix, const FVertexID VertexID, const FColor ColorAndOpacity, const float SizeBias, const bool bApplyDepthBias, class FDynamicMeshBuilder& MeshBuilder );
		
	/** Fills the specified dynamic mesh builder with primitives to a thick line.  Incoming positions are in world space. */
	void AddThickLineToDynamicMesh( const FTransform& CameraToWorld, const FVector EdgeVertexPositions[2], const FColor ColorAndOpacity, const float SizeBias, const bool bApplyDepthBias, FDynamicMeshBuilder& MeshBuilder );

	/** Fills the specified dynamic mesh builder with primitives to render a mesh edge */
	void AddEdgeToDynamicMesh( const UEditableMesh& EditableMesh, const FTransform& CameraToWorld, const FMatrix& ComponentToWorldMatrix, const FEdgeID EdgeID, const FColor ColorAndOpacity, const float SizeBias, class FDynamicMeshBuilder& MeshBuilder );

	/** Fills the specified dynamic mesh builders with primitives to render a polygon and it's edges */
	void AddPolygonToDynamicMesh( const UEditableMesh& EditableMesh, const FTransform& CameraToWorld, const FMatrix& ComponentToWorldMatrix, const FPolygonRef PolygonRef, const FColor ColorAndOpacity, const float SizeBias, const bool bFillFaces, class FDynamicMeshBuilder& VertexAndEdgeMeshBuilder, class FDynamicMeshBuilder* PolygonFaceMeshBuilder );

	/** Renders the spceified mesh element */
	void DrawMeshElements( const FTransform& CameraToWorld, FViewport* Viewport, FPrimitiveDrawInterface* PDI, const TArrayView<FMeshElement>& MeshElements, const FColor Color, const bool bFillFaces, const float HoverAnimation, const TArray<FColor>* OptionalPerElementColors = nullptr, const TArray<float>* OptionalPerElementSizeBiases = nullptr );

	/** Called every frame for each viewport interactor to update what's under the cursor */
	void OnViewportInteractionHoverUpdate( class UViewportInteractor* ViewportInteractor, FVector& OutHoverImpactPoint, bool& bWasHandled );
		
	/** Called when the user presses a button on their mouse or motion controller device */
	void OnViewportInteractionInputAction( class FEditorViewportClient& ViewportClient, class UViewportInteractor* ViewportInteractor, const struct FViewportActionKeyInput& Action, bool& bOutIsInputCaptured, bool& bWasHandled );

	/** Called when the user clicks on the background */
	void OnViewportInteractionInputUnhandled( class FEditorViewportClient& ViewportClient, class UViewportInteractor* ViewportInteractor, const struct FViewportActionKeyInput& Action );

	/** Called when the viewport interaction system starts dragging transformable objects around */
	void OnViewportInteractionStartDragging( class UViewportInteractor* ViewportInteractor );

	/** Called when the viewport interaction system stops dragging transformable objects around */
	void OnViewportInteractionStopDragging( class UViewportInteractor* ViewportInteractor );

	/** Called when the viewport interaction finishes moving a set of transformable objects */
	void OnViewportInteractionFinishedMovingTransformables();
	
	/** Called when VR editor world interaction drags a material onto a component */
	void OnVREditorModePlaceDraggedMaterial( class UPrimitiveComponent* HitComponent, class UMaterialInterface* MaterialInterface, bool& bPlaced );

	/** Applies a modification to the mesh that's currently hovered */
	void UpdateActiveAction( const bool bIsActionFinishing );

	/** Geometry tests */
	FEditableMeshElementAddress QueryElement( const UEditableMesh& EditableMesh, const bool bUseSphere, const FSphere& Sphere, const float SphereFuzzyDistance, bool bUseRay, const FVector& RayStart, const FVector& RayEnd, const float RayFuzzyDistance, const EEditableMeshElementType OnlyElementType, const FVector& CameraLocation, const float FuzzyDistanceScaleFactor, EInteractorShape& OutInteractorShape, FVector& OutHitLocation ) const;
	static bool CheckVertex( const bool bUseSphere, const FSphere& Sphere, const float SphereFuzzyDistance, bool bUseRay, const FVector& RayStart, const FVector& RayEnd, const float FuzzyDistance, const FVector& VertexPosition, const FVector& CameraLocation, const float FuzzyDistanceScaleFactor, EInteractorShape& ClosestInteractorShape, float& ClosestDistanceToRay, float& ClosestDistanceOnRay, FVector& ClosestHitLocation, const bool bAlreadyHitVertex );
	static bool CheckEdge( const bool bUseSphere, const FSphere& Sphere, const float SphereFuzzyDistance, bool bUseRay, const FVector& RayStart, const FVector& RayEnd, const float FuzzyDistance, const FVector EdgeVertexPositions[ 2 ], const FVector& CameraLocation, const float FuzzyDistanceScaleFactor, EInteractorShape& ClosestInteractorShape, float& ClosestDistanceToRay, float& ClosestDistanceOnRay, FVector& ClosestHitLocation, const bool bAlreadyEdge );
	static bool CheckTriangle( const bool bUseSphere, const FSphere& Sphere, const float SphereFuzzyDistance, bool bUseRay, const FVector& RayStart, const FVector& RayEnd, const float FuzzyDistance, const FVector TriangleVertexPositions[ 3 ], const FVector& CameraLocation, const float FuzzyDistanceScaleFactor, EInteractorShape& ClosestInteractorShape, float& ClosestDistanceToRay, float& ClosestDistanceOnRay, FVector& ClosestHitLocation, const bool bAlreadyHitTriangle );

	/** Returns the index of an element in the selection set, or INDEX_NONE if its not selected */
	int32 GetSelectedMeshElementIndex( const FMeshElement& MeshElement ) const;

	/** Clears hover and selection on mesh elements that may no longer be valid.  You'll want to call this if you change the mesh topology */
	TUniquePtr<FChange> ClearInvalidSelectedElements_Internal();

	/** Clears hover on mesh elements that may no longer be valid.  You'll want to call this if you change the mesh topology */
	void ClearInvalidHoveredElements();

	/** Updates the current view location, from either the viewport client or the VR interface, whichever is in use */
	void UpdateCameraToWorldTransform( const FEditorViewportClient& ViewportClient );

	enum class EMeshEditAction
	{
		/** Nothing going on right now */
		None,

		/** Selecting mesh elements by 'painting' over multiple elements */
		SelectByPainting,

		/** Moving elements using a transform gizmo */
		MoveUsingGizmo,

		/** Moving selected mesh elements (vertices, edges or polygons) */
		Move,

		/** Split an edge by inserting a vertex.  You can drag to preview where the vertex will be inserted. */
		SplitEdge,

		/** Splits an edge by inserting a new vertex, then immediately starts dragging that vertex */
		SplitEdgeAndDragVertex,

		/** Insert an edge loop */
		InsertEdgeLoop,

		/** Extrude polygon by making a copy of it and allowing you to shift it along the polygon normal axis */
		ExtrudePolygon,

		/** Extrude polygon by making a copy of it and allowing you to move it around freely */
		FreelyExtrudePolygon,

		/** Inset polygon by replacing it with a new polygon that is bordered by polygons of a specific relative size */
		InsetPolygon,

		/** Bevel polygons by adding angled bordering polygons of a specific relative size */
		BevelPolygon,

		/** Extend an edge by making a copy of it and allowing you to move it around */
		ExtendEdge,

		/** Extend a vertex by making a copy of it, creating new polygons to join the geometry together */
		ExtendVertex,

		/** For subdivision meshes, edits how sharp a vertex corner is by dragging in space */
		EditVertexCornerSharpness,

		/** For subdivision meshes, edits how sharp an edge crease is by dragging in space */
		EditEdgeCreaseSharpness,

		/** Freehand vertex drawing */
		DrawVertices,
	};

	/** Begins an action */
	void StartAction( const EMeshEditAction NewAction, class UViewportInteractor* ActionInteractor, const bool bActionNeedsHoverLocation, const FText& UndoText );

	/** Ends an action that's currently in progress.  Usually called when the user commits a change by clicking/releasing, but can
	    also be called when the user begins a new action while inertia is still influencing the active action */
	void FinishAction();

	/** @return Returns true if the undo system is available right now.  When in Simulate Mode, we can't store undo states or use undo/redo features */
	static bool IsUndoSystemAvailable();

	/** Saves undo state, if possible (e.g., not in Simulate mode.) */
	static void StoreUndo( UObject* Object, TUniquePtr<FChange> UndoChange );

	/** Binds UI commands to actions for the mesh editor */
	void BindCommands();

	void RegisterCommonEditingMode( const TSharedPtr<FUICommandInfo>& Command, EMeshEditAction EditingMode );
	void RegisterVertexEditingMode( const TSharedPtr<FUICommandInfo>& Command, EMeshEditAction EditingMode );
	void RegisterEdgeEditingMode( const TSharedPtr<FUICommandInfo>& Command, EMeshEditAction EditingMode );
	void RegisterPolygonEditingMode( const TSharedPtr<FUICommandInfo>& Command, EMeshEditAction EditingMode );

	void RegisterCommand( const TSharedPtr<FUICommandInfo>& Command, const FExecuteAction& ExecuteAction );
	void RegisterVertexCommand( const TSharedPtr<FUICommandInfo>& Command, const FExecuteAction& ExecuteAction );
	void RegisterEdgeCommand( const TSharedPtr<FUICommandInfo>& Command, const FExecuteAction& ExecuteAction );
	void RegisterPolygonCommand( const TSharedPtr<FUICommandInfo>& Command, const FExecuteAction& ExecuteAction );

	/** Return the CommandList pertinent to the currently selected element type, or nullptr if nothing is selected */
	const FUICommandList* GetCommandListForSelectedElementType() const;

	/** Commits the mesh instance for the given component */
	void CommitEditableMeshIfNecessary( UEditableMesh* EditableMesh, UPrimitiveComponent* Component );

	/** Commits all selected meshes */
	void CommitSelectedMeshes();

	/** Deletes selected polygons, or polygons partly defined by selected elements; returns whether successful */
	bool DeleteSelectedMeshElement();

	/** Tessellates selected polygons into smaller polygons; returns whether it did anything */
	bool TessellateSelectedPolygons();

	/** Adds or removes a subdivision level for selected meshes */
	void AddOrRemoveSubdivisionLevel( const bool bShouldAdd );

	/** Quadrangulates the currently selected mesh */
	void QuadrangulateMesh();

	/** Moves the viewport camera to frame the currently selected elements */
	void FrameSelectedElements( FEditorViewportClient* ViewportClient );

	/** Removes the selected edge if possible; returns whether successful */
	bool RemoveSelectedEdges();

	/** Selects the edge loops which contain the selected edges */
	bool SelectEdgeLoops();

	/** Removes the selected vertex if possible; returns whether successful */
	bool RemoveSelectedVertices();

	/** Welds the selected vertices if possible, keeping the first selected vertex */
	bool WeldSelectedVertices();

	/** Flips selected polygons; returns whether successful */
	bool FlipSelectedPolygons();

	/** Triangulates selected polygons; returns whether successful */
	bool TriangulateSelectedPolygons();
	
	/** Assigns a material to the selected polygons; returns whether successful */
	bool AssignSelectedMaterialToSelectedPolygons();

	/** Assigns a material to the selected polygons; returns whether successful */
	bool AssignMaterialToSelectedPolygons( UMaterialInterface* SelectedMaterial );

	/** Creases selected edges; returns whether successful */
	bool MakeSelectedEdgesHardOrSoft( bool bMakeEdgesHard );

	/** Rolls back whatever we changed last time while previewing */
	void RollbackPreviewChanges();

	/** Gets mesh editor interactor data for the specified viewport interactor.  If we've never seen this viewport interactor before,
	    new (empty) data will be created for it on demand */
	FMeshEditorInteractorData& GetMeshEditorInteractorData( UViewportInteractor* ViewportInteractor );

	/** Deselects the mesh elements specified */
	void DeselectMeshElements( const TMap<UEditableMesh*, TArray<FMeshElement>>& MeshElementsToDeselect );

	/** Helper function that returns a map keying an editable mesh with its selected elements */
	void GetSelectedMeshesAndElements( EEditableMeshElementType ElementType, TMap<UEditableMesh*, TArray<FMeshElement>>& OutMeshesAndElements );

	void GetSelectedMeshesAndVertices( TMap<UEditableMesh*, TArray<FMeshElement>>& OutMeshesAndVertices ) { GetSelectedMeshesAndElements( EEditableMeshElementType::Vertex, OutMeshesAndVertices ); }
	void GetSelectedMeshesAndEdges( TMap<UEditableMesh*, TArray<FMeshElement>>& OutMeshesAndEdges ) { GetSelectedMeshesAndElements( EEditableMeshElementType::Edge, OutMeshesAndEdges ); }
	void GetSelectedMeshesAndPolygons( TMap<UEditableMesh*, TArray<FMeshElement>>& OutMeshesAndPolygons ) { GetSelectedMeshesAndElements( EEditableMeshElementType::Polygon, OutMeshesAndPolygons ); }

	/** Helper function that converts selected vertices into a map of meshes to their respective selected vertex elements and vertex IDs */
	void GetSelectedMeshesAndVertices( TMap< UEditableMesh*, TArray< TTuple< FMeshElement, FVertexID > > >& OutMeshesAndVertices );

	/** Helper function that converts selected edges into a map of meshes to their respective selected edge elements and edge IDs */
	void GetSelectedMeshesAndEdges( TMap< UEditableMesh*, TArray< TTuple< FMeshElement, FEdgeID > > >& OutMeshesAndEdges );

	/** Helper function that converts selected polygons into a map of meshes to their respective selected polygon elements and polygon refs */
	void GetSelectedMeshesAndPolygons( TMap< UEditableMesh*, TArray< TTuple< FMeshElement, FPolygonRef > > >& OutMeshesAndPolygons );

	/** Given an interactor and a mesh, finds edges under the interactor along with their exact split position (progress along the edge) */
	void FindEdgeSplitUnderInteractor( UViewportInteractor* ViewportInteractor, const UEditableMesh* EditableMesh, const TArray<TTuple<FMeshElement, FEdgeID>>& Edges, TArray<float>& OutSplits );

	/** Selects elements of the given type captured by the last marquee select */
	void PerformMarqueeSelect( EEditableMeshElementType ElementType );

	/** Rebuilds the list of mesh element transformables and updates the world viewport interaction system with the new list */
	void RefreshTransformables();

	/** Callback when PIE/SIE ends */
	void OnEndPIE( bool bIsSimulating );

	/** Callback from the level editor when the map changes */
	void OnMapChanged( UWorld* World, EMapChangeType MapChangeType );

	/** Callback from the level editor when new actors become selected or deselected */
	void OnActorSelectionChanged( const TArray<UObject*>& NewSelection, bool bForceRefresh );

	/** Creates the mesh edit actions to pass to the radial menu generator */
	void MeshEditActionsGenerator(class FMenuBuilder MenuBuilder, TSharedPtr<class FUICommandList> CommandList, class UVREditorMode* VRMode, float& RadiusOverride);

	/** Clears any references to editable meshes (which may now be invalid) */
	void RemoveEditableMeshReferences();

	/** Returns whether the mode is currently active */
	bool IsActive() const { return ( ViewportWorldInteraction != nullptr ); }

	/** Plays sound when starting a mesh edit action */
	void PlayStartActionSound(EMeshEditAction NewAction, UViewportInteractor* ActionInteractor = nullptr);

	/** Plays sound when mesh edit action was finished */
	void PlayFinishActionSound(EMeshEditAction NewAction, UViewportInteractor* ActionInteractor = nullptr);

protected:

	struct FSelectOrDeselectMeshElementsChangeInput
	{
		/** New mesh elements that should become selected */
		TArray<FMeshElement> MeshElementsToSelect;

		/** Mesh elements that should be deselected */
		TArray<FMeshElement> MeshElementsToDeselect;

		/** Default constructor */
		FSelectOrDeselectMeshElementsChangeInput()
			: MeshElementsToSelect(),
			  MeshElementsToDeselect()
		{
		}
	};


	class FSelectOrDeselectMeshElementsChange : public FChange
	{

	public:

		/** Constructor */
		FSelectOrDeselectMeshElementsChange( const FSelectOrDeselectMeshElementsChangeInput& InitInput )
			: Input( InitInput )
		{
		}

		FSelectOrDeselectMeshElementsChange( FSelectOrDeselectMeshElementsChangeInput&& InitInput )
			: Input( MoveTemp( InitInput ) )
		{
		}

		// Parent class overrides
		virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
		virtual FString ToString() const override;

	private:

		/** The data we need to make this change */
		FSelectOrDeselectMeshElementsChangeInput Input;

	};

	struct FDeselectAllMeshElementsChangeInput
	{
		/** Default constructor */
		FDeselectAllMeshElementsChangeInput()
		{
		}
	};


	class FDeselectAllMeshElementsChange : public FChange
	{

	public:

		/** Constructor */
		FDeselectAllMeshElementsChange( const FDeselectAllMeshElementsChangeInput& InitInput )
			: Input( InitInput )
		{
		}

		// Parent class overrides
		virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
		virtual FString ToString() const override;

	private:

		/** The data we need to make this change */
		FDeselectAllMeshElementsChangeInput Input;

	};


	struct FSetElementSelectionModeChangeInput
	{
		/** Default constructor */
		FSetElementSelectionModeChangeInput()
			: Mode( EEditableMeshElementType::Invalid )
			, bApplyStoredSelection( false )
		{
		}

		/** The mesh element selection mode being set */
		EEditableMeshElementType Mode;

		/** Whether we should just apply the stored selection */
		bool bApplyStoredSelection;

		/** The stored selection to be optionally applied */
		TArray<FMeshElement> StoredSelection;
	};


	class FSetElementSelectionModeChange : public FChange
	{

	public:

		/** Constructor */
		FSetElementSelectionModeChange( const FSetElementSelectionModeChangeInput& InitInput )
			: Input( InitInput )
		{
		}

		// Parent class overrides
		virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
		virtual FString ToString() const override;

	private:

		/** The data we need to make this change */
		FSetElementSelectionModeChangeInput Input;
	};


	/** Cached pointer to the viewport world interaction object we're using to interact with mesh elements */
	class UViewportWorldInteraction* ViewportWorldInteraction;

	/** Cached pointer to the VREditorMode object we're using */
	class UVREditorMode* VREditorMode;

	/** Material to use to render hovered mesh geometry */
	UMaterialInterface* HoveredGeometryMaterial;

	/** Material to use to render hovered triangles or faces */
	UMaterialInterface* HoveredFaceMaterial;

	/** Hover feedback animation time value, ever-incrementing until selection changes */
	double HoverFeedbackTimeValue;

	/** Interactors for the mouse cursor, and also for either virtual hand (when using VR) */
	TArray<FMeshEditorInteractorData> MeshEditorInteractorDatas;

	/** Specifies the type of element which is currently being selected */
	EEditableMeshElementType MeshElementSelectionMode;

	/** List of mesh elements that we've selected.  All elements in this list will always have the same mesh element type.
	    We don't allow users to select both edges, faces and/or polygons at the same time. */
	TArray<FMeshElement> SelectedMeshElements;

	/** List of selected elements for different selection modes. */
	TArray<FMeshElement> SelectedVertices;
	TArray<FMeshElement> SelectedEdges;
	TArray<FMeshElement> SelectedPolygons;

	/** List of old hovered mesh elements that are in the process of being faded out */
	TArray<FMeshElement> FadingOutHoveredMeshElements;




	/** Cached editable meshes */
	// @todo mesheditor: Need to expire these at some point, otherwise we just grow and grow */
	TMap< FEditableMeshSubMeshAddress, class UEditableMesh* > CachedEditableMeshes;

	/** Manages saving undo for selected mesh elements while we're dragging them around */
	FTrackingTransaction TrackingTransaction;

	/** The next action that will be started when interacting with a selected vertex */
	EMeshEditAction EquippedVertexAction;

	/** The next action that will be started when interacting with a selected edge */
	EMeshEditAction EquippedEdgeAction;

	/** The next action that will be started when interacting with a selected polygon */
	EMeshEditAction EquippedPolygonAction;

	/** The interactive action currently being performed (and previewed).  These usually happen over multiple frames, and
	    result in a 'final' application of the change that performs a more exhaustive (and more expensive) update. */
	EMeshEditAction ActiveAction;

	/** When performing an interactive action that was initiated using an interactor, this is the interactor that was used. */
	class UViewportInteractor* ActiveActionInteractor;

	/** True if the ActiveAction needs us to update the mesh element under the cursor every frame */
	bool bActiveActionNeedsHoverLocation;

	/** True if UpdateActiveAction() has yet to be called since the current action started */
	bool bIsFirstActiveActionUpdate;

	/** Command list for actions available regardless of selection */
	TSharedPtr<FUICommandList> CommonCommands;

	/** Command list for actions available when a vertex is selected */
	TSharedPtr<FUICommandList> VertexCommands;

	/** Command list for actions available when an edge is selected */
	TSharedPtr<FUICommandList> EdgeCommands;

	/** Command list for actions available when a polygon is selected */
	TSharedPtr<FUICommandList> PolygonCommands;

	TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>> CommonActions;
	TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>> VertexActions;
	TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>> EdgeActions;
	TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>> PolygonActions;

	//
	// ExtrudePolygon
	//

	/** When extruding polygons, we need to keep track of the interactor's impact point and normal, because
	    the user is going to be aiming their interactor along that axis to choose an extrusion point */
	FVector ExtrudePolygonAxisOrigin;
	FVector ExtrudePolygonAxisDirection;


	//
	// InsetPolygon
	//

	/** The selected polygon we clicked on to start the inset action */
	FMeshElement InsetUsingPolygonElement;


	//
	// SplitEdgeAndDragVertex
	//

	/** When splitting an edge and dragging a vertex, this is the list of edges that will be split */
	TMap< UEditableMesh*, TArray< TTuple< FMeshElement, FEdgeID > > > SplitEdgeMeshesAndEdgesToSplit;

	/** When splitting an edge and dragging a vertex, this is the list of split positions along those edges */
	TArray<float> SplitEdgeSplitList;


	//
	// EditVertexCornerSharpness/EdgeEdgeCreaseSharpness
	//

	/** Where the active interactor's impact point was when the "edit sharpness" action started */
	FVector EditSharpnessStartLocation;

	
	//
	// DrawVertices
	//

	/** Array of all points drawn so far */
	TArray<TTuple<double, FVector>> DrawnPoints;


	// ...

	/** When interactively dragging to preview a change (that might not be fully committed), this is the Change that
	    will be used to roll back the previewed alternative from the previous frame */
	TArray<TTuple<class UObject*,TUniquePtr<FChange>>> PreviewRevertChanges;

	/** Whether topology changed while we were applying the preview changes that we might revert later */
	EMeshTopologyChange PreviewTopologyChange;

	/** Proxy UObject to pass to the undo system when performing interactions that affect state of the mode itself, 
	    such as the selection set.  We need this because the UE4 undo system requries a UObject, but we're an FEdMode. */
	UMeshEditorModeProxyObject* MeshEditorModeProxyObject;

	/** When selecting by painting (EMeshEditAction::SelectByPainting), this is the compound change that can be applied to
	    roll back the change to select.  We'll build this up as the user is painting select, then store it in the undo buffer */
	TUniquePtr<FCompoundChangeInput> SelectingByPaintingRevertChangeInput;

	/** Whether vertex normals should be displayed for the selected mesh */
	bool bShowVertexNormals;

	/** Results of marquee select operation, pending action */
	TArray<FMeshElement> MarqueeSelectVertices;
	TArray<FMeshElement> MarqueeSelectEdges;
	TArray<FMeshElement> MarqueeSelectPolygons;

	/** Active transaction while marquee select is in progress
	    @todo mesheditor: this will be removed when 'current element type' is a thing */
	TUniquePtr<FScopedTransaction> MarqueeSelectTransaction;

	/** Whether the marquee select transaction is currently active and needs to be ended */
	bool bMarqueeSelectTransactionActive;

	/** Current view transform.
	    This is cached from the last known viewport, or taken directly from the VR head transform, if valid */
	TOptional<FTransform> CachedCameraToWorld;

	/** Requested focus to selection */
	bool bShouldFocusToSelection;

	/** Whether edits are made per-instance or not */
	bool bPerInstanceEdits;

	/** Holds all the assets for the mesh editor */
	class UMeshEditorAssetContainer* AssetContainer;

	friend class FMeshElementViewportTransformable;
};

