// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NetAnalyticsAggregatorConfig.h"


/**
 * UNetAnalyticsAggregatorConfig
 */
UNetAnalyticsAggregatorConfig::UNetAnalyticsAggregatorConfig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNetAnalyticsAggregatorConfig::OverridePerObjectConfigSection(FString& SectionName)
{
	SectionName = GetName() + TEXT(" ") + GetClass()->GetName();
}
