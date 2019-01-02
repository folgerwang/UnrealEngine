// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Math/Vector.h"
#include "Chaos/PBDCollisionTypes.h"
#include "Physics/Experimental/PhysScene_Chaos.h"

#include "ChaosSolver.generated.h"

class UChaosSolver;

/**
* UChaosSolver (UObject)
*
*/
UCLASS(customconstructor)
class CHAOSSOLVERENGINE_API UChaosSolver : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UChaosSolver(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	bool IsVisible() { return true; }

#if INCLUDE_CHAOS
	static TSharedPtr<FPhysScene_Chaos> GetSolver()
	{
		return FPhysScene_Chaos::GetInstance();
	}
#endif

protected:
};



