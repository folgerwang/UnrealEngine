// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigVariableNodeSpawner.h"
#include "ControlRigGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Classes/EditorStyleSettings.h"
#include "Editor/EditorEngine.h"
#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"
#include "K2Node_Variable.h"
#include "BlueprintNodeTemplateCache.h"
#include "ControlRigBlueprintUtils.h"

#define LOCTEXT_NAMESPACE "ControlRigVariableNodeSpawner"

UControlRigVariableNodeSpawner* UControlRigVariableNodeSpawner::CreateFromPinType(const FEdGraphPinType& InPinType, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip)
{
	UControlRigVariableNodeSpawner* NodeSpawner = NewObject<UControlRigVariableNodeSpawner>(GetTransientPackage());
	NodeSpawner->EdGraphPinType = InPinType;
	NodeSpawner->NodeClass = UControlRigGraphNode::StaticClass();

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	
	MenuSignature.MenuName = InMenuDesc;
	MenuSignature.Tooltip  = InTooltip;
	MenuSignature.Category = InCategory;

	// add at least one character, so that PrimeDefaultUiSpec() doesn't 
	// attempt to query the template node
	//
	// @TODO: maybe UPROPERTY() fields should have keyword metadata like functions
	if (MenuSignature.Keywords.IsEmpty())
	{
		// want to set it to something so we won't end up back in this condition
		MenuSignature.Keywords = FText::FromString(TEXT(" "));
	}
	MenuSignature.Icon = UK2Node_Variable::GetVarIconFromPinType(NodeSpawner->GetVarType(), MenuSignature.IconTint);

	return NodeSpawner;
}

void UControlRigVariableNodeSpawner::Prime()
{
	// we expect that you don't need a node template to construct menu entries
	// from this, so we choose not to pre-cache one here
}

FBlueprintNodeSignature UControlRigVariableNodeSpawner::GetSpawnerSignature() const
{
	return FBlueprintNodeSignature(NodeClass);
}

FBlueprintActionUiSpec UControlRigVariableNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

UEdGraphNode* UControlRigVariableNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	UControlRigGraphNode* NewNode = nullptr;

//	const FScopedTransaction Transaction(LOCTEXT("AddRigPropertyNode", "Add Rig Property Node"));

	bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);

	// First create a backing member for our node
	UBlueprint* Blueprint = CastChecked<UBlueprint>(ParentGraph->GetOuter());
	FName MemberName = NAME_None;
	if(!bIsTemplateNode)
	{
		MemberName = FControlRigBlueprintUtils::AddPropertyMember(Blueprint, EdGraphPinType, DefaultMenuSignature.MenuName.ToString());
	}
	else
	{
		MemberName = FControlRigBlueprintUtils::GetNewPropertyMemberName(Blueprint, DefaultMenuSignature.MenuName.ToString());
	}

	if(MemberName != NAME_None)
	{
		NewNode = FControlRigBlueprintUtils::InstantiateGraphNodeForProperty(ParentGraph, MemberName, Location);
	}

	return NewNode;
}

FEdGraphPinType UControlRigVariableNodeSpawner::GetVarType() const
{
	return EdGraphPinType;
}

#undef LOCTEXT_NAMESPACE
