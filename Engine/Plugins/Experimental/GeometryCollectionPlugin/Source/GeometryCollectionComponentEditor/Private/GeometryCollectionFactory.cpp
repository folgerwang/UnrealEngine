// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollectionFactory.h"

#define LOCTEXT_NAMESPACE "GeometryCollection"

/////////////////////////////////////////////////////
// GeometryCollectionFactory

UGeometryCollectionFactory::UGeometryCollectionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UGeometryCollection::StaticClass();
}

UObject* UGeometryCollectionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UGeometryCollection* NewGeometryCollection = NewObject<UGeometryCollection>(InParent, Class, Name, Flags | RF_Transactional | RF_Public | RF_Standalone);
	return NewGeometryCollection;
}

#undef LOCTEXT_NAMESPACE



