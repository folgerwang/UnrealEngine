// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaOutputFactoryNew.h"

#include "AssetTypeCategories.h"
#include "BlackmagicMediaOutput.h"


/* UBlackmagicMediaOutputFactoryNew structors
 *****************************************************************************/

UBlackmagicMediaOutputFactoryNew::UBlackmagicMediaOutputFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UBlackmagicMediaOutput::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory overrides
 *****************************************************************************/

UObject* UBlackmagicMediaOutputFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UBlackmagicMediaOutput>(InParent, InClass, InName, Flags);
}


uint32 UBlackmagicMediaOutputFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}


bool UBlackmagicMediaOutputFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
