// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigGraphPanelNodeFactory.h"
#include "ControlRigGraphNode.h"
#include "ControlRigGraph.h"
#include "SControlRigGraphNode.h"

TSharedPtr<SGraphNode> FControlRigGraphPanelNodeFactory::CreateNode(UEdGraphNode* Node) const
{
	if (UControlRigGraphNode* ControlRigGraphNode = Cast<UControlRigGraphNode>(Node))
	{
		TSharedRef<SGraphNode> GraphNode = 
			SNew(SControlRigGraphNode)
			.GraphNodeObj(ControlRigGraphNode);

		GraphNode->SlatePrepass();
		ControlRigGraphNode->SetDimensions(GraphNode->GetDesiredSize());
		return GraphNode;
	}

	return nullptr;
}