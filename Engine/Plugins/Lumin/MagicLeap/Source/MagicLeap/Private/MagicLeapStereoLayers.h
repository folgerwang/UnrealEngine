// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "DefaultStereoLayers.h"

class FMagicLeapStereoLayers : public FDefaultStereoLayers
{
public:
	FMagicLeapStereoLayers(const class FAutoRegister& AutoRegister, class FHeadMountedDisplayBase* InHmd) : FDefaultStereoLayers(AutoRegister, InHmd) {}

public:
	//~ IStereoLayers interface
	virtual FLayerDesc GetDebugCanvasLayerDesc(FTextureRHIRef Texture) override;
};
