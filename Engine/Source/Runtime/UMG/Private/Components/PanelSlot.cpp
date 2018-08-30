// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"

/////////////////////////////////////////////////////
// UPanelSlot

UPanelSlot::UPanelSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPanelSlot::IsDesignTime() const
{
	return Parent->IsDesignTime();
}

void UPanelSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	// ReleaseSlateResources for Content unless the content is a UUserWidget as they are responsible for releasing their own content.
	if (Content && !Content->IsA<UUserWidget>())
	{
		Content->ReleaseSlateResources(bReleaseChildren);
	}
}
