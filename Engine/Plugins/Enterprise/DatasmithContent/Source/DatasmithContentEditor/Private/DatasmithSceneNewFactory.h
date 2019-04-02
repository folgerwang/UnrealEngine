// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "UObject/StrongObjectPtr.h"

#include "DatasmithSceneNewFactory.generated.h"

/** A factory for DatasmithScene assets. */
UCLASS(hidecategories = Object, MinimalAPI)
class UDatasmithSceneNewFactory : public UFactory
{
	GENERATED_BODY()

public:
	UDatasmithSceneNewFactory();

	/** Begin UFactory override */
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool CanCreateNew() const override { return true; }
	/** End UFactory override */
};
