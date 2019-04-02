// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "PhysicsPublic.h"
#include "PhysXPublic.h"
#include "Modules/ModuleManager.h"
#include "IPhysXVehiclesPlugin.h"
#include "WheeledVehicleMovementComponent.h"
#include "PhysXVehicleManager.h"
#include "UObject/UObjectIterator.h"
#include "Components/SkeletalMeshComponent.h"
#if ALLOW_CONSOLE
#include "Engine/Console.h"
#include "ConsoleSettings.h"
#endif // ALLOW_CONSOLE

class FPhysXVehiclesPlugin : public IPhysXVehiclesPlugin
{
private:

	void UpdatePhysXMaterial(UPhysicalMaterial* PhysicalMaterial)
	{
#if WITH_PHYSX_VEHICLES
		FPhysXVehicleManager::UpdateTireFrictionTable();
#endif // WITH_PHYSX
	}

	void PhysicsAssetChanged(const UPhysicsAsset* InPhysAsset)
	{
		for (FObjectIterator Iter(UWheeledVehicleMovementComponent::StaticClass()); Iter; ++Iter)
		{
			UWheeledVehicleMovementComponent * WheeledVehicleMovementComponent = Cast<UWheeledVehicleMovementComponent>(*Iter);
			if (USkeletalMeshComponent * SkeltalMeshComponent = Cast<USkeletalMeshComponent>(WheeledVehicleMovementComponent->UpdatedComponent))
			{
				if (SkeltalMeshComponent->GetPhysicsAsset() == InPhysAsset)
				{
					//Need to recreate car data
					WheeledVehicleMovementComponent->RecreatePhysicsState();
				}

			}
		}
	}

	void PhysSceneInit(FPhysScene* PhysScene)
	{
#if WITH_PHYSX_VEHICLES
		new FPhysXVehicleManager(PhysScene);
#endif // WITH_PHYSX
	}

	void PhysSceneTerm(FPhysScene* PhysScene)
	{
#if WITH_PHYSX_VEHICLES
		FPhysXVehicleManager* VehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(PhysScene);
		if (VehicleManager != nullptr)
		{
			VehicleManager->DetachFromPhysScene(PhysScene);
			delete VehicleManager;
			VehicleManager = nullptr;
		}
#endif // WITH_PHYSX
	}

	FDelegateHandle OnUpdatePhysXMaterialHandle;
	FDelegateHandle OnPhysicsAssetChangedHandle;
	FDelegateHandle OnPhysSceneInitHandle;
	FDelegateHandle OnPhysSceneTermHandle;

public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
#if WITH_PHYSX_VEHICLES
		PxInitVehicleSDK(*GPhysXSDK);
#endif // WITH_PHYSX

		OnUpdatePhysXMaterialHandle = FPhysicsDelegates::OnUpdatePhysXMaterial.AddRaw(this, &FPhysXVehiclesPlugin::UpdatePhysXMaterial);
		OnPhysicsAssetChangedHandle = FPhysicsDelegates::OnPhysicsAssetChanged.AddRaw(this, &FPhysXVehiclesPlugin::PhysicsAssetChanged);
		OnPhysSceneInitHandle = FPhysicsDelegates::OnPhysSceneInit.AddRaw(this, &FPhysXVehiclesPlugin::PhysSceneInit);
		OnPhysSceneTermHandle = FPhysicsDelegates::OnPhysSceneTerm.AddRaw(this, &FPhysXVehiclesPlugin::PhysSceneTerm);

#if ALLOW_CONSOLE
		UConsole::RegisterConsoleAutoCompleteEntries.AddStatic(&FPhysXVehiclesPlugin::PopulateAutoCompleteEntries);
#endif // ALLOW_CONSOLE
	}

	virtual void ShutdownModule() override
	{
		FPhysicsDelegates::OnUpdatePhysXMaterial.Remove(OnUpdatePhysXMaterialHandle);
		FPhysicsDelegates::OnPhysicsAssetChanged.Remove(OnPhysicsAssetChangedHandle);
		FPhysicsDelegates::OnPhysSceneInit.Remove(OnPhysSceneInitHandle);
		FPhysicsDelegates::OnPhysSceneTerm.Remove(OnPhysSceneTermHandle);

#if WITH_PHYSX_VEHICLES
		if (GPhysXSDK != NULL)
		{
			PxCloseVehicleSDK();
		}
#endif // WITH_PHYSX
	}

#if ALLOW_CONSOLE
	static void PopulateAutoCompleteEntries(TArray<FAutoCompleteCommand>& AutoCompleteList)
	{
		const UConsoleSettings* ConsoleSettings = GetDefault<UConsoleSettings>();

		AutoCompleteList.AddDefaulted();

		FAutoCompleteCommand& AutoCompleteCommand = AutoCompleteList.Last();
		AutoCompleteCommand.Command = TEXT("ShowDebug VEHICLE");
		AutoCompleteCommand.Desc = TEXT("Toggles display of vehicle information");
		AutoCompleteCommand.Color = ConsoleSettings->AutoCompleteCommandColor;
	}
#endif // ALLOW_CONSOLE
};

IMPLEMENT_MODULE(FPhysXVehiclesPlugin, PhysXVehicles )







