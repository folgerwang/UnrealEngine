// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceDirectorBlueprint.h"
#include "LevelSequenceDirectorGeneratedClass.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Modules/ModuleManager.h"

ULevelSequenceDirectorBlueprint::ULevelSequenceDirectorBlueprint(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	ParentClass = ULevelSequenceDirector::StaticClass();
}

UClass* ULevelSequenceDirectorBlueprint::GetBlueprintClass() const
{
	return ULevelSequenceDirectorGeneratedClass::StaticClass();
}

bool ULevelSequenceDirectorBlueprint::SupportsNativization(FText* OutReason) const
{
	if (OutReason)
	{
		*OutReason = NSLOCTEXT("LevelSequenceDirectorBlueprint", "NativizationError", "Level Sequence Director Blueprints do not support nativization.");
	}
	return false;
}

bool ULevelSequenceDirectorBlueprint::SupportedByDefaultBlueprintFactory() const
{
	return false;
}

bool ULevelSequenceDirectorBlueprint::AlwaysCompileOnLoad() const
{
	return true;
}

void ULevelSequenceDirectorBlueprint::LoadModulesRequiredForCompilation()
{
	static const FName ModuleName(TEXT("LevelSequence"));
	FModuleManager::Get().LoadModule(ModuleName);
}

void FLevelSequenceDirectorBlueprintCompiler::SpawnNewClass(const FString& NewClassName)
{
	NewClass = FindObject<ULevelSequenceDirectorGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);

	if (!NewClass)
	{
		NewClass = NewObject<ULevelSequenceDirectorGeneratedClass>(Blueprint->GetOutermost(), FName(*NewClassName), RF_Public | RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked in the Blueprint yet due to load ordering issues
		FBlueprintCompileReinstancer::Create(NewClass);
	}
}

void FLevelSequenceDirectorBlueprintCompiler::EnsureProperGeneratedClass(UClass*& TargetUClass)
{
	if ( TargetUClass && !( (UObject*)TargetUClass )->IsA(ULevelSequenceDirectorGeneratedClass::StaticClass()) )
	{
		FKismetCompilerUtilities::ConsignToOblivion(TargetUClass, Blueprint->bIsRegeneratingOnLoad);
		TargetUClass = nullptr;
	}
}