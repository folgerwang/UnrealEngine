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
	class UDatasmithSceneImportData* Scene;
};
