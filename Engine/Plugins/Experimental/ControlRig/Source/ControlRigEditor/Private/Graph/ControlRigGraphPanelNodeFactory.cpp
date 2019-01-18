// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ControlRigGraphPanelNodeFactory.h"
#include "Graph/ControlRigGraphNode.h"
#include "Graph/ControlRigGraph.h"
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