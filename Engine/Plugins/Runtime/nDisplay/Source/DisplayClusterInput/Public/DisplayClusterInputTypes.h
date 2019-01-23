// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

#include "DisplayClusterInputTypes.generated.h"


// Keyboard reflection modes
UENUM(Blueprintable)
enum class EDisplayClusterInputKeyboardReflectMode : uint8
{
	Refl_nDisplay UMETA(DisplayName = "nDisplay buttons only"),
	Refl_UECore   UMETA(DisplayName = "Native UE4 keyboard events"),
	Refl_Both     UMETA(DisplayName = "Both nDisplay and UE4 native"),
	Refl_None     UMETA(DisplayName = "No reflection")
};


// Binding description. Maps VRPN device channel to an UE4 FKey
USTRUCT(BlueprintType)
struct FDisplayClusterInputBinding
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DisplayCluster|Input")
	int32 VrpnChannel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DisplayCluster|Input")
	FKey Target;

	FDisplayClusterInputBinding(const FKey InKey = EKeys::Invalid)
		: VrpnChannel(0)
		, Target(InKey)
	{ }
};
