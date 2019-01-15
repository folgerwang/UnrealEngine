// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Graph/ControlRigGraph.h"
#include "ControlRigBlueprint.h"
#include "Graph/ControlRigGraphSchema.h"

UControlRigGraph::UControlRigGraph()
{
}

void UControlRigGraph::Initialize(UControlRigBlueprint* InBlueprint)
{

}

const UControlRigGraphSchema* UControlRigGraph::GetControlRigGraphSchema()
{
	return CastChecked<const UControlRigGraphSchema>(GetSchema());
}
