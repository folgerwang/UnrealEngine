// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/NumericTypeInterface.h"

class IDetailLayoutBuilder;

/**
 *  Customize the FTimecode.
 */
class FTimecodeDetailsCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FTimecodeDetailsCustomization>();
	}

	FTimecodeDetailsCustomization()
	{
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	FText OnGetTimecodeText() const;
	void OnTimecodeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);

	/** Store the property handle to the Timecode field so we can get/set the value on the object via text box callbacks. */
	TSharedPtr<IPropertyHandle> TimecodeProperty;
};
