// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;

/**
 * Customizes a soft object path to look like a UObject property
 */
class FSoftObjectPathCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() 
	{
		return MakeShareable( new FSoftObjectPathCustomization );
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

private:
	bool OnShouldFilterAsset( const struct FAssetData& InAssetData ) const;

private:
	/** Handle to the struct property being customized */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	/** Classes that can be used with this property */
	TArray<UClass*> AllowedClassFilters;
	/** Classes that can NOT be used with this property */
	TArray<UClass*> DisallowedClassFilters;
	/** Whether the classes in the above array must be an exact match, or whether they can also be a derived type; default is false */
	bool bExactClass;
};
