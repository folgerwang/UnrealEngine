// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "NavAreas/NavArea.h"
#include "NavAreaMeta.generated.h"

class AActor;

/** A convenience class for an area that has IsMetaArea() == true.
 *	Do not use this class when determining whether an area class is "meta". 
 *	Call IsMetaArea instead. */
UCLASS(Abstract)
class NAVIGATIONSYSTEM_API UNavAreaMeta : public UNavArea
{
	GENERATED_BODY()

public:
	UNavAreaMeta(const FObjectInitializer& ObjectInitializer);

	UE_DEPRECATED(4.20, "UNavAreaMeta::PickAreaClass is deprecated. Use UNavArea::PickAreaClassForAgent instead")
	static TSubclassOf<UNavArea> PickAreaClass(TSubclassOf<UNavArea> AreaClass, const AActor* Actor, const FNavAgentProperties& NavAgent);

	UE_DEPRECATED(4.20, "UNavAreaMeta::PickAreaClass is deprecated. Use UNavArea::PickAreaClassForAgent instead")
	virtual TSubclassOf<UNavArea> PickAreaClass(const AActor* Actor, const FNavAgentProperties& NavAgent);
};
