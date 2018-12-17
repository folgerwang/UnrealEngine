// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClothingSimulationFactoryInterface.h"

#include "ChaosClothingSimulationFactory.generated.h"

UCLASS()
class CHAOSCLOTH_API UChaosClothingSimulationFactory : public UClothingSimulationFactory
{
    GENERATED_BODY()
  public:
    virtual IClothingSimulation* CreateSimulation() override;
    virtual void DestroySimulation(IClothingSimulation* InSimulation) override;
    virtual bool SupportsAsset(UClothingAssetBase* InAsset) override;
    virtual bool SupportsRuntimeInteraction() override;
};
