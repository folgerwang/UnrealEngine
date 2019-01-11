// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertMessageData.h"
#include "Framework/SlateDelegates.h"

enum class EConcertUIStyle : uint8
{
	Normal,
	Primary,
	Info,
	Success,
	Warning,
	Danger,
	NUM,
};

struct FConcertUIButtonDefinition
{
	FConcertUIButtonDefinition()
		: Style(EConcertUIStyle::Normal)
		, IsEnabled(true)
		, Visibility(EVisibility::Visible)
	{
	}

	EConcertUIStyle Style;
	TAttribute<bool> IsEnabled;
	TAttribute<EVisibility> Visibility;
	TAttribute<FText> Text;
	TAttribute<FText> ToolTipText;
	FOnClicked OnClicked;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnConcertUIStatusButtonExtension, TArray<FConcertUIButtonDefinition>& /*OutButtonDefs*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnConcertUIServerButtonExtension, const FConcertServerInfo& /*InServerInfo*/, TArray<FConcertUIButtonDefinition>& /*OutButtonDefs*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnConcertUISessionButtonExtension, const FConcertSessionInfo& /*InSessionInfo*/, TArray<FConcertUIButtonDefinition>& /*OutButtonDefs*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnConcertUIClientButtonExtension, const FConcertSessionClientInfo& /*InClientInfo*/, TArray<FConcertUIButtonDefinition>& /*OutButtonDefs*/);
