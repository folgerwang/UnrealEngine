// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NavAreas/NavAreaMeta.h"

UNavAreaMeta::UNavAreaMeta(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	bIsMetaArea = true;
}

TSubclassOf<UNavArea> UNavAreaMeta::PickAreaClass(TSubclassOf<UNavArea> AreaClass, const AActor* Actor, const FNavAgentProperties& NavAgent)
{
	return Actor
		? TSubclassOf<UNavArea>(UNavAreaBase::PickAreaClassForAgent(AreaClass, *Actor, NavAgent))
		: AreaClass;
}

TSubclassOf<UNavArea> UNavAreaMeta::PickAreaClass(const AActor* Actor, const FNavAgentProperties& NavAgent)
{
	return Actor
		? TSubclassOf<UNavArea>(PickAreaClassForAgent(*Actor, NavAgent))
		: TSubclassOf<UNavArea>(GetClass());
}
