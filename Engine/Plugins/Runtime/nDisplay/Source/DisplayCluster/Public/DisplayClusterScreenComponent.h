// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterSceneComponent.h"
#include "DisplayClusterScreenComponent.generated.h"


/**
 * Projection screen component
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DISPLAYCLUSTER_API UDisplayClusterScreenComponent
	: public UDisplayClusterSceneComponent
{
	GENERATED_BODY()

public:
	UDisplayClusterScreenComponent(const FObjectInitializer& ObjectInitializer);

public:
	virtual void SetSettings(const FDisplayClusterConfigSceneNode* pConfig) override;
	virtual bool ApplySettings() override;

	inline FVector2D GetScreenSize() const
	{ return Size; }

public:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FVector2D Size;

	UPROPERTY(VisibleAnywhere, Category = Mesh)
	UStaticMeshComponent* ScreenGeometryComponent = nullptr;
};
