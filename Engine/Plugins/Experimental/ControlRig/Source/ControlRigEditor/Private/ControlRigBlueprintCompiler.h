// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KismetCompilerModule.h"
#include "KismetCompiler.h"

struct FControlRigBlueprintPropertyLink;
class UControlRigBlueprintGeneratedClass;

class FControlRigBlueprintCompiler : public IBlueprintCompiler
{
public:
	/** IBlueprintCompiler interface */
	virtual bool CanCompile(const UBlueprint* Blueprint) override;
	virtual void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results, TArray<UObject*>* ObjLoaded) override;
};

class FControlRigBlueprintCompilerContext : public FKismetCompilerContext
{
public:
	FControlRigBlueprintCompilerContext(UBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions, TArray<UObject*>* InObjLoaded)
		: FKismetCompilerContext(SourceSketch, InMessageLog, InCompilerOptions, InObjLoaded)
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