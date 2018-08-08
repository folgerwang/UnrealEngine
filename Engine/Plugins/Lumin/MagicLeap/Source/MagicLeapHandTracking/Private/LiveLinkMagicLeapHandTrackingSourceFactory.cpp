// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "LiveLinkMagicLeapHandTrackingSourceFactory.h"
#include "LiveLinkMagicLeapHandTrackingSourceEditor.h"
#include "IMagicLeapHandTrackingPlugin.h"
#include "MagicLeapHandTracking.h"

#define LOCTEXT_NAMESPACE "MagicLeapHandTracking"

FText ULiveLinkMagicLeapHandTrackingSourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("HandTrackingLiveLinkSourceName", "Hand Tracking Source");
}

FText ULiveLinkMagicLeapHandTrackingSourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("HandTrackingLiveLinkSourceTooltip", "Hand Tracking Key Points Source");
}

TSharedPtr<SWidget> ULiveLinkMagicLeapHandTrackingSourceFactory::CreateSourceCreationPanel()
{
	if (!ActiveSourceEditor.IsValid())
	{
		SAssignNew(ActiveSourceEditor, SLiveLinkMagicLeapHandTrackingSourceEditor);
	}
	return ActiveSourceEditor;
}

TSharedPtr<ILiveLinkSource> ULiveLinkMagicLeapHandTrackingSourceFactory::OnSourceCreationPanelClosed(bool bCreateSource)
{
	TSharedPtr<ILiveLinkSource> NewSource = nullptr;

	if (bCreateSource && ActiveSourceEditor.IsValid())
	{
		TSharedPtr<FMagicLeapHandTracking> HandTracking = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetLiveLinkSource());

		// Here we could apply settings from SLiveLinkMagicLeapHandTrackingSourceEditor

		NewSource = HandTracking;
	}
	ActiveSourceEditor = nullptr;
	return NewSource;
}

#undef LOCTEXT_NAMESPACE