// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "Field/FieldSystemActor.h"

#include "Field/FieldSystemComponent.h"

DEFINE_LOG_CATEGORY_STATIC(AFA_Log, NoLogging, All);

AFieldSystemActor::AFieldSystemActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UE_LOG(AFA_Log, Log, TEXT("AFieldSystemActor::AFieldSystemActor()"));

	FieldSystemComponent = CreateDefaultSubobject<UFieldSystemComponent>(TEXT("FieldSystemComponent"));
	RootComponent = FieldSystemComponent;
}





