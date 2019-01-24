// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KismetCompilerModule.h"
#include "KismetCompiler.h"

struct FControlRigBlueprintPropertyLink;
class UControlRigBlueprintGeneratedClass;

class CONTROLRIGDEVELOPER_API FControlRigBlueprintCompiler : public IBlueprintCompiler
{
public:
	/** IBlueprintCompiler interface */
	virtual bool CanCompile(const UBlueprint* Blueprint) override;
	virtual void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results) override;
};

class CONTROLRIGDEVELOPER_API FControlRigBlueprintCompilerContext : public FKismetCompilerContext
{
public:
	FControlRigBlueprintCompilerContext(UBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions)
		: FKismetCompilerContext(SourceSketch, InMessageLog, InCompilerOptions)
		, NewControlRigBlueprintGeneratedClass(nullptr)
	{
	}

	// FKismetCompilerContext interface
	virtual void MergeUbergraphPagesIn(UEdGraph* Ubergraph) override;
	virtual void PostCompile() override;
	virtual void CopyTermDefaultsToDefaultObject(UObject* DefaultObject) override;
	virtual void EnsureProperGeneratedClass(UClass*& TargetUClass) override;
	virtual void SpawnNewClass(const FString& NewClassName) override;
	virtual void OnNewClassSet(UBlueprintGeneratedClass* ClassToUse) override;
	virtual void CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& InOldCDO) override;

private:
	// utility function to add root property to links 
	void AddRootPropertyLinks(TArray<FControlRigBlueprintPropertyLink>& InLinks, TArray<FName>& OutSourceArray, TArray<FName>& OutDestArray) const;

	// utility funciton to build property links from the ubergraphs
	void BuildPropertyLinks();

private:
	/** the new class we are generating */
	UControlRigBlueprintGeneratedClass* NewControlRigBlueprintGeneratedClass;
};