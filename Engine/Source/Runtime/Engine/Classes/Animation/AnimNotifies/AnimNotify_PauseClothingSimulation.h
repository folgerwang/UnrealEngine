// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNotify.h"

#include "AnimNotify_PauseClothingSimulation.generated.h"

UCLASS(const, hidecategories=Object, collapsecategories, meta=(DisplayName="Pause Clothing Simulation"))
class ENGINE_API UAnimNotify_PauseClothingSimulation : public UAnimNotify
{
	GENERATED_BODY()

public:

	UAnimNotify_PauseClothingSimulation();

	// Begin UAnimNotify interface
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
	// End UAnimNotify interface

};



