// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "DatasmithSceneActor.generated.h"

UCLASS()
class DATASMITHCONTENT_API ADatasmithSceneActor : public AActor
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere, Category="Datasmith")
	class UDatasmithScene* Scene;

	/** Map of all the actors related to this Datasmith Scene */
	UPROPERTY(VisibleAnywhere, Category="Datasmith", AdvancedDisplay)
	TMap< FName, TSoftObjectPtr< AActor > > RelatedActors;
};
