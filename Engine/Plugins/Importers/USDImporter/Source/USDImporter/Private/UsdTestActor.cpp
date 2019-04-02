// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UsdTestActor.h"



AUsdTestActor::AUsdTestActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootComponent = TestComponent = CreateDefaultSubobject<UUsdTestComponent>(TEXT("Root"));
}

