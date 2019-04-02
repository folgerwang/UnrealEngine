// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ChaosClothingSimulationFactory.h"
#include "Assets/ClothingAsset.h"
#include "ChaosClothingSimulation.h"

IClothingSimulation* UChaosClothingSimulationFactory::CreateSimulation()
{
    return new Chaos::ClothingSimulation();
}

void UChaosClothingSimulationFactory::DestroySimulation(IClothingSimulation* InSimulation)
{
    delete InSimulation;
}

bool UChaosClothingSimulationFactory::SupportsAsset(UClothingAssetBase* InAsset)
{
    return true;
}

bool UChaosClothingSimulationFactory::SupportsRuntimeInteraction()
{
    return false;
}
