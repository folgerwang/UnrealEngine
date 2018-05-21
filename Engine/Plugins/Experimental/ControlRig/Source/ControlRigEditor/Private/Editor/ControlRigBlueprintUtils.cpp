// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Units/RigUnit.h"
#include "UObject/UObjectIterator.h"
#include "ControlRig.h"
#include "ControlRigGraphNode.h"
#include "ControlRigBlueprint.h"

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