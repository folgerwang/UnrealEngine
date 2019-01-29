// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/WidgetInteractionComponent.h"
#include "GoogleVRWidgetInteractionComponent.generated.h"

UCLASS(ClassGroup=(GoogleVRController), meta=(BlueprintSpawnableComponent))
class GOOGLEVRCONTROLLER_API UGoogleVRWidgetInteractionComponent : public UWidgetInteractionComponent
{
	GENERATED_BODY()

public:

	UGoogleVRWidgetInteractionComponent(const FObjectInitializer& ObjectInitializer);

	void UpdateState(const FHitResult& HitResult);

protected:
	virtual FWidgetPath FindHoveredWidgetPath(const FWidgetTraceResult& HitResult) const override;
};
