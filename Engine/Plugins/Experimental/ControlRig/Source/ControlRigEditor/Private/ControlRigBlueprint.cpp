// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "ControlRigGraphNode.h"
#include "ControlRigGraphSchema.h"
#include "Modules/ModuleManager.h"
#include "Engine/SkeletalMesh.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "ControlRigBlueprintUtils.h"
#include "NodeSpawners/ControlRigPropertyNodeSpawner.h"
#include "NodeSpawners/ControlRigUnitNodeSpawner.h"
#include "NodeSpawners/ControlRigVariableNodeSpawner.h"

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

void UControlRigBlueprint::GetTypeActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the class (so if the class 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (!ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		return;
	}

	// Add all rig units
	FControlRigBlueprintUtils::ForAllRigUnits([&](UStruct* InStruct)
	{
		FText NodeCategory = FText::FromString(InStruct->GetMetaData(TEXT("Category")));
		FText MenuDesc = FText::FromString(InStruct->GetMetaData(TEXT("DisplayName")));
		FText ToolTip = InStruct->GetToolTipText();

		UBlueprintNodeSpawner* NodeSpawner = UControlRigUnitNodeSpawner::CreateFromStruct(InStruct, MenuDesc, NodeCategory, ToolTip);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	});

	// Add 'new properties'
	TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>> PinTypes;
	GetDefault<UEdGraphSchema_K2>()->GetVariableTypeTree(PinTypes, ETypeTreeFilter::None);

	struct Local
	{
		static void AddVariableActions_Recursive(UClass* InActionKey, FBlueprintActionDatabaseRegistrar& InActionRegistrar, const TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& InPinTypeTreeItem, const FString& InCurrentCategory)
		{
			static const FString CategoryDelimiter(TEXT("|"));

			if (InPinTypeTreeItem->Children.Num() == 0)
			{
				FText NodeCategory = FText::FromString(InCurrentCategory);
				FText MenuDesc = InPinTypeTreeItem->GetDescription();
				FText ToolTip = InPinTypeTreeItem->GetToolTip();

				UBlueprintNodeSpawner* NodeSpawner = UControlRigVariableNodeSpawner::CreateFromPinType(InPinTypeTreeItem->GetPinType(false), MenuDesc, NodeCategory, ToolTip);
				check(NodeSpawner != nullptr);
				InActionRegistrar.AddBlueprintAction(InActionKey, NodeSpawner);
			}
			else
			{
				FString CurrentCategory = InCurrentCategory + CategoryDelimiter + InPinTypeTreeItem->FriendlyName.ToString();

				for (const TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& ChildTreeItem : InPinTypeTreeItem->Children)
				{
					AddVariableActions_Recursive(InActionKey, InActionRegistrar, ChildTreeItem, CurrentCategory);
				}
			}
		}
	};

	FString CurrentCategory = LOCTEXT("NewVariable", "New Variable").ToString();
	for (const TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& PinTypeTreeItem : PinTypes)
	{
		Local::AddVariableActions_Recursive(ActionKey, ActionRegistrar, PinTypeTreeItem, CurrentCategory);
	}
}

void UControlRigBlueprint::GetInstanceActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the generated class (so if the class 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GeneratedClass;
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (!ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		return;
	}

	for (TFieldIterator<UProperty> PropertyIt(ActionKey, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	{
		UBlueprintNodeSpawner* NodeSpawner = UControlRigPropertyNodeSpawner::CreateFromProperty(UControlRigGraphNode::StaticClass(), *PropertyIt);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}
#undef LOCTEXT_NAMESPACE