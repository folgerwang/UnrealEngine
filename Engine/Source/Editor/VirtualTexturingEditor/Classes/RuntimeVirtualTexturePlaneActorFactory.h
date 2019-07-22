// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorFactories/ActorFactory.h"
#include "RuntimeVirtualTexturePlaneActorFactory.generated.h"

/** Actor factory for ARuntimeVirtualTexturePlane */
UCLASS(MinimalAPI, config=Editor, collapsecategories, hidecategories=Object)
class URuntimeVirtualTexturePlaneActorFactory : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UActorFactory Interface
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	//~ End UActorFactory Interface
};
