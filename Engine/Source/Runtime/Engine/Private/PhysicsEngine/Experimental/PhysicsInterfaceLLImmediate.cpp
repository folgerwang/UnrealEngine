// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#if PHYSICS_INTERFACE_LLIMMEDIATE

#include "Physics/Experimental/PhysicsInterfaceLLImmediate.h"
#include "Engine/Engine.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Physics/PhysicsInterfaceUtils.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/ShapeElem.h"
#include "Physics/PhysicsGeometryPhysX.h"
#include "Components/PrimitiveComponent.h"

TSharedPtr<IContactModifyCallbackFactory> FPhysInterface_LLImmediate::ContactModifyCallbackFactory;
TSharedPtr<FPhysicsReplicationFactory> FPhysInterface_LLImmediate::PhysicsReplicationFactory;
TSharedPtr<FSimEventCallbackFactory> FPhysInterface_LLImmediate::SimEventCallbackFactory;

// Remove everthing in this block eventually - relied on elsewhere
float DebugLineLifetime = 2.f;

void FinishSceneStat(uint32 Scene)
{
}

//////////////////////////////////////////////////////////////////////////

DEFINE_STAT(STAT_TotalPhysicsTime);
DEFINE_STAT(STAT_NumCloths);
DEFINE_STAT(STAT_NumClothVerts);

//////////////////////////////////////////////////////////////////////////

template <>
int32 ENGINE_API FPhysInterface_LLImmediate::GetAllShapes_AssumedLocked(const FPhysicsActorHandle& InActorHandle, TArray<FPhysicsShapeHandle, FDefaultAllocator>& OutShapes, EPhysicsSceneType InSceneType);
template <>
int32 ENGINE_API FPhysInterface_LLImmediate::GetAllShapes_AssumedLocked(const FPhysicsActorHandle& InActorHandle, PhysicsInterfaceTypes::FInlineShapeArray& OutShapes, EPhysicsSceneType InSceneType);

//////////////////////////////////////////////////////////////////////////
// Actor implementation

bool FPhysicsActorHandle_LLImmediate::IsValid() const
{
	if(OwningScene)
	{
		FPhysInterface_LLImmediate::FActorRef* SceneRef = OwningScene->GetActorRef(*this);
		return SceneRef != nullptr;
	}

	return false;
}

bool FPhysicsActorHandle_LLImmediate::Equals(const FPhysicsActorHandle_LLImmediate& InOther) const
{
	if(OwningScene)
	{
		return OwningScene->GetActorRef(*this) == OwningScene->GetActorRef(InOther);
	}

	return false;
}

ImmediatePhysics::FActor* FPhysicsActorHandle_LLImmediate::GetActor() const
{
	if(OwningScene)
	{
		FPhysInterface_LLImmediate::FActorRef* SceneRef = OwningScene->GetActorRef(*this);

		if(SceneRef)
		{
			if(SceneRef->SimHandle)
			{
				return SceneRef->SimHandle->GetSimulationActor();
			}
			else if(SceneRef->PendingActorIndex != INDEX_NONE)
			{
				return &OwningScene->GetPendingActors()[SceneRef->PendingActorIndex].Actor;
			}
		}
	}

	return nullptr;
}

immediate::PxRigidBodyData* FPhysicsActorHandle_LLImmediate::GetActorData() const
{
	if(OwningScene)
	{
		FPhysInterface_LLImmediate::FActorRef* SceneRef = OwningScene->GetActorRef(*this);

		if(SceneRef)
		{
			if(SceneRef->SimHandle)
			{
				return SceneRef->SimHandle->GetSimulationRigidBodyData();
			}
			else if(SceneRef->PendingActorIndex)
			{
				return &OwningScene->GetPendingActors()[SceneRef->PendingActorIndex].ActorData.RigidBodyData;
			}
		}
	}

	return nullptr;
}

bool FPhysicsActorHandle_LLImmediate::IsStatic() const
{
	if(OwningScene)
	{
		FPhysInterface_LLImmediate::FActorRef* SceneRef = OwningScene->GetActorRef(*this);

		if(SceneRef)
		{
			if(SceneRef->SimHandle)
			{
				return !SceneRef->SimHandle->IsSimulated();
			}
			else if(SceneRef->PendingActorIndex)
			{
				ImmediatePhysics::FActorData& Data = OwningScene->GetPendingActors()[SceneRef->PendingActorIndex].ActorData;
				return Data.bStatic;
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

void FPhysInterface_LLImmediate::QueueNewActor(const FActorCreationParams& Params, FPhysicsActorHandle_LLImmediate& OutHandle)
{
	OutHandle.RefIndex = ActorRefs.Add(FActorRef());
	OutHandle.ComparisonId = ActorIdCounter++;
	FActorRef& NewRef = ActorRefs[OutHandle.RefIndex];

	PendingActors.AddDefaulted();
	FPendingActor& NewPendingActor = PendingActors.Last();
	NewPendingActor.ActorData = ImmediatePhysics::CreateActorData(Params);
	NewPendingActor.InterfaceHandle = OutHandle;

	NewRef.PendingActorIndex = PendingActors.Num() - 1;
	NewRef.ComparisonId = OutHandle.ComparisonId;
	NewRef.SimHandle = nullptr;
}

void FPhysInterface_LLImmediate::QueueReleaseActor(FPhysicsActorHandle& InHandle)
{
	FActorRef* Ref = GetActorRef(InHandle);

	if(Ref && Ref->SimHandle)
	{
		PendingRemoveActors.Add(Ref->SimHandle);
	}

	// Invalidate the external handle
	InHandle.RefIndex = INDEX_NONE;
	InHandle.ComparisonId = 0;
}

const FPhysInterface_LLImmediate::FActorRef* FPhysInterface_LLImmediate::GetActorRef(const FPhysicsActorHandle& InHandle) const
{
	const int32 ActorIndex = InHandle.RefIndex;
	const uint32 CompId = InHandle.ComparisonId;

	if(ActorIndex >= 0 && ActorIndex < ActorRefs.Num() && ActorRefs.IsAllocated(ActorIndex))
	{
		if(ActorRefs[ActorIndex].ComparisonId == CompId)
		{
			return &(ActorRefs[ActorIndex]);
		}
	}

	return nullptr;
}

FPhysInterface_LLImmediate::FActorRef* FPhysInterface_LLImmediate::GetActorRef(const FPhysicsActorHandle& InHandle)
{
	const int32 ActorIndex = InHandle.RefIndex;
	const uint32 CompId = InHandle.ComparisonId;

	if(ActorIndex >= 0 && ActorIndex < ActorRefs.Num() && ActorRefs.IsAllocated(ActorIndex))
	{
		if(ActorRefs[ActorIndex].ComparisonId == CompId)
		{
			return &(ActorRefs[ActorIndex]);
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
// Interface function implementations
//////////////////////////////////////////////////////////////////////////

FPhysInterface_LLImmediate::FPhysInterface_LLImmediate(const AWorldSettings* InWorldSettings /*= nullptr*/)
	: ActorIdCounter(0)
{
	Scene.SetCreateBodiesFunction([this](TArray<ImmediatePhysics::FActorHandle*>& ActorArray)
	{
		Callback_CreateActors(ActorArray);
	});
}

void FPhysInterface_LLImmediate::Callback_CreateActors(TArray<ImmediatePhysics::FActorHandle*>& ActorArray)
{
	ImmediatePhysics::FSimulation* Sim = Scene.GetImpl().GetSimulation();

	// First remove any pending remove actors
	for(ImmediatePhysics::FActorHandle* InternalHandle : PendingRemoveActors)
	{
		Sim->RemoveActor(InternalHandle);
	}

	PendingRemoveActors.Reset();

	for(FPendingActor& PendingActor : PendingActors)
	{
		check(PendingActor.InterfaceHandle.IsValid());

		// If we've removed a pending actor then this flag gets unset so we skip it
		// instead of handling handle rebasing as we're going to clear this list anyway
		if(!PendingActor.bValid)
		{
			continue;
		}

		FActorRef* ActorRef = GetActorRef(PendingActor.InterfaceHandle);

		ActorRef->SimHandle = Sim->InsertActorData(PendingActor.Actor, PendingActor.ActorData);
		ActorRef->PendingActorIndex = INDEX_NONE;
	}

	PendingActors.Reset();
}

FPhysicsActorHandle FPhysInterface_LLImmediate::CreateActor(const FActorCreationParams & Params)
{
	FPhysScene* InScene = Params.Scene;

	if(!InScene)
	{
		return FPhysicsActorHandle_LLImmediate();
	}

	FPhysicsActorHandle_LLImmediate NewHandle;

	NewHandle.OwningScene = InScene;
	InScene->QueueNewActor(Params, NewHandle);

	return NewHandle;
}

void FPhysInterface_LLImmediate::ReleaseActor(FPhysicsActorHandle_LLImmediate& InActorReference, FPhysScene* InScene /*= nullptr*/, bool bNeverDeferRelease /*= false*/)
{
	if(InActorReference.IsValid())
	{
		InScene->QueueReleaseActor(InActorReference);
	}
}

FPhysicsAggregateHandle_LLImmediate FPhysInterface_LLImmediate::CreateAggregate(int32 MaxBodies)
{
	return FPhysicsAggregateHandle_LLImmediate();
}

void FPhysInterface_LLImmediate::ReleaseAggregate(FPhysicsAggregateHandle_LLImmediate& InAggregate)
{

}

int32 FPhysInterface_LLImmediate::GetNumActorsInAggregate(const FPhysicsAggregateHandle_LLImmediate& InAggregate)
{
	return 0;
}

void FPhysInterface_LLImmediate::AddActorToAggregate_AssumesLocked(const FPhysicsAggregateHandle_LLImmediate& InAggregate, const FPhysicsActorHandle_LLImmediate& InActor)
{

}

FPhysicsMaterialHandle_LLImmediate FPhysInterface_LLImmediate::CreateMaterial(const UPhysicalMaterial* InMaterial)
{
	ImmediatePhysics::FMaterial* NewMat = new ImmediatePhysics::FMaterial();

	NewMat->StaticFriction = NewMat->DynamicFriction = InMaterial->Friction;
	NewMat->Restitution = InMaterial->Restitution;

	NewMat->FrictionCombineMode = InMaterial->FrictionCombineMode;
	NewMat->RestitutionCombineMode = InMaterial->RestitutionCombineMode;

	FPhysicsMaterialHandle_LLImmediate NewHandle;
	NewHandle.Material = NewMat;

	return NewHandle;
}

void FPhysInterface_LLImmediate::ReleaseMaterial(FPhysicsMaterialHandle_LLImmediate& InHandle)
{
	if(InHandle.IsValid())
	{
		delete InHandle.Material;
	}
}

void FPhysInterface_LLImmediate::UpdateMaterial(const FPhysicsMaterialHandle_LLImmediate& InHandle, UPhysicalMaterial* InMaterial)
{

}

FPhysScene* FPhysInterface_LLImmediate::GetCurrentScene(const FPhysicsActorHandle& InActorReference)
{
	return InActorReference.OwningScene;
}

void FPhysInterface_LLImmediate::CalculateMassPropertiesFromShapeCollection(physx::PxMassProperties& OutProperties, const TArray<FPhysicsShapeHandle>& InShapes, float InDensityKGPerCM)
{
	TArray<PxMassProperties> MassProps;
	TArray<PxTransform> LocalTransforms;

	const int32 NumShapes = InShapes.Num();

	MassProps.Reserve(NumShapes);
	LocalTransforms.Reserve(NumShapes);

	for(const FPhysicsShapeHandle_LLImmediate& Handle : InShapes)
	{
		if(Handle.IsValid())
		{
			MassProps.Add(PxMassProperties(*Handle.InnerShape->Geometry));
			LocalTransforms.Add(U2PTransform(GetLocalTransform(Handle)));
		}
	}

	OutProperties = PxMassProperties::sum(MassProps.GetData(), LocalTransforms.GetData(), MassProps.Num()) * InDensityKGPerCM;
}

FPhysicsShapeHandle FPhysInterface_LLImmediate::CreateShape(physx::PxGeometry* InGeom, bool bSimulation /*= true*/, bool bQuery /*= true*/, UPhysicalMaterial* InSimpleMaterial /*= nullptr*/, TArray<UPhysicalMaterial*>* InComplexMaterials /*= nullptr*/, bool bShared /*= false*/)
{
	// #PHYS2 a lot to handle here. Sim/Query - materials and shared shapes

	FPhysicsShapeHandle OutShapeHandle;
	OutShapeHandle.InnerShape = new ImmediatePhysics::FShape(PxTransform(PxIdentity), PxVec3(PxZero), 0.0f, InGeom, ImmediatePhysics::FMaterial());

	return OutShapeHandle;
}

void FPhysInterface_LLImmediate::ReleaseShape(FPhysicsShapeHandle& InShape)
{
	if(InShape.IsValid())
	{
		// Add any additional shutdown here

		// Free up the shape 
		delete InShape.InnerShape;
	}

	InShape.InnerShape = nullptr;
}

void FPhysInterface_LLImmediate::AddGeometry(const FPhysicsActorHandle_LLImmediate& InActor, const FGeometryAddParams& InParams, TArray<FPhysicsShapeHandle>* OutOptShapes /*= nullptr*/)
{
	auto AttachShape = [&](const PxGeometry& InGeometry, const PxTransform& InLocalTransform, const float InContactOffset, const float InRestOffset, const FPhysxUserData* InUserData, PxShapeFlags InShapeFlags) -> FPhysicsShapeHandle_LLImmediate
	{
		const bool bShapeSharing = InParams.bSharedShapes;
		const FBodyCollisionData& BodyCollisionData = InParams.CollisionData;

		// This is only using defaults #PHYS2 Handle materials fully, remove bouncing through PhysX types
		check(GEngine->DefaultPhysMaterial != NULL);
		const FPhysicsMaterialHandle& MaterialHandle = GEngine->DefaultPhysMaterial->GetPhysicsMaterial();
		UPhysicalMaterial* PhysMat = GEngine->DefaultPhysMaterial;

		const PxMaterial* PMaterial = GPhysXSDK->createMaterial(PhysMat->Friction, PhysMat->Friction, PhysMat->Restitution);
		PxShape* PNewShape = GPhysXSDK->createShape(InGeometry, *PMaterial, !bShapeSharing, InShapeFlags);

		if(PNewShape)
		{
			PNewShape->userData = (void*)InUserData;
			PNewShape->setLocalPose(InLocalTransform);

			PNewShape->setContactOffset(InContactOffset);
			PNewShape->setRestOffset(InRestOffset);

			const bool bSyncFlags = bShapeSharing || InParams.SceneType == PST_Sync;
			const bool bComplexShape = PNewShape->getGeometryType() == PxGeometryType::eTRIANGLEMESH;
			const bool bIsStatic = InActor.IsStatic();

			PxShapeFlags ShapeFlags = BuildPhysXShapeFlags(BodyCollisionData.CollisionFlags, bIsStatic, bSyncFlags, bComplexShape);

			PNewShape->setQueryFilterData(U2PFilterData(bComplexShape ? BodyCollisionData.CollisionFilterData.QueryComplexFilter : BodyCollisionData.CollisionFilterData.QuerySimpleFilter));
			PNewShape->setFlags(ShapeFlags);
			PNewShape->setSimulationFilterData(U2PFilterData(BodyCollisionData.CollisionFilterData.SimFilter));

			// PxShape has been built, transfer to internal types (#PHYS2 skip the PxShape conversion entirely in future)
			if(InActor.GetActor()->AddShape(PNewShape))
			{
				FPhysicsShapeHandle_LLImmediate TempHandle;
				TempHandle.InnerShape = &InActor.GetActor()->Shapes.Last();
				FBodyInstance::ApplyMaterialToShape_AssumesLocked(TempHandle, InParams.SimpleMaterial, InParams.ComplexMaterials, bShapeSharing);

				return TempHandle;
			}
		}

		return FPhysicsShapeHandle_LLImmediate();
	};
	
	auto SimpleIter = [AttachShape](const FKShapeElem& InShapeElement, const PxGeometry& InGeometry, const PxTransform& InLocalPose, float InContactOffset, float InRestOffset)
	{
		AttachShape(InGeometry, InLocalPose, InContactOffset, InRestOffset, InShapeElement.GetUserData(), PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE);
	};

	auto ComplexIter = [AttachShape](PxTriangleMesh* InTrimesh, const PxGeometry& InGeometry, const PxTransform& InLocalPose, float InContactOffset, float InRestOffset)
	{
		FPhysicsShapeHandle_LLImmediate Handle = AttachShape(InGeometry, InLocalPose, InContactOffset, InRestOffset, nullptr, PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eVISUALIZATION);
		if(!Handle.IsValid())
		{
			UE_LOG(LogPhysics, Log, TEXT("Can't create new mesh shape in AddGeometry"));
		}
	};

	if(InActor.IsValid())
	{
		check(InParams.Geometry);

		FBodySetupShapeIterator ShapeIterator(InParams.Scale, InParams.LocalTransform, InParams.bDoubleSided);

		FKAggregateGeom* AggGeom = InParams.Geometry;
		check(AggGeom);

		if(InParams.CollisionTraceType != ECollisionTraceFlag::CTF_UseComplexAsSimple)
		{
			ShapeIterator.ForEachShape<FKSphereElem, PxSphereGeometry>(AggGeom->SphereElems, SimpleIter);
			ShapeIterator.ForEachShape<FKSphylElem, PxCapsuleGeometry>(AggGeom->SphylElems, SimpleIter);
			ShapeIterator.ForEachShape<FKBoxElem, PxBoxGeometry>(AggGeom->BoxElems, SimpleIter);
			ShapeIterator.ForEachShape<FKConvexElem, PxConvexMeshGeometry>(AggGeom->ConvexElems, SimpleIter);
		}

		// Create tri-mesh shape, when we are not using simple collision shapes for 
		// complex queries as well
		if(InParams.CollisionTraceType != ECollisionTraceFlag::CTF_UseSimpleAsComplex)
		{
			ShapeIterator.ForEachShape<PxTriangleMesh*, PxTriangleMeshGeometry>(InParams.TriMeshes, ComplexIter);
		}

		if(OutOptShapes)
		{
			TArray<ImmediatePhysics::FShape>& ActorShapes = InActor.GetActor()->Shapes;
			OutOptShapes->Reserve(OutOptShapes->Num() + ActorShapes.Num());

			for(ImmediatePhysics::FShape& NewShape : ActorShapes)
			{
				FPhysicsShapeHandle_LLImmediate NewHandle;
				NewHandle.InnerShape = &NewShape;
				OutOptShapes->Add(NewHandle);
			}
		}
	}
}

FPhysicsShapeHandle FPhysInterface_LLImmediate::CloneShape(const FPhysicsShapeHandle& InShape)
{
	FPhysicsShapeHandle OutShapeHandle;

	if(InShape.IsValid())
	{
		OutShapeHandle.InnerShape = new ImmediatePhysics::FShape(*InShape.InnerShape);
	}

	return OutShapeHandle;
}

bool FPhysInterface_LLImmediate::IsSimulationShape(const FPhysicsShapeHandle& InShape)
{
	// for now everything is a simulation shape
	return true;
}

bool FPhysInterface_LLImmediate::IsQueryShape(const FPhysicsShapeHandle& InShape)
{
	return false;
}

bool FPhysInterface_LLImmediate::IsShapeType(const FPhysicsShapeHandle& InShape, ECollisionShapeType InType)
{
	return false;
}

ECollisionShapeType FPhysInterface_LLImmediate::GetShapeType(const FPhysicsShapeHandle& InShape)
{
	return ECollisionShapeType::None;
}

FPhysicsGeometryCollection FPhysInterface_LLImmediate::GetGeometryCollection(const FPhysicsShapeHandle& InShape)
{
	return FPhysicsGeometryCollection();
}

//////////////////////////////////////////////////////////////////////////
// Commands
//////////////////////////////////////////////////////////////////////////

bool FPhysicsCommand_LLImmediate::ExecuteRead(const FPhysicsActorHandle_LLImmediate& InActorReference, TFunctionRef<void(const FPhysicsActorHandle_LLImmediate& Actor)> InCallable)
{
	if(InActorReference.IsValid())
	{
		InCallable(InActorReference);
		return true;
	}

	return false;
}

bool FPhysicsCommand_LLImmediate::ExecuteRead(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable)
{
	InCallable();

	// Needs to be whether a read could actually have happened
	return true;
}

bool FPhysicsCommand_LLImmediate::ExecuteRead(const FPhysicsActorHandle_LLImmediate& InActorReferenceA, const FPhysicsActorHandle_LLImmediate& InActorReferenceB, TFunctionRef<void(const FPhysicsActorHandle_LLImmediate& ActorA, const FPhysicsActorHandle_LLImmediate& ActorB)> InCallable)
{
	if(InActorReferenceA.IsValid() || InActorReferenceB.IsValid())
	{
		InCallable(InActorReferenceA, InActorReferenceB);

		// Needs to be whether a read could actually have happened
		return true;
	}

	return false;
}

bool FPhysicsCommand_LLImmediate::ExecuteRead(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintHandle_LLImmediate& Constraint)> InCallable)
{
	if(InConstraintRef.IsValid())
	{
		InCallable(InConstraintRef);

		return true;
	}

	return false;
}

bool FPhysicsCommand_LLImmediate::ExecuteRead(FPhysScene* InScene, TFunctionRef<void()> InCallable)
{
	if(InScene)
	{
		InCallable();
		return true;
	}

	return false;
}

bool FPhysicsCommand_LLImmediate::ExecuteWrite(const FPhysicsActorHandle_LLImmediate& InActorReference, TFunctionRef<void(const FPhysicsActorHandle_LLImmediate& Actor)> InCallable)
{
	if(InActorReference.IsValid())
	{
		InCallable(InActorReference);
		return true;
	}

	return false;
}

bool FPhysicsCommand_LLImmediate::ExecuteWrite(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable)
{
	InCallable();

	// Needs to be whether a read could actually have happened
	return true;
}

bool FPhysicsCommand_LLImmediate::ExecuteWrite(const FPhysicsActorHandle_LLImmediate& InActorReferenceA, const FPhysicsActorHandle_LLImmediate& InActorReferenceB, TFunctionRef<void(const FPhysicsActorHandle_LLImmediate& ActorA, const FPhysicsActorHandle_LLImmediate& ActorB)> InCallable)
{
	if(InActorReferenceA.IsValid() || InActorReferenceB.IsValid())
	{
		InCallable(InActorReferenceA, InActorReferenceB);

		// Needs to be whether a read could actually have happened
		return true;
	}

	return false;
}

bool FPhysicsCommand_LLImmediate::ExecuteWrite(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintHandle_LLImmediate& Constraint)> InCallable)
{
	if(InConstraintRef.IsValid())
	{
		InCallable(InConstraintRef);

		return true;
	}

	return false;
}

bool FPhysicsCommand_LLImmediate::ExecuteWrite(FPhysScene* InScene, TFunctionRef<void()> InCallable)
{
	if(InScene)
	{
		InCallable();
		return true;
	}

	return false;
}

void FPhysicsCommand_LLImmediate::ExecuteShapeWrite(FBodyInstance* InInstance, FPhysicsShapeHandle_LLImmediate& InShape, TFunctionRef<void(const FPhysicsShapeHandle_LLImmediate& InShape)> InCallable)
{
	// Handle shape sharing?

	if(InShape.IsValid())
	{
		InCallable(InShape);
	}
}

//////////////////////////////////////////////////////////////////////////

FTransform FPhysInterface_LLImmediate::GetLocalTransform(const FPhysicsShapeHandle& InShape)
{
	return FTransform::Identity;
}

void* FPhysInterface_LLImmediate::GetUserData(const FPhysicsShapeHandle& InShape)
{
	if(InShape.IsValid())
	{
		return InShape.InnerShape->UserData;
	}

	return nullptr;
}

bool FPhysInterface_LLImmediate::LineTrace_Geom(FHitResult& OutHit, const FBodyInstance* InInstance, const FVector& InStart, const FVector& InEnd, bool bTraceComplex, bool bExtractPhysMaterial /*= false*/)
{
	return false;
}

bool FPhysInterface_LLImmediate::Sweep_Geom(FHitResult& OutHit, const FBodyInstance* InInstance, const FVector& InStart, const FVector& InEnd, const FQuat& InShapeRotation, const FCollisionShape& InShape, bool bSweepComplex)
{
	return false;
}

bool FPhysInterface_LLImmediate::Overlap_Geom(const FBodyInstance* InBodyInstance, const FPhysicsGeometryCollection& InGeometry, const FTransform& InShapeTransform, FMTDResult* OutOptResult /*= nullptr*/)
{
	return false;
}

bool FPhysInterface_LLImmediate::Overlap_Geom(const FBodyInstance* InBodyInstance, const FCollisionShape& InCollisionShape, const FQuat& InShapeRotation, const FTransform& InShapeTransform, FMTDResult* OutOptResult /*= nullptr*/)
{
	return false;
}

bool FPhysInterface_LLImmediate::GetSquaredDistanceToBody(const FBodyInstance* InInstance, const FVector& InPoint, float& OutDistanceSquared, FVector* OutOptPointOnBody /*= nullptr*/)
{
	return false;
}

void FPhysInterface_LLImmediate::SetUserData(const FPhysicsMaterialHandle_LLImmediate& InHandle, void* InUserData)
{

}

void FPhysInterface_LLImmediate::SetUserData(const FPhysicsShapeHandle& InShape, void* InUserData)
{
	if(InShape.IsValid())
	{
		InShape.InnerShape->UserData = InUserData;
	}
}

void FPhysInterface_LLImmediate::SetLocalTransform(const FPhysicsShapeHandle& InShape, const FTransform& NewLocalTransform)
{

}

void FPhysInterface_LLImmediate::AddForce_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InForce)
{

}

void FPhysInterface_LLImmediate::AddForceMassIndependent_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InForce)
{

}

void FPhysInterface_LLImmediate::AddTorqueMassIndependent_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InTorque)
{

}

void FPhysInterface_LLImmediate::AddImpulseAtLocation_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InImpulse, const FVector& InLocation)
{

}

void FPhysInterface_LLImmediate::AddRadialImpulse_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InOrigin, float InRadius, float InStrength, ERadialImpulseFalloff InFalloff, bool bInVelChange)
{

}

bool FPhysInterface_LLImmediate::IsGravityEnabled_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return false;
}

void FPhysInterface_LLImmediate::SetGravityEnabled_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, bool bEnabled)
{

}

float FPhysInterface_LLImmediate::GetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return 0.0f;
}

void FPhysInterface_LLImmediate::SetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, float InEnergyThreshold)
{

}

void FPhysInterface_LLImmediate::SetMass_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, float InMass)
{
	if(InHandle.IsValid())
	{
		InHandle.GetActorData()->invMass = 1.0f / InMass;
	}
}

void FPhysInterface_LLImmediate::SetMassSpaceInertiaTensor_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, const FVector& InTensor)
{
	if(InHandle.IsValid())
	{
		InHandle.GetActorData()->invInertia = PxVec3(1.0f / InTensor.X, 1.0f / InTensor.Y, 1.0f / InTensor.Z);
	}
}

void FPhysInterface_LLImmediate::SetComLocalPose_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, const FTransform& InComLocalPose)
{

}

float FPhysInterface_LLImmediate::GetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle)
{
	return 0.0f;
}

void FPhysInterface_LLImmediate::SetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, float InThreshold)
{

}

uint32 FPhysInterface_LLImmediate::GetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle)
{
	return 0;
}

void FPhysInterface_LLImmediate::SetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, uint32 InSolverIterationCount)
{

}

uint32 FPhysInterface_LLImmediate::GetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle)
{
	return 0;
}

void FPhysInterface_LLImmediate::SetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, uint32 InSolverIterationCount)
{

}

float FPhysInterface_LLImmediate::GetWakeCounter_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle)
{
	return 0.0f;
}

void FPhysInterface_LLImmediate::SetWakeCounter_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, float InWakeCounter)
{

}

SIZE_T FPhysInterface_LLImmediate::GetResourceSizeEx(const FPhysicsActorHandle_LLImmediate& InActorRef)
{
	return 0;
}

FPhysicsConstraintHandle_LLImmediate FPhysInterface_LLImmediate::CreateConstraint(const FPhysicsActorHandle_LLImmediate& InActorRef1, const FPhysicsActorHandle_LLImmediate& InActorRef2, const FTransform& InLocalFrame1, const FTransform& InLocalFrame2)
{
	return FPhysicsConstraintHandle_LLImmediate();
}

void FPhysInterface_LLImmediate::SetConstraintUserData(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, void* InUserData)
{

}

void FPhysInterface_LLImmediate::ReleaseConstraint(FPhysicsConstraintHandle_LLImmediate& InConstraintRef)
{

}

FTransform FPhysInterface_LLImmediate::GetLocalPose(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, EConstraintFrame::Type InFrame)
{
	return FTransform::Identity;
}

FTransform FPhysInterface_LLImmediate::GetGlobalPose(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, EConstraintFrame::Type InFrame)
{
	return FTransform::Identity;
}

FVector FPhysInterface_LLImmediate::GetLocation(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef)
{
	return FVector::ZeroVector;
}

void FPhysInterface_LLImmediate::GetForce(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, FVector& OutLinForce, FVector& OutAngForce)
{

}

void FPhysInterface_LLImmediate::GetDriveLinearVelocity(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, FVector& OutLinVelocity)
{

}

void FPhysInterface_LLImmediate::GetDriveAngularVelocity(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, FVector& OutAngVelocity)
{

}

float FPhysInterface_LLImmediate::GetCurrentSwing1(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef)
{
	return 0.0f;
}

float FPhysInterface_LLImmediate::GetCurrentSwing2(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef)
{
	return 0.0f;
}

float FPhysInterface_LLImmediate::GetCurrentTwist(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef)
{
	return 0.0f;
}

void FPhysInterface_LLImmediate::SetCanVisualize(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, bool bInCanVisualize)
{

}

void FPhysInterface_LLImmediate::SetCollisionEnabled(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, bool bInCollisionEnabled)
{

}

void FPhysInterface_LLImmediate::SetProjectionEnabled_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, bool bInProjectionEnabled, float InLinearTolerance /*= 0.0f*/, float InAngularToleranceDegrees /*= 0.0f*/)
{

}

void FPhysInterface_LLImmediate::SetParentDominates_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, bool bInParentDominates)
{

}

void FPhysInterface_LLImmediate::SetBreakForces_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InLinearBreakForce, float InAngularBreakForce)
{

}

void FPhysInterface_LLImmediate::SetLocalPose(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FTransform& InPose, EConstraintFrame::Type InFrame)
{

}

void FPhysInterface_LLImmediate::SetLinearMotionLimitType_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, PhysicsInterfaceTypes::ELimitAxis InAxis, ELinearConstraintMotion InMotion)
{

}

void FPhysInterface_LLImmediate::SetAngularMotionLimitType_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, PhysicsInterfaceTypes::ELimitAxis InAxis, EAngularConstraintMotion InMotion)
{

}

void FPhysInterface_LLImmediate::UpdateLinearLimitParams_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InLimit, float InAverageMass, const FLinearConstraint& InParams)
{

}

void FPhysInterface_LLImmediate::UpdateConeLimitParams_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InAverageMass, const FConeConstraint& InParams)
{

}

void FPhysInterface_LLImmediate::UpdateTwistLimitParams_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InAverageMass, const FTwistConstraint& InParams)
{

}

void FPhysInterface_LLImmediate::UpdateLinearDrive_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FLinearDriveConstraint& InDriveParams)
{

}

void FPhysInterface_LLImmediate::UpdateAngularDrive_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FAngularDriveConstraint& InDriveParams)
{

}

void FPhysInterface_LLImmediate::UpdateDriveTarget_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FLinearDriveConstraint& InLinDrive, const FAngularDriveConstraint& InAngDrive)
{

}

void FPhysInterface_LLImmediate::SetDrivePosition(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FVector& InPosition)
{

}

void FPhysInterface_LLImmediate::SetDriveOrientation(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FQuat& InOrientation)
{

}

void FPhysInterface_LLImmediate::SetDriveLinearVelocity(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FVector& InLinVelocity)
{

}

void FPhysInterface_LLImmediate::SetDriveAngularVelocity(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FVector& InAngVelocity)
{

}

void FPhysInterface_LLImmediate::SetTwistLimit(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InLowerLimit, float InUpperLimit, float InContactDistance)
{

}

void FPhysInterface_LLImmediate::SetSwingLimit(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InYLimit, float InZLimit, float InContactDistance)
{

}

void FPhysInterface_LLImmediate::SetLinearLimit(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InLimit)
{

}

bool FPhysInterface_LLImmediate::IsBroken(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef)
{
	return false;
}

bool FPhysInterface_LLImmediate::ExecuteOnUnbrokenConstraintReadOnly(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintHandle_LLImmediate&)> Func)
{
	return false;
}

bool FPhysInterface_LLImmediate::ExecuteOnUnbrokenConstraintReadWrite(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintHandle_LLImmediate&)> Func)
{
	return false;
}

void FPhysInterface_LLImmediate::SetKinematicTarget_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FTransform& InNewTarget)
{

}

FVector FPhysInterface_LLImmediate::GetLinearVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return FVector::ZeroVector;
}

void FPhysInterface_LLImmediate::SetLinearVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InNewVelocity, bool bAutoWake)
{

}

FVector FPhysInterface_LLImmediate::GetAngularVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return FVector::ZeroVector;
}

void FPhysInterface_LLImmediate::SetAngularVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InNewVelocity, bool bAutoWake)
{

}

float FPhysInterface_LLImmediate::GetMaxAngularVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return 0.0f;
}

void FPhysInterface_LLImmediate::SetMaxAngularVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, float InMaxAngularVelocity)
{

}

float FPhysInterface_LLImmediate::GetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return 0.0f;
}

void FPhysInterface_LLImmediate::SetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, float InMaxDepenetrationVelocity)
{

}

FVector FPhysInterface_LLImmediate::GetWorldVelocityAtPoint_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InPoint)
{
	return FVector::ZeroVector;
}

FTransform FPhysInterface_LLImmediate::GetComTransform_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return FTransform::Identity;
}

FVector FPhysInterface_LLImmediate::GetLocalInertiaTensor_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return FVector::ZeroVector;
}

FBox FPhysInterface_LLImmediate::GetBounds_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return FBox();
}

void FPhysInterface_LLImmediate::SetLinearDamping_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, float InDamping)
{

}

void FPhysInterface_LLImmediate::SetAngularDamping_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, float InDamping)
{

}

bool FPhysInterface_LLImmediate::HasKinematicTarget_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return false;
}

template<typename AllocatorType>
int32 GetAllShapesInternal_AssumedLocked(const FPhysicsActorHandle& InActorHandle, TArray<FPhysicsShapeHandle, AllocatorType>& OutShapes, EPhysicsSceneType InSceneType)
{
	if(ImmediatePhysics::FActor* Actor = InActorHandle.GetActor())
	{
		const int32 NumShapes = Actor->Shapes.Num();
		OutShapes.Reset(NumShapes);

		for(ImmediatePhysics::FShape& Shape : Actor->Shapes)
		{
			OutShapes.Add(FPhysicsShapeHandle_LLImmediate());
			FPhysicsShapeHandle_LLImmediate& CurrHandle = OutShapes.Last();

			CurrHandle.InnerShape = &Shape;
		}

		//for(int32 ShapeIndex = 0; ShapeIndex < NumShapes; ++ShapeIndex)
		//{
		//	OutShapes.Add(FPhysicsShapeHandle_LLImmediate());
		//
		//	FPhysicsShapeHandle_LLImmediate& CurrHandle = OutShapes.Last();
		//
		//	CurrHandle.OwningActorHandle = InActorHandle;
		//	CurrHandle.ShapeIndex = ShapeIndex;
		//}
	}

	return OutShapes.Num();
}

template <>
int32 FPhysInterface_LLImmediate::GetAllShapes_AssumedLocked(const FPhysicsActorHandle& InActorHandle, TArray<FPhysicsShapeHandle, FDefaultAllocator>& OutShapes, EPhysicsSceneType InSceneType)
{
	return GetAllShapesInternal_AssumedLocked(InActorHandle, OutShapes, InSceneType);
}

template <>
int32 FPhysInterface_LLImmediate::GetAllShapes_AssumedLocked(const FPhysicsActorHandle& InActorHandle, PhysicsInterfaceTypes::FInlineShapeArray& OutShapes, EPhysicsSceneType InSceneType)
{
	return GetAllShapesInternal_AssumedLocked(InActorHandle, OutShapes, InSceneType);
}

void FPhysInterface_LLImmediate::GetNumShapes(const FPhysicsActorHandle& InHandle, int32& OutNumSyncShapes, int32& OutNumAsyncShapes)
{
	OutNumSyncShapes = InHandle.GetActor()->Shapes.Num();
	OutNumAsyncShapes = 0;
}

void FPhysInterface_LLImmediate::AttachShape(const FPhysicsActorHandle& InActor, const FPhysicsShapeHandle& InNewShape)
{

}

void FPhysInterface_LLImmediate::AttachShape(const FPhysicsActorHandle& InActor, const FPhysicsShapeHandle& InNewShape, EPhysicsSceneType SceneType)
{

}

void FPhysInterface_LLImmediate::DetachShape(const FPhysicsActorHandle& InActor, FPhysicsShapeHandle& InShape, bool bWakeTouching /*= true*/)
{

}

void FPhysInterface_LLImmediate::SetActorUserData_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, FPhysxUserData* InUserData)
{
	InActorReference.GetActor()->UserData = InUserData;
}

bool FPhysInterface_LLImmediate::IsRigidBody(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return InActorReference.IsValid();
}

bool FPhysInterface_LLImmediate::IsDynamic(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return !IsStatic(InActorReference);
}

bool FPhysInterface_LLImmediate::IsStatic(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return InActorReference.IsStatic();
}

bool FPhysInterface_LLImmediate::IsKinematic_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return false;
}

bool FPhysInterface_LLImmediate::IsSleeping(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return false;
}

bool FPhysInterface_LLImmediate::IsCcdEnabled(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return false;
}

bool FPhysInterface_LLImmediate::IsInScene(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return false;
}

bool FPhysInterface_LLImmediate::CanSimulate_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return false;
}

float FPhysInterface_LLImmediate::GetMass_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return 0.0f;
}

void FPhysInterface_LLImmediate::SetSendsSleepNotifies_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, bool bSendSleepNotifies)
{

}

void FPhysInterface_LLImmediate::PutToSleep_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{

}

void FPhysInterface_LLImmediate::WakeUp_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{

}

void FPhysInterface_LLImmediate::SetIsKinematic_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, bool bIsKinematic)
{

}

void FPhysInterface_LLImmediate::SetCcdEnabled_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, bool bIsCcdEnabled)
{

}

FTransform FPhysInterface_LLImmediate::GetGlobalPose_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	if(InActorReference.IsValid())
	{
		FPhysInterface_LLImmediate* SceneInstance = InActorReference.OwningScene;

		int32 LowLevelDataIndex = SceneInstance->ActorRefs[InActorReference.RefIndex].SimHandle->ActorDataIndex;

		immediate::PxRigidBodyData& Data = SceneInstance->RigidBodiesData[LowLevelDataIndex];

		return P2UTransform(Data.body2World);
	}

	return FTransform::Identity;
}

void FPhysInterface_LLImmediate::SetGlobalPose_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FTransform& InNewPose, bool bAutoWake)
{

}

FTransform FPhysInterface_LLImmediate::GetTransform_AssumesLocked(const FPhysicsActorHandle& InRef, bool bForceGlobalPose /*= false*/)
{
	if(!bForceGlobalPose)
	{
		if(IsDynamic(InRef))
		{
			if(HasKinematicTarget_AssumesLocked(InRef))
			{
				return GetKinematicTarget_AssumesLocked(InRef);
			}
		}
	}

	return GetGlobalPose_AssumesLocked(InRef);
}

//////////////////////////////////////////////////////////////////////////
// Scene function implementations
//////////////////////////////////////////////////////////////////////////

void FPhysInterface_LLImmediate::AddActorsToScene_AssumesLocked(const TArray<FPhysicsActorHandle>& InActors)
{

}

void FPhysInterface_LLImmediate::RemoveBodyInstanceFromPendingLists_AssumesLocked(FBodyInstance* BodyInstance, int32 SceneType)
{

}

void FPhysInterface_LLImmediate::AddForce_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, bool bAllowSubstepping, bool bAccelChange)
{

}

void FPhysInterface_LLImmediate::AddForceAtPosition_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce /*= false*/)
{

}

void FPhysInterface_LLImmediate::AddRadialForceToBody_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, bool bAccelChange, bool bAllowSubstepping)
{

}

void FPhysInterface_LLImmediate::ClearForces_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping)
{

}

void FPhysInterface_LLImmediate::AddTorque_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InTorque)
{

}

void FPhysInterface_LLImmediate::AddTorque_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Torque, bool bAllowSubstepping, bool bAccelChange)
{

}

void FPhysInterface_LLImmediate::ClearTorques_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping)
{

}

void FPhysInterface_LLImmediate::SetKinematicTarget_AssumesLocked(FBodyInstance* BodyInstance, const FTransform& TargetTM, bool bAllowSubstepping)
{

}

FTransform FPhysInterface_LLImmediate::GetKinematicTarget_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference)
{
	return FTransform::Identity;
}

void FPhysInterface_LLImmediate::EndFrame(ULineBatchComponent* InLineBatcher)
{
	check(IsInGameThread());

	ImmediatePhysics::FSimulation* Simulation = Scene.GetImpl().GetSimulation();

	// Pull the body data out of the sim
	RigidBodiesData = Simulation->GetRigidBodyData();

	// #PHYS2 collision notifications here

	// Sync components, safe to probe the simulation here as we should be done with it
	typedef TTuple<TWeakObjectPtr<UPrimitiveComponent>, FTransform> FPendingTransform;
	TArray<FPendingTransform> PendingTransforms;

	TArray<ImmediatePhysics::FActorHandle*> LowLevelHandles = Simulation->GetActorHandles();

	for(ImmediatePhysics::FActorHandle* Handle : LowLevelHandles)
	{
		void* ActorUserData = Handle->GetSimulationActor()->UserData;
		ensure(!ActorUserData || !FPhysxUserData::IsGarbage(ActorUserData));
		FBodyInstance* BodyInstance = FPhysxUserData::Get<FBodyInstance>(ActorUserData);

		if(BodyInstance && BodyInstance->InstanceBodyIndex == INDEX_NONE && BodyInstance->OwnerComponent.IsValid())
		{
			check(BodyInstance->OwnerComponent->IsRegistered()); // shouldn't have a physics body for a non-registered component!

			const FTransform NewTransform = BodyInstance->GetUnrealWorldTransform_AssumesLocked();
			PendingTransforms.Add(FPendingTransform(BodyInstance->OwnerComponent, NewTransform));
		}
		// #PHYS2 Add custom syncs here? Might be required for skel meshes
	}

	for(FPendingTransform& PendingTransform : PendingTransforms)
	{
		UPrimitiveComponent* OwnerComponent = PendingTransform.Get<0>().Get();
		if(OwnerComponent)
		{
			AActor* OwnerActor = OwnerComponent->GetOwner();
			FTransform& NewTransform = PendingTransform.Get<1>();

			// See if the transform is actually different, and if so, move the component to match physics
			if(!NewTransform.EqualsNoScale(OwnerComponent->GetComponentTransform()))
			{
				const FVector MoveBy = NewTransform.GetLocation() - OwnerComponent->GetComponentTransform().GetLocation();
				const FQuat NewRotation = NewTransform.GetRotation();

				//@warning: do not reference BodyInstance again after calling MoveComponent() - events from the move could have made it unusable (destroying the actor, SetPhysics(), etc)
				OwnerComponent->MoveComponent(MoveBy, NewRotation, false, NULL, MOVECOMP_SkipPhysicsMove);
			}

			// Check if we didn't fall out of the world
			if(OwnerActor != NULL && !OwnerActor->IsPendingKill())
			{
				OwnerActor->CheckStillInWorld();
			}
		}
	}
}

bool FPhysInterface_LLImmediate::HandleExecCommands(const TCHAR* Cmd, FOutputDevice* Ar)
{
	return false;
}

void FPhysInterface_LLImmediate::ListAwakeRigidBodies(bool bIncludeKinematic)
{
	
}

int32 FPhysInterface_LLImmediate::GetNumAwakeBodies() const
{
	return 0;
}

#endif
