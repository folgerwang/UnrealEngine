// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GauntletTestController.h"
#include "GauntletTestControllerBootTest.generated.h"

UCLASS()
class GAUNTLET_API UGauntletTestControllerBootTest : public UGauntletTestController
{
	GENERATED_BODY()

protected:

	virtual bool	IsBootProcessComplete()		const { return false; }
	void			OnTick(float TimeDelta)		override;
};

