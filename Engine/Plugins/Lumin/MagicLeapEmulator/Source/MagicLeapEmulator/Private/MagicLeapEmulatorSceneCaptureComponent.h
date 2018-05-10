// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "MagicLeapEmulatorSceneCaptureComponent.generated.h"

class FMagicLeapEmulator;

UCLASS(ClassGroup = MagicLeap)
class UMagicLeapEmulatorSceneCaptureComponent : public USceneCaptureComponent2D
{
  GENERATED_BODY()

public:
	UMagicLeapEmulatorSceneCaptureComponent();

	virtual void UpdateSceneCaptureContents(FSceneInterface* Scene) override;

	FMagicLeapEmulator* Emulator;
};

