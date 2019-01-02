// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Chaos/ChaosSolver.h"

DEFINE_LOG_CATEGORY_STATIC(FSC_Log, NoLogging, All);

UChaosSolver::UChaosSolver(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer)
{
	check(ObjectInitializer.GetClass() == GetClass());
}


