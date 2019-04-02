// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ControlRigPropertyNodeSpawner.h"
#include "Graph/ControlRigGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Classes/EditorStyleSettings.h"
#include "Editor/EditorEngine.h"
#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"
#include "K2Node_Variable.h"

#define LOCTEXT_NAMESPACE "ControlRigPropertyNodeSpawner"

UControlRigPropertyNodeSpawner* UControlRigPropertyNodeSpawner::CreateFromProperty(TSubclassOf<UControlRigGraphNode> NodeClass, UProperty const* VarProperty, UEdGraph* VarContext, UObject* Outer/*= nullptr*/)
{
	check(VarProperty != nullptr);
	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}

	UControlRigPropertyNodeSpawner* NodeSpawner = NewObject<UControlRigPropertyNodeSpawner>(Outer);
	NodeSpawner->NodeClass = NodeClass;
	NodeSpawner->Field = VarProperty;

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	FString const VarSubCategory = FObjectEditorUtils::GetCategory(VarProperty);
	MenuSignature.Category = FEditorCategoryUtils::BuildCategoryString(FCommonEditorCategory::Variables, FText::FromString(VarSubCategory));

	FText const VarName = NodeSpawner->GetVariableName();
	MenuSignature.MenuName = VarName;
	MenuSignature.Tooltip  = FText::Format(LOCTEXT("PropertySpawnerTooltip", "Property {0}"), VarName);
	MenuSignature.Category = LOCTEXT("PropertiesCategory", "Properties");

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

	auto MemberVarSetupLambda = [](UEdGraphNode* NewNode, UField const* InField)
	{
		if (UProperty const* Property = Cast<UProperty>(InField))
		{
			UControlRigGraphNode* ControlRigGraphNode = CastChecked<UControlRigGraphNode>(NewNode);
			ControlRigGraphNode->SetPropertyName(Property->GetFName());
		}
	};
	NodeSpawner->SetNodeFieldDelegate = FSetNodeFieldDelegate::CreateStatic(MemberVarSetupLambda);

	return NodeSpawner;
}

void UControlRigPropertyNodeSpawner::Prime()
{
	// we expect that you don't need a node template to construct menu entries
	// from this, so we choose not to pre-cache one here
}

FBlueprintNodeSignature UControlRigPropertyNodeSpawner::GetSpawnerSignature() const
{
	return FBlueprintNodeSignature(NodeClass);
}

FBlueprintActionUiSpec UControlRigPropertyNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

UProperty const* UControlRigPropertyNodeSpawner::GetProperty() const
{
	return Cast<UProperty>(GetField());
}

FEdGraphPinType UControlRigPropertyNodeSpawner::GetVarType() const
{
	FEdGraphPinType VarType;
	if (UProperty const* Property = GetProperty())
	{
		UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();
		K2Schema->ConvertPropertyToPinType(Property, VarType);
	}
	return VarType;
}

FText UControlRigPropertyNodeSpawner::GetVariableName() const
{
	FText VarName;
	if (UProperty const* Property = GetProperty())
	{
		VarName = FText::FromName(Property->GetFName());
	}
	return VarName;
}

UEdGraphNode* UControlRigPropertyNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	// Invoking this doesnt produce any nodes, we just show these items in the rig view
	return nullptr;
}

bool UControlRigPropertyNodeSpawner::IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const
{
	// This is always filtered out of action palettes
	return true;
}

#undef LOCTEXT_NAMESPACE
