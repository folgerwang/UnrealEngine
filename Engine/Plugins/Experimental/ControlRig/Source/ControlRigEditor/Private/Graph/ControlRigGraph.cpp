// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigGraph.h"
#include "ControlRigBlueprint.h"
#include "ControlRigGraphSchema.h"

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
