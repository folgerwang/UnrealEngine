// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigUnitEditor_Base.h"
#include "Units/RigUnit_TwoBoneIKFK.h"
#include "RigUnitEditor_TwoBoneIKFK.generated.h"

struct FRigUnit_TwoBoneIKFK;
/**
  * This is the base class for any Rig unit editor features. This class can be derived to your RigUnit if you want editor functionality
  * You'll have to register the proper class for your RigUnit
  */
UCLASS(BlueprintType)
class CONTROLRIGEDITOR_API URigUnitEditor_TwoBoneIKFK : public URigUnitEditor_Base
{
	GENERATED_UCLASS_BODY()

public:
	/* 
	 * Only interface that can work right now is this signature 
	 */
	UFUNCTION(BlueprintCallable, Category = "ControlRig")
	void Snap();

private:
	void MatchToIK(FRigUnit_TwoBoneIKFK* RigUnit_IKFK) const;
	void MatchToFK(FRigUnit_TwoBoneIKFK* RigUnit_IKFK) const;

	virtual FString GetDisplayName() const override;
};

