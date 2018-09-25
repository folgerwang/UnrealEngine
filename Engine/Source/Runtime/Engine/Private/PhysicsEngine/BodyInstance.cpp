// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/BodyInstance.h"
#include "EngineGlobals.h"
#include "HAL/IConsoleManager.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Volume.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ShapeComponent.h"
#include "Engine/CollisionProfile.h"
#include "SceneManagement.h"
#include "Collision.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysicsFiltering.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/ShapeElem.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/BodySetup.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/IConsoleManager.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

#if WITH_PHYSX
	#include "PhysXPublic.h"
	#include "Physics/PhysicsFiltering.h"
	#include "PhysicsEngine/PhysXSupport.h"
	#include "Collision/CollisionConversions.h"
#include "PxShape.h"
#endif // WITH_PHYSX

#define LOCTEXT_NAMESPACE "BodyInstance"

#include "Components/ModelComponent.h"
#include "Components/BrushComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/PxQueryFilterCallback.h"

DECLARE_CYCLE_STAT(TEXT("Init Body"), STAT_InitBody, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Init Body Debug"), STAT_InitBodyDebug, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Init Body Scene Interaction"), STAT_InitBodySceneInteraction, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Init Body Post Add to Scene"), STAT_InitBodyPostAdd, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Term Body"), STAT_TermBody, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Update Materials"), STAT_UpdatePhysMats, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Update Materials Scene Interaction"), STAT_UpdatePhysMatsSceneInteraction, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Filter Update"), STAT_UpdatePhysFilter, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Filter Update (PhysX Code)"), STAT_UpdatePhysFilterPhysX, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Init Bodies"), STAT_InitBodies, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Bulk Body Scene Add"), STAT_BulkSceneAdd, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Static Init Bodies"), STAT_StaticInitBodies, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("UpdateBodyScale"), STAT_BodyInstanceUpdateBodyScale, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("CreatePhysicsShapesAndActors"), STAT_CreatePhysicsShapesAndActors, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("BodyInstance SetCollisionProfileName"), STAT_BodyInst_SetCollisionProfileName, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Phys SetBodyTransform"), STAT_SetBodyTransform, STATGROUP_Physics);

// @HACK Guard to better encapsulate game related hacks introduced into UpdatePhysicsFilterData()
TAutoConsoleVariable<int32> CVarEnableDynamicPerBodyFilterHacks(
	TEXT("p.EnableDynamicPerBodyFilterHacks"), 
	0, 
	TEXT("Enables/Disables the use of a set of game focused hacks - allowing users to modify skel body collision dynamically (changes the behavior of per-body collision filtering)."),
	ECVF_ReadOnly
);

using namespace PhysicsInterfaceTypes;

bool IsRigidBodyKinematic_AssumesLocked(const FPhysicsActorHandle& InActorRef)
{
	if(FPhysicsInterface::IsRigidBody(InActorRef))
	{
		return FPhysicsInterface::IsKinematic_AssumesLocked(InActorRef);
	}

	return false;
}

int32 FillInlineShapeArray_AssumesLocked(PhysicsInterfaceTypes::FInlineShapeArray& Array, const FPhysicsActorHandle& Actor, EPhysicsSceneType InSceneType)
{
	FPhysicsInterface::GetAllShapes_AssumedLocked(Actor, Array, InSceneType);

	return Array.Num();
}

////////////////////////////////////////////////////////////////////////////
// FCollisionResponse
////////////////////////////////////////////////////////////////////////////

FCollisionResponse::FCollisionResponse()
{

}

FCollisionResponse::FCollisionResponse(ECollisionResponse DefaultResponse)
{
	SetAllChannels(DefaultResponse);
}

/** Set the response of a particular channel in the structure. */
void FCollisionResponse::SetResponse(ECollisionChannel Channel, ECollisionResponse NewResponse)
{
#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
	ECollisionResponse DefaultResponse = FCollisionResponseContainer::GetDefaultResponseContainer().GetResponse(Channel);
	if (DefaultResponse == NewResponse)
	{
		RemoveReponseFromArray(Channel);
	}
	else
	{
		AddReponseToArray(Channel, NewResponse);
	}
#endif

	ResponseToChannels.SetResponse(Channel, NewResponse);
}

/** Set all channels to the specified response */
void FCollisionResponse::SetAllChannels(ECollisionResponse NewResponse)
{
	ResponseToChannels.SetAllChannels(NewResponse);
#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
	UpdateArrayFromResponseContainer();
#endif
}

void FCollisionResponse::ReplaceChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse)
{
	ResponseToChannels.ReplaceChannels(OldResponse, NewResponse);
#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
	UpdateArrayFromResponseContainer();
#endif
}

/** Set all channels from ChannelResponse Array **/
void FCollisionResponse::SetCollisionResponseContainer(const FCollisionResponseContainer& InResponseToChannels)
{
	ResponseToChannels = InResponseToChannels;
#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
	// this is only valid case that has to be updated
	UpdateArrayFromResponseContainer();
#endif
}

void FCollisionResponse::SetResponsesArray(const TArray<FResponseChannel>& InChannelResponses)
{
#if DO_GUARD_SLOW
	// verify if the name is overlapping, if so, ensure, do not remove in debug because it will cause inconsistent bug between debug/release
	int32 const ResponseNum = InChannelResponses.Num();
	for (int32 I=0; I<ResponseNum; ++I)
	{
		for (int32 J=I+1; J<ResponseNum; ++J)
		{
			if (InChannelResponses[I].Channel == InChannelResponses[J].Channel)
			{
				UE_LOG(LogCollision, Warning, TEXT("Collision Channel : Redundant name exists"));
			}
		}
	}
#endif

	ResponseArray = InChannelResponses;
	UpdateResponseContainerFromArray();
}

#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
bool FCollisionResponse::RemoveReponseFromArray(ECollisionChannel Channel)
{
	// this is expensive operation, I'd love to remove names but this operation is supposed to do
	// so only allow it in editor
	// without editor, this does not have to match 
	// We'd need to save name just in case that name is gone or not
	FName ChannelName = UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(Channel);
	for (auto Iter=ResponseArray.CreateIterator(); Iter; ++Iter)
	{
		if (ChannelName == (*Iter).Channel)
		{
			ResponseArray.RemoveAt(Iter.GetIndex());
			return true;
		}
	}
	return false;
}

bool FCollisionResponse::AddReponseToArray(ECollisionChannel Channel, ECollisionResponse Response)
{
	// this is expensive operation, I'd love to remove names but this operation is supposed to do
	// so only allow it in editor
	// without editor, this does not have to match 
	FName ChannelName = UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(Channel);
	for (auto Iter=ResponseArray.CreateIterator(); Iter; ++Iter)
	{
		if (ChannelName == (*Iter).Channel)
		{
			(*Iter).Response = Response;
			return true;
		}
	}

	// if not add to the list
	ResponseArray.Add(FResponseChannel(ChannelName, Response));
	return true;
}

void FCollisionResponse::UpdateArrayFromResponseContainer()
{
	ResponseArray.Empty(ARRAY_COUNT(ResponseToChannels.EnumArray));

	const FCollisionResponseContainer& DefaultResponse = FCollisionResponseContainer::GetDefaultResponseContainer();
	const UCollisionProfile* CollisionProfile = UCollisionProfile::Get();

	for (int32 i = 0; i < ARRAY_COUNT(ResponseToChannels.EnumArray); i++)
	{
		// if not same as default
		if (ResponseToChannels.EnumArray[i] != DefaultResponse.EnumArray[i])
		{
			FName ChannelName = CollisionProfile->ReturnChannelNameFromContainerIndex(i);
			if (ChannelName != NAME_None)
			{
				FResponseChannel NewResponse;
				NewResponse.Channel = ChannelName;
				NewResponse.Response = (ECollisionResponse)ResponseToChannels.EnumArray[i];
				ResponseArray.Add(NewResponse);
			}
		}
	}
}

#endif // WITH_EDITOR

void FCollisionResponse::UpdateResponseContainerFromArray()
{
	ResponseToChannels = FCollisionResponseContainer::GetDefaultResponseContainer();

	for (auto Iter = ResponseArray.CreateIterator(); Iter; ++Iter)
	{
		FResponseChannel& Response = *Iter;

		int32 EnumIndex = UCollisionProfile::Get()->ReturnContainerIndexFromChannelName(Response.Channel);
		if ( EnumIndex != INDEX_NONE )
		{
			ResponseToChannels.SetResponse((ECollisionChannel)EnumIndex, Response.Response);
		}
		else
		{
			// otherwise remove
			ResponseArray.RemoveAt(Iter.GetIndex());
			--Iter;
		}
	}
}

bool FCollisionResponse::operator==(const FCollisionResponse& Other) const
{
	bool bCollisionResponseEqual = ResponseArray.Num() == Other.ResponseArray.Num();
	if(bCollisionResponseEqual)
	{
		for(int32 ResponseIdx = 0; ResponseIdx < ResponseArray.Num(); ++ResponseIdx)
		{
			for(int32 InternalIdx = 0; InternalIdx < ResponseArray.Num(); ++InternalIdx)
			{
				if(ResponseArray[ResponseIdx].Channel == Other.ResponseArray[InternalIdx].Channel)
				{
					bCollisionResponseEqual &= ResponseArray[ResponseIdx] == Other.ResponseArray[InternalIdx];
					break;
				}
			}
			
		}
	}

	return bCollisionResponseEqual;
}
////////////////////////////////////////////////////////////////////////////


FBodyInstance::FBodyInstance()
	: InstanceBodyIndex(INDEX_NONE)
	, InstanceBoneIndex(INDEX_NONE)
	, ObjectType(ECC_WorldStatic)
	, MaskFilter(0)
	, CollisionEnabled(ECollisionEnabled::QueryAndPhysics)
#if WITH_PHYSX
	, CurrentSceneState(BodyInstanceSceneState::NotAdded)
#endif // WITH_PHYSX
	, SleepFamily(ESleepFamily::Normal)
	, DOFMode(0)
	, bUseCCD(false)
	, bNotifyRigidBodyCollision(false)
	, bSimulatePhysics(false)
	, bOverrideMass(false)
	, bEnableGravity(true)
	, bAutoWeld(false)
	, bStartAwake(true)
	, bGenerateWakeEvents(false)
	, bUpdateMassWhenScaleChanges(false)
	, bLockTranslation(true)
	, bLockRotation(true)
	, bLockXTranslation(false)
	, bLockYTranslation(false)
	, bLockZTranslation(false)
	, bLockXRotation(false)
	, bLockYRotation(false)
	, bLockZRotation(false)
	, bOverrideMaxAngularVelocity(false)
	, bUseAsyncScene(false)
	, bOverrideMaxDepenetrationVelocity(false)
	, bOverrideWalkableSlopeOnInstance(false)
	, bInterpolateWhenSubStepping(true)
	, bPendingCollisionProfileSetup(false)
	, bHasSharedShapes(false)
	, Scale3D(1.0f)
	, CollisionProfileName(UCollisionProfile::CustomCollisionProfileName)
	, MaxDepenetrationVelocity(0.f)
	, MassInKgOverride(100.f)
	, ExternalCollisionProfileBodySetup(nullptr)
	, LinearDamping(0.01)
	, AngularDamping(0.0)
	, CustomDOFPlaneNormal(FVector::ZeroVector)
	, COMNudge(ForceInit)
	, MassScale(1.f)
	, InertiaTensorScale(1.f)
	, DOFConstraint(nullptr)
	, WeldParent(nullptr)
	, PhysMaterialOverride(nullptr)
	, CustomSleepThresholdMultiplier(1.f)
	, StabilizationThresholdMultiplier(1.f)
	, PhysicsBlendWeight(0.f)
	, PositionSolverIterationCount(8)
	, VelocitySolverIterationCount(1)
{
	MaxAngularVelocity = UPhysicsSettings::Get()->MaxAngularVelocity;
}

const FPhysicsActorHandle& FBodyInstance::GetActorReferenceWithWelding() const
{
	return WeldParent ? WeldParent->ActorHandle : ActorHandle;
}

FArchive& operator<<(FArchive& Ar,FBodyInstance& BodyInst)
{
	if (!Ar.IsLoading() && !Ar.IsSaving())
	{
		Ar << BodyInst.OwnerComponent;
		Ar << BodyInst.PhysMaterialOverride;
	}

	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_MAX_ANGULAR_VELOCITY_DEFAULT)
	{
		if(BodyInst.MaxAngularVelocity != 400.f)
		{
			BodyInst.bOverrideMaxAngularVelocity = true;
		}
	}

	return Ar;
}


/** Util for finding the parent bodyinstance of a specified body, using skeleton hierarchy */
FBodyInstance* FindParentBodyInstance(FName BodyName, USkeletalMeshComponent* SkelMeshComp)
{
	FName TestBoneName = BodyName;
	while(true)
	{
		TestBoneName = SkelMeshComp->GetParentBone(TestBoneName);
		// Bail out if parent bone not found
		if(TestBoneName == NAME_None)
		{
			return NULL;
		}

		// See if we have a body for the parent bone
		FBodyInstance* BI = SkelMeshComp->GetBodyInstance(TestBoneName);
		if(BI != NULL)
		{
			// We do - return it
			return BI;
		}

		// Don't repeat if we are already at the root!
		if(SkelMeshComp->GetBoneIndex(TestBoneName) == 0)
		{
			return NULL;
		}
	}

	return NULL;
}

//Determine that the shape is associated with this subbody (or root body)
bool FBodyInstance::IsShapeBoundToBody(const FPhysicsShapeHandle& Shape) const
{
	const FBodyInstance* BI = GetOriginalBodyInstance(Shape);
	return BI == this;
}

const TMap<FPhysicsShapeHandle, FBodyInstance::FWeldInfo>* FBodyInstance::GetCurrentWeldInfo() const
{
	return ShapeToBodiesMap.Get();
	}

int32 FBodyInstance::GetAllShapes_AssumesLocked(TArray<FPhysicsShapeHandle>& OutShapes) const
	{
	// If we're sharing shapes we only want to grab from the sync scene, otherwise get everything
	EPhysicsSceneType SceneType = HasSharedShapes() ? PST_Sync : PST_MAX;
	return FPhysicsInterface::GetAllShapes_AssumedLocked(ActorHandle, OutShapes, SceneType);
}

void FBodyInstance::UpdateTriMeshVertices(const TArray<FVector> & NewPositions)
{
#if WITH_APEIRON || WITH_IMMEDIATE_PHYSX
	check(false);
#else
#if WITH_PHYSX
	if (BodySetup.IsValid())
	{
		FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
		{
			BodySetup->UpdateTriMeshVertices(NewPositions);

			//after updating the vertices we must call setGeometry again to update any shapes referencing the mesh
			TArray<FPhysicsShapeHandle> Shapes;
			const int32 SyncShapeCount = GetAllShapes_AssumesLocked(Shapes);

			PxTriangleMeshGeometry PTriangleMeshGeometry;
			for(FPhysicsShapeHandle& Shape : Shapes)
			{
				if(FPhysicsInterface::GetShapeType(Shape) == ECollisionShapeType::Trimesh)
				{
					FPhysicsGeometryCollection GeoCollection = FPhysicsInterface::GetGeometryCollection(Shape);
					GeoCollection.GetTriMeshGeometry(PTriangleMeshGeometry);
					FPhysicsInterface::SetGeometry(Shape, PTriangleMeshGeometry);
				}
			}
		});
	}
#endif
#endif
}

void FBodyInstance::UpdatePhysicalMaterials()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePhysMats);
	UPhysicalMaterial* SimplePhysMat = GetSimplePhysicalMaterial();
	TArray<UPhysicalMaterial*> ComplexPhysMats = GetComplexPhysicalMaterials();

	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		ApplyMaterialToInstanceShapes_AssumesLocked(SimplePhysMat, ComplexPhysMats);
	});
}

void FBodyInstance::InvalidateCollisionProfileName()
{
	CollisionProfileName = UCollisionProfile::CustomCollisionProfileName;
	ExternalCollisionProfileBodySetup = nullptr;
	bPendingCollisionProfileSetup = false;
}

void FBodyInstance::SetResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse)
{
	InvalidateCollisionProfileName();
	CollisionResponses.SetResponse(Channel, NewResponse);
	UpdatePhysicsFilterData();
}

void FBodyInstance::SetResponseToAllChannels(ECollisionResponse NewResponse)
{
	InvalidateCollisionProfileName();
	CollisionResponses.SetAllChannels(NewResponse);
	UpdatePhysicsFilterData();
}
	
void FBodyInstance::ReplaceResponseToChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse)
{
	InvalidateCollisionProfileName();
	CollisionResponses.ReplaceChannels(OldResponse, NewResponse);
	UpdatePhysicsFilterData();
}

void FBodyInstance::SetResponseToChannels(const FCollisionResponseContainer& NewReponses)
{
	InvalidateCollisionProfileName();
	CollisionResponses.SetCollisionResponseContainer(NewReponses);
	UpdatePhysicsFilterData();
}

void FBodyInstance::SetObjectType(ECollisionChannel Channel)
{
	InvalidateCollisionProfileName();
	ObjectType = Channel;
	UpdatePhysicsFilterData();
}

void FBodyInstance::ApplyDeferredCollisionProfileName()
{
	if(bPendingCollisionProfileSetup)
	{
		SetCollisionProfileName(CollisionProfileName);
		bPendingCollisionProfileSetup = false;
	}
}

void FBodyInstance::SetCollisionProfileNameDeferred(FName InCollisionProfileName)
{
	CollisionProfileName = InCollisionProfileName;
	ExternalCollisionProfileBodySetup = nullptr;
	bPendingCollisionProfileSetup = true;
}

void FBodyInstance::SetCollisionProfileName(FName InCollisionProfileName)
{
	SCOPE_CYCLE_COUNTER(STAT_BodyInst_SetCollisionProfileName);

	//Note that GetCollisionProfileName will use the external profile if one is set.
	//GetCollisionProfileName will be consistent with the values set by LoadProfileData.
	//This is why we can't use CollisionProfileName directly during the equality check
	if (bPendingCollisionProfileSetup || GetCollisionProfileName() != InCollisionProfileName)
	{
		//LoadProfileData uses GetCollisionProfileName internally so we must now set the external collision data to null.
		ExternalCollisionProfileBodySetup = nullptr;
		CollisionProfileName = InCollisionProfileName;
		// now Load ProfileData
		LoadProfileData(false);

		bPendingCollisionProfileSetup = false;
	}
	
	ExternalCollisionProfileBodySetup = nullptr;	//Even if incoming is the same as GetCollisionProfileName we turn it into "manual mode"
}

FName FBodyInstance::GetCollisionProfileName() const
{
	FName ReturnProfileName = CollisionProfileName;
	if (UBodySetup* BodySetupPtr = ExternalCollisionProfileBodySetup.Get(true))
	{
		ReturnProfileName = BodySetupPtr->DefaultInstance.CollisionProfileName;
	}
	
	return ReturnProfileName;
}


bool FBodyInstance::DoesUseCollisionProfile() const
{
	return IsValidCollisionProfileName(GetCollisionProfileName());
}

void FBodyInstance::SetMassScale(float InMassScale)
{
	MassScale = InMassScale;
	UpdateMassProperties();
}

void FBodyInstance::SetCollisionEnabled(ECollisionEnabled::Type NewType, bool bUpdatePhysicsFilterData)
{
	if (CollisionEnabled != NewType)
	{
		ECollisionEnabled::Type OldType = CollisionEnabled;
		InvalidateCollisionProfileName();
		CollisionEnabled = NewType;
		
		// Only update physics filter data if required
		if (bUpdatePhysicsFilterData)
		{
			UpdatePhysicsFilterData();
		}

		bool bWasPhysicsEnabled = CollisionEnabledHasPhysics(OldType);
		bool bIsPhysicsEnabled = CollisionEnabledHasPhysics(NewType);

		// Whenever we change physics state, call Recreate
		// This should also handle destroying the state (in case it's newly disabled).
		if (bWasPhysicsEnabled != bIsPhysicsEnabled)
		{
			if(UPrimitiveComponent* PrimComponent = OwnerComponent.Get())
			{
				PrimComponent->RecreatePhysicsState();
			}

		}

	}
}

EDOFMode::Type FBodyInstance::ResolveDOFMode(EDOFMode::Type DOFMode)
{
	EDOFMode::Type ResultDOF = DOFMode;
	if (DOFMode == EDOFMode::Default)
	{
		ESettingsDOF::Type SettingDefaultPlane = UPhysicsSettings::Get()->DefaultDegreesOfFreedom;
		if (SettingDefaultPlane == ESettingsDOF::XYPlane) ResultDOF = EDOFMode::XYPlane;
		if (SettingDefaultPlane == ESettingsDOF::XZPlane) ResultDOF = EDOFMode::XZPlane;
		if (SettingDefaultPlane == ESettingsDOF::YZPlane) ResultDOF = EDOFMode::YZPlane;
		if (SettingDefaultPlane == ESettingsDOF::Full3D) ResultDOF  = EDOFMode::SixDOF;
	}

	return ResultDOF;
}

FVector FBodyInstance::GetLockedAxis() const
{
	EDOFMode::Type MyDOFMode = ResolveDOFMode(DOFMode);

	switch (MyDOFMode)
	{
	case EDOFMode::None: return FVector::ZeroVector;
	case EDOFMode::YZPlane: return FVector(1, 0, 0);
	case EDOFMode::XZPlane: return FVector(0, 1, 0);
	case EDOFMode::XYPlane: return FVector(0, 0, 1);
	case EDOFMode::CustomPlane: return CustomDOFPlaneNormal;
	case EDOFMode::SixDOF: return FVector::ZeroVector;
	default:	check(0);	//unsupported locked axis type
	}

	return FVector::ZeroVector;
}

void FBodyInstance::UseExternalCollisionProfile(UBodySetup* InExternalCollisionProfileBodySetup)
{
	ensureAlways(InExternalCollisionProfileBodySetup);
	ExternalCollisionProfileBodySetup = InExternalCollisionProfileBodySetup;
	bPendingCollisionProfileSetup = false;
	LoadProfileData(false);
}

void FBodyInstance::ClearExternalCollisionProfile()
{
	ExternalCollisionProfileBodySetup = nullptr;
	LoadProfileData(false);
}

void FBodyInstance::SetDOFLock(EDOFMode::Type NewAxisMode)
{
	DOFMode = NewAxisMode;

	CreateDOFLock();
}

void FBodyInstance::CreateDOFLock()
{
	if (DOFConstraint)
	{
		DOFConstraint->TermConstraint();
		FConstraintInstance::Free(DOFConstraint);
		DOFConstraint = NULL;
	}

	const FVector LockedAxis = GetLockedAxis();
	const EDOFMode::Type DOF = ResolveDOFMode(DOFMode);

	if (IsDynamic() == false || (LockedAxis.IsNearlyZero() && DOF != EDOFMode::SixDOF))
	{
		return;
	}

	//if we're using SixDOF make sure we have at least one constraint
	if (DOF == EDOFMode::SixDOF && !bLockXTranslation && !bLockYTranslation && !bLockZTranslation && !bLockXRotation && !bLockYRotation && !bLockZRotation)
	{
		return;
	}

	DOFConstraint = FConstraintInstance::Alloc();
	{
		DOFConstraint->ProfileInstance.ConeLimit.bSoftConstraint = false;
		DOFConstraint->ProfileInstance.TwistLimit.bSoftConstraint  = false;
		DOFConstraint->ProfileInstance.LinearLimit.bSoftConstraint  = false;

		const FTransform TM = GetUnrealWorldTransform(false);
		FVector Normal = FVector(1, 0, 0);
		FVector Sec = FVector(0, 1, 0);


		if(DOF != EDOFMode::SixDOF)
		{
			DOFConstraint->SetAngularSwing1Motion((bLockRotation || DOFMode != EDOFMode::CustomPlane) ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free);
			DOFConstraint->SetAngularSwing2Motion((bLockRotation || DOFMode != EDOFMode::CustomPlane) ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free);
			DOFConstraint->SetAngularTwistMotion(EAngularConstraintMotion::ACM_Free);
			
			DOFConstraint->SetLinearXMotion((bLockTranslation || DOFMode != EDOFMode::CustomPlane) ? ELinearConstraintMotion::LCM_Locked : ELinearConstraintMotion::LCM_Free);
			DOFConstraint->SetLinearYMotion(ELinearConstraintMotion::LCM_Free);
			DOFConstraint->SetLinearZMotion(ELinearConstraintMotion::LCM_Free);

			Normal = LockedAxis.GetSafeNormal();
			FVector Garbage;
			Normal.FindBestAxisVectors(Garbage, Sec);
		}
		else
		{
			DOFConstraint->SetAngularTwistMotion(bLockXRotation ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free);
			DOFConstraint->SetAngularSwing2Motion(bLockYRotation ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free);
			DOFConstraint->SetAngularSwing1Motion(bLockZRotation ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free);

			DOFConstraint->SetLinearXMotion(bLockXTranslation ? ELinearConstraintMotion::LCM_Locked : ELinearConstraintMotion::LCM_Free);
			DOFConstraint->SetLinearYMotion(bLockYTranslation ? ELinearConstraintMotion::LCM_Locked : ELinearConstraintMotion::LCM_Free);
			DOFConstraint->SetLinearZMotion(bLockZTranslation ? ELinearConstraintMotion::LCM_Locked : ELinearConstraintMotion::LCM_Free);
		}

		DOFConstraint->PriAxis1 = TM.InverseTransformVectorNoScale(Normal);
		DOFConstraint->SecAxis1 = TM.InverseTransformVectorNoScale(Sec);

		DOFConstraint->PriAxis2 = Normal;
		DOFConstraint->SecAxis2 = Sec;
		DOFConstraint->Pos2 = TM.GetLocation();

		// Create constraint instance based on DOF
		DOFConstraint->InitConstraint(this, nullptr, 1.f, OwnerComponent.Get());
	}
}

ECollisionEnabled::Type FBodyInstance::GetCollisionEnabled_CheckOwner() const
{
	// Check actor override
	if (const UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get())
	{
		AActor* Owner = OwnerComponentInst->GetOwner();
		if (Owner && !Owner->GetActorEnableCollision())
		{
			return ECollisionEnabled::NoCollision;
		}
		else if(const USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(OwnerComponentInst))
{
			// Check component override (skel mesh case)
			return SkelMeshComp->BodyInstance.CollisionEnabled;
}
	}

	return CollisionEnabled;
			}

void FBodyInstance::SetMaskFilter(FMaskFilter InMaskFilter)
{
	if (MaskFilter == InMaskFilter)
	{
		return;
	}

	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		TArray<FPhysicsShapeHandle> Shapes;
		FPhysicsInterface::GetAllShapes_AssumedLocked(Actor, Shapes);

		for(FPhysicsShapeHandle& Shape : Shapes)
		{
			const FBodyInstance* BI = GetOriginalBodyInstance(Shape);

			if(BI == this)
				{
				FPhysicsCommand::ExecuteShapeWrite(this, Shape, [&](const FPhysicsShapeHandle& InnerShape)
					{
					FPhysicsInterface::SetMaskFilter(InnerShape, InMaskFilter);
					});
				}
			}
	});

	MaskFilter = InMaskFilter;
}

/** Update the filter data on the physics shapes, based on the owning component flags. */
void FBodyInstance::UpdatePhysicsFilterData()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePhysFilter);

	if(WeldParent)
	{
		WeldParent->UpdatePhysicsFilterData();
		return;
	}

	// Do nothing if no physics actor
	if (!IsValidBodyInstance())
	{
		return;
	}

	// this can happen in landscape height field collision component
	if (!BodySetup.IsValid())
	{
		return;
	}

	FPhysicsCommand::ExecuteWrite(GetActorReferenceWithWelding(), [&](const FPhysicsActorHandle& Actor)
	{
		TArray<FPhysicsShapeHandle> AllShapes;
		const int32 NumSyncShapes = FPhysicsInterface::GetAllShapes_AssumedLocked(ActorHandle, AllShapes);
		const int32 NumTotalShapes = AllShapes.Num();
		// In skeletal case, collision enable/disable/movement should be overriden by mesh component
		FBodyCollisionData BodyCollisionData;
		BuildBodyFilterData(BodyCollisionData.CollisionFilterData);
		BuildBodyCollisionFlags(BodyCollisionData.CollisionFlags, GetCollisionEnabled(), BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple);

		bool bUpdateMassProperties = false;

		for(int32 ShapeIndex = 0; ShapeIndex < NumTotalShapes; ++ShapeIndex)
		{
			FPhysicsShapeHandle& Shape = AllShapes[ShapeIndex];
			const FBodyInstance* BI = GetOriginalBodyInstance(Shape);

			// If the BodyInstance that owns this shape is not 'this' BodyInstance (ie in the case of welding)
			// we need to generate new filter data using the owning original instance (and its BodySetup) 
			FBodyCollisionData PerShapeCollisionData;
			if(BI != this)
	{
				BI->BuildBodyFilterData(PerShapeCollisionData.CollisionFilterData);

				const bool bInstanceComplexAsSimple = BI->BodySetup.IsValid() ? (BI->BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple) : false;
				BuildBodyCollisionFlags(PerShapeCollisionData.CollisionFlags, BI->GetCollisionEnabled(), bInstanceComplexAsSimple);
			}
			else
			{
				PerShapeCollisionData = BodyCollisionData;
		}

			FPhysicsCommand::ExecuteShapeWrite(this, Shape, [&](const FPhysicsShapeHandle& InnerShape)
			{
				// See if we currently have sim collision
				const bool bWasSimulationShape = FPhysicsInterface::IsSimulationShape(InnerShape);
				const bool bSyncShape = ShapeIndex < NumSyncShapes;
				const bool bIsTrimesh = FPhysicsInterface::IsShapeType(InnerShape, ECollisionShapeType::Trimesh);
				const bool bIsStatic = FPhysicsInterface::IsStatic(Actor);

				const FBodyCollisionFlags& CollisionFlags = PerShapeCollisionData.CollisionFlags;
				const FBodyCollisionFilterData& FilterData = PerShapeCollisionData.CollisionFilterData;
				const bool bNewQueryShape = CollisionFlags.bEnableQueryCollision && (!bIsStatic || bSyncShape);
				const bool bNewSimShape = bIsTrimesh ? CollisionFlags.bEnableSimCollisionComplex : CollisionFlags.bEnableSimCollisionSimple;

				FPhysicsInterface::SetIsQueryShape(InnerShape, bNewQueryShape);
				FPhysicsInterface::SetIsSimulationShape(InnerShape, bNewSimShape);

				// If we changed 'simulation collision' on a shape, we need to recalc mass properties
				if(bWasSimulationShape != bNewSimShape)
		{
					bUpdateMassProperties = true;
		}

				// Apply new collision settings to this shape
				FPhysicsInterface::SetSimulationFilter(InnerShape, FilterData.SimFilter);
				FPhysicsInterface::SetQueryFilter(InnerShape, bIsTrimesh ? FilterData.QueryComplexFilter : FilterData.QuerySimpleFilter);
			});
	}

		if(bUpdateMassProperties)
	{
			UpdateMassProperties();
		}
	});

	UpdateInterpolateWhenSubStepping();
}

TAutoConsoleVariable<int32> CDisableQueryOnlyActors(TEXT("p.DisableQueryOnlyActors"), 0, TEXT("If QueryOnly is used, actors are marked as simulation disabled. This is NOT compatible with origin shifting at the moment."));

TSharedPtr<TArray<ANSICHAR>> GetDebugDebugName(const UPrimitiveComponent* PrimitiveComp, const UBodySetup* BodySetup, FString& DebugName)
{
	// Setup names
	// Make the debug name for this geometry...
	DebugName = (TEXT(""));
	TSharedPtr<TArray<ANSICHAR>> PhysXName;

#if (WITH_EDITORONLY_DATA || UE_BUILD_DEBUG || LOOKING_FOR_PERF_ISSUES) && !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && !NO_LOGGING
	if (PrimitiveComp)
	{
		DebugName += FString::Printf(TEXT("Component: '%s' "), *PrimitiveComp->GetPathName());
	}

	if (BodySetup->BoneName != NAME_None)
	{
		DebugName += FString::Printf(TEXT("Bone: '%s' "), *BodySetup->BoneName.ToString());
	}

	// Convert to char* for PhysX
	PhysXName = MakeShareable(new TArray<ANSICHAR>(StringToArray<ANSICHAR>(*DebugName, DebugName.Len() + 1)));
#endif

	return PhysXName;
}

void GetSimulatingAndBlendWeight(const USkeletalMeshComponent* SkelMeshComp, const UBodySetup* BodySetup, float& InstanceBlendWeight, bool& bInstanceSimulatePhysics)
{
	bool bEnableSim = false;
	if (SkelMeshComp)
	{
		if(CollisionEnabledHasPhysics(SkelMeshComp->BodyInstance.GetCollisionEnabled()))
		{
			if ((BodySetup->PhysicsType == PhysType_Simulated) || (BodySetup->PhysicsType == PhysType_Default))
			{
				bEnableSim = (SkelMeshComp && IsRunningDedicatedServer()) ? SkelMeshComp->bEnablePhysicsOnDedicatedServer : true;
				bEnableSim &= ((BodySetup->PhysicsType == PhysType_Simulated) || (SkelMeshComp->BodyInstance.bSimulatePhysics));	//if unfixed enable. If default look at parent
			}
		}
	}
	else
	{
		//not a skeletal mesh so don't bother with default and skeletal mesh component
		bEnableSim = BodySetup->PhysicsType == PhysType_Simulated;
	}

	if (bEnableSim)
	{
		// set simulate to true if using physics
		bInstanceSimulatePhysics = true;
		if (BodySetup->PhysicsType == PhysType_Simulated)
		{
			InstanceBlendWeight = 1.f;
		}
	}
	else
	{
		bInstanceSimulatePhysics = false;
		if (BodySetup->PhysicsType == PhysType_Simulated)
		{
			InstanceBlendWeight = 0.f;
		}
	}
}

struct FPhysicsActorCreatePayload_PhysX
{
	bool bUseAsync;

};

template <bool bCompileStatic>
struct FInitBodiesHelper
{
	FInitBodiesHelper(TArray<FBodyInstance*>& InBodies, TArray<FTransform>& InTransforms, class UBodySetup* InBodySetup, class UPrimitiveComponent* InPrimitiveComp, FPhysScene* InRBScene, const FBodyInstance::FInitBodySpawnParams& InSpawnParams, FPhysicsAggregateHandle InAggregate)
	: Bodies(InBodies)
	, Transforms(InTransforms)
	, BodySetup(InBodySetup)
	, PrimitiveComp(InPrimitiveComp)
		, PhysScene(InRBScene)
		, Aggregate(InAggregate)
	, DebugName(TEXT(""))
	, InstanceBlendWeight(-1.f)
	, bInstanceSimulatePhysics(false)
	, SkelMeshComp(nullptr)
	, SpawnParams(InSpawnParams)
	{
		//Compute all the needed constants

		PhysXName = GetDebugDebugName(PrimitiveComp, BodySetup, DebugName);

		bStatic = bCompileStatic || SpawnParams.bStaticPhysics;
		SkelMeshComp = bCompileStatic ? nullptr : Cast<USkeletalMeshComponent>(PrimitiveComp);
		if(SpawnParams.bPhysicsTypeDeterminesSimulation)
		{
			GetSimulatingAndBlendWeight(SkelMeshComp, BodySetup, InstanceBlendWeight, bInstanceSimulatePhysics);
		}
		
	}

	FORCEINLINE bool IsStatic() const { return bCompileStatic || bStatic; }

	//The arguments passed into InitBodies
	TArray<FBodyInstance*>& Bodies;   
	TArray<FTransform>& Transforms;
	class UBodySetup* BodySetup;
	class UPrimitiveComponent* PrimitiveComp;
	FPhysScene* PhysScene;
	FPhysicsAggregateHandle Aggregate;

	FString DebugName;
	TSharedPtr<TArray<ANSICHAR>> PhysXName; // Get rid of ANSICHAR in physics

	//The constants shared between PhysX and Box2D
	bool bStatic;
	float InstanceBlendWeight;
	bool bInstanceSimulatePhysics;

	const USkeletalMeshComponent* SkelMeshComp;

	const FBodyInstance::FInitBodySpawnParams& SpawnParams;

	// Return to actor ref
	void CreateActor_AssumesLocked(FBodyInstance* Instance, const FTransform& Transform) const
	{
		checkSlow(!Instance->ActorHandle.IsValid());
		const ECollisionEnabled::Type CollisionType = Instance->GetCollisionEnabled();
		const bool bDisableSim = !CollisionEnabledHasPhysics(CollisionType) && CDisableQueryOnlyActors.GetValueOnGameThread();

		FActorCreationParams ActorParams;
		ActorParams.InitialTM = Transform;
		ActorParams.DebugName = Instance->CharDebugName.IsValid() ? Instance->CharDebugName->GetData() : nullptr;
		ActorParams.bEnableGravity = Instance->bEnableGravity;
		ActorParams.bQueryOnly = bDisableSim;
		ActorParams.Scene = PhysScene;

		if (IsStatic())
		{
			ActorParams.bStatic = true;

			Instance->ActorHandle = FPhysicsInterface::CreateActor(ActorParams);
		}
		else
		{
			if(SpawnParams.DynamicActorScene == EDynamicActorScene::Default)
			{
				ActorParams.bUseAsyncScene = Instance->bUseAsyncScene;
			}
			else
			{
				ActorParams.bUseAsyncScene = (SpawnParams.DynamicActorScene == EDynamicActorScene::UseAsyncScene);
			}

			Instance->ActorHandle = FPhysicsInterface::CreateActor(ActorParams);

			FPhysicsInterface::SetCcdEnabled_AssumesLocked(Instance->ActorHandle, Instance->bUseCCD);
			FPhysicsInterface::SetIsKinematic_AssumesLocked(Instance->ActorHandle, !Instance->ShouldInstanceSimulatingPhysics());

			// Set sleep even notification
			FPhysicsInterface::SetSendsSleepNotifies_AssumesLocked(Instance->ActorHandle, Instance->bGenerateWakeEvents);
		}
	}

	bool CreateShapes_AssumesLocked(FBodyInstance* Instance) const
	{
		UPhysicalMaterial* SimplePhysMat = Instance->GetSimplePhysicalMaterial();
		TArray<UPhysicalMaterial*> ComplexPhysMats = Instance->GetComplexPhysicalMaterials();

		FBodyCollisionData BodyCollisionData;
		Instance->BuildBodyFilterData(BodyCollisionData.CollisionFilterData);
		Instance->BuildBodyCollisionFlags(BodyCollisionData.CollisionFlags, Instance->GetCollisionEnabled(), BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple);

		bool bInitFail = false;

		// possibly remove
		const bool bShapeSharing = Instance->HasSharedShapes(); //If we have a static actor we can reuse the shapes between sync and async scene
		TArray<FPhysicsShapeHandle> SharedShapes;

		// #PHYS2 Call interface AddGeometry
		if(FPhysicsInterface::HasSyncSceneData(Instance->ActorHandle))
		{
			BodySetup->AddShapesToRigidActor_AssumesLocked(Instance, PST_Sync, Instance->Scale3D, SimplePhysMat, ComplexPhysMats, BodyCollisionData, FTransform::Identity, bShapeSharing ? &SharedShapes : nullptr, bShapeSharing);

			int32 NumSyncShapes = 0;
			int32 NumAsyncShapes = 0;
			FPhysicsInterface::GetNumShapes(Instance->ActorHandle, NumSyncShapes, NumAsyncShapes);
			bInitFail |= NumSyncShapes == 0;
		}

		if(FPhysicsInterface::HasAsyncSceneData(Instance->ActorHandle))
		{
			if (bShapeSharing)
			{
				for(FPhysicsShapeHandle& PShape : SharedShapes)
				{
					FPhysicsInterface::AttachShape(Instance->ActorHandle, PShape, PST_Async);
				}
			}
			else
			{
				BodySetup->AddShapesToRigidActor_AssumesLocked(Instance, PST_Async, Instance->Scale3D, SimplePhysMat, ComplexPhysMats, BodyCollisionData);
			}

			int32 NumSyncShapes = 0;
			int32 NumAsyncShapes = 0;
			FPhysicsInterface::GetNumShapes(Instance->ActorHandle, NumSyncShapes, NumAsyncShapes);

			bInitFail |= NumAsyncShapes == 0;
		}

		return bInitFail;
	}

	// Takes actor ref arrays.
	// #PHYS2 this used to return arrays of low-level physics bodies, which would be added to scene in InitBodies. Should it still do that, rather then later iterate over BodyInstances to get phys actor refs?
	bool CreateShapesAndActors()
	{
		SCOPE_CYCLE_COUNTER(STAT_CreatePhysicsShapesAndActors);

		const int32 NumBodies = Bodies.Num();

		// Ensure we have the AggGeom inside the body setup so we can calculate the number of shapes
		BodySetup->CreatePhysicsMeshes();

		for (int32 BodyIdx = NumBodies - 1; BodyIdx >= 0; BodyIdx--)   // iterate in reverse since list might shrink
		{
			FBodyInstance* Instance = Bodies[BodyIdx];
			const FTransform& Transform = Transforms[BodyIdx];

			FBodyInstance::ValidateTransform(Transform, DebugName, BodySetup);

			Instance->OwnerComponent = PrimitiveComp;
			Instance->BodySetup = BodySetup;
			Instance->Scale3D = Transform.GetScale3D();
			Instance->CharDebugName = PhysXName;
			Instance->bHasSharedShapes = IsStatic() && PhysScene && PhysScene->HasAsyncScene() && UPhysicsSettings::Get()->bEnableShapeSharing;
			Instance->bEnableGravity = Instance->bEnableGravity && (SkelMeshComp ? SkelMeshComp->BodyInstance.bEnableGravity : true);	//In the case of skeletal mesh component we AND bodies with the parent body

			// Handle autowelding here to avoid extra work
			if (!IsStatic() && Instance->bAutoWeld)
			{
				ECollisionEnabled::Type CollisionType = Instance->GetCollisionEnabled();
				if (CollisionType != ECollisionEnabled::QueryOnly)
				{
					if (UPrimitiveComponent * ParentPrimComponent = PrimitiveComp ? Cast<UPrimitiveComponent>(PrimitiveComp->GetAttachParent()) : NULL)
					{
						UWorld* World = PrimitiveComp->GetWorld();
						if (World && World->IsGameWorld())
						{
							//if we have a parent we will now do the weld and exit any further initialization
							if (PrimitiveComp->WeldToImplementation(ParentPrimComponent, PrimitiveComp->GetAttachSocketName(), false))	//welded new simulated body so initialization is done
							{
								return false;
							}
						}
					}
				}
			}

			// Don't process if we've already got a body
			// Just ask actorref
			if(Instance->GetPhysicsActorHandle().IsValid())
			{
				Instance->OwnerComponent = nullptr;
				Instance->BodySetup      = nullptr;
				Bodies.RemoveAt(BodyIdx);  // so we wont add it to the physx scene again later.
				Transforms.RemoveAt(BodyIdx);
				continue;
			}

			// Set sim parameters for bodies from skeletal mesh components
			if (!IsStatic() && SpawnParams.bPhysicsTypeDeterminesSimulation)
			{
				Instance->bSimulatePhysics = bInstanceSimulatePhysics;
				if (InstanceBlendWeight != -1.0f)
				{
					Instance->PhysicsBlendWeight = InstanceBlendWeight;
				}
			}

			// Init user data structure to point back at this instance
			Instance->PhysxUserData = FPhysxUserData(Instance);

			CreateActor_AssumesLocked(Instance, Transform);
			const bool bInitFail = CreateShapes_AssumesLocked(Instance);
			if (bInitFail)
			{
#if WITH_EDITOR
				//In the editor we may have ended up here because of world trace ignoring our EnableCollision. Since we can't get at the data in that function we check for it here
				if(!PrimitiveComp || PrimitiveComp->IsCollisionEnabled())
#endif
				{
					UE_LOG(LogPhysics, Log, TEXT("Init Instance %d of Primitive Component %s failed. Does it have collision data available?"), BodyIdx, *PrimitiveComp->GetReadableName());
				}

				FPhysicsInterface::ReleaseActor(Instance->ActorHandle, PhysScene);

				Instance->OwnerComponent = nullptr;
				Instance->BodySetup = nullptr;
				Instance->ExternalCollisionProfileBodySetup = nullptr;

				continue;
			}

			FPhysicsInterface::SetActorUserData_AssumesLocked(Instance->ActorHandle, &Instance->PhysxUserData);
			}

		return true;
			}


	void InitBodies()
				{
		LLM_SCOPE(ELLMTag::PhysX);

		check(IsInGameThread());

		if(CreateShapesAndActors())
					{
			FPhysicsCommand::ExecuteWrite(PhysScene, [&]()
	{
				// If an aggregate present, add to that
				if(Aggregate.IsValid())
		{
					for(FBodyInstance* BI : Bodies)
			{
						const FPhysicsActorHandle& ActorHandle = BI->GetPhysicsActorHandle();
						if(ActorHandle.IsValid())
				{
							FPhysicsInterface::AddActorToAggregate_AssumesLocked(Aggregate, ActorHandle);
				}
			}
		}
				else if(PhysScene)
		{
					TArray<FPhysicsActorHandle> ActorHandles;
					ActorHandles.Reserve(Bodies.Num());

					for(FBodyInstance* BI : Bodies)
			{
						const FPhysicsActorHandle& ActorHandle = BI->GetPhysicsActorHandle();
						if(ActorHandle.IsValid())
			{
							ActorHandles.Add(ActorHandle);
			}
		}

					PhysScene->AddActorsToScene_AssumesLocked(ActorHandles);
				}
				
				// Set up dynamic instance data
				if (!IsStatic())
				{
					SCOPE_CYCLE_COUNTER(STAT_InitBodyPostAdd);
					for (int32 BodyIdx = 0, NumBodies = Bodies.Num(); BodyIdx < NumBodies; ++BodyIdx)
					{
						FBodyInstance* Instance = Bodies[BodyIdx];
						Instance->InitDynamicProperties_AssumesLocked();
					}
				}
			});
			}
	}
};

FBodyInstance::FInitBodySpawnParams::FInitBodySpawnParams(const UPrimitiveComponent* PrimComp)
{
	bStaticPhysics = PrimComp == nullptr || PrimComp->Mobility != EComponentMobility::Movable;
	DynamicActorScene = EDynamicActorScene::Default;

	if(const USkeletalMeshComponent* SKOwner = Cast<USkeletalMeshComponent>(PrimComp))
	{
		bPhysicsTypeDeterminesSimulation = true;
	}
	else
	{
		bPhysicsTypeDeterminesSimulation = false;
	}
}

void FBodyInstance::InitBody(class UBodySetup* Setup, const FTransform& Transform, UPrimitiveComponent* PrimComp, FPhysScene* InRBScene, const FInitBodySpawnParams& SpawnParams)
{
	SCOPE_CYCLE_COUNTER(STAT_InitBody);
	check(Setup);
	
	static TArray<FBodyInstance*> Bodies;
	static TArray<FTransform> Transforms;

	check(Bodies.Num() == 0);
	check(Transforms.Num() == 0);

	Bodies.Add(this);
	Transforms.Add(Transform);

	bool bIsStatic = SpawnParams.bStaticPhysics;
	if(bIsStatic)
	{
		FInitBodiesHelper<true> InitBodiesHelper(Bodies, Transforms, Setup, PrimComp, InRBScene, SpawnParams, SpawnParams.Aggregate);
		InitBodiesHelper.InitBodies();
	}
	else
	{
		FInitBodiesHelper<false> InitBodiesHelper(Bodies, Transforms, Setup, PrimComp, InRBScene, SpawnParams, SpawnParams.Aggregate);
		InitBodiesHelper.InitBodies();
	}

	Bodies.Reset();
	Transforms.Reset();

	UpdateInterpolateWhenSubStepping();
}

FVector GetInitialLinearVelocity(const AActor* OwningActor, bool& bComponentAwake)
{
	FVector InitialLinVel(ForceInitToZero);
	if (OwningActor)
	{
		InitialLinVel = OwningActor->GetVelocity();

		if (InitialLinVel.SizeSquared() > FMath::Square(KINDA_SMALL_NUMBER))
		{
			bComponentAwake = true;
		}
	}

	return InitialLinVel;
}


const FBodyInstance* FBodyInstance::GetOriginalBodyInstance(const FPhysicsShapeHandle& InShape) const
{
	const FBodyInstance* BI = WeldParent ? WeldParent : this;
	const FWeldInfo* Result = BI->ShapeToBodiesMap.IsValid() ? BI->ShapeToBodiesMap->Find(InShape) : nullptr;
	return Result ? Result->ChildBI : BI;
}

const FTransform& FBodyInstance::GetRelativeBodyTransform(const FPhysicsShapeHandle& InShape) const
{
	check(IsInGameThread());
	const FBodyInstance* BI = WeldParent ? WeldParent : this;
	const FWeldInfo* Result = BI->ShapeToBodiesMap.IsValid() ? BI->ShapeToBodiesMap->Find(InShape) : nullptr;
	return Result ? Result->RelativeTM : FTransform::Identity;
}

/**
 *	Clean up the physics engine info for this instance.
 */
void FBodyInstance::TermBody(bool bNeverDeferRelease)
{
	SCOPE_CYCLE_COUNTER(STAT_TermBody);

	FPhysicsInterface::ReleaseActor(ActorHandle, GetPhysicsScene(), bNeverDeferRelease);

	// @TODO UE4: Release spring body here

	CurrentSceneState = BodyInstanceSceneState::NotAdded;
	BodySetup = NULL;
	OwnerComponent = NULL;
	ExternalCollisionProfileBodySetup = nullptr;

	if (DOFConstraint)
	{
		DOFConstraint->TermConstraint();
		FConstraintInstance::Free(DOFConstraint);
			DOFConstraint = NULL;
	}
	
}

bool FBodyInstance::Weld(FBodyInstance* TheirBody, const FTransform& TheirTM)
{
	check(IsInGameThread());
	check(TheirBody);
	if (TheirBody->BodySetup.IsValid() == false)	//attach actor can be called before body has been initialized. In this case just return false
	{
		return false;
	}

    if (TheirBody->WeldParent == this) // The body is already welded to this component. Do nothing.
    {
        return false;
    }

	TArray<FPhysicsShapeHandle> PNewShapes;

	FTransform MyTM = GetUnrealWorldTransform(false);
	MyTM.SetScale3D(Scale3D);	//physx doesn't store 3d so set it here

	FTransform RelativeTM = TheirTM.GetRelativeTransform(MyTM);

	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdatePhysMats);

		TheirBody->WeldParent = this;

		UPhysicalMaterial* SimplePhysMat = TheirBody->GetSimplePhysicalMaterial();
		TArray<UPhysicalMaterial*> ComplexPhysMats = TheirBody->GetComplexPhysicalMaterials();

		// This builds collision data based on this (parent) body, not their body. This gets fixed  up later though when PostShapeChange() calls UpdatePhysicsFilterData().
		FBodyCollisionData BodyCollisionData;
		BuildBodyFilterData(BodyCollisionData.CollisionFilterData);
		BuildBodyCollisionFlags(BodyCollisionData.CollisionFlags, GetCollisionEnabled(), BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple);

		//child body gets placed into the same scenes as parent body
		if(FPhysicsInterface::HasSyncSceneData(Actor))
			{
			TheirBody->BodySetup->AddShapesToRigidActor_AssumesLocked(this, PST_Sync, Scale3D, SimplePhysMat, ComplexPhysMats, BodyCollisionData, RelativeTM, &PNewShapes);
		}

		if(FPhysicsInterface::HasAsyncSceneData(Actor))
			{
			TheirBody->BodySetup->AddShapesToRigidActor_AssumesLocked(this, PST_Async, Scale3D, SimplePhysMat, ComplexPhysMats, BodyCollisionData, RelativeTM, &PNewShapes);
		}

		FPhysicsInterface::SetSendsSleepNotifies_AssumesLocked(Actor, TheirBody->bGenerateWakeEvents);

		if(PNewShapes.Num())
		{
			if(!ShapeToBodiesMap.IsValid())
			{
				ShapeToBodiesMap = TSharedPtr<TMap<FPhysicsShapeHandle, FWeldInfo>>(new TMap<FPhysicsShapeHandle, FWeldInfo>());
			}

			for (int32 ShapeIdx = 0; ShapeIdx < PNewShapes.Num(); ++ShapeIdx)
			{
				ShapeToBodiesMap->Add(PNewShapes[ShapeIdx], FWeldInfo(TheirBody, RelativeTM));
			}

			if(TheirBody->ShapeToBodiesMap.IsValid())
			{
				TSet<FBodyInstance*> Bodies;
				//If the body that is welding to us has things welded to it, make sure to weld those things to us as well
				TMap<FPhysicsShapeHandle, FWeldInfo>& TheirWeldInfo = *TheirBody->ShapeToBodiesMap.Get();
				for(auto Itr = TheirWeldInfo.CreateIterator(); Itr; ++Itr)
				{
					const FWeldInfo& WeldInfo = Itr->Value;
					if(!Bodies.Contains(WeldInfo.ChildBI))
					{
						Bodies.Add(WeldInfo.ChildBI);	//only want to weld once per body and can have multiple shapes
						const FTransform ChildWorldTM = WeldInfo.RelativeTM * TheirTM;
						Weld(WeldInfo.ChildBI, ChildWorldTM);
					}
				}

				TheirWeldInfo.Empty();	//They are no longer root so empty this
			}
		}

		PostShapeChange();

		// remove their body from scenes (don't call TermBody because we don't want to clear things like BodySetup)
		FPhysicsInterface::ReleaseActor(TheirBody->ActorHandle, TheirBody->GetPhysicsScene());
	});
	
	UpdateInterpolateWhenSubStepping();

	TheirBody->UpdateDebugRendering();
	UpdateDebugRendering();

	return true;
}

void FBodyInstance::UnWeld(FBodyInstance* TheirBI)
{
	check(IsInGameThread());

	bool bShapesChanged = false;

	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
				{
		TArray<FPhysicsShapeHandle> Shapes;
		const int32 NumSyncShapes = GetAllShapes_AssumesLocked(Shapes);
		const int32 NumTotalShapes = Shapes.Num();

		for(FPhysicsShapeHandle& Shape : Shapes)
		{
			const FBodyInstance* BI = GetOriginalBodyInstance(Shape);
			if (TheirBI == BI)
			{
				ShapeToBodiesMap->Remove(Shape);
				FPhysicsInterface::DetachShape(Actor, Shape);
				bShapesChanged = true;
			}
		}

	if (bShapesChanged)
	{
		PostShapeChange();
	}

		TheirBI->WeldParent = nullptr;
	});

	UpdateInterpolateWhenSubStepping();

	TheirBI->UpdateDebugRendering();
	UpdateDebugRendering();
}

void FBodyInstance::PostShapeChange()
{
	// Set the filter data on the shapes (call this after setting BodyData because it uses that pointer)
	UpdatePhysicsFilterData();

	UpdateMassProperties();
	// Update damping
	UpdateDampingProperties();
}

float AdjustForSmallThreshold(float NewVal, float OldVal)
{
	float Threshold = 0.1f;
	float Delta = NewVal - OldVal;
	if (Delta < 0 && FMath::Abs(NewVal) < Threshold)	//getting smaller and passed threshold so flip sign
	{
		return -Threshold;
	}
	else if (Delta > 0 && FMath::Abs(NewVal) < Threshold)	//getting bigger and passed small threshold so flip sign
	{
		return Threshold;
	}

	return NewVal;
}

//Non uniform scaling depends on the primitive that has the least non uniform scaling capability. So for example, a capsule's x and y axes scale are locked.
//So if a capsule exists in this body we must use locked x and y scaling for all shapes.
namespace EScaleMode
{
	enum Type
	{
		Free,
		LockedXY,
		LockedXYZ
	};
}

//computes the relative scaling vectors based on scale mode used
void ComputeScalingVectors(EScaleMode::Type ScaleMode, const FVector& InScale3D, FVector& OutScale3D, FVector& OutScale3DAbs)
{
	// Ensure no zeroes in any dimension
	FVector NewScale3D;
	NewScale3D.X = FMath::IsNearlyZero(InScale3D.X) ? KINDA_SMALL_NUMBER : InScale3D.X;
	NewScale3D.Y = FMath::IsNearlyZero(InScale3D.Y) ? KINDA_SMALL_NUMBER : InScale3D.Y;
	NewScale3D.Z = FMath::IsNearlyZero(InScale3D.Z) ? KINDA_SMALL_NUMBER : InScale3D.Z;

	const FVector NewScale3DAbs = NewScale3D.GetAbs();
	switch (ScaleMode)
	{
	case EScaleMode::Free:
	{
		OutScale3D = NewScale3D;
		break;
	}
	case EScaleMode::LockedXY:
	{
		float XYScaleAbs = FMath::Max(NewScale3DAbs.X, NewScale3DAbs.Y);
		float XYScale = FMath::Max(NewScale3D.X, NewScale3D.Y) < 0.f ? -XYScaleAbs : XYScaleAbs;	//if both xy are negative we should make the xy scale negative

		OutScale3D = NewScale3D;
		OutScale3D.X = OutScale3D.Y = XYScale;

		break;
	}
	case EScaleMode::LockedXYZ:
	{
		float UniformScaleAbs = NewScale3DAbs.GetMin();	//uniform scale uses the smallest magnitude
		float UniformScale = FMath::Max3(NewScale3D.X, NewScale3D.Y, NewScale3D.Z) < 0.f ? -UniformScaleAbs : UniformScaleAbs;	//if all three values are negative we should make uniform scale negative

		OutScale3D = FVector(UniformScale);
		break;
	}
	default:
	{
		check(false);	//invalid scale mode
	}
	}

	OutScale3DAbs = OutScale3D.GetAbs();
}

EScaleMode::Type ComputeScaleMode(const TArray<FPhysicsShapeHandle>& Shapes)
{
	EScaleMode::Type ScaleMode = EScaleMode::Free;

	for(int32 ShapeIdx = 0; ShapeIdx < Shapes.Num(); ++ShapeIdx)
	{
		const FPhysicsShapeHandle& Shape = Shapes[ShapeIdx];
		ECollisionShapeType GeomType = FPhysicsInterface::GetShapeType(Shape);

		if(GeomType == ECollisionShapeType::Sphere)
		{
			ScaleMode = EScaleMode::LockedXYZ;	//sphere is most restrictive so we can stop
			break;
		}
		else if(GeomType == ECollisionShapeType::Capsule)
		{
			ScaleMode = EScaleMode::LockedXY;
		}
	}

	return ScaleMode;
}

void FBodyInstance::SetMassOverride(float MassInKG, bool bNewOverrideMass)
{
	bOverrideMass = bNewOverrideMass;
	MassInKgOverride = MassInKG;
}

bool FBodyInstance::GetRigidBodyState(FRigidBodyState& OutState)
{
	if (IsInstanceSimulatingPhysics())
	{
		FTransform BodyTM = GetUnrealWorldTransform();
		OutState.Position = BodyTM.GetTranslation();
		OutState.Quaternion = BodyTM.GetRotation();
		OutState.LinVel = GetUnrealWorldVelocity();
		OutState.AngVel = FMath::RadiansToDegrees(GetUnrealWorldAngularVelocityInRadians());
		OutState.Flags = (IsInstanceAwake() ? ERigidBodyFlags::None : ERigidBodyFlags::Sleeping);
		return true;
	}

	return false;
}

bool FBodyInstance::UpdateBodyScale(const FVector& InScale3D, bool bForceUpdate)
{
	SCOPE_CYCLE_COUNTER(STAT_BodyInstanceUpdateBodyScale);

	if (!IsValidBodyInstance())
	{
		//UE_LOG(LogPhysics, Log, TEXT("Body hasn't been initialized. Call InitBody to initialize."));
		return false;
	}

	// if scale is already correct, and not forcing an update, do nothing
	if (Scale3D.Equals(InScale3D) && !bForceUpdate)
	{
		return false;
	}

	bool bSuccess = false;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ensureMsgf ( !Scale3D.ContainsNaN() && !InScale3D.ContainsNaN(), TEXT("Scale3D = (%f,%f,%f) InScale3D = (%f,%f,%f)"), Scale3D.X, Scale3D.Y, Scale3D.Z, InScale3D.X, InScale3D.Y, InScale3D.Z );
#endif

	FVector UpdatedScale3D;
#if WITH_PHYSX
	//Get all shapes
	EScaleMode::Type ScaleMode = EScaleMode::Free;

	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		TArray<PxShape *> PShapes;
		TArray<FPhysicsShapeHandle> Shapes;
		GetAllShapes_AssumesLocked(Shapes);
		ScaleMode = ComputeScaleMode(Shapes);

		FVector AdjustedScale3D;
		FVector AdjustedScale3DAbs;

		// Apply scaling
		ComputeScalingVectors(ScaleMode, InScale3D, AdjustedScale3D, AdjustedScale3DAbs);

		//we need to allocate all of these here because PhysX insists on using the stack. This is wasteful, but reduces a lot of code duplication
		PxSphereGeometry PSphereGeom;
		PxBoxGeometry PBoxGeom;
		PxCapsuleGeometry PCapsuleGeom;
		PxConvexMeshGeometry PConvexGeom;
		PxTriangleMeshGeometry PTriMeshGeom;

		for(FPhysicsShapeHandle& Shape : Shapes)
		{
			bool bInvalid = false;	//we only mark invalid if actually found geom and it's invalid scale
			PxGeometry* UpdatedGeometry = NULL;
			FTransform LocalTransform = FPhysicsInterface::GetLocalTransform(Shape);

			ECollisionShapeType GeomType = FPhysicsInterface::GetShapeType(Shape);
			FPhysicsGeometryCollection GeoCollection = FPhysicsInterface::GetGeometryCollection(Shape);
			FKShapeElem* ShapeElem = FPhysxUserData::Get<FKShapeElem>(FPhysicsInterface::GetUserData(Shape));
			const FTransform& RelativeTM = GetRelativeBodyTransform(Shape);

#if WITH_APEIRON
			check(false);
#else
			switch (GeomType)
			{
				case ECollisionShapeType::Sphere:
				{
					FKSphereElem* SphereElem = ShapeElem->GetShapeCheck<FKSphereElem>();
					ensure(ScaleMode == EScaleMode::LockedXYZ);

					GeoCollection.GetSphereGeometry(PSphereGeom);
					 
					PSphereGeom.radius = FMath::Max(SphereElem->Radius * AdjustedScale3DAbs.X, FCollisionShape::MinSphereRadius());

					FVector NewTranslation = RelativeTM.TransformPosition(SphereElem->Center) * AdjustedScale3D.X;
					LocalTransform.SetTranslation(NewTranslation);

					if (PSphereGeom.isValid())
					{
						UpdatedGeometry = &PSphereGeom;
						bSuccess = true;
					}
					else
					{
						bInvalid = true;
					}
					break;
				}
				case ECollisionShapeType::Box:
				{
					FKBoxElem* BoxElem = ShapeElem->GetShapeCheck<FKBoxElem>();
					GeoCollection.GetBoxGeometry(PBoxGeom);

					PBoxGeom.halfExtents.x = FMath::Max((0.5f * BoxElem->X * AdjustedScale3DAbs.X), FCollisionShape::MinBoxExtent());
					PBoxGeom.halfExtents.y = FMath::Max((0.5f * BoxElem->Y * AdjustedScale3DAbs.Y), FCollisionShape::MinBoxExtent());
					PBoxGeom.halfExtents.z = FMath::Max((0.5f * BoxElem->Z * AdjustedScale3DAbs.Z), FCollisionShape::MinBoxExtent());

					LocalTransform = BoxElem->GetTransform() * RelativeTM;
					LocalTransform.ScaleTranslation(AdjustedScale3D);

					if (PBoxGeom.isValid())
					{
						UpdatedGeometry = &PBoxGeom;
						bSuccess = true;
					}
					else
					{
						bInvalid = true;
					}
					break;
				}
				case ECollisionShapeType::Capsule:
				{
					FKSphylElem* SphylElem = ShapeElem->GetShapeCheck<FKSphylElem>();
					ensure(ScaleMode == EScaleMode::LockedXY || ScaleMode == EScaleMode::LockedXYZ);

					float ScaleRadius = FMath::Max(AdjustedScale3DAbs.X, AdjustedScale3DAbs.Y);
					float ScaleLength = AdjustedScale3DAbs.Z;

					GeoCollection.GetCapsuleGeometry(PCapsuleGeom);

					// this is a bit confusing since radius and height is scaled
					// first apply the scale first 
					float Radius = FMath::Max(SphylElem->Radius * ScaleRadius, 0.1f);
					float Length = SphylElem->Length + SphylElem->Radius * 2.f;
					float HalfLength = Length * ScaleLength * 0.5f;
					Radius = FMath::Min(Radius, HalfLength);	//radius is capped by half length
					Radius = FMath::Max(Radius, FCollisionShape::MinCapsuleRadius()); // bounded by minimum limit.
					float HalfHeight = HalfLength - Radius;
					HalfHeight = FMath::Max(FCollisionShape::MinCapsuleAxisHalfHeight(), HalfHeight);

					PCapsuleGeom.halfHeight = FMath::Max(HalfHeight, KINDA_SMALL_NUMBER);
					PCapsuleGeom.radius = FMath::Max(Radius, KINDA_SMALL_NUMBER);

					LocalTransform.SetTranslation(RelativeTM.TransformPosition(SphylElem->Center));
					LocalTransform.SetRotation(SphylElem->Rotation.Quaternion() * U2PSphylBasis_UE); // #PHYS2 we probably want to put this behind the interface?
					LocalTransform.ScaleTranslation(AdjustedScale3D);

					if (PCapsuleGeom.isValid())
					{
						UpdatedGeometry = &PCapsuleGeom;
						bSuccess = true;
					}
					else
					{
						bInvalid = true;
					}

					break;
				}
				case ECollisionShapeType::Convex:
				{
					FKConvexElem* ConvexElem = ShapeElem->GetShapeCheck<FKConvexElem>();
					GeoCollection.GetConvexGeometry(PConvexGeom);

					bool bUseNegX = CalcMeshNegScaleCompensation(AdjustedScale3D, LocalTransform);

					PConvexGeom.convexMesh = bUseNegX ? ConvexElem->GetMirroredConvexMesh() : ConvexElem->GetConvexMesh();
					PConvexGeom.scale.scale = U2PVector(AdjustedScale3DAbs);

					LocalTransform.TransformRotation(RelativeTM.GetRotation());
					LocalTransform.ScaleTranslation(AdjustedScale3D);

					if (PConvexGeom.isValid())
					{
						UpdatedGeometry = &PConvexGeom;
						bSuccess = true;
					}
					else
					{
						bInvalid = true;
					}

					break;
				}
				case ECollisionShapeType::Trimesh:
				{
					check(ShapeElem == nullptr);	//trimesh shape doesn't have userData

					GeoCollection.GetTriMeshGeometry(PTriMeshGeom);

					// find which trimesh elems it is
					// it would be nice to know if the order of PShapes array index is in the order of createShape
					if (BodySetup.IsValid())
					{
						for (PxTriangleMesh* TriMesh : BodySetup->TriMeshes)
						{
							// found it
							if (TriMesh == PTriMeshGeom.triangleMesh)
							{
								PTriMeshGeom.scale.scale = U2PVector(AdjustedScale3D);

								LocalTransform = RelativeTM;
								LocalTransform.ScaleTranslation(AdjustedScale3D);

								if (PTriMeshGeom.isValid())
								{
									UpdatedGeometry = &PTriMeshGeom;
									bSuccess = true;
								}
								else
								{
									bInvalid = true;
								}
							}
						}
					}

					break;
				}
				case ECollisionShapeType::Heightfield:
				{
					// HeightField is only used by Landscape, which does different code path from other primitives
					break;
				}
				default:
				{
						   UE_LOG(LogPhysics, Error, TEXT("Unknown geom type."));
				}
			}// end switch
#endif

			if (UpdatedGeometry)
			{
				FPhysicsCommand::ExecuteShapeWrite(this, Shape, [&](const FPhysicsShapeHandle& InShape)
				{
					FPhysicsInterface::SetLocalTransform(InShape, LocalTransform);
					FPhysicsInterface::SetGeometry(InShape, *UpdatedGeometry);
				});

				UpdatedScale3D = AdjustedScale3D;
			}
			else if (bInvalid)
			{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				FMessageLog("PIE").Warning()
					->AddToken(FTextToken::Create(LOCTEXT("InvalidScaleStart", "Applying invalid scale")))
					->AddToken(FTextToken::Create(AdjustedScale3D.ToCompactText()))
					->AddToken(FTextToken::Create(LOCTEXT("InvalidScaleMid", "to body")))
					->AddToken(FUObjectToken::Create(OwnerComponent.Get()));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			}
		}
	});
	
#endif

	// if success, overwrite old Scale3D, otherwise, just don't do it. It will have invalid scale next time
	if (bSuccess)
	{
		Scale3D = UpdatedScale3D;

		// update mass if required
		if (bUpdateMassWhenScaleChanges)
		{
			UpdateMassProperties();
		}
	}

	return bSuccess;
}

void FBodyInstance::UpdateInstanceSimulatePhysics()
{
	// In skeletal case, we need both our bone and skelcomponent flag to be true.
	// This might be 'and'ing us with ourself, but thats fine.
	const bool bUseSimulate = IsInstanceSimulatingPhysics();
	bool bInitialized = false;

	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		bInitialized = true;
		FPhysicsInterface::SetIsKinematic_AssumesLocked(Actor, !bUseSimulate);
		FPhysicsInterface::SetCcdEnabled_AssumesLocked(Actor, bUseCCD);

		if(bSimulatePhysics && bStartAwake)
				{
			FPhysicsInterface::WakeUp_AssumesLocked(Actor);
		}
	});

	//In the original physx only implementation this was wrapped in a PRigidDynamic != NULL check.
	//We use bInitialized to check rigid actor has been created in either engine because if we haven't even initialized yet, we don't want to undo our settings
	if (bInitialized)
	{
		if (bUseSimulate)
		{
			PhysicsBlendWeight = 1.f;
		}
		else
		{
			PhysicsBlendWeight = 0.f;
		}

		bSimulatePhysics = bUseSimulate;
	}
}

bool FBodyInstance::IsNonKinematic() const
{
	return bSimulatePhysics;
}

bool FBodyInstance::IsDynamic() const
{
	return FPhysicsInterface::IsDynamic(ActorHandle);
}

void FBodyInstance::ApplyWeldOnChildren()
{
	if(UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get())
	{
		TArray<FBodyInstance*> ChildrenBodies;
		TArray<FName> ChildrenLabels;
		OwnerComponentInst->GetWeldedBodies(ChildrenBodies, ChildrenLabels, /*bIncludingAutoWeld=*/true);

		for (int32 ChildIdx = 0; ChildIdx < ChildrenBodies.Num(); ++ChildIdx)
		{
			FBodyInstance* ChildBI = ChildrenBodies[ChildIdx];
			checkSlow(ChildBI);
			if (ChildBI != this)
			{
				const ECollisionEnabled::Type ChildCollision = ChildBI->GetCollisionEnabled();
				if(CollisionEnabledHasPhysics(ChildCollision))
				{
					if(UPrimitiveComponent* PrimOwnerComponent = ChildBI->OwnerComponent.Get())
					{
						Weld(ChildBI, PrimOwnerComponent->GetSocketTransform(ChildrenLabels[ChildIdx]));
					}
				}
			}
		}
	}
	
}

bool FBodyInstance::ShouldInstanceSimulatingPhysics() const
{
	return bSimulatePhysics && BodySetup.IsValid() && BodySetup->GetCollisionTraceFlag() != ECollisionTraceFlag::CTF_UseComplexAsSimple;
}


void FBodyInstance::SetInstanceSimulatePhysics(bool bSimulate, bool bMaintainPhysicsBlending)
{
	if (bSimulate)
	{
		UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();

		// If we are enabling simulation, and we are the root body of our component (or we are welded), we detach the component 
		if (OwnerComponentInst && OwnerComponentInst->IsRegistered() && (OwnerComponentInst->GetBodyInstance() == this || OwnerComponentInst->IsWelded()))
		{
			if (OwnerComponentInst->GetAttachParent())
			{
				OwnerComponentInst->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}

			if (bSimulatePhysics == false)	//if we're switching from kinematic to simulated
			{
				ApplyWeldOnChildren();
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (OwnerComponentInst)
		{
			if (!IsValidBodyInstance())
			{
				FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SimPhysNoBody", "Trying to simulate physics on ''{0}'' but no physics body."),
					FText::FromString(GetPathNameSafe(OwnerComponentInst))));
			}
			else if (!IsDynamic())
			{
				FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SimPhysStatic", "Trying to simulate physics on ''{0}'' but it is static."),
					FText::FromString(GetPathNameSafe(OwnerComponentInst))));
			}
			else if(BodySetup.IsValid() && BodySetup->GetCollisionTraceFlag() == ECollisionTraceFlag::CTF_UseComplexAsSimple)
			{
				FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SimComplexAsSimple", "Trying to simulate physics on ''{0}'' but it has ComplexAsSimple collision."),
					FText::FromString(GetPathNameSafe(OwnerComponentInst))));
			}
		}
#endif
	}

	bSimulatePhysics = bSimulate;
	if ( !bMaintainPhysicsBlending )
	{
		if (bSimulatePhysics)
		{
			PhysicsBlendWeight = 1.f;
		}
		else
		{
			PhysicsBlendWeight = 0.f;
		}
	}

	UpdateInstanceSimulatePhysics();
}

bool FBodyInstance::IsValidBodyInstance() const
{
	return ActorHandle.IsValid();
}

FTransform GetUnrealWorldTransformImp_AssumesLocked(const FBodyInstance* BodyInstance, bool bWithProjection, bool bGlobalPose)
{
	FTransform WorldTM = FTransform::Identity;

	if(BodyInstance && BodyInstance->IsValidBodyInstance())
	{
		WorldTM = FPhysicsInterface::GetTransform_AssumesLocked(BodyInstance->ActorHandle, bGlobalPose);

		if(bWithProjection)
		{
			BodyInstance->OnCalculateCustomProjection.ExecuteIfBound(BodyInstance, WorldTM);
		}
	}

	return WorldTM;
}

FTransform FBodyInstance::GetUnrealWorldTransform(bool bWithProjection /* = true*/, bool bForceGlobalPose /* = true*/) const
{
	FTransform OutTransform = FTransform::Identity;

	FPhysicsCommand::ExecuteRead(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		OutTransform = GetUnrealWorldTransformImp_AssumesLocked(this, bWithProjection, bForceGlobalPose);
	});

	return OutTransform;
}


FTransform FBodyInstance::GetUnrealWorldTransform_AssumesLocked(bool bWithProjection /* = true*/, bool bForceGlobalPose /* = true*/) const
{
	return GetUnrealWorldTransformImp_AssumesLocked(this, bWithProjection, bForceGlobalPose);
}

void FBodyInstance::SetBodyTransform(const FTransform& NewTransform, ETeleportType Teleport, bool bAutoWake)
{
	SCOPE_CYCLE_COUNTER(STAT_SetBodyTransform);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	extern bool GShouldLogOutAFrameOfSetBodyTransform;
	if (GShouldLogOutAFrameOfSetBodyTransform == true)
	{
		UE_LOG(LogPhysics, Log, TEXT("SetBodyTransform: %s"), *GetBodyDebugName());
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	// Catch NaNs and elegantly bail out.

	if( !ensureMsgf(!NewTransform.ContainsNaN(), TEXT("SetBodyTransform contains NaN (%s)\n%s"), (OwnerComponent.Get() ? *OwnerComponent->GetPathName() : TEXT("NONE")), *NewTransform.ToString()) )
	{
		return;
	}

	if(ActorHandle.IsValid())
	{
		if(!NewTransform.IsValid())
		{
			UE_LOG(LogPhysics, Warning, TEXT("FBodyInstance::SetBodyTransform: Trying to set new transform with bad data: %s"), *NewTransform.ToString());
			return;
		}

		bool bEditorWorld = false;
#if WITH_EDITOR
		//If the body is moved in the editor we avoid setting the kinematic target. This is useful for tools that rely on the physx data being up to date in the editor (and velocities aren't important in this case)
		UPrimitiveComponent* OwnerComp = OwnerComponent.Get();
		UWorld* World = OwnerComp ? OwnerComp->GetWorld() : nullptr;
		bEditorWorld = World && World->WorldType == EWorldType::Editor;
#endif

		FPhysScene* Scene = GetPhysicsScene();

		if(FPhysicsInterface::IsDynamic(ActorHandle) && !bEditorWorld && Scene)
		{
			FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
			{
				const bool bKinematic = FPhysicsInterface::IsKinematic_AssumesLocked(Actor);
				const bool bSimulated = FPhysicsInterface::CanSimulate_AssumesLocked(Actor);
				const bool bIsSimKinematic = bKinematic && bSimulated;

				if(bIsSimKinematic && Teleport == ETeleportType::None)
					{
					Scene->SetKinematicTarget_AssumesLocked(this, NewTransform, true);
					}
					else
					{
					if(bIsSimKinematic)
						{
						FPhysicsInterface::SetKinematicTarget_AssumesLocked(Actor, NewTransform);
						}

					FPhysicsInterface::SetGlobalPose_AssumesLocked(Actor, NewTransform, bAutoWake);
					}
			});
				}
		else if(Scene)
		{
			FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
			{
				FPhysicsInterface::SetGlobalPose_AssumesLocked(Actor, NewTransform, bAutoWake);
			});
			}
	}
	else if(WeldParent)
	{
		WeldParent->SetWeldedBodyTransform(this, NewTransform);
	}
}

void FBodyInstance::SetWeldedBodyTransform(FBodyInstance* TheirBody, const FTransform& NewTransform)
{
	UnWeld(TheirBody);
	Weld(TheirBody, NewTransform);
}

FVector FBodyInstance::GetUnrealWorldVelocity() const
{
	FVector OutVelocity = FVector::ZeroVector;

	FPhysicsCommand::ExecuteRead(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		OutVelocity = GetUnrealWorldVelocity_AssumesLocked();
	});

	return OutVelocity;
}

FVector FBodyInstance::GetUnrealWorldVelocity_AssumesLocked() const
{
	FVector LinVel(EForceInit::ForceInitToZero);
	if(ActorHandle.IsValid())
{
		LinVel = FPhysicsInterface::GetLinearVelocity_AssumesLocked(ActorHandle);
}

	return LinVel;
}

/** Note: returns angular velocity in radians per second. */
FVector FBodyInstance::GetUnrealWorldAngularVelocityInRadians() const
{
	FVector OutVelocity = FVector::ZeroVector;

	FPhysicsCommand::ExecuteRead(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		OutVelocity = FPhysicsInterface::GetAngularVelocity_AssumesLocked(Actor);
	});

	return OutVelocity;
}

/** Note: returns angular velocity in radians per second. */
FVector FBodyInstance::GetUnrealWorldAngularVelocityInRadians_AssumesLocked() const
{
	return FPhysicsInterface::GetAngularVelocity_AssumesLocked(ActorHandle);
}

FVector FBodyInstance::GetUnrealWorldVelocityAtPoint(const FVector& Point) const
{
	FVector OutVelocity = FVector::ZeroVector;
	FPhysicsCommand::ExecuteRead(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		OutVelocity = FPhysicsInterface::GetWorldVelocityAtPoint_AssumesLocked(Actor, Point);
	});

	return OutVelocity;
}


FVector FBodyInstance::GetUnrealWorldVelocityAtPoint_AssumesLocked(const FVector& Point) const
{
	return FPhysicsInterface::GetWorldVelocityAtPoint_AssumesLocked(ActorHandle, Point);
}

FTransform FBodyInstance::GetMassSpaceToWorldSpace() const
{
	FTransform MassSpaceToWorldSpace = FTransform::Identity;

	FPhysicsCommand::ExecuteRead(ActorHandle, [&](const FPhysicsActorHandle& Actor)
		{
		MassSpaceToWorldSpace = FPhysicsInterface::GetComTransform_AssumesLocked(Actor);
	});

	return MassSpaceToWorldSpace;
}

FTransform FBodyInstance::GetMassSpaceLocal() const
{
	FTransform MassSpaceLocal = FTransform::Identity;

	FPhysicsCommand::ExecuteRead(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		MassSpaceLocal = FPhysicsInterface::GetComTransformLocal_AssumesLocked(Actor);
	});

	return MassSpaceLocal;
}

void FBodyInstance::SetMassSpaceLocal(const FTransform& NewMassSpaceLocalTM)
{
	//TODO: UE doesn't store this so any changes to mass properties will not remember about this properly
	FPhysicsCommand::ExecuteRead(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		FPhysicsInterface::SetComLocalPose_AssumesLocked(Actor, NewMassSpaceLocalTM);
	});
}

float FBodyInstance::GetBodyMass() const
{
	float OutMass = 0.0f;

	FPhysicsCommand::ExecuteRead(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		OutMass = FPhysicsInterface::GetMass_AssumesLocked(Actor);
	});

	return OutMass;
}


FVector FBodyInstance::GetBodyInertiaTensor() const
{
	FVector OutTensor = FVector::ZeroVector;

	FPhysicsCommand::ExecuteRead(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		OutTensor = FPhysicsInterface::GetLocalInertiaTensor_AssumesLocked(Actor);
	});

	return OutTensor;
}

FBox FBodyInstance::GetBodyBounds() const
{
	FBox OutBox(EForceInit::ForceInitToZero);

	FPhysicsCommand::ExecuteRead(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		OutBox = FPhysicsInterface::GetBounds_AssumesLocked(Actor);
	});

	return OutBox;
}

void FBodyInstance::DrawCOMPosition(FPrimitiveDrawInterface* PDI, float COMRenderSize, const FColor& COMRenderColor)
{
	if (IsValidBodyInstance())
	{
		DrawWireStar(PDI, GetCOMPosition(), COMRenderSize, COMRenderColor, SDPG_World);
	}
}

/** Utility for copying properties from one BodyInstance to another. */
void FBodyInstance::CopyBodyInstancePropertiesFrom(const FBodyInstance* FromInst)
{
	// No copying of runtime instances (strictly defaults off BodySetup)
	check(FromInst);
	check(FromInst->OwnerComponent.Get() == NULL);
	check(FromInst->BodySetup.Get() == NULL);
	check(!FromInst->ActorHandle.IsValid());
	check(!ActorHandle.IsValid());

	*this = *FromInst;
}

FPhysScene* FBodyInstance::GetPhysicsScene() const
{
	if(ActorHandle.IsValid())
{
		return FPhysicsInterface::GetCurrentScene(ActorHandle);
	}
	
	return nullptr;
}

const FPhysicsActorHandle& FBodyInstance::GetPhysicsActorHandle() const
{
	return ActorHandle;
}

const FWalkableSlopeOverride& FBodyInstance::GetWalkableSlopeOverride() const
{
	if (bOverrideWalkableSlopeOnInstance || !BodySetup.IsValid())
	{
		return WalkableSlopeOverride;
	}
	else
	{
		return BodySetup->WalkableSlopeOverride;
	}
}

void FBodyInstance::SetWalkableSlopeOverride(const FWalkableSlopeOverride& NewOverride)
{
	bOverrideWalkableSlopeOnInstance = true;
	WalkableSlopeOverride = NewOverride;
}


/** 
*	Changes the current PhysMaterialOverride for this body. 
*	Note that if physics is already running on this component, this will _not_ alter its mass/inertia etc, it will only change its 
*	surface properties like friction and the damping.
*/
void FBodyInstance::SetPhysMaterialOverride( UPhysicalMaterial* NewPhysMaterial )
{
	// Save ref to PhysicalMaterial
	PhysMaterialOverride = NewPhysMaterial;

	// Go through the chain of physical materials and update the shapes 
	UpdatePhysicalMaterials();

	// Because physical material has changed, we need to update the mass
	UpdateMassProperties();
}

UPhysicalMaterial* FBodyInstance::GetSimplePhysicalMaterial() const
{
	return GetSimplePhysicalMaterial(this, OwnerComponent, BodySetup);
}

UPhysicalMaterial* FBodyInstance::GetSimplePhysicalMaterial(const FBodyInstance* BodyInstance, TWeakObjectPtr<UPrimitiveComponent> OwnerComp, TWeakObjectPtr<UBodySetup> BodySetupPtr)
{
	if(!GEngine || !GEngine->DefaultPhysMaterial)
	{
		UE_LOG(LogPhysics, Error, TEXT("FBodyInstance::GetSimplePhysicalMaterial : GEngine not initialized! Cannot call this during native CDO construction, wrap with if(!HasAnyFlags(RF_ClassDefaultObject)) or move out of constructor, material parameters will not be correct."));

		return nullptr;
	}

	// Find the PhysicalMaterial we need to apply to the physics bodies.
	// (LOW priority) Engine Mat, Material PhysMat, BodySetup Mat, Component Override, Body Override (HIGH priority)
	
	UPhysicalMaterial* ReturnPhysMaterial = NULL;

	// BodyInstance override
	if (BodyInstance->PhysMaterialOverride != NULL)
	{
		ReturnPhysMaterial = BodyInstance->PhysMaterialOverride;
		check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
	}
	else
	{
		// Component override
		UPrimitiveComponent* OwnerPrimComponent = OwnerComp.Get();
		if (OwnerPrimComponent && OwnerPrimComponent->BodyInstance.PhysMaterialOverride != NULL)
		{
			ReturnPhysMaterial = OwnerComp->BodyInstance.PhysMaterialOverride;
			check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
		}
		else
		{
			// BodySetup
			UBodySetup* BodySetupRawPtr = BodySetupPtr.Get();
			if (BodySetupRawPtr && BodySetupRawPtr->PhysMaterial != NULL)
			{
				ReturnPhysMaterial = BodySetupPtr->PhysMaterial;
				check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
			}
			else
			{
				// See if the Material has a PhysicalMaterial
				UMeshComponent* MeshComp = Cast<UMeshComponent>(OwnerPrimComponent);
				UPhysicalMaterial* PhysMatFromMaterial = NULL;
				if (MeshComp != NULL)
				{
					UMaterialInterface* Material = MeshComp->GetMaterial(0);
					if (Material != NULL)
					{
						PhysMatFromMaterial = Material->GetPhysicalMaterial();
					}
				}

				if (PhysMatFromMaterial != NULL)
				{
					ReturnPhysMaterial = PhysMatFromMaterial;
					check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
				}
				// fallback is default physical material
				else
				{
					ReturnPhysMaterial = GEngine->DefaultPhysMaterial;
					check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
				}
			}
		}
	}
	
	return ReturnPhysMaterial;
}

TArray<UPhysicalMaterial*> FBodyInstance::GetComplexPhysicalMaterials() const
{
	TArray<UPhysicalMaterial*> PhysMaterials;
	GetComplexPhysicalMaterials(PhysMaterials);
	return PhysMaterials;
}

void FBodyInstance::GetComplexPhysicalMaterials(TArray<UPhysicalMaterial*>& PhysMaterials) const
{
	GetComplexPhysicalMaterials(this, OwnerComponent, PhysMaterials);
}


void FBodyInstance::GetComplexPhysicalMaterials(const FBodyInstance*, TWeakObjectPtr<UPrimitiveComponent> OwnerComp, TArray<UPhysicalMaterial*>& OutPhysicalMaterials)
{
	check(GEngine->DefaultPhysMaterial != NULL);
	// See if the Material has a PhysicalMaterial
	UPrimitiveComponent* PrimComp = OwnerComp.Get();
	if (PrimComp)
	{
		const int32 NumMaterials = PrimComp->GetNumMaterials();
		OutPhysicalMaterials.SetNum(NumMaterials);

		for (int32 MatIdx = 0; MatIdx < NumMaterials; MatIdx++)
		{
			UPhysicalMaterial* PhysMat = GEngine->DefaultPhysMaterial;
			UMaterialInterface* Material = PrimComp->GetMaterial(MatIdx);
			if (Material)
			{
				PhysMat = Material->GetPhysicalMaterial();
			}

			check(PhysMat != NULL);
			OutPhysicalMaterials[MatIdx] = PhysMat;
		}
	}
}

/** Util for finding the number of 'collision sim' shapes on this Actor */
int32 GetNumSimShapes_AssumesLocked(const FPhysicsActorHandle& ActorRef)
{
	FInlineShapeArray PShapes;
	const int32 NumShapes = FillInlineShapeArray_AssumesLocked(PShapes, ActorRef);

	int32 NumSimShapes = 0;

	for(FPhysicsShapeHandle& Shape : PShapes)
	{
		if(FPhysicsInterface::IsSimulationShape(Shape))
		{
			NumSimShapes++;
		}
	}

	return NumSimShapes;
}

float KgPerM3ToKgPerCm3(float KgPerM3)
{
	//1m = 100cm => 1m^3 = (100cm)^3 = 1000000cm^3
	//kg/m^3 = kg/1000000cm^3
	const float M3ToCm3Inv = 1.f / (100.f * 100.f * 100.f);
	return KgPerM3 * M3ToCm3Inv;
}

float gPerCm3ToKgPerCm3(float gPerCm3)
{
	//1000g = 1kg
	//kg/cm^3 = 1000g/cm^3 => g/cm^3 = kg/1000 cm^3
	const float gToKG = 1.f / 1000.f;
	return gPerCm3 * gToKG;
}

#if WITH_PHYSX
/** Computes and adds the mass properties (inertia, com, etc...) based on the mass settings of the body instance. */
PxMassProperties ComputeMassProperties(const FBodyInstance* OwningBodyInstance, TArray<FPhysicsShapeHandle> Shapes, const FTransform& MassModifierTransform)
{
	// physical material - nothing can weigh less than hydrogen (0.09 kg/m^3)
	float DensityKGPerCubicUU = 1.0f;
	float RaiseMassToPower = 0.75f;
	if (UPhysicalMaterial* PhysMat = OwningBodyInstance->GetSimplePhysicalMaterial())
	{
		DensityKGPerCubicUU = FMath::Max(KgPerM3ToKgPerCm3(0.09f), gPerCm3ToKgPerCm3(PhysMat->Density));
		RaiseMassToPower = PhysMat->RaiseMassToPower;
	}

	PxMassProperties MassProps;
	FPhysicsInterface::CalculateMassPropertiesFromShapeCollection(MassProps, Shapes, DensityKGPerCubicUU);

	float OldMass = MassProps.mass;
	float NewMass = 0.f;

	if (OwningBodyInstance->bOverrideMass == false)
	{
		float UsePow = FMath::Clamp<float>(RaiseMassToPower, KINDA_SMALL_NUMBER, 1.f);
		NewMass = FMath::Pow(OldMass, UsePow);

		// Apply user-defined mass scaling.
		NewMass = FMath::Max(OwningBodyInstance->MassScale * NewMass, 0.001f);	//min weight of 1g
	}
	else
	{
		NewMass = FMath::Max(OwningBodyInstance->GetMassOverride(), 0.001f);	//min weight of 1g
	}

	check(NewMass > 0.f);

	float MassRatio = NewMass / OldMass;
	
	PxMassProperties FinalMassProps = MassProps * MassRatio;

	FinalMassProps.centerOfMass += U2PVector(MassModifierTransform.TransformVector(OwningBodyInstance->COMNudge));
	FinalMassProps.inertiaTensor = PxMassProperties::scaleInertia(FinalMassProps.inertiaTensor, PxQuat(PxIdentity), U2PVector(OwningBodyInstance->InertiaTensorScale));

	return FinalMassProps;
}
#endif

void FBodyInstance::UpdateMassProperties()
{
	UPhysicalMaterial* PhysMat = GetSimplePhysicalMaterial();

#if WITH_PHYSX
	if(ActorHandle.IsValid() && FPhysicsInterface::IsRigidBody(ActorHandle))
	{
		FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
		{
			check(Actor.IsValid());

			if(GetNumSimShapes_AssumesLocked(Actor) > 0)
			{
				int32 NumSyncShapes = 0;
				int32 NumAsyncShapes = 0;
				FPhysicsInterface::GetNumShapes(Actor, NumSyncShapes, NumAsyncShapes);

				bool bSyncData = FPhysicsInterface::HasSyncSceneData(Actor);
				check(bSyncData || FPhysicsInterface::HasAsyncSceneData(Actor));

				TArray<FPhysicsShapeHandle> Shapes;
				const uint32 NumShapes = bSyncData ? NumSyncShapes : NumAsyncShapes;
			Shapes.AddUninitialized(NumShapes);
				FPhysicsInterface::GetAllShapes_AssumedLocked(Actor, Shapes, bSyncData ? PST_Sync : PST_Async);

			// Ignore trimeshes & shapes which don't contribute to the mass
			for(int32 ShapeIdx = Shapes.Num() - 1; ShapeIdx >= 0; --ShapeIdx)
			{
					const FPhysicsShapeHandle& Shape = Shapes[ShapeIdx];
					const FKShapeElem* ShapeElem = FPhysxUserData::Get<FKShapeElem>(FPhysicsInterface::GetUserData(Shape));
					bool bIsTriangleMesh = FPhysicsInterface::GetShapeType(Shape) == ECollisionShapeType::Trimesh;
				bool bHasNoMass = ShapeElem && !ShapeElem->GetContributeToMass();
				if (bIsTriangleMesh || bHasNoMass)
				{
					Shapes.RemoveAtSwap(ShapeIdx);
				}
			}

			PxMassProperties TotalMassProperties;
			if(ShapeToBodiesMap.IsValid() && ShapeToBodiesMap->Num() > 0)
			{
				struct FWeldedBatch
				{
						TArray<FPhysicsShapeHandle> Shapes;
					FTransform RelTM;
				};

				//If we have welded children we must compute the mass properties of each individual body first and then combine them all together
				TMap<FBodyInstance*, FWeldedBatch> BodyToShapes;

					for(const FPhysicsShapeHandle& Shape : Shapes) //sort all welded children by their original bodies
				{
					if (FWeldInfo* WeldInfo = ShapeToBodiesMap->Find(Shape))
					{
						FWeldedBatch* WeldedBatch = BodyToShapes.Find(WeldInfo->ChildBI);
						if(!WeldedBatch)
						{
							WeldedBatch = &BodyToShapes.Add(WeldInfo->ChildBI);
							WeldedBatch->RelTM = WeldInfo->RelativeTM;
						}

						WeldedBatch->Shapes.Add(Shape);
					}
					else
					{
						//no weld info so shape really belongs to this body
						FWeldedBatch* WeldedBatch = BodyToShapes.Find(this);
						if (!WeldedBatch)
						{
							WeldedBatch = &BodyToShapes.Add(this);
							WeldedBatch->RelTM = FTransform::Identity;
						}

						WeldedBatch->Shapes.Add(Shape);
					}
				}

				TArray<PxMassProperties> SubMassProperties;
				TArray<PxTransform> MassTMs;
				for(auto BodyShapesItr : BodyToShapes)
				{
					const FBodyInstance* OwningBI = BodyShapesItr.Key;
					const FWeldedBatch& WeldedBatch = BodyShapesItr.Value;
					FTransform MassModifierTransform = WeldedBatch.RelTM;
					MassModifierTransform.SetScale3D(MassModifierTransform.GetScale3D() * Scale3D);	//Ensure that any scaling that is done on the component is passed into the mass frame modifiers

					PxMassProperties BodyMassProperties = ComputeMassProperties(OwningBI, WeldedBatch.Shapes, MassModifierTransform);
					SubMassProperties.Add(BodyMassProperties);
					MassTMs.Add(PxTransform(PxIdentity));
				}

				TotalMassProperties = PxMassProperties::sum(SubMassProperties.GetData(), MassTMs.GetData(), SubMassProperties.Num());
			}
			else
			{
				//No children welded so just get this body's mass properties
				FTransform MassModifierTransform(FQuat::Identity, FVector(0.f, 0.f, 0.f), Scale3D);	//Ensure that any scaling that is done on the component is passed into the mass frame modifiers
				TotalMassProperties = ComputeMassProperties(this, Shapes, MassModifierTransform);
			}
			
				// #PHYS2 Refactor out PxMassProperties (Our own impl?)
			PxQuat MassOrientation;
				const FVector MassSpaceInertiaTensor = P2UVector(PxMassProperties::getMassSpaceInertia(TotalMassProperties.inertiaTensor, MassOrientation));

				FPhysicsInterface::SetMass_AssumesLocked(Actor, TotalMassProperties.mass);
				FPhysicsInterface::SetMassSpaceInertiaTensor_AssumesLocked(Actor, MassSpaceInertiaTensor);

				FTransform Com(P2UQuat(MassOrientation), P2UVector(TotalMassProperties.centerOfMass));
				FPhysicsInterface::SetComLocalPose_AssumesLocked(Actor, Com);
		}
	});
	}
#endif

	//Let anyone who cares about mass properties know they've been updated
	OnRecalculatedMassProperties.Broadcast(this);
}

void FBodyInstance::UpdateDebugRendering()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	//After we update the mass properties, we should update any debug rendering
	if (UPrimitiveComponent* OwnerPrim = OwnerComponent.Get())
	{
		OwnerPrim->SendRenderDebugPhysics();
	}
#endif
}

void FBodyInstance::UpdateDampingProperties()
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(FPhysicsInterface::IsDynamic(Actor))
	{
			FPhysicsInterface::SetLinearDamping_AssumesLocked(Actor, LinearDamping);
			FPhysicsInterface::SetAngularDamping_AssumesLocked(Actor, AngularDamping);
		}
	});
}

bool FBodyInstance::IsInstanceAwake() const
{
	bool bIsAwake = false;

	FPhysicsCommand::ExecuteRead(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(FPhysicsInterface::IsDynamic(Actor))
	{
			bIsAwake = !FPhysicsInterface::IsSleeping(Actor);
		}
	});

	return bIsAwake;
}

void FBodyInstance::WakeInstance()
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(FPhysicsInterface::IsDynamic(Actor) && FPhysicsInterface::IsInScene(Actor) && !FPhysicsInterface::IsKinematic_AssumesLocked(Actor))
		{
			FPhysicsInterface::WakeUp_AssumesLocked(Actor);
		}
	});
}

void FBodyInstance::PutInstanceToSleep()
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(FPhysicsInterface::IsDynamic(Actor) && FPhysicsInterface::IsInScene(Actor) && !FPhysicsInterface::IsKinematic_AssumesLocked(Actor))
		{
			FPhysicsInterface::PutToSleep_AssumesLocked(Actor);
		}
	});
}

float FBodyInstance::GetSleepThresholdMultiplier() const
{
	if (SleepFamily == ESleepFamily::Sensitive)
	{
		return 1 / 20.0f;
	}
	else if (SleepFamily == ESleepFamily::Custom)
	{
		return CustomSleepThresholdMultiplier;
	}

	return 1.f;
}

void FBodyInstance::SetLinearVelocity(const FVector& NewVel, bool bAddToCurrent, bool bAutoWake)
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(FPhysicsInterface::IsRigidBody(Actor))
	{
			FVector FinalVelocity = NewVel;

		if (bAddToCurrent)
		{
				FinalVelocity += FPhysicsInterface::GetLinearVelocity_AssumesLocked(Actor);
		}

			FPhysicsInterface::SetLinearVelocity_AssumesLocked(Actor, FinalVelocity, bAutoWake);
		}
	});
}

/** Note NewAngVel is in degrees per second */
void FBodyInstance::SetAngularVelocityInRadians(const FVector& NewAngVel, bool bAddToCurrent, bool bAutoWake)
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(FPhysicsInterface::IsRigidBody(Actor))
	{
			FVector FinalVelocity = NewAngVel;

		if (bAddToCurrent)
		{
				FinalVelocity += FPhysicsInterface::GetAngularVelocity_AssumesLocked(Actor);
		}

			FPhysicsInterface::SetAngularVelocity_AssumesLocked(Actor, FinalVelocity, bAutoWake);
		}
	});
}

float FBodyInstance::GetMaxAngularVelocityInRadians() const
{
	return bOverrideMaxAngularVelocity ? FMath::DegreesToRadians(MaxAngularVelocity) : FMath::DegreesToRadians(UPhysicsSettings::Get()->MaxAngularVelocity);
}

void FBodyInstance::SetMaxAngularVelocityInRadians(float NewMaxAngVel, bool bAddToCurrent, bool bUpdateOverrideMaxAngularVelocity)
{
	float NewMaxInDegrees = FMath::RadiansToDegrees(NewMaxAngVel);

	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if (bAddToCurrent)
		{
			float OldValue = FPhysicsInterface::GetMaxAngularVelocity_AssumesLocked(Actor);
			NewMaxAngVel += OldValue;
			float OldValueInDegrees = FMath::RadiansToDegrees(OldValue);
			NewMaxInDegrees += OldValueInDegrees;
		}

		FPhysicsInterface::SetMaxAngularVelocity_AssumesLocked(Actor, NewMaxAngVel);
	});

	MaxAngularVelocity = NewMaxInDegrees;

	if(bUpdateOverrideMaxAngularVelocity)
	{
		bOverrideMaxAngularVelocity = true;
	}
}

void FBodyInstance::SetMaxDepenetrationVelocity(float MaxVelocity)
{
	bOverrideMaxDepenetrationVelocity = MaxVelocity > 0.f;
	MaxDepenetrationVelocity = MaxVelocity;

	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		FPhysicsInterface::SetMaxDepenetrationVelocity_AssumesLocked(Actor, MaxDepenetrationVelocity);
	});
}


void FBodyInstance::AddCustomPhysics(FCalculateCustomPhysics& CalculateCustomPhysics)
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(!IsRigidBodyKinematic_AssumesLocked(Actor))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene())
			{
				PhysScene->AddCustomPhysics_AssumesLocked(this, CalculateCustomPhysics);
			}
		}
	});
}

void FBodyInstance::AddForce(const FVector& Force, bool bAllowSubstepping, bool bAccelChange)
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(!IsRigidBodyKinematic_AssumesLocked(Actor))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene())
			{
				PhysScene->AddForce_AssumesLocked(this, Force, bAllowSubstepping, bAccelChange);
			}
		}
	});
}

void FBodyInstance::AddForceAtPosition(const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce)
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(!IsRigidBodyKinematic_AssumesLocked(Actor))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene())
			{
				PhysScene->AddForceAtPosition_AssumesLocked(this, Force, Position, bAllowSubstepping, bIsLocalForce);
			}
		}
	});
}

void FBodyInstance::ClearForces(bool bAllowSubstepping)
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(!IsRigidBodyKinematic_AssumesLocked(Actor))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene())
			{
				PhysScene->ClearForces_AssumesLocked(this, bAllowSubstepping);
			}
		}
	});
}

void FBodyInstance::AddTorqueInRadians(const FVector& Torque, bool bAllowSubstepping, bool bAccelChange)
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(!IsRigidBodyKinematic_AssumesLocked(Actor))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene())
			{
				PhysScene->AddTorque_AssumesLocked(this, Torque, bAllowSubstepping, bAccelChange);
			}
		}
	});
}

void FBodyInstance::ClearTorques(bool bAllowSubstepping)
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(!IsRigidBodyKinematic_AssumesLocked(Actor))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene())
			{
				PhysScene->ClearTorques_AssumesLocked(this, bAllowSubstepping);
			}
		}
	});
}


void FBodyInstance::AddAngularImpulseInRadians(const FVector& AngularImpulse, bool bVelChange)
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(FPhysicsInterface::IsRigidBody(Actor) && FPhysicsInterface::IsInScene(Actor) && !IsRigidBodyKinematic_AssumesLocked(Actor))
		{
			if(bVelChange)
	{
				FPhysicsInterface::AddTorqueMassIndependent_AssumesLocked(Actor, AngularImpulse);
			}
			else
		{
				FPhysicsInterface::AddTorque_AssumesLocked(Actor, AngularImpulse);
			}
		}
	});
}

void FBodyInstance::AddImpulse(const FVector& Impulse, bool bVelChange)
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(FPhysicsInterface::IsRigidBody(Actor) && FPhysicsInterface::IsInScene(Actor) && !IsRigidBodyKinematic_AssumesLocked(Actor))
		{
			if(bVelChange)
	{
				FPhysicsInterface::AddForceMassIndependent_AssumesLocked(Actor, Impulse);
			}
			else
		{
				FPhysicsInterface::AddForce_AssumesLocked(Actor, Impulse);
			}
		}
	});
}

void FBodyInstance::AddImpulseAtPosition(const FVector& Impulse, const FVector& Position)
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(FPhysicsInterface::IsRigidBody(Actor) && FPhysicsInterface::IsInScene(Actor) && !IsRigidBodyKinematic_AssumesLocked(Actor))
		{
			FPhysicsInterface::AddImpulseAtLocation_AssumesLocked(Actor, Impulse, Position);
		}
	});
}

void FBodyInstance::SetInstanceNotifyRBCollision(bool bNewNotifyCollision)
{
	bNotifyRigidBodyCollision = bNewNotifyCollision;
	UpdatePhysicsFilterData();
}

void FBodyInstance::SetEnableGravity(bool bInGravityEnabled)
{
	if (bEnableGravity != bInGravityEnabled)
	{
		bEnableGravity = bInGravityEnabled;

		{
			FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
			{
				if(FPhysicsInterface::IsRigidBody(Actor))
				{
					FPhysicsInterface::SetGravityEnabled_AssumesLocked(Actor, bEnableGravity);
				}
		});
		}

		if (bEnableGravity)
		{
			WakeInstance();
		}
	}
}

void FBodyInstance::SetContactModification(bool bNewContactModification)
{
	if (bNewContactModification != bContactModification)
	{
		bContactModification = bNewContactModification;
		UpdatePhysicsFilterData();
	}
}

void FBodyInstance::SetUseCCD(bool bInUseCCD)
{
	if (bUseCCD != bInUseCCD)
	{
		bUseCCD = bInUseCCD;
		// Need to set body flag
		FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
		{
			if(FPhysicsInterface::IsRigidBody(Actor))
			{
				FPhysicsInterface::SetCcdEnabled_AssumesLocked(Actor, bUseCCD);
			}
		});
		// And update collision filter data
		UpdatePhysicsFilterData();
	}
}


void FBodyInstance::AddRadialImpulseToBody(const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bVelChange)
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(FPhysicsInterface::IsRigidBody(Actor) && FPhysicsInterface::IsInScene(Actor) && !IsRigidBodyKinematic_AssumesLocked(Actor))
		{
			FPhysicsInterface::AddRadialImpulse_AssumesLocked(Actor, Origin, Radius, Strength, (ERadialImpulseFalloff)Falloff, bVelChange);
		}
	});
}

void FBodyInstance::AddRadialForceToBody(const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bAccelChange, bool bAllowSubstepping)
{
	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(FPhysicsInterface::IsRigidBody(Actor) && FPhysicsInterface::IsInScene(Actor) && !IsRigidBodyKinematic_AssumesLocked(Actor))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene())
			{
				PhysScene->AddRadialForceToBody_AssumesLocked(this, Origin, Radius, Strength, Falloff, bAccelChange, bAllowSubstepping);
			}
		}
	});
}

FString FBodyInstance::GetBodyDebugName() const
{
	FString DebugName;

	UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
	if (OwnerComponentInst != NULL)
	{
		DebugName = OwnerComponentInst->GetPathName();
		if (const UObject* StatObject = OwnerComponentInst->AdditionalStatObject())
		{
			DebugName += TEXT(" ");
			StatObject->AppendName(DebugName);
		}
	}

	if ((BodySetup != NULL) && (BodySetup->BoneName != NAME_None))
	{
		DebugName += FString(TEXT(" Bone: ")) + BodySetup->BoneName.ToString();
	}

	return DebugName;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// COLLISION

bool FBodyInstance::LineTrace(struct FHitResult& OutHit, const FVector& Start, const FVector& End, bool bTraceComplex, bool bReturnPhysicalMaterial) const
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_FBodyInstance_LineTrace);

	return FPhysicsInterface::LineTrace_Geom(OutHit, this, Start, End, bTraceComplex, bReturnPhysicalMaterial);
}

bool FBodyInstance::Sweep(struct FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& ShapeWorldRotation, const FCollisionShape& CollisionShape, bool bTraceComplex) const
{
	return FPhysicsInterface::Sweep_Geom(OutHit, this, Start, End, ShapeWorldRotation, CollisionShape, bTraceComplex);
		}

bool FBodyInstance::GetSquaredDistanceToBody(const FVector& Point, float& OutDistanceSquared, FVector& OutPointOnBody) const
{
	return FPhysicsInterface::GetSquaredDistanceToBody(this, Point, OutDistanceSquared, &OutPointOnBody);
}

float FBodyInstance::GetDistanceToBody(const FVector& Point, FVector& OutPointOnBody) const
{
	float DistanceSqr = -1.f;
	return (GetSquaredDistanceToBody(Point, DistanceSqr, OutPointOnBody) ? FMath::Sqrt(DistanceSqr) : -1.f);
}

template <typename AllocatorType>
bool FBodyInstance::OverlapTestForBodiesImpl(const FVector& Pos, const FQuat& Rot, const TArray<FBodyInstance*, AllocatorType>& Bodies) const
{
	bool bHaveOverlap = false;

	FPhysicsCommand::ExecuteRead(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		// calculate the test global pose of the rigid body
		FTransform PTestGlobalPose = FTransform(Rot, Pos);

		// Get all the shapes from the actor
		FInlineShapeArray TargetShapes;
		const int32 NumTargetShapes = FillInlineShapeArray_AssumesLocked(TargetShapes, Actor);

		for(const FPhysicsShapeHandle& Shape : TargetShapes)
		{
#if WITH_APEIRON || WITH_IMMEDIATE_PHYSX
			check(false);
#else

			ECollisionShapeType ShapeType = FPhysicsInterface::GetShapeType(Shape);
			if(ShapeType == ECollisionShapeType::Heightfield || ShapeType == ECollisionShapeType::Trimesh)
			{
				continue;	//we skip complex shapes - should this respect ComplexAsSimple?
			}

			// Calc shape global pose
			FTransform PShapeGlobalPose = FPhysicsInterface::GetLocalTransform(Shape) * PTestGlobalPose;
			for (const FBodyInstance* BodyInstance : Bodies)
			{
				bHaveOverlap = FPhysicsInterface::Overlap_Geom(BodyInstance, FPhysicsInterface::GetGeometryCollection(Shape), PShapeGlobalPose);

				if (bHaveOverlap)
				{
					return;
				}
			}
#endif
		}
	});
	return bHaveOverlap;
}

// Explicit template instantiation for the above.
template bool FBodyInstance::OverlapTestForBodiesImpl(const FVector& Pos, const FQuat& Rot, const TArray<FBodyInstance*>& Bodies) const;
template bool FBodyInstance::OverlapTestForBodiesImpl(const FVector& Pos, const FQuat& Rot, const TArray<FBodyInstance*, TInlineAllocator<1>>& Bodies) const;


bool FBodyInstance::OverlapTest(const FVector& Position, const FQuat& Rotation, const struct FCollisionShape& CollisionShape, FMTDResult* OutMTD) const
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_FBodyInstance_OverlapTest);

	bool bHasOverlap = false;

	FPhysicsCommand::ExecuteRead(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		FTransform GeomTransform(Rotation, Position);

		bHasOverlap = FPhysicsInterface::Overlap_Geom(this, CollisionShape, Rotation, GeomTransform, OutMTD);
	});

	return bHasOverlap;
}

FTransform RootSpaceToWeldedSpace(const FBodyInstance* BI, const FTransform& RootTM)
{
	if (BI->WeldParent)
	{
		UPrimitiveComponent* BIOwnerComponentInst = BI->OwnerComponent.Get();
		if (BIOwnerComponentInst)
		{
			FTransform RootToWelded = BIOwnerComponentInst->GetRelativeTransform().Inverse();
			RootToWelded.ScaleTranslation(BI->Scale3D);

			return RootToWelded * RootTM;
		}
	}

	return RootTM;
}

bool FBodyInstance::OverlapMulti(TArray<struct FOverlapResult>& InOutOverlaps, const class UWorld* World, const FTransform* pWorldToComponent, const FVector& Pos, const FQuat& Quat, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectQueryParams) const
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_FBodyInstance_OverlapMulti);

	if ( !IsValidBodyInstance()  && (!WeldParent || !WeldParent->IsValidBodyInstance()))
	{
		UE_LOG(LogCollision, Log, TEXT("FBodyInstance::OverlapMulti : (%s) No physics data"), *GetBodyDebugName());
		return false;
	}

	bool bHaveBlockingHit = false;

	// Determine how to convert the local space of this body instance to the test space
	const FTransform ComponentSpaceToTestSpace(Quat, Pos);

	FTransform BodyInstanceSpaceToTestSpace(NoInit);
	if (pWorldToComponent)
	{
		const FTransform RootTM = WeldParent ? WeldParent->GetUnrealWorldTransform() : GetUnrealWorldTransform();
		const FTransform LocalOffset = (*pWorldToComponent) * RootTM;
		BodyInstanceSpaceToTestSpace = ComponentSpaceToTestSpace * LocalOffset;
	}
	else
	{
		BodyInstanceSpaceToTestSpace = ComponentSpaceToTestSpace;
	}

	//We want to test using global position. However, the global position of the body will be in terms of the root body which we are welded to. So we must undo the relative transform so that our shapes are centered
	//Global = Parent * Relative => Global * RelativeInverse = Parent
	if (WeldParent)
	{
		BodyInstanceSpaceToTestSpace = RootSpaceToWeldedSpace(this, BodyInstanceSpaceToTestSpace);
	}

	const FBodyInstance* TargetInstance = WeldParent ? WeldParent : this;

	FPhysicsCommand::ExecuteRead(TargetInstance->ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		if(Actor.IsValid())
		{
		// Get all the shapes from the actor
			FInlineShapeArray PShapes;
			const int32 NumShapes = FillInlineShapeArray_AssumesLocked(PShapes, Actor);

			// Iterate over each shape
			TArray<struct FOverlapResult> TempOverlaps;
			for (int32 ShapeIdx = 0; ShapeIdx < NumShapes; ShapeIdx++)
			{
#if WITH_APEIRON || WITH_IMMEDIATE_PHYSX
				check(false);
#else
				FPhysicsShapeHandle& ShapeRef = PShapes[ShapeIdx];

				FPhysicsGeometryCollection GeomCollection = FPhysicsInterface::GetGeometryCollection(ShapeRef);

				if(IsShapeBoundToBody(ShapeRef) == false)
				{
					continue;
				}

				ECollisionShapeType ShapeType = FPhysicsInterface::GetShapeType(ShapeRef);
				if(ShapeType == ECollisionShapeType::Heightfield || ShapeType == ECollisionShapeType::Trimesh)
				{
					continue;	//we skip complex shapes - should this respect ComplexAsSimple?
				}

				// Calc shape global pose
				const FTransform LocalTransform = FPhysicsInterface::GetLocalTransform(ShapeRef);
				const FTransform GlobalTransform = LocalTransform * BodyInstanceSpaceToTestSpace;
			
				TempOverlaps.Reset();
				if(FPhysicsInterface::GeomOverlapMulti(World, GeomCollection, GlobalTransform.GetTranslation(), GlobalTransform.GetRotation(), TempOverlaps, TestChannel, Params, ResponseParams, ObjectQueryParams))
				{
					bHaveBlockingHit = true;
				}
				InOutOverlaps.Append(TempOverlaps);
#endif
			}
			}
		});

	return bHaveBlockingHit;
}

bool FBodyInstance::IsValidCollisionProfileName(FName InCollisionProfileName)
{
	return (InCollisionProfileName != NAME_None) && (InCollisionProfileName != UCollisionProfile::CustomCollisionProfileName);
}

void FBodyInstance::LoadProfileData(bool bVerifyProfile)
{
	const FName UseCollisionProfileName = GetCollisionProfileName();
	if ( bVerifyProfile )
	{
		// if collision profile name exists, 
		// check with current settings
		// if same, then keep the profile name
		// if not same, that means it has been modified from default
		// leave it as it is, and clear profile name
		if ( IsValidCollisionProfileName(UseCollisionProfileName) )
		{
			FCollisionResponseTemplate Template;
			if ( UCollisionProfile::Get()->GetProfileTemplate(UseCollisionProfileName, Template) ) 
			{
				// this function is only used for old code that did require verification of using profile or not
				// so that means it will have valid ResponsetoChannels value, so this is okay to access. 
				if (Template.IsEqual(CollisionEnabled, ObjectType, CollisionResponses.GetResponseContainer()) == false)
				{
					InvalidateCollisionProfileName(); 
				}
			}
			else
			{
				UE_LOG(LogPhysics, Warning, TEXT("COLLISION PROFILE [%s] is not found"), *UseCollisionProfileName.ToString());
				// if not nothing to do
				InvalidateCollisionProfileName(); 
			}
		}
	}
	else
	{
		if ( IsValidCollisionProfileName(UseCollisionProfileName) )
		{
			if ( UCollisionProfile::Get()->ReadConfig(UseCollisionProfileName, *this) == false)
			{
				// clear the name
				InvalidateCollisionProfileName();
			}
		}

		// no profile, so it just needs to update container from array data
		if ( DoesUseCollisionProfile() == false )
		{
			// if external profile copy the data over
			if (ExternalCollisionProfileBodySetup.IsValid(true))
			{
				UBodySetup* BodySetupInstance = ExternalCollisionProfileBodySetup.Get(true);
				const FBodyInstance& ExternalBodyInstance = BodySetupInstance->DefaultInstance;
				CollisionProfileName = ExternalBodyInstance.CollisionProfileName;
				ObjectType = ExternalBodyInstance.ObjectType;
				CollisionEnabled = ExternalBodyInstance.CollisionEnabled;
				CollisionResponses.SetCollisionResponseContainer(ExternalBodyInstance.CollisionResponses.ResponseToChannels);
			}
			else
			{
				CollisionResponses.UpdateResponseContainerFromArray();
			}
		}
	}
}

void FBodyInstance::GetBodyInstanceResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const
{
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(FPhysicsInterface::GetResourceSizeEx(ActorHandle));
}

void FBodyInstance::FixupData(class UObject* Loader)
{
	check (Loader);

	int32 const UE4Version = Loader->GetLinkerUE4Version();

#if WITH_EDITOR
	if (UE4Version < VER_UE4_ADD_CUSTOMPROFILENAME_CHANGE)
	{
		if (CollisionProfileName == NAME_None)
		{
			CollisionProfileName = UCollisionProfile::CustomCollisionProfileName;
		}
	}

	if (UE4Version < VER_UE4_SAVE_COLLISIONRESPONSE_PER_CHANNEL)
	{
		CollisionResponses.SetCollisionResponseContainer(ResponseToChannels_DEPRECATED);
	}
#endif // WITH_EDITORONLY_DATA

	// Load profile. If older version, please verify profile name first
	bool bNeedToVerifyProfile = (UE4Version < VER_UE4_COLLISION_PROFILE_SETTING) || 
		// or shape component needs to convert since we added profile
		(UE4Version < VER_UE4_SAVE_COLLISIONRESPONSE_PER_CHANNEL && Loader->IsA(UShapeComponent::StaticClass()));
	LoadProfileData(bNeedToVerifyProfile);

	// if profile isn't set, then fix up channel responses
	if( CollisionProfileName == UCollisionProfile::CustomCollisionProfileName ) 
	{
		if (UE4Version >= VER_UE4_SAVE_COLLISIONRESPONSE_PER_CHANNEL)
		{
			CollisionResponses.UpdateResponseContainerFromArray();
		}
	}
}

bool FBodyInstance::UseAsyncScene(const FPhysScene* PhysScene) const
{
	bool bHasAsyncScene = true;
	if (PhysScene)
	{
		bHasAsyncScene = PhysScene->HasAsyncScene();
	}

	return bUseAsyncScene && bHasAsyncScene;
}


void FBodyInstance::SetUseAsyncScene(bool bNewUseAsyncScene)
{
	// Set flag
	bUseAsyncScene = bNewUseAsyncScene;
}

void FBodyInstance::ApplyMaterialToShape_AssumesLocked(const FPhysicsShapeHandle& InShape, UPhysicalMaterial* SimplePhysMat, const TArrayView<UPhysicalMaterial*>& ComplexPhysMats, const bool bSharedShape)
{
	if(!bSharedShape && FPhysicsInterface::IsShared(InShape))	//user says the shape is exclusive, but physx says it's shared
	{
		UE_LOG(LogPhysics, Warning, TEXT("FBodyInstance::ApplyMaterialToShape_AssumesLocked : Trying to change the physical material of a shared shape. If this is your intention pass bSharedShape = true"));
	}

	// If a triangle mesh, need to get array of materials...
	ECollisionShapeType GeomType = FPhysicsInterface::GetShapeType(InShape);
	if(GeomType == ECollisionShapeType::Trimesh)
		{
		if(ComplexPhysMats.Num())
		{
			FPhysicsInterface::SetMaterials(InShape, ComplexPhysMats);
		}
		else
		{
			if(SimplePhysMat)
			{
				UE_LOG(LogPhysics, Verbose, TEXT("FBodyInstance::ApplyMaterialToShape_AssumesLocked : PComplexMats is empty - falling back on simple physical material."));
				FPhysicsInterface::SetMaterials(InShape, {&SimplePhysMat, 1});
			}
			else
			{
				UE_LOG(LogPhysics, Error, TEXT("FBodyInstance::ApplyMaterialToShape_AssumesLocked : PComplexMats is empty, and we do not have a valid simple material."));
			}
		}

	}
	// Simple shape, 
	else if(SimplePhysMat)
	{
		FPhysicsInterface::SetMaterials(InShape, {&SimplePhysMat, 1});
	}
	else
	{
		UE_LOG(LogPhysics, Error, TEXT("FBodyInstance::ApplyMaterialToShape_AssumesLocked : No valid simple physics material found."));
	}
}

void FBodyInstance::ApplyMaterialToInstanceShapes_AssumesLocked(UPhysicalMaterial* SimplePhysMat, TArray<UPhysicalMaterial*>& ComplexPhysMats)
{
	FBodyInstance* TheirBI = this;
	FBodyInstance* BIWithActor = TheirBI->WeldParent ? TheirBI->WeldParent : TheirBI;

	TArray<FPhysicsShapeHandle> AllShapes;
	BIWithActor->GetAllShapes_AssumesLocked(AllShapes);

	for(FPhysicsShapeHandle& Shape : AllShapes)
	{
		if(TheirBI->IsShapeBoundToBody(Shape))
		{
			FPhysicsCommand::ExecuteShapeWrite(BIWithActor, Shape, [&](const FPhysicsShapeHandle& InnerShape)
		{
				ApplyMaterialToShape_AssumesLocked(InnerShape, SimplePhysMat, ComplexPhysMats, TheirBI->HasSharedShapes());
		});		
	}
}
}

bool FBodyInstance::ValidateTransform(const FTransform &Transform, const FString& DebugName, const UBodySetup* Setup)
{
	if(Transform.GetScale3D().IsNearlyZero())
	{
		UE_LOG(LogPhysics, Warning, TEXT("Initialising Body : Scale3D is (nearly) zero: %s"), *DebugName);
		return false;
	}

	// Check we support mirroring/non-mirroring
	const float TransformDet = Transform.GetDeterminant();
	if(TransformDet < 0.f && !Setup->bGenerateMirroredCollision)
	{
		UE_LOG(LogPhysics, Warning, TEXT("Initialising Body : Body is mirrored but bGenerateMirroredCollision == false: %s"), *DebugName);
		return false;
	}

	if(TransformDet > 0.f && !Setup->bGenerateNonMirroredCollision)
	{
		UE_LOG(LogPhysics, Warning, TEXT("Initialising Body : Body is not mirrored but bGenerateNonMirroredCollision == false: %s"), *DebugName);
		return false;
	}

#if !(UE_BUILD_SHIPPING)
	if(Transform.ContainsNaN())
	{
		UE_LOG(LogPhysics, Warning, TEXT("Initialising Body : Bad transform - %s %s\n%s"), *DebugName, *Setup->BoneName.ToString(), *Transform.ToString());
		return false;
	}
#endif

	return true;
}

void FBodyInstance::InitDynamicProperties_AssumesLocked()
{
	if (!BodySetup.IsValid())
	{
		// This may be invalid following an undo if the BodySetup was a transient object (e.g. in Mesh Paint mode)
		// Just exit gracefully if so.
		return;
	}

	//QueryOnly bodies cannot become simulated at runtime. To do this they must change their CollisionEnabled which recreates the physics state
	//So early out to save a lot of useless work
	if (GetCollisionEnabled() == ECollisionEnabled::QueryOnly)
	{
		return;
	}
	
	if(FPhysicsInterface::IsDynamic(ActorHandle))
	{
		//A non simulated body may become simulated at runtime, so we need to compute its mass.
		//However, this is not supported for complexAsSimple since a trimesh cannot itself be simulated, it can only be used for collision of other simple shapes.
		if (BodySetup->GetCollisionTraceFlag() != ECollisionTraceFlag::CTF_UseComplexAsSimple)
		{
			UpdateMassProperties();
			UpdateDampingProperties();
			SetMaxAngularVelocityInRadians(GetMaxAngularVelocityInRadians(), false, false);
			SetMaxDepenetrationVelocity(bOverrideMaxDepenetrationVelocity ? MaxDepenetrationVelocity : UPhysicsSettings::Get()->MaxDepenetrationVelocity);
		}
		else
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (bSimulatePhysics)
			{
				if(UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get())
				{
					FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SimComplexAsSimple", "Trying to simulate physics on ''{0}'' but it has ComplexAsSimple collision."),
						FText::FromString(GetPathNameSafe(OwnerComponentInst))));
				}
			}
#endif
		}

		UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
		const AActor* OwningActor = OwnerComponentInst ? OwnerComponentInst->GetOwner() : nullptr;

		bool bComponentAwake = false;
		FVector InitialLinVel = GetInitialLinearVelocity(OwningActor, bComponentAwake);

		if (ShouldInstanceSimulatingPhysics())
		{
			FPhysicsInterface::SetLinearVelocity_AssumesLocked(ActorHandle, InitialLinVel);
		}

		float SleepEnergyThresh = FPhysicsInterface::GetSleepEnergyThreshold_AssumesLocked(ActorHandle);
		SleepEnergyThresh *= GetSleepThresholdMultiplier();
		FPhysicsInterface::SetSleepEnergyThreshold_AssumesLocked(ActorHandle, SleepEnergyThresh);

		// PhysX specific dynamic parameters, not generically exposed to physics interface

		float StabilizationThreshold = FPhysicsInterface::GetStabilizationEnergyThreshold_AssumesLocked(ActorHandle);
		StabilizationThreshold *= StabilizationThresholdMultiplier;
		FPhysicsInterface::SetStabilizationEnergyThreshold_AssumesLocked(ActorHandle, StabilizationThreshold);

		uint32 PositionIterCount = FMath::Clamp(PositionSolverIterationCount, 1, 255);
		uint32 VelocityIterCount = FMath::Clamp(VelocitySolverIterationCount, 1, 255);
		FPhysicsInterface::SetSolverPositionIterationCount_AssumesLocked(ActorHandle, PositionIterCount);
		FPhysicsInterface::SetSolverVelocityIterationCount_AssumesLocked(ActorHandle, VelocityIterCount);

		CreateDOFLock();
		if(!IsRigidBodyKinematic_AssumesLocked(ActorHandle))
		{
			if(!bStartAwake && !bComponentAwake)
			{
				FPhysicsInterface::SetWakeCounter_AssumesLocked(ActorHandle, 0.0f);
				FPhysicsInterface::PutToSleep_AssumesLocked(ActorHandle);
			}
		}
	}
}

void FBodyInstance::BuildBodyFilterData(FBodyCollisionFilterData& OutFilterData) const
	{
	// this can happen in landscape height field collision component
	if(!BodySetup.IsValid())
	{
		return;
	}

	// Figure out if we are static
	UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
	AActor* Owner = OwnerComponentInst ? OwnerComponentInst->GetOwner() : NULL;
	const bool bPhysicsStatic = !OwnerComponentInst || OwnerComponentInst->IsWorldGeometry();

	// Grab collision setting from body instance
	ECollisionEnabled::Type UseCollisionEnabled = GetCollisionEnabled(); // this checks actor/component override
	bool bUseNotifyRBCollision = bNotifyRigidBodyCollision;
	FCollisionResponseContainer UseResponse = CollisionResponses.GetResponseContainer();
	ECollisionChannel UseChannel = ObjectType;

	bool bUseContactModification = bContactModification;

	// @TODO: The skel mesh really shouldn't be the (pseudo-)authority here on the body's collision.
	//        This block should ultimately be removed, and outside of this (in the skel component) we 
	//        should configure the bodies to reflect this desired behavior.
	if(USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(OwnerComponentInst))
	{
		UseChannel = SkelMeshComp->GetCollisionObjectType();

		if (CVarEnableDynamicPerBodyFilterHacks.GetValueOnGameThread() && bHACK_DisableCollisionResponse)
		{
			UseResponse.SetAllChannels(ECR_Ignore);
			UseCollisionEnabled = ECollisionEnabled::PhysicsOnly;
			}
		else if(BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Enabled)
		{
			UseResponse.SetAllChannels(ECR_Block);
		}
		else if(BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Disabled)
		{
			UseResponse.SetAllChannels(ECR_Ignore);
			UseCollisionEnabled = ECollisionEnabled::PhysicsOnly;		// this will prevent object traces hitting this as well
		}

		const bool bDisableSkelComponentOverride = CVarEnableDynamicPerBodyFilterHacks.GetValueOnGameThread() && bHACK_DisableSkelComponentFilterOverriding;
		if(!bDisableSkelComponentOverride)
		{
		UseResponse = FCollisionResponseContainer::CreateMinContainer(UseResponse, SkelMeshComp->BodyInstance.CollisionResponses.GetResponseContainer());
		}

		bUseNotifyRBCollision = bUseNotifyRBCollision && SkelMeshComp->BodyInstance.bNotifyRigidBodyCollision;
	}

#if WITH_EDITOR
	// if no collision, but if world wants to enable trace collision for components, allow it
	if((UseCollisionEnabled == ECollisionEnabled::NoCollision) && Owner && (Owner->IsA(AVolume::StaticClass()) == false))
	{
		UWorld* World = Owner->GetWorld();
		UPrimitiveComponent* PrimComp = OwnerComponentInst;
		if(World && World->bEnableTraceCollision &&
		   (PrimComp->IsA(UStaticMeshComponent::StaticClass()) || PrimComp->IsA(USkeletalMeshComponent::StaticClass()) || PrimComp->IsA(UBrushComponent::StaticClass())))
		{
			//UE_LOG(LogPhysics, Warning, TEXT("Enabling collision %s : %s"), *GetNameSafe(Owner), *GetNameSafe(OwnerComponent.Get()));
			// clear all other channel just in case other people using those channels to do something
			UseResponse.SetAllChannels(ECR_Ignore);
			UseCollisionEnabled = ECollisionEnabled::QueryOnly;
		}
	}
#endif

	const bool bUseComplexAsSimple = (BodySetup.Get()->GetCollisionTraceFlag() == CTF_UseComplexAsSimple);
	const bool bUseSimpleAsComplex = (BodySetup.Get()->GetCollisionTraceFlag() == CTF_UseSimpleAsComplex);

	if(UseCollisionEnabled != ECollisionEnabled::NoCollision)
	{
		// CCD is determined by root body in case of welding
		bool bRootCCD = (WeldParent != nullptr) ? WeldParent->bUseCCD : bUseCCD;

		FCollisionFilterData SimFilterData;
		FCollisionFilterData SimpleQueryData;

			uint32 ActorID = Owner ? Owner->GetUniqueID() : 0;
			uint32 CompID = (OwnerComponentInst != nullptr) ? OwnerComponentInst->GetUniqueID() : 0;
		CreateShapeFilterData(UseChannel, MaskFilter, ActorID, UseResponse, CompID, InstanceBodyIndex, SimpleQueryData, SimFilterData, bRootCCD && !bPhysicsStatic, bUseNotifyRBCollision, bPhysicsStatic, bUseContactModification);

		FCollisionFilterData ComplexQueryData = SimpleQueryData;
			
			// Set output sim data
		OutFilterData.SimFilter = SimFilterData;

			// Build filterdata variations for complex and simple
		SimpleQueryData.Word3 |= EPDF_SimpleCollision;
			if(bUseSimpleAsComplex)
			{
			SimpleQueryData.Word3 |= EPDF_ComplexCollision;
			}

		ComplexQueryData.Word3 |= EPDF_ComplexCollision;
			if(bUseComplexAsSimple)
			{
			ComplexQueryData.Word3 |= EPDF_SimpleCollision;
			}
			
		OutFilterData.QuerySimpleFilter = SimpleQueryData;
		OutFilterData.QueryComplexFilter = ComplexQueryData;
	}
}

void FBodyInstance::InitStaticBodies(const TArray<FBodyInstance*>& Bodies, const TArray<FTransform>& Transforms, UBodySetup* BodySetup, UPrimitiveComponent* PrimitiveComp, FPhysScene* InRBScene)
{
	SCOPE_CYCLE_COUNTER(STAT_StaticInitBodies);

	check(BodySetup);
	check(InRBScene);
	check(Bodies.Num() > 0);

	static TArray<FBodyInstance*> BodiesStatic;
	static TArray<FTransform> TransformsStatic;

	check(BodiesStatic.Num() == 0);
	check(TransformsStatic.Num() == 0);

	BodiesStatic = Bodies;
	TransformsStatic = Transforms;

	FInitBodiesHelper<true> InitBodiesHelper(BodiesStatic, TransformsStatic, BodySetup, PrimitiveComp, InRBScene, FInitBodySpawnParams(PrimitiveComp), FPhysicsAggregateHandle());
	InitBodiesHelper.InitBodies();

	BodiesStatic.Reset();
	TransformsStatic.Reset();
}



void FBodyInstance::BuildBodyCollisionFlags(FBodyCollisionFlags& OutFlags, ECollisionEnabled::Type UseCollisionEnabled, bool bUseComplexAsSimple)
	{
	if(UseCollisionEnabled != ECollisionEnabled::NoCollision)
	{
		// Query collision
		OutFlags.bEnableQueryCollision = CollisionEnabledHasQuery(UseCollisionEnabled);

		// Sim collision
		const bool bSimCollision = CollisionEnabledHasPhysics(UseCollisionEnabled);

		// Enable sim collision
		if(bSimCollision)
		{
			OutFlags.bEnableSimCollisionSimple = true;
			
			// on dynamic objects and objects which don't use complex as simple, tri mesh not used for sim
			if(bUseComplexAsSimple)
			{
				OutFlags.bEnableSimCollisionComplex = true;
			}
		}
			}
		}

void FBodyInstance::UpdateInterpolateWhenSubStepping()
{
	if(UPhysicsSettings::Get()->bSubstepping)
	{
		// We interpolate based around our current collision enabled flag
		ECollisionEnabled::Type UseCollisionEnabled = ECollisionEnabled::NoCollision;
		if(OwnerComponent.IsValid() && OwnerComponent.Get()->GetBodyInstance() != this)
		{
			UseCollisionEnabled = OwnerComponent->GetCollisionEnabled();
		}
		else
		{
			UseCollisionEnabled = GetCollisionEnabled();
		}
	
		bInterpolateWhenSubStepping = UseCollisionEnabled == ECollisionEnabled::PhysicsOnly || UseCollisionEnabled == ECollisionEnabled::QueryAndPhysics;

		// If we have a weld parent we should take into account that too as that may be simulating while we are not
		if(WeldParent)
		{
			// Potentially recurse here
			WeldParent->UpdateInterpolateWhenSubStepping();
			bInterpolateWhenSubStepping |= WeldParent->bInterpolateWhenSubStepping;
		}
	}
}

////////////////////////////////////////////////////////////////////////////
// FBodyInstanceEditorHelpers

#if WITH_EDITOR

void FBodyInstanceEditorHelpers::EnsureConsistentMobilitySimulationSettingsOnPostEditChange(UPrimitiveComponent* Component, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (UProperty* PropertyThatChanged = PropertyChangedEvent.Property)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();

		// Automatically change collision profile based on mobility and physics settings (if it is currently one of the default profiles)
		const bool bMobilityChanged = PropertyName == GET_MEMBER_NAME_CHECKED(USceneComponent, Mobility);
		const bool bSimulatePhysicsChanged = PropertyName == GET_MEMBER_NAME_CHECKED(FBodyInstance, bSimulatePhysics);

		if (bMobilityChanged || bSimulatePhysicsChanged)
		{
			// If we enabled physics simulation, but we are not marked movable, do that for them
			if (bSimulatePhysicsChanged && Component->BodyInstance.bSimulatePhysics && (Component->Mobility != EComponentMobility::Movable))
			{
				Component->SetMobility(EComponentMobility::Movable);
			}
			// If we made the component no longer movable, but simulation was enabled, disable that for them
			else if (bMobilityChanged && (Component->Mobility != EComponentMobility::Movable) && Component->BodyInstance.bSimulatePhysics)
			{
				Component->BodyInstance.bSimulatePhysics = false;
			}

			// If the collision profile is one of the 'default' ones for a StaticMeshActor, make sure it is the correct one
			// If user has changed it to something else, don't touch it
			const FName CurrentProfileName = Component->BodyInstance.GetCollisionProfileName();
			if ((CurrentProfileName == UCollisionProfile::BlockAll_ProfileName) ||
				(CurrentProfileName == UCollisionProfile::BlockAllDynamic_ProfileName) ||
				(CurrentProfileName == UCollisionProfile::PhysicsActor_ProfileName))
			{
				if (Component->Mobility == EComponentMobility::Movable)
				{
					if (Component->BodyInstance.bSimulatePhysics)
					{
						Component->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
					}
					else
					{
						Component->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
					}
				}
				else
				{
					Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
				}
			}
		}
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
