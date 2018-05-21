// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "NiagaraNodeFactory.h"
#include "NiagaraNodeReroute.h"
#include "SGraphNodeKnot.h"


FNiagaraNodeFactory::~FNiagaraNodeFactory()
{
}

TSharedPtr<SGraphNode> FNiagaraNodeFactory::CreateNodeWidget(UEdGraphNode* InNode)
{
	if (UNiagaraNodeReroute* RerouteNode = Cast<UNiagaraNodeReroute>(InNode))
	{
		return SNew(SGraphNodeKnot, RerouteNode);
	}

	return FGraphNodeFactory::CreateNodeWidget(InNode);
}
