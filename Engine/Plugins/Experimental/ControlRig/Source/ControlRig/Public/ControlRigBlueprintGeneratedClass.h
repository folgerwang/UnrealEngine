// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "ControlRigBlueprintGeneratedClass.generated.h"

UCLASS()
class CONTROLRIG_API UControlRigBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_BODY()

public:
	UControlRigBlueprintGeneratedClass();

	// UStruct interface
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;

	// UClass interface
	virtual void PurgeClass(bool bRecompilingOnLoad) override;
	virtual uint8* GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const override;

public:
#if WITH_EDITORONLY_DATA
	/** The properties of all of the control units */
	TArray<UStructProperty*> ControlUnitProperties;

	/** The properties of all the rig units */
	TArray<UStructProperty*> RigUnitProperties;
#endif
};