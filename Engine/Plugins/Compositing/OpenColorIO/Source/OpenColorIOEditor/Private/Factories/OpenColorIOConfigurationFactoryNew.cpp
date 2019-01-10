// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOConfigurationFactoryNew.h"

#include "AssetTypeCategories.h"
#include "OpenColorIOConfiguration.h"


/* UOpenColorIOConfigAssetFactoryNew structors
 *****************************************************************************/

UOpenColorIOConfigurationFactoryNew::UOpenColorIOConfigurationFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UOpenColorIOConfiguration::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory overrides
 *****************************************************************************/

UObject* UOpenColorIOConfigurationFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UOpenColorIOConfiguration>(InParent, InClass, InName, Flags);
}


uint32 UOpenColorIOConfigurationFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Misc;
}


bool UOpenColorIOConfigurationFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
