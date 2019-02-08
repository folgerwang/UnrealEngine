// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TimeSynchronizationSource.h"

#if WITH_EDITOR
#include "Widgets/SNullWidget.h"
#include "Widgets/SWidget.h"
#endif

UTimeSynchronizationSource::UTimeSynchronizationSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUseForSynchronization(true)
{

}

#if WITH_EDITOR
TSharedRef<SWidget> UTimeSynchronizationSource::GetVisualWidget() const 
{ 
	return SNullWidget::NullWidget; 
};
#endif