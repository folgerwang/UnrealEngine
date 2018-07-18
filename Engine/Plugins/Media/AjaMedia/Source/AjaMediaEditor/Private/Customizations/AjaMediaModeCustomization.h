// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AjaMediaFinder.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/SWidget.h"

/**
 * Implements a details view customization for the MediaPort
 */
class FAjaMediaModeCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FAjaMediaModeCustomization());
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;

private:
	TSharedRef<SWidget> HandleSourceComboButtonMenuContent() const;
	FAjaMediaMode GetMediaModeValue(const FAjaMediaMode& InMediaModeValue) const;

	/** Direction filter */
	bool bOutput;

	/** Pointer to the MediaMode property handle. */
	TSharedPtr<IPropertyHandle> MediaModeProperty;

	/** Pointer to the MediaPort property handle. */
	TSharedPtr<IPropertyHandle> MediaPortProperty;

	/** Pointer to the OverrideDefault property handle. */
	TWeakObjectPtr<UBoolProperty> OverrideProperty;

	/** Pointer to the parent Object. */
	TWeakObjectPtr<UObject> ParentObject;
};
