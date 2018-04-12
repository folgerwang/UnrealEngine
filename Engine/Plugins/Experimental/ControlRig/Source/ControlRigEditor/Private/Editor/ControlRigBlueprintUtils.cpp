// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Units/RigUnit.h"
#include "UObject/UObjectIterator.h"
#include "ControlRig.h"
#include "ControlRigGraphNode.h"
#include "ControlRigBlueprint.h"
#include "NodeSpawners/ControlRigPropertyNodeSpawner.h"
#include "NodeSpawners/ControlRigUnitNodeSpawner.h"
#include "NodeSpawners/ControlRigVariableNodeSpawner.h"

#define LOCTEXT_NAMESPACE "ControlRigBlueprintUtils"

FName FControlRigBlueprintUtils::GetNewUnitMemberName(UBlueprint* InBlueprint, UStruct* InStructTemplate)
{
	FString VariableBaseName = InStructTemplate->GetName();
	VariableBaseName.RemoveFromStart(TEXT("RigUnit_"));
	return FBlueprintEditorUtils::FindUniqueKismetName(InBlueprint, VariableBaseName);
}

FName FControlRigBlueprintUtils::AddUnitMember(UBlueprint* InBlueprint, UStruct* InStructTemplate)
{
	FName VarName = FControlRigBlueprintUtils::GetNewUnitMemberName(InBlueprint, InStructTemplate);

	UScriptStruct* ScriptStruct = FindObjectChecked<UScriptStruct>(ANY_PACKAGE, *InStructTemplate->GetName());
	UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if(FBlueprintEditorUtils::AddMemberVariable(InBlueprint, VarName, FEdGraphPinType(UEdGraphSchema_K2::PC_Struct, InStructTemplate->GetFName(), ScriptStruct, EPinContainerType::None, false, FEdGraphTerminalType())))
	{
		FBPVariableDescription& Variable = InBlueprint->NewVariables.Last();
		Variable.Category = LOCTEXT("UnitsCategory", "Units");

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);	

		return Variable.VarName;
	}

	return NAME_None;
}

FName FControlRigBlueprintUtils::GetNewPropertyMemberName(UBlueprint* InBlueprint, const FString& InVariableDesc)
{
	FString VariableBaseName(TEXT("New"));
	VariableBaseName += InVariableDesc;
	
	return FBlueprintEditorUtils::FindUniqueKismetName(InBlueprint, VariableBaseName);
}

FName FControlRigBlueprintUtils::AddPropertyMember(UBlueprint* InBlueprint, const FEdGraphPinType& InPinType, const FString& InVariableDesc)
{
	FName VarName = GetNewPropertyMemberName(InBlueprint, InVariableDesc);
	if(FBlueprintEditorUtils::AddMemberVariable(InBlueprint, VarName, InPinType))
	{
		return InBlueprint->NewVariables.Last().VarName;
	}

	return NAME_None;
}

UControlRigGraphNode* FControlRigBlueprintUtils::InstantiateGraphNodeForProperty(UEdGraph* InGraph, const FName& InPropertyName, const FVector2D& InLocation)
{
	check(InGraph);

	InGraph->Modify();

	UControlRigGraphNode* NewNode = NewObject<UControlRigGraphNode>(InGraph);
	NewNode->SetPropertyName(InPropertyName);

	InGraph->AddNode(NewNode, true);

	NewNode->CreateNewGuid();
	NewNode->PostPlacedNewNode();
	NewNode->AllocateDefaultPins();

	NewNode->NodePosX = InLocation.X;
	NewNode->NodePosY = InLocation.Y;

	NewNode->SetFlags(RF_Transactional);

	return NewNode;
}

bool FControlRigBlueprintUtils::CanInstantiateGraphNodeForProperty(UEdGraph* InGraph, const FName& InPropertyName)
{
	for(UEdGraphNode* Node : InGraph->Nodes)
	{
		if(UControlRigGraphNode* ControlRigGraphNode = Cast<UControlRigGraphNode>(Node))
		{
			if(ControlRigGraphNode->GetPropertyName() == InPropertyName)
			{
				return false;
			}
		}
	}

	return true;
}

void FControlRigBlueprintUtils::ForAllRigUnits(TFunction<void(UStruct*)> InFunction)
{
	// Run over all unit types
	for(TObjectIterator<UStruct> StructIt; StructIt; ++StructIt)
	{
		if(StructIt->IsChildOf(FRigUnit::StaticStruct()) && !StructIt->HasMetaData(UControlRig::AbstractMetaName))
		{
			InFunction(*StructIt);
		}
	}
}

void FControlRigBlueprintUtils::HandleReconstructAllNodes(UBlueprint* InBlueprint)
{
	if(InBlueprint->IsA<UControlRigBlueprint>())
	{
		TArray<UControlRigGraphNode*> AllNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(InBlueprint, AllNodes);

		for(UControlRigGraphNode* Node : AllNodes)
		{
			Node->ReconstructNode();
		}
	}
}

void FControlRigBlueprintUtils::HandleRefreshAllNodes(UBlueprint* InBlueprint)
{
	if(InBlueprint->IsA<UControlRigBlueprint>())
	{
		TArray<UControlRigGraphNode*> AllNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(InBlueprint, AllNodes);

		for(UControlRigGraphNode* Node : AllNodes)
		{
			Node->ReconstructNode();
		}
	}
}

void FControlRigBlueprintUtils::HandleRenameVariableReferencesEvent(UBlueprint* InBlueprint, UClass* InVariableClass, const FName& InOldVarName, const FName& InNewVarName)
{
	if(InBlueprint->IsA<UControlRigBlueprint>())
	{
		TArray<UControlRigGraphNode*> AllNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(InBlueprint, AllNodes);

		for(UControlRigGraphNode* Node : AllNodes)
		{
			Node->HandleVariableRenamed(InBlueprint, InVariableClass, Node->GetGraph(), InOldVarName, InNewVarName);
		}
	}
}

void FControlRigBlueprintUtils::HandleGetClassPropertyActions(UClass const* const Class, FBlueprintActionDatabase::FActionList& ActionListOut)
{
	if(Class->IsChildOf<UControlRig>())
	{
		for (TFieldIterator<UProperty> PropertyIt(Class, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
		{
			UBlueprintNodeSpawner* NodeSpawner = UControlRigPropertyNodeSpawner::CreateFromProperty(UControlRigGraphNode::StaticClass(), *PropertyIt);
			check(NodeSpawner != nullptr);
			ActionListOut.Add(NodeSpawner);
		}

		// We're adding class specific options here per object. 
		// But when they display the menu items, they allow every object to add options for it regardless object type
		// (This ActionListOut will be used for ALL objects)
		// This causes issue where same item is displayed multiple times because it was added multiple times if you open multiple graph
		// to avoid this issue, I'm only allowing once using static bool
		// As an alternative to this and maybe more extensible option is to extend void FBlueprintActionDatabase::RefreshAll()
		// to register class type actions - RefreshClassActions(Class);
		// However, that function is very performance sensitive function, and BP team expressed the performance hit that can cause by it
		// for now we opt into just using static bool to make sure we only add ONCE per class type actions
		static bool bAddClassTypeActions = true;
		//if (bAddClassTypeActions)
		{
			// Add all rig units
			FControlRigBlueprintUtils::ForAllRigUnits([&](UStruct* InStruct)
			{
				FText NodeCategory = FText::FromString(InStruct->GetMetaData(TEXT("Category")));
				FText MenuDesc = FText::FromString(InStruct->GetMetaData(TEXT("DisplayName")));
				FText ToolTip = InStruct->GetToolTipText();

				UBlueprintNodeSpawner* NodeSpawner = UControlRigUnitNodeSpawner::CreateFromStruct(InStruct, MenuDesc, NodeCategory, ToolTip);
				check(NodeSpawner != nullptr);
				ActionListOut.Add(NodeSpawner);
			});

			// Add 'new properties'
			TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>> PinTypes;
			GetDefault<UEdGraphSchema_K2>()->GetVariableTypeTree(PinTypes, ETypeTreeFilter::None);

			struct Local
			{
				static void AddVariableActions_Recursive(FBlueprintActionDatabase::FActionList& InActionListOut, const TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& InPinTypeTreeItem, const FString& InCurrentCategory)
				{
					static const FString CategoryDelimiter(TEXT("|"));

					if (InPinTypeTreeItem->Children.Num() == 0)
					{
						FText NodeCategory = FText::FromString(InCurrentCategory);
						FText MenuDesc = InPinTypeTreeItem->GetDescription();
						FText ToolTip = InPinTypeTreeItem->GetToolTip();

						UBlueprintNodeSpawner* NodeSpawner = UControlRigVariableNodeSpawner::CreateFromPinType(InPinTypeTreeItem->GetPinType(false), MenuDesc, NodeCategory, ToolTip);
						check(NodeSpawner != nullptr);
						InActionListOut.Add(NodeSpawner);
					}
					else
					{
						FString CurrentCategory = InCurrentCategory + CategoryDelimiter + InPinTypeTreeItem->FriendlyName.ToString();

						for (const TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& ChildTreeItem : InPinTypeTreeItem->Children)
						{
							AddVariableActions_Recursive(InActionListOut, ChildTreeItem, CurrentCategory);
						}
					}
				}
			};

			FString CurrentCategory = LOCTEXT("NewVariable", "New Variable").ToString();
			for (const TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& PinTypeTreeItem : PinTypes)
			{
				Local::AddVariableActions_Recursive(ActionListOut, PinTypeTreeItem, CurrentCategory);
			}

			// we added it, so set it to false. Class type actions don't have to be added every time
			bAddClassTypeActions = false;
		}
	}
}

void FControlRigBlueprintUtils::RemoveMemberVariableIfNotUsed(UBlueprint* Blueprint, const FName VarName, UControlRigGraphNode* ToBeDeleted)
{
	if (Blueprint->IsA<UControlRigBlueprint>())
	{
		bool bDeleteVariable = true;

		TArray<UControlRigGraphNode*> AllNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, AllNodes);

		for (UControlRigGraphNode* Node : AllNodes)
		{
			if (Node != ToBeDeleted && Node->GetPropertyName() == VarName)
			{
				bDeleteVariable = false;
				break;
			}
		}

		if (bDeleteVariable)
		{
			FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VarName);
		}
	}
}
#undef LOCTEXT_NAMESPACE