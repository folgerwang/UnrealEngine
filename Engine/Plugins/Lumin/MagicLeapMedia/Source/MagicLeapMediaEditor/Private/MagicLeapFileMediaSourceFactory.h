// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "MagicLeapFileMediaSourceFactory.generated.h"

/**
 * Implements a factory for UFileMediaSource objects.
 */
UCLASS(hidecategories=Object)
class UMagicLeapFileMediaSourceFactory : public UFactory
{
  GENERATED_UCLASS_BODY()

public:
  /** UFactory interface */
  virtual bool FactoryCanImport(const FString& Filename) override;
  virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
};
