// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameRate.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SWidget;

/*
 * STimecodeProviderTab
 */
class STimecodeProviderTab : public SCompoundWidget
{
public:
	static void RegisterNomadTabSpawner();
	static void UnregisterNomadTabSpawner();

public:
	SLATE_BEGIN_ARGS(STimecodeProviderTab) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> OnGetMenuContent();
};
