// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicEntryWidgetDetailsBase.h"


class IPropertyHandle;
class UDynamicEntryBox;

class FDynamicEntryBoxDetails : public FDynamicEntryWidgetDetailsBase
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	/* Main customization of details */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:
	bool CanEditSpacingPattern() const;
	bool CanEditEntrySpacing() const;
	bool CanEditAlignment() const;
	bool CanEditMaxElementSize() const;

	TWeakObjectPtr<UDynamicEntryBox> EntryBox;
};