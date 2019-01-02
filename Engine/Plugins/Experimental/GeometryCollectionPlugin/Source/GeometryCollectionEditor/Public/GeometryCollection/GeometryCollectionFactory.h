// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/** Factory which allows import of an GeometryCollectionAsset */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "GeometryCollection/GeometryCollectionObject.h"

#include "GeometryCollectionFactory.generated.h"


/**
* Factory for Simple Cube
*/

UCLASS()
class GEOMETRYCOLLECTIONEDITOR_API UGeometryCollectionFactory : public UFactory
{
    GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	

	static UGeometryCollection* StaticFactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn);

};


