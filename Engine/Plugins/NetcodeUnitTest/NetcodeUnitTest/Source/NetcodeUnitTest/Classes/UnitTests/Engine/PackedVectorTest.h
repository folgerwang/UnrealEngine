// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UnitTest.h"

#include "PackedVectorTest.generated.h"


/**
 * Basic unit test for WritePackedValue and ReadPackedValue
 */
UCLASS()
class UPackedVectorTest : public UUnitTest
{
	GENERATED_UCLASS_BODY()

private:
	virtual bool ExecuteUnitTest() override;
};
