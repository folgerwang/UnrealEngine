// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Info.h"
#include "MagicLeapEmulatorBackgroundMarker.generated.h"

class FMagicLeapEmulator;

UCLASS(ClassGroup = MagicLeap)
class MAGICLEAPEMULATOR_API AMagicLeapEmulatorBackgroundMarker : public AInfo
{
  GENERATED_BODY()

public:
	AMagicLeapEmulatorBackgroundMarker();

	void Tick(float DeltaTime);

	/** True to mark the level containing this actor as a "background" level for the ML emulator. False means it can be part of the emulated AR scene */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magic Leap")
	uint32 bParentLevelIsBackgroundLevel : 1;

	FMagicLeapEmulator* Emulator;
};

