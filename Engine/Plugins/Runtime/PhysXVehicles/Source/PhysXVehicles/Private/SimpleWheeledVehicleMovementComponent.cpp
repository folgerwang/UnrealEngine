// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SimpleWheeledVehicleMovementComponent.h"
#include "Components/PrimitiveComponent.h"

#if WITH_PHYSX
#include "PhysXPublic.h"
#include "PhysXVehicleManager.h"
#endif // WITH_PHYSX

void USimpleWheeledVehicleMovementComponent::SetBrakeTorque(float BrakeTorque, int32 WheelIndex)
{
#if WITH_PHYSX_VEHICLES
	if (PVehicle && UpdatedPrimitive)
	{
		PxVehicleWheels* PVehicleLocal = PVehicle;
		if (WheelSetups.IsValidIndex(WheelIndex))
		{
			FBodyInstance* TargetInstance = UpdatedPrimitive->GetBodyInstance();
			const FPhysicsActorHandle& ActorHandle = TargetInstance->GetPhysicsActorHandle();
			if(ActorHandle.IsValid())
			{
				FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
				{
					if(FPhysicsInterface::IsDynamic(Actor))
					{
						((PxVehicleNoDrive*)PVehicleLocal)->setBrakeTorque(WheelIndex, M2ToCm2(BrakeTorque));
					}
				});
			}
		}
	}
#endif // WITH_PHYSX
}

void USimpleWheeledVehicleMovementComponent::SetDriveTorque(float DriveTorque, int32 WheelIndex)
{
#if WITH_PHYSX_VEHICLES
	if (PVehicle && UpdatedPrimitive)
	{
		PxVehicleWheels* PVehicleLocal = PVehicle;
		if (WheelSetups.IsValidIndex(WheelIndex))
		{
			FBodyInstance* TargetInstance = UpdatedPrimitive->GetBodyInstance();
			const FPhysicsActorHandle& ActorHandle = TargetInstance->GetPhysicsActorHandle();

			FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
			{
				if(FPhysicsInterface::IsDynamic(ActorHandle))
				{
					((PxVehicleNoDrive*)PVehicleLocal)->setDriveTorque(WheelIndex, M2ToCm2(DriveTorque));
				}
			});
		}
	}
#endif // WITH_PHYSX
}

void USimpleWheeledVehicleMovementComponent::SetSteerAngle(float SteerAngle, int32 WheelIndex)
{
#if WITH_PHYSX_VEHICLES
	if (PVehicle && UpdatedPrimitive)
	{
		PxVehicleWheels* PVehicleLocal = PVehicle;
		if (WheelSetups.IsValidIndex(WheelIndex))
		{
			const float SteerRad = FMath::DegreesToRadians(SteerAngle);

			FBodyInstance* TargetInstance = UpdatedPrimitive->GetBodyInstance();
			const FPhysicsActorHandle& ActorHandle = TargetInstance->GetPhysicsActorHandle();

			FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
			{
				if(FPhysicsInterface::IsDynamic(Actor))
				{
					((PxVehicleNoDrive*)PVehicleLocal)->setSteerAngle(WheelIndex, SteerRad);
				}
			});
		}
	}
#endif // WITH_PHYSX
}

#if WITH_PHYSX_VEHICLES
void USimpleWheeledVehicleMovementComponent::SetupVehicleDrive(PxVehicleWheelsSimData* PWheelsSimData)
{
	//Use a simple PxNoDrive which will give us suspension but no engine forces which we leave to the user

	// Create the vehicle
	PxVehicleNoDrive* PVehicleNoDrive = PxVehicleNoDrive::allocate(WheelSetups.Num());
	check(PVehicleNoDrive);

	FBodyInstance* TargetInstance = UpdatedPrimitive->GetBodyInstance();
	const FPhysicsActorHandle& ActorHandle = TargetInstance->GetPhysicsActorHandle();

	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		PxRigidActor* PActor = FPhysicsInterface::GetPxRigidActor_AssumesLocked(Actor);
		if(PxRigidDynamic* PDynamic = PActor->is<PxRigidDynamic>())
		{
			PVehicleNoDrive->setup(GPhysXSDK, PDynamic, *PWheelsSimData);
			PVehicleNoDrive->setToRestState();

			// cleanup
			PWheelsSimData->free();
		}
	});

	// cache values
	PVehicle = PVehicleNoDrive;
}

#endif // WITH_PHYSX_VEHICLES
