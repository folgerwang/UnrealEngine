// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaOutputFactoryNew.h"

#include "AssetTypeCategories.h"
#include "AjaMediaOutput.h"


/* UAjaMediaSourceFactoryNew structors
 *****************************************************************************/

UAjaMediaOutputFactoryNew::UAjaMediaOutputFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UAjaMediaOutput::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory overrides
 *****************************************************************************/

UObject* UAjaMediaOutputFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UAjaMediaOutput>(InParent, InClass, InName, Flags);
}


uint32 UAjaMediaOutputFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}


bool UAjaMediaOutputFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
