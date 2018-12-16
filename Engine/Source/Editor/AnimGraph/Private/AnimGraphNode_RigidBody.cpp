// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_RigidBody.h"
#include "Kismet2/CompilerResultsLog.h"
#include "BoneControllers/AnimNode_RigidBody.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_RigidBody

#define LOCTEXT_NAMESPACE "RigidBody"

UAnimGraphNode_RigidBody::UAnimGraphNode_RigidBody(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_RigidBody::GetControllerDescription() const
{
	return LOCTEXT("AnimGraphNode_RigidBody_ControllerDescription", "Rigid body simulation for physics asset");
}

FText UAnimGraphNode_RigidBody::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_RigidBody_Tooltip", "This simulates based on the skeletal mesh component's physics asset");
}

FText UAnimGraphNode_RigidBody::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText(LOCTEXT("AnimGraphNode_RigidBody_NodeTitle", "RigidBody"));
}

void UAnimGraphNode_RigidBody::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	if(Node.bEnableWorldGeometry && Node.SimulationSpace != ESimulationSpace::WorldSpace)
	{
		MessageLog.Error(*LOCTEXT("AnimGraphNode_CompileError", "@@ - uses world collision without world space simulation. This is not supported").ToString());
	}
	
	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

#undef LOCTEXT_NAMESPACE
