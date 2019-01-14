// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "Modules/ModuleManager.h"
#include "Engine/SkeletalMesh.h"
#include "BlueprintActionDatabaseRegistrar.h"

#if WITH_EDITOR
#include "IControlRigEditorModule.h"
#endif//WITH_EDITOR

#define LOCTEXT_NAMESPACE "ControlRigBlueprint"

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
UClass* UControlRigBlueprint::GetBlueprintClass() const
{
	return UControlRigBlueprintGeneratedClass::StaticClass();
}

void UControlRigBlueprint::LoadModulesRequiredForCompilation() 
{
}

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

void UControlRigBlueprint::GetTypeActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	IControlRigEditorModule::Get().GetTypeActions(this, ActionRegistrar);
}

void UControlRigBlueprint::GetInstanceActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	IControlRigEditorModule::Get().GetInstanceActions(this, ActionRegistrar);
}

#undef LOCTEXT_NAMESPACE

