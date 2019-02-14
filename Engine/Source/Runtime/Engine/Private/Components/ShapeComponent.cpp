// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "Components/ShapeComponent.h"
#include "AI/Navigation/NavAreaBase.h"
#include "AI/NavigationSystemBase.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/BodySetup.h"

#if WITH_PHYSX
	#include "PhysXPublic.h"
#endif // WITH_PHYSX



UShapeComponent::UShapeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static const FName CollisionProfileName(TEXT("OverlapAllDynamic"));
	SetCollisionProfileName(CollisionProfileName);
	BodyInstance.bAutoWeld = true;	//UShapeComponent by default has auto welding

	bHiddenInGame = true;
	bCastDynamicShadow = false;
	ShapeColor = FColor(223, 149, 157, 255);
	bShouldCollideWhenPlacing = false;

	bUseArchetypeBodySetup = !IsTemplate();
	
	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::Yes;
	bCanEverAffectNavigation = true;
	bDynamicObstacle = false;
	AreaClass = FNavigationSystem::GetDefaultObstacleArea();

	// Ignore streaming updates since GetUsedMaterials() is not implemented.
	bIgnoreStreamingManagerUpdate = true;
}

FPrimitiveSceneProxy* UShapeComponent::CreateSceneProxy()
{
	check( false && "Subclass needs to Implement this" );
	return NULL;
}

FBoxSphereBounds UShapeComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	check( false && "Subclass needs to Implement this" );
	return FBoxSphereBounds();
}

void UShapeComponent::UpdateBodySetup()
{
	check( false && "Subclass needs to Implement this" );
}

UBodySetup* UShapeComponent::GetBodySetup()
{
	UpdateBodySetup();
	return ShapeBodySetup;
}

#if WITH_EDITOR
void UShapeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!IsTemplate())
	{
		UpdateBodySetup(); // do this before reregistering components so that new values are used for collision
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

bool UShapeComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	return !bDynamicObstacle;
}

void UShapeComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	if (bDynamicObstacle)
	{
		Data.Modifiers.CreateAreaModifiers(this, AreaClass);
	}
}

bool UShapeComponent::IsNavigationRelevant() const
{
	// failed CanEverAffectNavigation() always takes priority
	// dynamic obstacle overrides collision check

	return (bDynamicObstacle && CanEverAffectNavigation()) || Super::IsNavigationRelevant();
}

template <> void UShapeComponent::AddShapeToGeomArray<FKBoxElem>() { ShapeBodySetup->AggGeom.BoxElems.Add(FKBoxElem()); }
template <> void UShapeComponent::AddShapeToGeomArray<FKSphereElem>() { ShapeBodySetup->AggGeom.SphereElems.Add(FKSphereElem()); }
template <> void UShapeComponent::AddShapeToGeomArray<FKSphylElem>() { ShapeBodySetup->AggGeom.SphylElems.Add(FKSphylElem()); }

#if WITH_PHYSX
template <>
void UShapeComponent::SetShapeToNewGeom<FKBoxElem>(const FPhysicsShapeHandle& Shape)
{
	FPhysicsInterface::SetUserData(Shape, (void*)ShapeBodySetup->AggGeom.BoxElems[0].GetUserData());
}

template <>
void UShapeComponent::SetShapeToNewGeom<FKSphereElem>(const FPhysicsShapeHandle& Shape)
{
	FPhysicsInterface::SetUserData(Shape, (void*)ShapeBodySetup->AggGeom.SphereElems[0].GetUserData());
}

template <>
void UShapeComponent::SetShapeToNewGeom<FKSphylElem>(const FPhysicsShapeHandle& Shape)
{
	FPhysicsInterface::SetUserData(Shape, (void*)ShapeBodySetup->AggGeom.SphylElems[0].GetUserData());
}
#endif

template <typename ShapeElemType>
void UShapeComponent::CreateShapeBodySetupIfNeeded()
{
	if (ShapeBodySetup == nullptr || ShapeBodySetup->IsPendingKill())
	{
		ShapeBodySetup = NewObject<UBodySetup>(this, NAME_None, RF_Transient);
		if (GUObjectArray.IsDisregardForGC(this))
		{
			ShapeBodySetup->AddToRoot();
		}

		// If this component is in GC cluster, make sure we add the body setup to it to
		ShapeBodySetup->AddToCluster(this);
		// if we got created outside of game thread, but got added to a cluster, 
		// we no longer need the Async flag
		if (ShapeBodySetup->HasAnyInternalFlags(EInternalObjectFlags::Async) && GUObjectClusters.GetObjectCluster(ShapeBodySetup))
		{
			ShapeBodySetup->ClearInternalFlags(EInternalObjectFlags::Async);
		}
		
		ShapeBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		AddShapeToGeomArray<ShapeElemType>();
		ShapeBodySetup->bNeverNeedsCookedCollisionData = true;
		bUseArchetypeBodySetup = false;	//We're making our own body setup, so don't use the archetype's.

		//Update bodyinstance and shapes
		BodyInstance.BodySetup = ShapeBodySetup;
		{
			if(BodyInstance.IsValidBodyInstance())
			{
#if WITH_PHYSX
				FPhysicsCommand::ExecuteWrite(BodyInstance.GetActorReferenceWithWelding(), [this](const FPhysicsActorHandle& Actor)
				{
					TArray<FPhysicsShapeHandle> Shapes;
					BodyInstance.GetAllShapes_AssumesLocked(Shapes);

					for(FPhysicsShapeHandle& Shape : Shapes)	//The reason we iterate is we may have multiple scenes and thus multiple shapes, but they are all pointing to the same geometry
					{
						//Update shape with the new body setup. Make sure to only update shapes owned by this body instance
						if(BodyInstance.IsShapeBoundToBody(Shape))
						{
							SetShapeToNewGeom<ShapeElemType>(Shape);
						}
					}
				});
#endif
			}
		}
	}
}

//Explicit instantiation of the different shape components
template void UShapeComponent::CreateShapeBodySetupIfNeeded<FKSphylElem>();
template void UShapeComponent::CreateShapeBodySetupIfNeeded<FKBoxElem>();
template void UShapeComponent::CreateShapeBodySetupIfNeeded<FKSphereElem>();
