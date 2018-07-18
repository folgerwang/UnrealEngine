// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigGraphSchema.h"
#include "ControlRigGraph.h"
#include "ControlRigGraphNode.h"
#include "ControlRigConnectionDrawingPolicy.h"
#include "UObject/UObjectIterator.h"
#include "Units/RigUnit.h"
#include "ControlRigBlueprintUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GraphEditorActions.h"

#define LOCTEXT_NAMESPACE "ControlRigGraphSchema"

const FName UControlRigGraphSchema::GraphName_ControlRig(TEXT("Rig Graph"));

UControlRigGraphSchema::UControlRigGraphSchema()
{
}

void UControlRigGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{

}

void UControlRigGraphSchema::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	if(MenuBuilder)
	{
		MenuBuilder->BeginSection("ContextMenu");

		UEdGraphSchema::GetContextMenuActions(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);

		MenuBuilder->EndSection();

		if (InGraphPin != NULL)
		{
			MenuBuilder->BeginSection("EdGraphSchemaPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
			{
				// Break pin links
				if (InGraphPin->LinkedTo.Num() > 0)
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().BreakPinLinks );
				}
			}
			MenuBuilder->EndSection();
		}
	}
}

bool UControlRigGraphSchema::TryCreateConnection_Extended(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	const FControlRigPinConnectionResponse Response = CanCreateConnection_Extended(PinA, PinB);
	bool bModified = false;

	struct Local
	{
		static void BreakParentConnections_Recursive(UEdGraphPin* InPin)
		{
			if(InPin->ParentPin)
			{
				InPin->ParentPin->Modify();
				InPin->ParentPin->BreakAllPinLinks();
				InPin->GetOwningNode()->PinConnectionListChanged(InPin->ParentPin);
				BreakParentConnections_Recursive(InPin->ParentPin);
			}
		}

		static void BreakChildConnections_Recursive(UEdGraphPin* InPin)
		{
			for(UEdGraphPin* SubPin : InPin->SubPins)
			{
				if(SubPin->LinkedTo.Num() > 0)
				{
					SubPin->Modify();
					SubPin->BreakAllPinLinks();
					SubPin->GetOwningNode()->PinConnectionListChanged(SubPin);
				}

				BreakChildConnections_Recursive(SubPin);
			}
		}
	};

	switch (Response.Response.Response)
	{
	case CONNECT_RESPONSE_MAKE:
		PinA->Modify();
		PinB->Modify();
		PinA->MakeLinkTo(PinB);
		bModified = true;
		switch(Response.ExtendedResponse)
		{
		case ECanCreateConnectionResponse_Extended::None:
			break;
		case ECanCreateConnectionResponse_Extended::BreakChildren:
			if(PinA->Direction == EGPD_Input)
			{
				Local::BreakChildConnections_Recursive(PinA);
			}
			else if(PinB->Direction == EGPD_Input)
			{
				Local::BreakChildConnections_Recursive(PinB);
			}
			break;
		case ECanCreateConnectionResponse_Extended::BreakParent:
			if(PinA->Direction == EGPD_Input)
			{
				Local::BreakParentConnections_Recursive(PinA);
			}
			else if(PinB->Direction == EGPD_Input)
			{
				Local::BreakParentConnections_Recursive(PinB);
			}
			break;
		}
		PinA->GetOwningNode()->PinConnectionListChanged(PinA);
		PinB->GetOwningNode()->PinConnectionListChanged(PinB);
		break;

	default:
		bModified = UEdGraphSchema::TryCreateConnection(PinA, PinB);
		break;
	}

	return bModified;
}

bool UControlRigGraphSchema::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(PinA->GetOwningNode());

	bool bModified = TryCreateConnection_Extended(PinA, PinB);

	if (bModified && !PinA->IsPendingKill())
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}

	return bModified;	
}

static bool HasParentConnection_Recursive(const UEdGraphPin* InPin)
{
	if(InPin->ParentPin)
	{
		return InPin->ParentPin->LinkedTo.Num() > 0 || HasParentConnection_Recursive(InPin->ParentPin);
	}

	return false;
}

static bool HasChildConnection_Recursive(const UEdGraphPin* InPin)
{
	for(const UEdGraphPin* SubPin : InPin->SubPins)
	{
		if(SubPin->LinkedTo.Num() > 0 || HasChildConnection_Recursive(SubPin))
		{
			return true;
		}
	}

	return false;
}

const FControlRigPinConnectionResponse UControlRigGraphSchema::CanCreateConnection_Extended(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	check(A != nullptr);
	check(B != nullptr);

	// Deal with basic connections (same pins, same node, differing types etc.)
	if(A == B)
	{
		return FControlRigPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectResponse_Disallowed_Self", "Cannot link a pin to itself"));
	}

	if(A->Direction == B->Direction)
	{
		return FControlRigPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, A->Direction == EGPD_Input ? LOCTEXT("ConnectResponse_Disallowed_Direction_Input", "Cannot link input pin to input pin") : LOCTEXT("ConnectResponse_Disallowed_Direction_Output", "Cannot link output pin to output pin"));
	}

	if(A->GetOwningNode() == B->GetOwningNode())
	{
		return FControlRigPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectResponse_Disallowed_SameNode", "Cannot link two pins on the same node"));
	}

	if(A->PinType != B->PinType)
	{
		return FControlRigPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectResponse_Disallowed_Different_Types", "Cannot link pins of differing types"));
	}

	// Deal with many-to-one and one to many connections
	if(A->Direction == EGPD_Input && A->LinkedTo.Num() > 0)
	{
		return FControlRigPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, LOCTEXT("ConnectResponse_Replace_Input", "Replace connection"));
	}
	else if(B->Direction == EGPD_Input && B->LinkedTo.Num() > 0)
	{
		return FControlRigPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, LOCTEXT("ConnectResponse_Replace_Input", "Replace connection"));
	}

	// Deal with sub-struct pins

	if(A->Direction == EGPD_Input && HasParentConnection_Recursive(A))
	{
		return FControlRigPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("ConnectResponse_Replace_Parent", "Replace parent connection"), ECanCreateConnectionResponse_Extended::BreakParent);
	}
	else if(B->Direction == EGPD_Input && HasParentConnection_Recursive(B))
	{
		return FControlRigPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("ConnectResponse_Replace_Parent", "Replace parent connection"), ECanCreateConnectionResponse_Extended::BreakParent);
	}

	if(A->Direction == EGPD_Input && HasChildConnection_Recursive(A))
	{
		return FControlRigPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("ConnectResponse_Replace_Child", "Replace child connection(s)"), ECanCreateConnectionResponse_Extended::BreakChildren);
	}
	else if(B->Direction == EGPD_Input && HasChildConnection_Recursive(B))
	{
		return FControlRigPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("ConnectResponse_Replace_Child", "Replace child connection(s)"), ECanCreateConnectionResponse_Extended::BreakChildren);
	}

	return FControlRigPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("ConnectResponse_Allowed", "Connect"));	
}

const FPinConnectionResponse UControlRigGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	const FControlRigPinConnectionResponse Response = CanCreateConnection_Extended(A, B);
	return Response.Response;
}

FLinearColor UControlRigGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return GetDefault<UEdGraphSchema_K2>()->GetPinTypeColor(PinType);
}

void UControlRigGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	const FScopedTransaction Transaction( LOCTEXT("GraphEd_BreakPinLinks", "Break Pin Links") );

	// cache this here, as BreakPinLinks can trigger a node reconstruction invalidating the TargetPin referenceS
	UBlueprint* const Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(TargetPin.GetOwningNode());

	Super::BreakPinLinks(TargetPin, bSendsNodeNotifcation);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);	
}

void UControlRigGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	const FScopedTransaction Transaction(LOCTEXT("GraphEd_BreakSinglePinLink", "Break Pin Link") );

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(TargetPin->GetOwningNode());

	Super::BreakSinglePinLink(SourcePin, TargetPin);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

FConnectionDrawingPolicy* UControlRigGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new FControlRigConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

bool UControlRigGraphSchema::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	// we should hide default values if any of our parents are connected
	return HasParentConnection_Recursive(Pin);
}

UControlRigGraphNode* UControlRigGraphSchema::CreateGraphNode(UControlRigGraph* InGraph, const FName& InPropertyName) const
{
	const bool bSelectNewNode = true;
	FGraphNodeCreator<UControlRigGraphNode> GraphNodeCreator(*InGraph);
	UControlRigGraphNode* ControlRigGraphNode = GraphNodeCreator.CreateNode(bSelectNewNode);
	ControlRigGraphNode->SetPropertyName(InPropertyName);
	GraphNodeCreator.Finalize();

	return ControlRigGraphNode;
}

void UControlRigGraphSchema::TrySetDefaultValue(UEdGraphPin& InPin, const FString& InNewDefaultValue) const
{
	GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(InPin, InNewDefaultValue);
}

void UControlRigGraphSchema::TrySetDefaultObject(UEdGraphPin& InPin, UObject* InNewDefaultObject) const
{
	GetDefault<UEdGraphSchema_K2>()->TrySetDefaultObject(InPin, InNewDefaultObject);
}

void UControlRigGraphSchema::TrySetDefaultText(UEdGraphPin& InPin, const FText& InNewDefaultText) const
{
	GetDefault<UEdGraphSchema_K2>()->TrySetDefaultText(InPin, InNewDefaultText);
}

bool UControlRigGraphSchema::ArePinsCompatible(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UClass* CallingContext, bool bIgnoreArray /*= false*/) const
{
	return GetDefault<UEdGraphSchema_K2>()->ArePinsCompatible(PinA, PinB, CallingContext, bIgnoreArray);
}

#undef LOCTEXT_NAMESPACE