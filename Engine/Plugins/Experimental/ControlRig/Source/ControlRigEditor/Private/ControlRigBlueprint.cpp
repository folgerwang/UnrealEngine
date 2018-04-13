// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "ControlRigGraphNode.h"
#include "ControlRigGraphSchema.h"
#include "Modules/ModuleManager.h"
#include "Engine/SkeletalMesh.h"

UControlRigBlueprint::UControlRigBlueprint()
{
}

UControlRigBlueprintGeneratedClass* UControlRigBlueprint::GetControlRigBlueprintGeneratedClass() const
{
	UControlRigBlueprintGeneratedClass* Result = Cast<UControlRigBlueprintGeneratedClass>(*GeneratedClass);
	return Result;
}

UControlRigBlueprintGeneratedClass* UControlRigBlueprint::GetControlRigBlueprintSkeletonClass() const
{
	UControlRigBlueprintGeneratedClass* Result = Cast<UControlRigBlueprintGeneratedClass>(*SkeletonGeneratedClass);
	return Result;
}

#if WITH_EDITOR

UClass* UControlRigBlueprint::GetBlueprintClass() const
{
	return UControlRigBlueprintGeneratedClass::StaticClass();
}

void UControlRigBlueprint::LoadModulesRequiredForCompilation() 
{
	static const FName ModuleName(TEXT("ControlRigEditor"));
	FModuleManager::Get().LoadModule(ModuleName);
}
#endif

void UControlRigBlueprint::MakePropertyLink(const FString& InSourcePropertyPath, const FString& InDestPropertyPath)
{
	PropertyLinks.AddUnique(FControlRigBlueprintPropertyLink(InSourcePropertyPath, InDestPropertyPath));
}

USkeletalMesh* UControlRigBlueprint::GetPreviewMesh() const
{
	if (!PreviewSkeletalMesh.IsValid())
	{
		PreviewSkeletalMesh.LoadSynchronous();
	}

	return PreviewSkeletalMesh.Get();
}

void UControlRigBlueprint::SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty/*=true*/)
{
	if(bMarkAsDirty)
	{
		Modify();
	}

	PreviewSkeletalMesh = PreviewMesh;
}
