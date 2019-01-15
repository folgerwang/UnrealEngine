// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/SWidget.h"

/**
 * Implements a details view customization for the FOpenColorIOColorSpaceConversion
 */
class FOpenColorIOColorSpaceConversionCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FOpenColorIOColorSpaceConversionCustomization);
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;

private:
	void AddColorSpaceRow(FDetailWidgetRow& InWidgetRow, TSharedRef<IPropertyHandle> InChildHandle, IPropertyTypeCustomizationUtils& InCustomizationUtils) const;
	TSharedRef<SWidget> HandleColorSpaceComboButtonMenuContent(TSharedPtr<IPropertyHandle> InPropertyHandle) const;

	/** Pointer to the ColorConversion struct property handle. */
	TSharedPtr<IPropertyHandle> ColorConversionProperty;
	
	/** Pointer to the ColorConversion struct member SourceColorSpace property handle. */
	TSharedPtr<IPropertyHandle> SourceColorSpaceProperty;
	
	/** Pointer to the ColorConversion struct member DestinationColorSpace property handle. */
	TSharedPtr<IPropertyHandle> DestinationColorSpaceProperty;
};
