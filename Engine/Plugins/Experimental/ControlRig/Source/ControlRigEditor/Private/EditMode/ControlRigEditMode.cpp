// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ControlRigEditMode.h"
#include "ControlRigEditModeToolkit.h"
#include "Toolkits/ToolkitManager.h"
#include "SControlRigEditModeTools.h"
#include "Algo/Transform.h"
#include "ControlRig.h"
#include "HitProxies.h"
#include "ControlRigEditModeSettings.h"
#include "ISequencer.h"
#include "SequencerSettings.h"
#include "Sequencer/ControlRigSequence.h"
#include "Sequencer/ControlRigBindingTemplate.h"
#include "Sections/MovieSceneSpawnSection.h"
#include "MovieScene.h"
#include "EditorViewportClient.h"
#include "EditorModeManager.h"
#include "Engine/Selection.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRigEditModeCommands.h"
#include "Framework/Application/SlateApplication.h"
#include "Modules/ModuleManager.h"
#include "ISequencerModule.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "ControlRigEditorModule.h"
#include "ControlUnitProxy.h"
#include "Constraint.h"
#include "Units/RigUnit_Control.h"
#include "ControlRigControl.h"
#include "EngineUtils.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "IControlRigObjectBinding.h"
#include "Kismet2/BlueprintEditorUtils.h"

FName FControlRigEditMode::ModeName("EditMode.ControlRig");

#define LOCTEXT_NAMESPACE "ControlRigEditMode"

/** The different parts of a transform that manipulators can support */
enum class ETransformComponent
{
	None,

	Rotation,

	Translation,

	Scale
};

FControlRigEditMode::FControlRigEditMode()
	: bIsTransacting(false)
	, bManipulatorMadeChange(false)
	, bSelectedJoint(false)
	, bSelecting(false)
	, bSelectingByPath(false)
	, PivotTransform(FTransform::Identity)
{
	Settings = NewObject<UControlRigEditModeSettings>(GetTransientPackage(), TEXT("Settings"));

	OnControlsSelectedDelegate.AddRaw(this, &FControlRigEditMode::HandleSelectionChanged);

	CommandBindings = MakeShareable(new FUICommandList);
	BindCommands();

#if WITH_EDITOR
	GEditor->OnObjectsReplaced().AddRaw(this, &FControlRigEditMode::OnObjectsReplaced);
#endif
}

FControlRigEditMode::~FControlRigEditMode()
{
	CommandBindings = nullptr;

#if WITH_EDITOR
	GEditor->OnObjectsReplaced().RemoveAll(this);
#endif
}

void FControlRigEditMode::SetSequencer(TSharedPtr<ISequencer> InSequencer)
{
	static bool bRecursionGuard = false;
	if (!bRecursionGuard)
	{
		TGuardValue<bool> ScopeGuard(bRecursionGuard, true);

		Settings->Sequence = nullptr;

		WeakSequencer = InSequencer;
		if(UsesToolkits())
		{
			StaticCastSharedPtr<SControlRigEditModeTools>(Toolkit->GetInlineContent())->SetSequencer(InSequencer);
		}

		if (InSequencer.IsValid())
		{
			if (UControlRigSequence* Sequence = ExactCast<UControlRigSequence>(InSequencer->GetFocusedMovieSceneSequence()))
			{
				Settings->Sequence = Sequence;
				ReBindToActor();
			}
		}
	}
}

void FControlRigEditMode::SetObjects(const TWeakObjectPtr<>& InSelectedObject, const FGuid& InObjectBinding)
{
	WeakControlRig = Cast<UControlRig>(InSelectedObject.Get());
	ControlRigGuid = InObjectBinding;

	SetObjects_Internal();
}

void FControlRigEditMode::SetObjects_Internal()
{
	TArray<TWeakObjectPtr<>> SelectedObjects;
	if(IsInLevelEditor())
	{
		SelectedObjects.Add(Settings);
	}
	if(WeakControlRig.IsValid())
	{
		SelectedObjects.Add(WeakControlRig);
	}

	if(UsesToolkits())
	{
		StaticCastSharedPtr<SControlRigEditModeTools>(Toolkit->GetInlineContent())->SetDetailsObjects(SelectedObjects);
	}

	RefreshControlProxies();
}

void FControlRigEditMode::HandleBindToActor(AActor* InActor, bool bFocus)
{
	static bool bRecursionGuard = false;
	if (!bRecursionGuard)
	{
		TGuardValue<bool> ScopeGuard(bRecursionGuard, true);

		if(IsInLevelEditor())
		{
			FControlRigBindingTemplate::SetObjectBinding(InActor);
		}

		if (WeakSequencer.IsValid())
		{
			TSharedRef<ISequencer> Sequencer = WeakSequencer.Pin().ToSharedRef();

			// Modify the sequence
			if (UControlRigSequence* Sequence = ExactCast<UControlRigSequence>(Sequencer->GetFocusedMovieSceneSequence()))
			{
				Sequence->Modify(false);

				// Also modify the binding tracks in the sequence, so bindings get regenerated to this actor
				UMovieScene* MovieScene = Sequence->GetMovieScene();
				for (UMovieSceneSection* Section : MovieScene->GetAllSections())
				{
					if (UMovieSceneSpawnSection* SpawnSection = Cast<UMovieSceneSpawnSection>(Section))
					{
						SpawnSection->TryModify(false);
					}
				}

				// now notify the sequence (will rebind when it re-evaluates
				Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);

				// Force a rig evaluation here to make sure our manipulators are up to date
 				if (UControlRig* ControlRig = WeakControlRig.Get())
 				{
 					ControlRig->PreEvaluate_GameThread();
 					ControlRig->Evaluate_AnyThread();
 					ControlRig->PostEvaluate_GameThread();
 				}

				// Now re-display our objects in the details panel (they may have changed)
				if (MovieScene->GetSpawnableCount() > 0)
				{
					FGuid SpawnableGuid = MovieScene->GetSpawnable(0).GetGuid();
					TWeakObjectPtr<> BoundObject = Sequencer->FindSpawnedObjectOrTemplate(SpawnableGuid);
					SetObjects(BoundObject, SpawnableGuid);
				}
			}

			if (bFocus && InActor && IsInLevelEditor())
			{
				const bool bNotifySelectionChanged = false;
				const bool bDeselectBSP = true;
				const bool bWarnAboutTooManyActors = false;
				const bool bSelectEvenIfHidden = true;

				// Select & focus the actor
				GEditor->GetSelectedActors()->Modify();
				GEditor->GetSelectedActors()->BeginBatchSelectOperation();
				GEditor->SelectNone(bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors);
				GEditor->SelectActor(InActor, true, bNotifySelectionChanged, bSelectEvenIfHidden);
				GEditor->Exec(InActor->GetWorld(), TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY"));
				GEditor->SelectNone(bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors);
				GEditor->GetSelectedActors()->EndBatchSelectOperation();
			}
		}
	}
}

void FControlRigEditMode::ReBindToActor()
{
	if (Settings->Actor.IsValid())
	{
		HandleBindToActor(Settings->Actor.Get(), false);
	}
}

bool FControlRigEditMode::UsesToolkits() const
{
	return IsInLevelEditor();
}

void FControlRigEditMode::Enter()
{
	// Call parent implementation
	FEdMode::Enter();

	if(UsesToolkits())
	{
		if (!Toolkit.IsValid())
		{
			Toolkit = MakeShareable(new FControlRigEditModeToolkit(*this));
		}

		Toolkit->Init(Owner->GetToolkitHost());
	}

	SetObjects_Internal();
}

void FControlRigEditMode::Exit()
{
	if (bIsTransacting)
	{
		GEditor->EndTransaction();
		bIsTransacting = false;
		bManipulatorMadeChange = false;
	}

	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
	}

	// Call parent implementation
	FEdMode::Exit();
}

static bool ModeSupportedByTransformFilter(const FTransformFilter& InFilter, FWidget::EWidgetMode InMode)
{
	if(InMode == FWidget::WM_Translate && InFilter.TranslationFilter.IsValid())
	{
		return true;
	}

	if(InMode == FWidget::WM_Rotate && InFilter.RotationFilter.IsValid())
	{
		return true;
	}

	if(InMode == FWidget::WM_Scale && InFilter.ScaleFilter.IsValid())
	{
		return true;
	}

	return false;
}

void FControlRigEditMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);

	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		if (bSelectedJoint)
		{
			if(AreControlsSelected())
			{
				// cycle the widget mode if it is not supported on this selection
				FWidget::EWidgetMode CurrentMode = GetModeManager()->GetWidgetMode();
				bool bModeSupported = false;
				for (FControlUnitProxy& UnitProxy : ControlUnits)
				{
					if(UnitProxy.IsSelected())
					{
						if(FRigUnit_Control* ControlUnit = GetRigUnit(UnitProxy, ControlRig))
						{
							if(ModeSupportedByTransformFilter(ControlUnit->Filter, CurrentMode))
							{
								bModeSupported = true;
							}
						}
					}
				}

				if (!bModeSupported)
				{
					GetModeManager()->CycleWidgetMode();
				}
			}
		}

		ViewportClient->Invalidate();
		bSelectedJoint = false;

		// If we have detached from sequencer, unbind the settings UI
		if (!WeakSequencer.IsValid() && Settings->Sequence != nullptr)
		{
			Settings->Sequence = nullptr;
			RefreshObjects();
		}

		USceneComponent* Component = Cast<USceneComponent>(ControlRig->GetObjectBinding()->GetBoundObject());
		FTransform ComponentTransform = Component ? Component->GetComponentTransform() : FTransform::Identity;

		// Update controls from rig
		for(const FControlUnitProxy& UnitProxy : ControlUnits)
		{
			if(UnitProxy.Control)
			{
				UScriptStruct* Struct = nullptr;
				if(FRigUnit_Control* ControlUnit = GetRigUnit(UnitProxy, ControlRig, &Struct))
				{
					UnitProxy.Control->SetTransform(ControlUnit->GetResultantTransform() * ComponentTransform);
					UnitProxy.Control->TickControl(DeltaTime, *ControlUnit, Struct);
				}
			}
		}

		// update the pivot transform of our selected objects (they could be animating)
		RecalcPivotTransform();

		// Tick controls
		for(FControlUnitProxy& UnitProxy : ControlUnits)
		{
			if(UnitProxy.Control)
			{
				UnitProxy.Control->Tick(DeltaTime);
			}
		}
	}
}

void FControlRigEditMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		bool bRender = true;
		if (WeakSequencer.IsValid())
		{
			bRender = WeakSequencer.Pin()->GetPlaybackStatus() != EMovieScenePlayerStatus::Playing || Settings->bShowManipulatorsDuringPlayback;
		}

		// Force off manipulators if hide flag is set
		if (Settings->bHideManipulators)
		{
			bRender = false;
		}

		if (bRender)
		{
			if (Settings->bDisplayHierarchy)
			{
				USceneComponent* Component = Cast<USceneComponent>(ControlRig->GetObjectBinding()->GetBoundObject());
				FTransform ComponentTransform = Component ? Component->GetComponentTransform() : FTransform::Identity;

				// each base hierarchy Joint
				const FRigHierarchy& BaseHierarchy = ControlRig->GetBaseHierarchy();
				for (int32 JointIndex = 0; JointIndex < BaseHierarchy.Joints.Num(); ++JointIndex)
				{
					const FRigJoint& CurrentJoint = BaseHierarchy.Joints[JointIndex];
					const FTransform Transform = BaseHierarchy.GetGlobalTransform(JointIndex);

					if (CurrentJoint.ParentIndex != INDEX_NONE)
					{
						const FTransform ParentTransform = BaseHierarchy.GetGlobalTransform(CurrentJoint.ParentIndex);

						PDI->DrawLine(Transform.GetLocation(), ParentTransform.GetLocation(), FLinearColor::White, SDPG_Foreground);
					}

					PDI->DrawPoint(Transform.GetLocation(), FLinearColor::White, 5.0f, SDPG_Foreground);
				}
			}

			// @TODO: debug drawing per rig Joint (like details customizations) for this

// 			if (Settings->bDisplayTrajectories)
// 			{
// 				TrajectoryCache.RenderTrajectories(ComponentTransform, PDI);
// 			}
		}
	}
}

bool FControlRigEditMode::InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent)
{
	if (InEvent != IE_Released)
	{
		FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();
		if (CommandBindings->ProcessCommandBindings(InKey, KeyState, (InEvent == IE_Repeat)))
		{
			return true;
		}
	}

	return FEdMode::InputKey(InViewportClient, InViewport, InKey, InEvent);
}

bool FControlRigEditMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (bIsTransacting)
	{
		if (bManipulatorMadeChange)
		{
			// One final notify of our manipulators to make sure the property is keyed
			if(UControlRig* ControlRig = WeakControlRig.Get())
			{
				for(FControlUnitProxy& UnitProxy : ControlUnits)
				{
					if(UnitProxy.IsManipulating())
					{
						UnitProxy.SetManipulating(false);
						UnitProxy.NotifyPostEditChangeProperty(ControlRig);
					}
				}
			}

			if (Settings->bDisplayTrajectories)
			{
				TrajectoryCache.ForceRecalc();
			}
		}

		GEditor->EndTransaction();
		bIsTransacting = false;
		bManipulatorMadeChange = false;
		return true;
	}

	bManipulatorMadeChange = false;

	return false;
}

bool FControlRigEditMode::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (!bIsTransacting)
	{
		GEditor->BeginTransaction(LOCTEXT("MoveControlTransaction", "Move Control"));

		if(UControlRig* ControlRig = WeakControlRig.Get())
		{
			ControlRig->SetFlags(RF_Transactional);
			ControlRig->Modify();

			for(FControlUnitProxy& UnitProxy : ControlUnits)
			{
				UnitProxy.SetManipulating(true);
			}
		}

		bIsTransacting = true;
		bManipulatorMadeChange = false;

		return bIsTransacting;
	}

	return false;
}

bool FControlRigEditMode::UsesTransformWidget() const
{
	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		for (const FControlUnitProxy& UnitProxy : ControlUnits)
		{
			if(UnitProxy.IsSelected())
			{
				return true;
			}
		}
	}

	if (AreJointSelectedAndMovable())
	{
		return true;
	}

	return FEdMode::UsesTransformWidget();
}

bool FControlRigEditMode::UsesTransformWidget(FWidget::EWidgetMode CheckMode) const
{
	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		for (const FControlUnitProxy& UnitProxy : ControlUnits)
		{
			if(UnitProxy.IsSelected())
			{
				if(FRigUnit_Control* ControlUnit = GetRigUnit(UnitProxy, ControlRig))
				{
					return ModeSupportedByTransformFilter(ControlUnit->Filter, CheckMode);
				}
			}
		}

		if (AreJointSelectedAndMovable())
		{
			return true;
		}
	}

	return FEdMode::UsesTransformWidget(CheckMode);
}

FVector FControlRigEditMode::GetWidgetLocation() const
{
	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		USceneComponent* Component = Cast<USkeletalMeshComponent>(ControlRig->GetObjectBinding()->GetBoundObject());
		FTransform ComponentTransform = Component ? Component->GetComponentTransform() : FTransform::Identity;

		for (const FControlUnitProxy& UnitProxy : ControlUnits)
		{
			if(UnitProxy.IsSelected())
			{
				return ComponentTransform.TransformPosition(PivotTransform.GetLocation());
			}
		}

		// @todo: we only supports the first ast one for now
		// later we support multi select
		if (AreJointSelectedAndMovable())
		{
			return ComponentTransform.TransformPosition(OnGetJointTransformDelegate.Execute(SelectedJoints[0], false).GetLocation());
		}
	}

	return FEdMode::GetWidgetLocation();
}

bool FControlRigEditMode::GetCustomDrawingCoordinateSystem(FMatrix& OutMatrix, void* InData)
{
	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		for (const FControlUnitProxy& UnitProxy : ControlUnits)
		{
			if(UnitProxy.IsSelected())
			{
				OutMatrix = PivotTransform.ToMatrixNoScale().RemoveTranslation();
				return true;
			}
		}

		if (AreJointSelectedAndMovable())
		{
			USceneComponent* Component = Cast<USkeletalMeshComponent>(ControlRig->GetObjectBinding()->GetBoundObject());
			FTransform ComponentTransform = Component ? Component->GetComponentTransform() : FTransform::Identity;
			FTransform JointTransform = OnGetJointTransformDelegate.Execute(SelectedJoints[0], false)*ComponentTransform;
			OutMatrix = JointTransform.ToMatrixWithScale().RemoveTranslation();
			return true;
		}
	}

	return false;
}

bool FControlRigEditMode::GetCustomInputCoordinateSystem(FMatrix& OutMatrix, void* InData)
{
	return GetCustomDrawingCoordinateSystem(OutMatrix, InData);
}

bool FControlRigEditMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click)
{
	if(HActor* ActorHitProxy = HitProxyCast<HActor>(HitProxy))
	{
		if(ActorHitProxy->Actor && ActorHitProxy->Actor->IsA<AControlRigControl>())
		{
			AControlRigControl* ControlRigControl = Cast<AControlRigControl>(ActorHitProxy->Actor);
			if (Click.IsShiftDown() || Click.IsControlDown())
			{
				SetControlSelection(ControlRigControl->GetPropertyPath(), !IsControlSelected(ControlRigControl->GetPropertyPath()));
			}
			else
			{
				ClearControlSelection();
				SetControlSelection(ControlRigControl->GetPropertyPath(), true);
			}

			return true;
		}
	}

	// clear selected controls
	ClearControlSelection();

	// If we are animating then swallow clicks so we dont select things other than controls
	if(WeakSequencer.IsValid() && WeakSequencer.Pin()->GetFocusedMovieSceneSequence()->IsA<UControlRigSequence>())
	{
		return true;
	}

	return FEdMode::HandleClick(InViewportClient, HitProxy, Click);
}

bool FControlRigEditMode::IntersectSelect(bool InSelect, const TFunctionRef<bool(const FControlUnitProxy&, const FTransform&)>& Intersects)
{
	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		USceneComponent* Component = Cast<USceneComponent>(ControlRig->GetObjectBinding()->GetBoundObject());
		FTransform ComponentTransform = Component ? Component->GetComponentTransform() : FTransform::Identity;

		bool bSelected = false;
		for (const FControlUnitProxy& UnitProxy : ControlUnits)
		{
			if(FRigUnit_Control* ControlUnit = GetRigUnit(UnitProxy, ControlRig))
			{
				const FTransform ControlTransform = ControlUnit->GetResultantTransform() * ComponentTransform;
				if (Intersects(UnitProxy, ControlTransform))
				{
					SetControlSelection(UnitProxy.PropertyPathString, InSelect);
					bSelected = true;
				}
			}
		}

		return bSelected;
	}
	return false;
}

bool FControlRigEditMode::BoxSelect(FBox& InBox, bool InSelect)
{
	bool bIntersects = IntersectSelect(InSelect, [&](const FControlUnitProxy& ControlProxy, const FTransform& Transform)
	{ 
		if(ControlProxy.Control != nullptr)
		{
			FBox Bounds = ControlProxy.Control->GetComponentsBoundingBox(true);
			Bounds = Bounds.TransformBy(Transform);
			return InBox.Intersect(Bounds);
		}
		return false;
	});

	if (bIntersects)
	{
		return true;
	}

	return FEdMode::BoxSelect(InBox, InSelect);
}

bool FControlRigEditMode::FrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect)
{
	bool bIntersects = IntersectSelect(InSelect, [&](const FControlUnitProxy& ControlProxy, const FTransform& Transform) 
	{
		if(ControlProxy.Control != nullptr)
		{
			FBox Bounds = ControlProxy.Control->GetComponentsBoundingBox(true);
			Bounds = Bounds.TransformBy(Transform);
			return InFrustum.IntersectBox(Bounds.GetCenter(), Bounds.GetExtent());
		}
		return false;
	});

	if (bIntersects)
	{
		return true;
	}

	return FEdMode::FrustumSelect(InFrustum, InViewportClient, InSelect);
}

void FControlRigEditMode::SelectNone()
{
	ClearControlSelection();

	SelectedJoints.Reset();

	FEdMode::SelectNone();
}

bool FControlRigEditMode::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		FVector Drag = InDrag;
		FRotator Rot = InRot;
		FVector Scale = InScale;

		const bool bCtrlDown = InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);
		const bool bShiftDown = InViewport->KeyState(EKeys::LeftShift) || InViewport->KeyState(EKeys::RightShift);
		const bool bAltDown = InViewport->KeyState(EKeys::LeftAlt) || InViewport->KeyState(EKeys::RightAlt);
		const bool bMouseButtonDown = InViewport->KeyState(EKeys::LeftMouseButton);

		const FWidget::EWidgetMode WidgetMode = InViewportClient->GetWidgetMode();
		const EAxisList::Type CurrentAxis = InViewportClient->GetCurrentWidgetAxis();
		const ECoordSystem CoordSystem = InViewportClient->GetWidgetCoordSystemSpace();

		if (bIsTransacting && bMouseButtonDown && !bCtrlDown && !bShiftDown && !bAltDown && CurrentAxis != EAxisList::None)
		{
			const bool bDoRotation = !Rot.IsZero() && (WidgetMode == FWidget::WM_Rotate || WidgetMode == FWidget::WM_TranslateRotateZ);
			const bool bDoTranslation = !Drag.IsZero() && (WidgetMode == FWidget::WM_Translate || WidgetMode == FWidget::WM_TranslateRotateZ);
			const bool bDoScale = !Scale.IsZero() && WidgetMode == FWidget::WM_Scale;

			USceneComponent* Component = Cast<USceneComponent>(ControlRig->GetObjectBinding()->GetBoundObject());
			FTransform ComponentTransform = Component ? Component->GetComponentTransform() : FTransform::Identity;

			if (AreControlsSelected())
			{
				// manipulator transform is always on actor base - (actor origin being 0)
				for (FControlUnitProxy& UnitProxy : ControlUnits)
				{
					if (UnitProxy.IsSelected())
					{
						if (FRigUnit_Control* ControlUnit = GetRigUnit(UnitProxy, ControlRig))
						{
							FTransform NewWorldTransform = ControlUnit->GetResultantTransform() * ComponentTransform;

							bool bTransformChanged = false;
							if (bDoRotation && ControlUnit->Filter.RotationFilter.IsValid())
							{
								FQuat CurrentRotation = NewWorldTransform.GetRotation();
								CurrentRotation = (Rot.Quaternion() * CurrentRotation);
								NewWorldTransform.SetRotation(CurrentRotation);
								bTransformChanged = true;
							}

							if (bDoTranslation && ControlUnit->Filter.TranslationFilter.IsValid())
							{
								FVector CurrentLocation = NewWorldTransform.GetLocation();
								CurrentLocation = CurrentLocation + Drag;
								NewWorldTransform.SetLocation(CurrentLocation);
								bTransformChanged = true;
							}

							if (bDoScale && ControlUnit->Filter.ScaleFilter.IsValid())
							{
								FVector CurrentScale = NewWorldTransform.GetScale3D();
								CurrentScale = CurrentScale + Scale;
								NewWorldTransform.SetScale3D(CurrentScale);
								bTransformChanged = true;
							}

							if (bTransformChanged)
							{
								FTransform ResultantTransform = NewWorldTransform.GetRelativeTransform(ComponentTransform);

								UnitProxy.NotifyPreEditChangeProperty(ControlRig);

								ControlUnit->SetResultantTransform(ResultantTransform);

								if (UnitProxy.Control)
								{
									UnitProxy.Control->SetTransform(NewWorldTransform);
								}
								UnitProxy.NotifyPostEditChangeProperty(ControlRig);

								// Push to CDO if we are not in the level editor
								if (!IsInLevelEditor())
								{
									UClass* Class = ControlRig->GetClass();
									UControlRig* CDO = Class->GetDefaultObject<UControlRig>();
									if (FRigUnit_Control* DefaultControlUnit = GetRigUnit(UnitProxy, CDO))
									{
										CDO->Modify();

										DefaultControlUnit->SetResultantTransform(ResultantTransform);

										UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy);
										if (Blueprint)
										{
											FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
										}
									}
								}

								bManipulatorMadeChange = true;
							}
						}
					}
				}

				RecalcPivotTransform();

				return true;
			}
			else if (AreJointSelectedAndMovable())
			{
				// set joint transform
				// that will set initial joint transform
				const FName CurrentJoint = SelectedJoints[0];
				FTransform NewWorldTransform = OnGetJointTransformDelegate.Execute(CurrentJoint, false) * ComponentTransform;
				bool bTransformChanged = false;
				if (bDoRotation)
				{
					FQuat CurrentRotation = NewWorldTransform.GetRotation();
					CurrentRotation = (Rot.Quaternion() * CurrentRotation);
					NewWorldTransform.SetRotation(CurrentRotation);
					bTransformChanged = true;
				}

				if (bDoTranslation)
				{
					FVector CurrentLocation = NewWorldTransform.GetLocation();
					CurrentLocation = CurrentLocation + Drag;
					NewWorldTransform.SetLocation(CurrentLocation);
					bTransformChanged = true;
				}

				if (bDoScale)
				{
					FVector CurrentScale = NewWorldTransform.GetScale3D();
					CurrentScale = CurrentScale + Scale;
					NewWorldTransform.SetScale3D(CurrentScale);
					bTransformChanged = true;
				}

				if (bTransformChanged)
				{
					FTransform NewComponentTransform = NewWorldTransform.GetRelativeTransform(ComponentTransform);
					OnSetJointTransformDelegate.Execute(CurrentJoint, NewComponentTransform);
				}

				return true;
			}
		}
	}

	return false;
}

bool FControlRigEditMode::ShouldDrawWidget() const
{
	if (AreControlsSelected() || AreJointSelectedAndMovable())
	{
		return true;
	}

	return FEdMode::ShouldDrawWidget();
}

bool FControlRigEditMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	if (OtherModeID == FBuiltinEditorModes::EM_Placement)
	{
		return false;
	}
	return true;
}

void FControlRigEditMode::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(Settings);

	for(FControlUnitProxy& UnitProxy : ControlUnits)
	{
		Collector.AddReferencedObject(UnitProxy.Control);
	}
}

void FControlRigEditMode::ClearControlSelection()
{
	if (!bSelecting)
	{
		TGuardValue<bool> ReentrantGuard(bSelecting, true);

		for(FControlUnitProxy& UnitProxy : ControlUnits)
		{
			UnitProxy.SetSelected(false);
		}

		bSelectedJoint = true;
		OnControlsSelectedDelegate.Broadcast(TArray<FString>());
	}
}

void FControlRigEditMode::SetControlSelection(const FString& InControlPropertyPath, bool bSelected)
{
	if (!bSelecting)
	{
		TGuardValue<bool> ReentrantGuard(bSelecting, true);

		TArray<FString> SelectedPropertyPaths;
		for(FControlUnitProxy& UnitProxy : ControlUnits)
		{
			if(InControlPropertyPath == UnitProxy.PropertyPathString || InControlPropertyPath == UnitProxy.TransformPropertyPathString)
			{
				UnitProxy.SetSelected(bSelected);
				SelectedPropertyPaths.Add(UnitProxy.TransformPropertyPathString);
			}
		}

		bSelectedJoint = true;
		OnControlsSelectedDelegate.Broadcast(SelectedPropertyPaths);
	}
}

void FControlRigEditMode::SetControlSelection(const TArray<FString>& InControlPropertyPaths, bool bSelected)
{
	if (!bSelecting)
	{
		TGuardValue<bool> ReentrantGuard(bSelecting, true);

		TArray<FString> SelectedPropertyPaths;
		for(FControlUnitProxy& UnitProxy : ControlUnits)
		{
			for (const FString& ControlPropertyPath : InControlPropertyPaths)
			{
				if(ControlPropertyPath == UnitProxy.PropertyPathString || ControlPropertyPath == UnitProxy.TransformPropertyPathString)
				{
					UnitProxy.SetSelected(bSelected);
					SelectedPropertyPaths.Add(UnitProxy.TransformPropertyPathString);
					break;
				}
			}
		}

		bSelectedJoint = true;
		OnControlsSelectedDelegate.Broadcast(SelectedPropertyPaths);
	}
}

bool FControlRigEditMode::IsControlSelected(const FString& InControlPropertyPath) const
{
	for(const FControlUnitProxy& UnitProxy : ControlUnits)
	{
		if(UnitProxy.PropertyPathString == InControlPropertyPath)
		{
			return UnitProxy.IsSelected();
		}
	}

	return false;
}

bool FControlRigEditMode::AreControlsSelected() const
{
	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		for(const FControlUnitProxy& UnitProxy : ControlUnits)
		{
			if(UnitProxy.IsSelected())
			{
				return true;
			}
		}
	}

	return false;
}

int32 FControlRigEditMode::GetNumSelectedControls() const
{
	int32 NumSelected = 0;
	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		for(const FControlUnitProxy& UnitProxy : ControlUnits)
		{
			if(UnitProxy.IsSelected())
			{
				NumSelected++;
			}
		}
	}

	return NumSelected;
}

void FControlRigEditMode::SetControlEnabled(const FString& InControlPropertyPath, bool bEnabled)
{
	for(FControlUnitProxy& UnitProxy : ControlUnits)
	{
		if(UnitProxy.PropertyPathString == InControlPropertyPath)
		{
			UnitProxy.SetEnabled(bEnabled);
		}
	}
}

bool FControlRigEditMode::IsControlEnabled(const FString& InControlPropertyPath) const
{
	for(const FControlUnitProxy& UnitProxy : ControlUnits)
	{
		if(UnitProxy.PropertyPathString == InControlPropertyPath)
		{
			return UnitProxy.IsEnabled();
		}
	}

	return false;
}

FString FControlRigEditMode::GetControlFromPropertyPath(const FString& InPropertyPath) const
{
	for (const FControlUnitProxy& UnitProxy : ControlUnits)
	{
		if (UnitProxy.PropertyPathString == InPropertyPath)
		{
			// the output
			return UnitProxy.PropertyPath.ToString();
		}
	}
	
	return TEXT("");
}

void FControlRigEditMode::HandleObjectSpawned(FGuid InObjectBinding, UObject* SpawnedObject, IMovieScenePlayer& Player)
{
	if (WeakSequencer.IsValid())
	{
		// check whether this spawned object is from our sequence
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer.Get() == &Player)
		{
			RefreshObjects();

			// check if the object is being displayed currently
			if (ControlRigGuid == InObjectBinding)
			{
				if (WeakControlRig != Cast<UControlRig>(SpawnedObject))
				{
					WeakControlRig = Cast<UControlRig>(SpawnedObject);
					SetObjects_Internal();
				}
				return;
			}

			// We didnt find an existing Guid, so set up our internal cache
			if (!ControlRigGuid.IsValid())
			{
				SetObjects(SpawnedObject, InObjectBinding);
				if (UControlRig* ControlRig = Cast<UControlRig>(SpawnedObject))
				{
					if (Settings->Actor.IsValid() && ControlRig->GetObjectBinding()->GetBoundObject() == nullptr)
					{
						ControlRig->GetObjectBinding()->BindToObject(Settings->Actor.Get());
					}
				}
				ReBindToActor();
			}
		}
	}
}

void FControlRigEditMode::RefreshObjects()
{
	if (WeakSequencer.IsValid())
	{
		TSharedRef<ISequencer> Sequencer = WeakSequencer.Pin().ToSharedRef();
		UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence() ? Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene() : nullptr;
		if (MovieScene)
		{
			// check if we have an invalid Guid & invalidate Guid if so
			if (ControlRigGuid.IsValid() && MovieScene->FindSpawnable(ControlRigGuid) == nullptr)
			{
				ControlRigGuid.Invalidate();
				WeakControlRig = nullptr;
			}

			SetObjects_Internal();
		}
	}
	else
	{
		WeakControlRig = nullptr;
		ControlRigGuid.Invalidate();

		SetObjects_Internal();
	}
}

void FControlRigEditMode::RecalcPivotTransform()
{
	int32 NumSelectedControls = GetNumSelectedControls();

	PivotTransform = FTransform::Identity;

	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		if(NumSelectedControls > 0)
		{
			FTransform LastTransform = FTransform::Identity;

			// Use average location as pivot location
			FVector PivotLocation = FVector::ZeroVector;

			for(const FControlUnitProxy& UnitProxy : ControlUnits)
			{
				if(UnitProxy.IsSelected())
				{
					if(FRigUnit_Control* ControlUnit = GetRigUnit(UnitProxy, ControlRig))
					{
						FTransform ResultantTransform = ControlUnit->GetResultantTransform();
						PivotLocation += ResultantTransform.GetLocation();
						LastTransform = ResultantTransform;
					}
				}
			}

			PivotLocation /= (float)NumSelectedControls;
			PivotTransform.SetLocation(PivotLocation);

			// recalc coord system too
			USceneComponent* Component = Cast<USceneComponent>(ControlRig->GetObjectBinding()->GetBoundObject());
			FTransform ComponentTransform = Component ? Component->GetComponentTransform() : FTransform::Identity;

			if (NumSelectedControls == 1)
			{
				// A single Joint just uses its own transform
				FTransform WorldTransform = LastTransform * ComponentTransform;
				PivotTransform.SetRotation(WorldTransform.GetRotation());
			}
			else if (NumSelectedControls > 1)
			{
				// If we have more than one Joint selected, use the coordinate space of the component
				PivotTransform.SetRotation(ComponentTransform.GetRotation());
			}
		}
	}
}

void FControlRigEditMode::HandleSelectionChanged(const TArray<FString>& InSelectedPropertyPaths)
{
	if(!bSelecting)
	{
		ClearControlSelection();
		SetControlSelection(InSelectedPropertyPaths, true);
	}

	if (WeakSequencer.IsValid())
	{
		if (InSelectedPropertyPaths.Num() > 0)
		{
			WeakSequencer.Pin()->SelectByPropertyPaths(InSelectedPropertyPaths);
		}
	}

	for(const FControlUnitProxy& UnitProxy : ControlUnits)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		if (UnitProxy.Control)
		{
			UnitProxy.Control->GetComponents(PrimitiveComponents, true);
			for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
			{
				PrimitiveComponent->PushSelectionToProxy();
			}
		}
	}

	if (WeakSequencer.IsValid())
	{
		if (WeakSequencer.Pin()->GetSequencerSettings()->GetShowSelectedNodesOnly())
		{
			WeakSequencer.Pin()->RefreshTree();
		}
	}
}

void FControlRigEditMode::BindCommands()
{
	const FControlRigEditModeCommands& Commands = FControlRigEditModeCommands::Get();

	CommandBindings->MapAction(
		Commands.SetKey,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::SetKeysForSelectedManipulators));

	CommandBindings->MapAction(
		Commands.ToggleManipulators,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::ToggleManipulators));

	CommandBindings->MapAction(
		Commands.ToggleTrajectories,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::ToggleTrajectories));
}

void FControlRigEditMode::SetKeysForSelectedManipulators()
{
	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		for(const FControlUnitProxy& UnitProxy : ControlUnits)
		{
			if(UnitProxy.IsSelected())
			{
				SetKeyForControl(UnitProxy);
			}
		}
	}
}

void FControlRigEditMode::SetKeyForControl(const FControlUnitProxy& UnitProxy)
{
	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer.IsValid())
		{
			// @TODO: need sequencer support for the new property path lib
			TArray<UObject*> Objects({ ControlRig });
		//	FKeyPropertyParams KeyPropertyParams(Objects, UnitProxy.PropertyPathString, ESequencerKeyMode::ManualKeyForced);
		//	Sequencer->KeyProperty(KeyPropertyParams);
		}
	}
}

void FControlRigEditMode::ToggleManipulators()
{
	// Toggle flag (is used in drawing code)
	Settings->bHideManipulators = !Settings->bHideManipulators;
}

void FControlRigEditMode::ToggleTrajectories()
{
	Settings->bDisplayTrajectories = !Settings->bDisplayTrajectories;
//	TrajectoryCache.RebuildMesh(SelectedIndices);
}


void FControlRigEditMode::RefreshTrajectoryCache()
{
//	TrajectoryCache.ForceRecalc();
}

bool FControlRigEditMode::MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y)
{
	// Inform units of hover state
	HActor* ActorHitProxy = HitProxyCast<HActor>(Viewport->GetHitProxy(x, y));
	if(ActorHitProxy && ActorHitProxy->Actor && ActorHitProxy->Actor->IsA<AControlRigControl>())
	{
		for(FControlUnitProxy& UnitProxy : ControlUnits)
		{
			UnitProxy.SetHovered(ActorHitProxy->Actor == UnitProxy.Control);
		}
	}

	return false;
}

bool FControlRigEditMode::MouseLeave(FEditorViewportClient* ViewportClient, FViewport* Viewport)
{
	// Remove hover state from all units
	for(FControlUnitProxy& UnitProxy : ControlUnits)
	{
		UnitProxy.SetHovered(false);
	}

	return false;
}

void FControlRigEditMode::RefreshControlProxies()
{
	TArray<FString> SelectedPropertyPaths;

	for(FControlUnitProxy& UnitProxy : ControlUnits)
	{
		if(UnitProxy.IsSelected())
		{
			SelectedPropertyPaths.Add(UnitProxy.PropertyPathString);
		}

		if(UnitProxy.Control)
		{
			GetWorld()->DestroyActor(UnitProxy.Control, false, false);
			UnitProxy.Control = nullptr;
		}
	}

	ControlUnits.Reset();

	if(UControlRig* ControlRig = WeakControlRig.Get())
	{
		UControlRigBlueprintGeneratedClass* Class = Cast<UControlRigBlueprintGeneratedClass>(ControlRig->GetClass());
		for(UStructProperty* ControlUnitProperty : Class->ControlUnitProperties)
		{
			FRigUnit_Control* Control = ControlUnitProperty->ContainerPtrToValuePtr<FRigUnit_Control>(ControlRig);
			FControlUnitProxy& UnitProxy = ControlUnits[ControlUnits.AddDefaulted()];
			UnitProxy.PropertyPath = FCachedPropertyPath(ControlUnitProperty->GetName());
			UnitProxy.PropertyPathString = UnitProxy.PropertyPath.ToString();
			UnitProxy.TransformPropertyPath = FCachedPropertyPath(ControlUnitProperty->GetName() + TEXT(".Transform"));
			UnitProxy.TransformPropertyPathString = UnitProxy.TransformPropertyPath.ToString();
			UnitProxy.SetSelected(SelectedPropertyPaths.Contains(UnitProxy.PropertyPathString));

			if(Control->ControlClass)
			{
				FActorSpawnParameters ActorSpawnParameters;
				ActorSpawnParameters.bTemporaryEditorActor = true;
				UnitProxy.Control = GetWorld()->SpawnActor<AControlRigControl>(Control->ControlClass, ActorSpawnParameters);
				UnitProxy.Control->SetPropertyPath(UnitProxy.PropertyPathString);

				TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
				UnitProxy.Control->GetComponents(PrimitiveComponents, true);
				for(UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
				{
					PrimitiveComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FControlRigEditMode::PreviewComponentSelectionOverride);
					PrimitiveComponent->PushSelectionToProxy();
				}
			}
		}
	}
}

FControlRigEditMode* FControlRigEditMode::GetEditModeFromWorldContext(UWorld* InWorldContext)
{
	return nullptr;
}

FRigUnit_Control* FControlRigEditMode::GetRigUnit(const FControlUnitProxy& InProxy, UControlRig* InControlRig, UScriptStruct** OutControlStructPtr /*= nullptr*/)
{
	UControlRigBlueprintGeneratedClass* Class = CastChecked<UControlRigBlueprintGeneratedClass>(InControlRig->GetClass());
	for(UStructProperty* Property : Class->ControlUnitProperties)
	{
		if(Property->GetFName() == InProxy.PropertyPath.GetLastSegment().GetName())
		{
			if(OutControlStructPtr)
			{
				*OutControlStructPtr = Property->Struct;
			}
			return Property->ContainerPtrToValuePtr<FRigUnit_Control>(InControlRig);
		}
	}

	return nullptr;
}

bool FControlRigEditMode::PreviewComponentSelectionOverride(const UPrimitiveComponent* InComponent) const
{
	AActor* OwnerActor = InComponent->GetOwner();
	if(OwnerActor)
	{
		// See if the actor is in a selected unit proxy
		for(const FControlUnitProxy& UnitProxy : ControlUnits)
		{
			if(UnitProxy.Control == OwnerActor)
			{
				return UnitProxy.IsSelected();
			}
		}
	}

	return false;
}

void FControlRigEditMode::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	if (WeakControlRig.IsValid())
	{
		UObject* OldObject = WeakControlRig.Get();
		UObject* NewObject = OldToNewInstanceMap.FindRef(OldObject);
		if (NewObject)
		{
			WeakControlRig = Cast<UControlRig>(NewObject);
			WeakControlRig->PostReinstanceCallback(CastChecked<UControlRig>(OldObject));
			SetObjects_Internal();
		}
	}
}

bool FControlRigEditMode::AreJointSelectedAndMovable() const
{
	if (UControlRig* ControlRig = WeakControlRig.Get())
	{
		return (!ControlRig->bExecutionOn && OnGetJointTransformDelegate.IsBound() && OnSetJointTransformDelegate.IsBound() && SelectedJoints.Num() > 0);
	}

	return false;
}

bool FControlRigEditMode::AreJointSelected() const
{
	return (SelectedJoints.Num() > 0);
}

void FControlRigEditMode::SelectJoint(const FName& InJoint)
{
	ClearControlSelection();

	SelectedJoints.Reset();
	if (InJoint != NAME_None)
	{
		SelectedJoints.Add(InJoint);
	}
}
#undef LOCTEXT_NAMESPACE
