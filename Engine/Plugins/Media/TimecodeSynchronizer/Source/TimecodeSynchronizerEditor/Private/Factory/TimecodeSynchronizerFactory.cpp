// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TimecodeSynchronizerFactory.h"

#include "TimecodeSynchronizer.h"

UTimecodeSynchronizerFactory::UTimecodeSynchronizerFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UTimecodeSynchronizer::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UTimecodeSynchronizerFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	ensure(UTimecodeSynchronizer::StaticClass() == Class);
	ensure(0 != (RF_Public & Flags));
	UTimecodeSynchronizer* TimecodeSynchronizer = NewObject<UTimecodeSynchronizer>(InParent, Name, Flags);
	return TimecodeSynchronizer;
}
