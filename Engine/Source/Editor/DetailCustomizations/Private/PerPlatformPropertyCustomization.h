// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "UnrealClient.h"
#include "IPropertyTypeCustomization.h"
#include "PerPlatformProperties.h"

class FDetailWidgetDecl;

/**
* Implements a details panel customization for the FPerPlatform structures.
*/
template<typename PerPlatformType>
class FPerPlatformPropertyCustomization : public IPropertyTypeCustomization
{
public:
	FPerPlatformPropertyCustomization()
	{}

	// IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}

	/**
	* Creates a new instance.
	*
	* @return A new customization for FPerPlatform structs.
	*/
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	TSharedRef<SWidget> GetWidget(FName PlatformGroupName, TSharedRef<IPropertyHandle> StructPropertyHandle) const;
	TArray<FName> GetPlatformOverrideNames(TSharedRef<IPropertyHandle> StructPropertyHandle) const;
	bool AddPlatformOverride(FName PlatformGroupName, TSharedRef<IPropertyHandle> StructPropertyHandle);
	bool RemovePlatformOverride(FName PlatformGroupName, TSharedRef<IPropertyHandle> StructPropertyHandle);
	float CalcDesiredWidth(TSharedRef<IPropertyHandle> StructPropertyHandle);

private:
	/** Cached utils used for resetting customization when layout changes */
	TWeakPtr<IPropertyUtilities> PropertyUtilities;
};
