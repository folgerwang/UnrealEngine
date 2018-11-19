// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "WidgetGraphSchema.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_ClearDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_WidgetAnimationEvent.h"
#include "Animation/WidgetAnimation.h"
#include "BlueprintNodeSpawner.h"

UWidgetGraphSchema::UWidgetGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

EWidgetAnimationEvent GetAnimationEventFromDelegateName(FName DelegateName)
{
	if (DelegateName == TEXT("OnAnimationStarted"))
	{
		return EWidgetAnimationEvent::Started;
	}
	else if (DelegateName == TEXT("OnAnimationFinished"))
	{
		return EWidgetAnimationEvent::Finished;
	}

	ensure(false);
	return EWidgetAnimationEvent::Started;
}

void UWidgetGraphSchema::BackwardCompatibilityNodeConversion(UEdGraph* Graph, bool bOnlySafeChanges) const
{
	if (Graph)
	{
		ConvertAnimationEventNodes(Graph);

		ConvertAddAnimationDelegate(Graph);
		ConvertRemoveAnimationDelegate(Graph);
		ConvertClearAnimationDelegate(Graph);
	}

	Super::BackwardCompatibilityNodeConversion(Graph, bOnlySafeChanges);
}

void UWidgetGraphSchema::ConvertAnimationEventNodes(UEdGraph* Graph) const
{
	TArray<UK2Node_ComponentBoundEvent*> ComponentBoundEventNodes;
	Graph->GetNodesOfClass<UK2Node_ComponentBoundEvent>(ComponentBoundEventNodes);

	for (UK2Node_ComponentBoundEvent* Node : ComponentBoundEventNodes)
	{
		if (Node->DelegateOwnerClass == UWidgetAnimation::StaticClass())
		{
			UBlueprintNodeSpawner* GetItemNodeSpawner = UBlueprintNodeSpawner::Create(UK2Node_WidgetAnimationEvent::StaticClass(), /*Outer =*/nullptr);

			FVector2D NodePos(Node->NodePosX, Node->NodePosY);
			IBlueprintNodeBinder::FBindingSet Bindings;
			UK2Node_WidgetAnimationEvent* GetItemNode = Cast<UK2Node_WidgetAnimationEvent>(GetItemNodeSpawner->Invoke(Graph, Bindings, NodePos));
			GetItemNode->SourceWidgetBlueprint = CastChecked<UWidgetBlueprint>(Graph->GetOuter());
			GetItemNode->Action = GetAnimationEventFromDelegateName(Node->DelegatePropertyName);
			GetItemNode->AnimationPropertyName = Node->ComponentPropertyName;

			TMap<FName, FName> OldToNewPinMap;
			ReplaceOldNodeWithNew(Node, GetItemNode, OldToNewPinMap);

			GetItemNode->MarkDirty();
		}
	}
}

void UWidgetGraphSchema::ConvertAddAnimationDelegate(UEdGraph* Graph) const
{
	UBlueprintNodeSpawner* CallFunctionSpawner = UBlueprintNodeSpawner::Create(UK2Node_CallFunction::StaticClass(), /*Outer =*/nullptr);

	TArray<UK2Node_AddDelegate*> AddDelegateNodes;
	Graph->GetNodesOfClass<UK2Node_AddDelegate>(AddDelegateNodes);

	for (UK2Node_AddDelegate* Node : AddDelegateNodes)
	{
		if (Node->DelegateReference.GetMemberParentClass() == UWidgetAnimation::StaticClass())
		{
			FVector2D NodePos(Node->NodePosX, Node->NodePosY);
			IBlueprintNodeBinder::FBindingSet Bindings;
			UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(CallFunctionSpawner->Invoke(Graph, Bindings, NodePos));

			switch (GetAnimationEventFromDelegateName(Node->DelegateReference.GetMemberName()))
			{
			case EWidgetAnimationEvent::Started:
				CallFunction->FunctionReference.SetExternalMember(TEXT("BindToAnimationStarted"), UWidgetAnimation::StaticClass());
				break;
			case EWidgetAnimationEvent::Finished:
				CallFunction->FunctionReference.SetExternalMember(TEXT("BindToAnimationFinished"), UWidgetAnimation::StaticClass());
				break;
			}

			CallFunction->AllocateDefaultPins();

			TMap<FName, FName> OldToNewPinMap;
			//OldToNewPinMap.Add(UEdGraphSchema_K2::PN_Self, TEXT("Animation"));
			OldToNewPinMap.Add(TEXT("Delegate"), TEXT("Delegate"));
			ReplaceOldNodeWithNew(Node, CallFunction, OldToNewPinMap);
		}
	}
}

void UWidgetGraphSchema::ConvertRemoveAnimationDelegate(UEdGraph* Graph) const
{
	UBlueprintNodeSpawner* CallFunctionSpawner = UBlueprintNodeSpawner::Create(UK2Node_CallFunction::StaticClass(), /*Outer =*/nullptr);

	TArray<UK2Node_RemoveDelegate*> RemoveDelegateNodes;
	Graph->GetNodesOfClass<UK2Node_RemoveDelegate>(RemoveDelegateNodes);

	for (UK2Node_RemoveDelegate* Node : RemoveDelegateNodes)
	{
		if (Node->DelegateReference.GetMemberParentClass() == UWidgetAnimation::StaticClass())
		{
			FVector2D NodePos(Node->NodePosX, Node->NodePosY);
			IBlueprintNodeBinder::FBindingSet Bindings;
			UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(CallFunctionSpawner->Invoke(Graph, Bindings, NodePos));

			switch (GetAnimationEventFromDelegateName(Node->DelegateReference.GetMemberName()))
			{
			case EWidgetAnimationEvent::Started:
				CallFunction->FunctionReference.SetExternalMember(TEXT("UnbindFromAnimationStarted"), UWidgetAnimation::StaticClass());
				break;
			case EWidgetAnimationEvent::Finished:
				CallFunction->FunctionReference.SetExternalMember(TEXT("UnbindFromAnimationFinished"), UWidgetAnimation::StaticClass());
				break;
			}

			CallFunction->AllocateDefaultPins();

			TMap<FName, FName> OldToNewPinMap;
			//OldToNewPinMap.Add(UEdGraphSchema_K2::PN_Self, TEXT("Animation"));
			OldToNewPinMap.Add(TEXT("Delegate"), TEXT("Delegate"));
			ReplaceOldNodeWithNew(Node, CallFunction, OldToNewPinMap);
		}
	}
}

void UWidgetGraphSchema::ConvertClearAnimationDelegate(UEdGraph* Graph) const
{
	UBlueprintNodeSpawner* CallFunctionSpawner = UBlueprintNodeSpawner::Create(UK2Node_CallFunction::StaticClass(), /*Outer =*/nullptr);

	TArray<UK2Node_ClearDelegate*> ClearDelegateNodes;
	Graph->GetNodesOfClass<UK2Node_ClearDelegate>(ClearDelegateNodes);

	for (UK2Node_ClearDelegate* Node : ClearDelegateNodes)
	{
		if (Node->DelegateReference.GetMemberParentClass() == UWidgetAnimation::StaticClass())
		{
			FVector2D NodePos(Node->NodePosX, Node->NodePosY);
			IBlueprintNodeBinder::FBindingSet Bindings;
			UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(CallFunctionSpawner->Invoke(Graph, Bindings, NodePos));

			switch (GetAnimationEventFromDelegateName(Node->DelegateReference.GetMemberName()))
			{
			case EWidgetAnimationEvent::Started:
				CallFunction->FunctionReference.SetExternalMember(TEXT("UnbindAllFromAnimationStarted"), UWidgetAnimation::StaticClass());
				break;
			case EWidgetAnimationEvent::Finished:
				CallFunction->FunctionReference.SetExternalMember(TEXT("UnbindAllFromAnimationFinished"), UWidgetAnimation::StaticClass());
				break;
			}

			CallFunction->AllocateDefaultPins();

			TMap<FName, FName> OldToNewPinMap;
			//OldToNewPinMap.Add(UEdGraphSchema_K2::PN_Self, TEXT("Animation"));
			ReplaceOldNodeWithNew(Node, CallFunction, OldToNewPinMap);
		}
	}
}