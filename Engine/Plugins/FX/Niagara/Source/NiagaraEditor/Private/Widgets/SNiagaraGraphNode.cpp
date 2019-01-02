// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraGraphNode.h"
#include "GraphEditorSettings.h"
#include "Rendering/DrawElements.h"
#include "SGraphPin.h"

#define LOCTEXT_NAMESPACE "SNiagaraGraphNode"

SNiagaraGraphNode::SNiagaraGraphNode() : SGraphNode()
{
	NiagaraNode = nullptr;
}

SNiagaraGraphNode::~SNiagaraGraphNode()
{
	if (NiagaraNode.IsValid())
	{
		NiagaraNode->OnVisualsChanged().RemoveAll(this);
	}
}

void SNiagaraGraphNode::Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode)
{
	GraphNode = InGraphNode;
	RegisterNiagaraGraphNode(InGraphNode);
	UpdateGraphNode();
}


void SNiagaraGraphNode::HandleNiagaraNodeChanged(UNiagaraNode* InNode)
{
	check(InNode == NiagaraNode);
	UpdateGraphNode();
}

void SNiagaraGraphNode::RegisterNiagaraGraphNode(UEdGraphNode* InNode)
{
	NiagaraNode = Cast<UNiagaraNode>(InNode);
	NiagaraNode->OnVisualsChanged().AddSP(this, &SNiagaraGraphNode::HandleNiagaraNodeChanged);
}

void SNiagaraGraphNode::UpdateGraphNode()
{
	check(NiagaraNode.IsValid());
	SGraphNode::UpdateGraphNode();
	LastSyncedNodeChangeId = NiagaraNode->GetChangeId();
}


#undef LOCTEXT_NAMESPACE