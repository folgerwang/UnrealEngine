// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/SWidget.h"

/**
 * Implements a details view customization for the FOpenColorIOConfigurationCustomization
 */
class FOpenColorIOColorSpaceCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FOpenColorIOColorSpaceCustomization);
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;

private:
	TSharedRef<SWidget> HandleSourceComboButtonMenuContent() const;

	/** Pointer to the ColorSpace property handle. */
	TSharedPtr<IPropertyHandle> ColorSpaceProperty;

	/** Pointer to the ConfigurationFile property handle. */
	TSharedPtr<IPropertyHandle> ConfigurationFileProperty;
};

