// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/SWidget.h"

/**
 * Implements a details view customization for the MediaPort
 */
class FAjaMediaModeCustomization : public IPropertyTypeCustomization
{
public:
	FAjaMediaModeCustomization(bool InOutput = false);

	static TSharedRef<IPropertyTypeCustomization> MakeInputInstance()
	{
		return MakeShareable(new FAjaMediaModeCustomization());
	}

	static TSharedRef<IPropertyTypeCustomization> MakeOutputInstance()
	{
		return MakeShareable(new FAjaMediaModeCustomization(true));
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;

private:
	TSharedRef<SWidget> HandleSourceComboButtonMenuContent() const;

	/** Direction filter */
	bool bOutput;

	/** Pointer to the MediaPort property handle. */
	TSharedPtr<IPropertyHandle> MediaModeProperty;
};
