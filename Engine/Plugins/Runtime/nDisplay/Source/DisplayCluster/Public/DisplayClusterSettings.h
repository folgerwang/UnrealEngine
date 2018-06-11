// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "GameFramework/Actor.h"
#include "DisplayClusterSettings.generated.h"


/**
 * Per-level custom settings
 */
UCLASS()
class DISPLAYCLUSTER_API ADisplayClusterSettings
	: public AActor
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	ADisplayClusterSettings(const FObjectInitializer& ObjectInitializer);
	virtual ~ADisplayClusterSettings();

public:
	UPROPERTY(EditAnywhere, Category = "DisplayCluster (Editor only)", meta = (DisplayName = "Config file"))
	FString EditorConfigPath;

	UPROPERTY(EditAnywhere, Category = "DisplayCluster (Editor only)", meta = (DisplayName = "Node ID"))
	FString EditorNodeId;

	UPROPERTY(EditAnywhere, Category = "DisplayCluster (Editor only)", meta = (DisplayName = "Show projection screens"))
	bool bEditorShowProjectionScreens;

	UPROPERTY(EditAnywhere, Category = "DisplayCluster|Pawn", meta = (DisplayName = "Enable DisplayCluster collisions"))
	bool bEnableCollisions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DisplayCluster|Pawn|Control|Movement", meta = (DisplayName = "Max speed", ClampMin = "0.0", ClampMax = "1000000.0", UIMin = "0.0", UIMax = "1000000.0"))
	float MovementMaxSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DisplayCluster|Pawn|Control|Movement", meta = (DisplayName = "Acceleration", ClampMin = "0.0", ClampMax = "1000000.0", UIMin = "0.0", UIMax = "1000000.0"))
	float MovementAcceleration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DisplayCluster|Pawn|Control|Movement", meta = (DisplayName = "Deceleration", ClampMin = "0.0", ClampMax = "1000000.0", UIMin = "0.0", UIMax = "1000000.0"))
	float MovementDeceleration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DisplayCluster|Pawn|Control|Movement", meta = (DisplayName = "Turning boost", ClampMin = "0.0", ClampMax = "1000000.0", UIMin = "0.0", UIMax = "1000000.0"))
	float MovementTurningBoost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DisplayCluster|Pawn|Control|Rotation", meta = (DisplayName = "Speed", ClampMin = "0.0", ClampMax = "360.0", UIMin = "0.0", UIMax = "360.0"))
	float RotationSpeed;
};
