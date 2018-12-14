// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Layout/WidgetPath.h"

class IApplicationElement;

class FSlateWidgetElementFactory
{
public:

	static TSharedRef<IApplicationElement> Create(
		const FWidgetPath& WidgetPath);
};