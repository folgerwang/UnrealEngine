// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Tasks/BTTask_FinishWithResult.h"

UBTTask_FinishWithResult::UBTTask_FinishWithResult(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = "FinishWithResult";
	Result = EBTNodeResult::Succeeded;
}

EBTNodeResult::Type UBTTask_FinishWithResult::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return Result;
}

FString UBTTask_FinishWithResult::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s %s"), *Super::GetStaticDescription()
		, *UBehaviorTreeTypes::DescribeNodeResult(Result.GetValue()));
}
