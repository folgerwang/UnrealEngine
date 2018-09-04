// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Engine/Blueprint.h"
#include "KismetCompiler.h"

#include "LevelSequenceDirectorBlueprint.generated.h"

class ULevelSequence;

UCLASS(BlueprintType)
class ULevelSequenceDirectorBlueprint : public UBlueprint
{
	GENERATED_BODY()

	ULevelSequenceDirectorBlueprint(const FObjectInitializer& ObjInit);

public:

	UPROPERTY()
	ULevelSequence* OwnerSequence;

	//~ Begin UBlueprint Interface
	virtual UClass* GetBlueprintClass() const override;
	virtual bool SupportedByDefaultBlueprintFactory() const override;
	virtual bool AlwaysCompileOnLoad() const override;
	virtual bool SupportsNativization(FText* OutReason) const override;

private:

#if WITH_EDITOR
	virtual void LoadModulesRequiredForCompilation() override;
#endif
};

class FLevelSequenceDirectorBlueprintCompiler : public FKismetCompilerContext
{
public:

	FLevelSequenceDirectorBlueprintCompiler(UBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions, TArray<UObject*>* InObjLoaded)
		: FKismetCompilerContext(SourceSketch, InMessageLog, InCompilerOptions, InObjLoaded)
	{}

protected:

	// FKismetCompilerContext
	virtual void SpawnNewClass(const FString& NewClassName) override;
	virtual void EnsureProperGeneratedClass(UClass*& TargetClass) override;
	// End FKismetCompilerContext
};

