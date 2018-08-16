// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "IMessageContext.h"
#include "MessageEndpoint.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Misc/Guid.h"

struct FLiveLinkPongMessage;
struct FMessageAddress;
struct FProviderPollResult;
class ITableRow;
class STableViewBase;

typedef TSharedPtr<FProviderPollResult> FProviderPollResultPtr;

class SLiveLinkMagicLeapHandTrackingSourceEditor : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLiveLinkMagicLeapHandTrackingSourceEditor)
	{
	}

	SLATE_END_ARGS()

	~SLiveLinkMagicLeapHandTrackingSourceEditor();

	void Construct(const FArguments& Args);
};