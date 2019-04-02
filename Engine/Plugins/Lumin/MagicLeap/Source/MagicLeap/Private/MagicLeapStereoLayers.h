// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "DefaultStereoLayers.h"

class IConsoleVariable;

class FMagicLeapStereoLayers : public FDefaultStereoLayers
{
public:
	FMagicLeapStereoLayers(const class FAutoRegister& AutoRegister, class FHeadMountedDisplayBase* InHmd);

public:
	//~ IStereoLayers interface
	virtual FLayerDesc GetDebugCanvasLayerDesc(FTextureRHIRef Texture) override;

protected:
	const float DefaultX;
	const float DefaultY;
	const float DefaultZ;
	const float DefaultWidth;
	const float DefaultHeight;
	IConsoleVariable* CVarX;
	IConsoleVariable* CVarY;
	IConsoleVariable* CVarZ;
	IConsoleVariable* CVarWidth;
	IConsoleVariable* CVarHeight;
};
