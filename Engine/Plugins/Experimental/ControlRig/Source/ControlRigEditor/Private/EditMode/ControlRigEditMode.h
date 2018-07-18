// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "IPersonaEditMode.h"
#include "ControlRigTrajectoryCache.h"
#include "ControlUnitProxy.h"

class FEditorViewportClient;
class FViewport;
class UActorFactory;
struct FViewportClick;
class UControlRig;
class ISequencer;
class UControlRigEditModeSettings;
class UControlManipulator;
class FUICommandList;
class FPrimitiveDrawInterface;
class FToolBarBuilder;
class FExtender;
class IMovieScenePlayer;
struct FRigUnit_Control;

/** Delegate fired when controls are selected */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnControlsSelected, const TArray<FString>& /*SelectedControlPropertyPaths*/);
DECLARE_DELEGATE_RetVal_TwoParams(FTransform, FOnGetJointTransform, const FName& /*JointName*/, bool /*bLocal*/);
DECLARE_DELEGATE_TwoParams(FOnSetJointTransform, const FName& /*JointName*/, const FTransform& /*Transform*/);

class FControlRigEditMode : public IPersonaEditMode
{
public:
	static FName ModeName;

	FControlRigEditMode();
	~FControlRigEditMode();

	/** Set the objects to be displayed in the details panel */
	void SetObjects(const TWeakObjectPtr<>& InSelectedObject, const FGuid& InObjectBinding);

	/** Set the sequencer we are bound to */
	void SetSequencer(TSharedPtr<ISequencer> InSequencer);

	/** This edit mode is re-used between the level editor and the control rig editor. Calling this indicates which context we are in */
	virtual bool IsInLevelEditor() const { return true; }

	// FEdMode interface
	virtual bool UsesToolkits() const override;
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual bool InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override;
	virtual bool EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click) override;
	virtual bool BoxSelect(FBox& InBox, bool InSelect = true) override;
	virtual bool FrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect = true) override;
	virtual void SelectNone() override;
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) override;
	virtual bool UsesTransformWidget() const override;
	virtual bool UsesTransformWidget(FWidget::EWidgetMode CheckMode) const;
	virtual FVector GetWidgetLocation() const override;
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& OutMatrix, void* InData) override;
	virtual bool GetCustomInputCoordinateSystem(FMatrix& OutMatrix, void* InData) override;
	virtual bool ShouldDrawWidget() const override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override;
	virtual bool MouseLeave(FEditorViewportClient* ViewportClient, FViewport* Viewport);

	/* IPersonaEditMode interface */
	virtual bool GetCameraTarget(FSphere& OutTarget) const override { return false; }
	virtual class IPersonaPreviewScene& GetAnimPreviewScene() const override { check(false); return *(IPersonaPreviewScene*)this; }
	virtual void GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const override {}

	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	/** Clear all selected controls */
	void ClearControlSelection();

	/** Set a control's selection state */
	void SetControlSelection(const FString& InControlPropertyPath, bool bSelected);

	/** Set multiple control's selection states */
	void SetControlSelection(const TArray<FString>& InControlPropertyPaths, bool bSelected);

	/** Check if the specified control is selected */
	bool IsControlSelected(const FString& InControlPropertyPath) const;

	/** Check if any controls are selected */
	bool AreControlsSelected() const;

	/** Get the number of selected controls */
	int32 GetNumSelectedControls() const;

	/** Set a control's enabled state */
	void SetControlEnabled(const FString& InControlPropertyPath, bool bEnabled);

	/** Get the node name from the property path */
	FString GetControlFromPropertyPath(const FString& PropertyPath) const;
	/** Check if the specified control is enabled */
	bool IsControlEnabled(const FString& InControlPropertyPath) const;

	/** 
	 * Lets the edit mode know that an object has just been spawned. 
	 * Allows us to redisplay different underlying objects in the details panel.
	 */
	void HandleObjectSpawned(FGuid InObjectBinding, UObject* SpawnedObject, IMovieScenePlayer& Player);

	/** Re-bind to the current actor - used when sequence, selection etc. changes */
	void ReBindToActor();

	/** Bind us to an actor for editing */
	void HandleBindToActor(AActor* InActor, bool bFocus);

	/** Refresh our internal object list (they may have changed) */
	void RefreshObjects();

	/** Delegate fired when controls are selected */
	FOnControlsSelected& OnControlsSelected() { return OnControlsSelectedDelegate; }

	/** Refresh our trajectory cache */
	void RefreshTrajectoryCache();

	/** Set a key for a specific control */
	void SetKeyForControl(const FControlUnitProxy& UnitProxy);

	/** Get the settings we are using */
	const UControlRigEditModeSettings* GetSettings() { return Settings; }

	/** Find the edit mode corresponding to the specified world context */
	static FControlRigEditMode* GetEditModeFromWorldContext(UWorld* InWorldContext);

	/** Helper function - get a rig unit from a proxy and a rig */
	static FRigUnit_Control* GetRigUnit(const FControlUnitProxy& InProxy, UControlRig* InControlRig, UScriptStruct** OutControlStructPtr = nullptr);

	/** Select Joint */
	void SelectJoint(const FName& InJoint);
	FOnGetJointTransform& OnGetJointTransform() { return OnGetJointTransformDelegate; }
	FOnSetJointTransform& OnSetJointTransform() { return OnSetJointTransformDelegate; }
protected:
	/** Helper function: set ControlRigs array to the details panel */
	void SetObjects_Internal();

	/** Updates cached pivot transform */
	void RecalcPivotTransform();

	/** Helper function for box/frustum intersection */
	bool IntersectSelect(bool InSelect, const TFunctionRef<bool(const FControlUnitProxy&, const FTransform&)>& Intersects);

	/** Handle selection internally */
	void HandleSelectionChanged(const TArray<FString>& InSelectedJoints);

	/** Set keys on all selected manipulators */
	void SetKeysForSelectedManipulators();

	/** Toggles visibility of manipulators in the viewport */
	void ToggleManipulators();

	/** Toggles visibility of trajectories in the viewport */
	void ToggleTrajectories();

	/** Bind our keyboard commands */
	void BindCommands();

	/** Refresh control proxies when the control rig changes */
	void RefreshControlProxies();

	/** Let the preview scene know how we want to select components */
	bool PreviewComponentSelectionOverride(const UPrimitiveComponent* InComponent) const;

protected:
	/** Cache for rendering trajectories */
	FControlRigTrajectoryCache TrajectoryCache;

	/** Settings object used to insert controls into the details panel */
	UControlRigEditModeSettings* Settings;

	/** The units we use to represent the rig */
	TArray<FControlUnitProxy> ControlUnits;

	/** Whether we are in the middle of a transaction */
	bool bIsTransacting;

	/** Whether a manipulator actually made a change when transacting */
	bool bManipulatorMadeChange;

	/** The ControlRig we are animating */
	TWeakObjectPtr<UControlRig> WeakControlRig;

	/** The sequencer GUID of the object we are animating */
	FGuid ControlRigGuid;

	/** Sequencer we are currently bound to */
	TWeakPtr<ISequencer> WeakSequencer;

	/** As we cannot cycle widget mode during tracking, we defer cycling until after a click with this flag */
	bool bSelectedJoint;

	/** Delegate fired when controls are selected */
	FOnControlsSelected OnControlsSelectedDelegate;

	/** Guard value for selection */
	bool bSelecting;

	/** Guard value for selection by property path */
	bool bSelectingByPath;

	/** Cached transform of pivot point for selected Joints */
	FTransform PivotTransform;

	/** Command bindings for keyboard shortcuts */
	TSharedPtr<FUICommandList> CommandBindings;

	/** Called from the editor when a blueprint object replacement has occurred */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	/** Selected Joints */
	TArray<FName> SelectedJoints;

	FOnGetJointTransform OnGetJointTransformDelegate;
	FOnSetJointTransform OnSetJointTransformDelegate;

	bool AreJointSelected() const;
};
