// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicEntryWidgetDetailsBase.h"

class FListViewBaseDetails : public FDynamicEntryWidgetDetailsBase
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
};