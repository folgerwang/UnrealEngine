// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ITDSpatializationSourceSettingsFactory.h"
#include "ITDSpatializationSourceSettings.h"

FText FAssetTypeActions_ITDSpatializationSettings::GetName() const
{
	return NSLOCTEXT("Spatialization", "FAssetTypeActions_ITDSpatializationSettings", "ITD Source Spatialization Settings");
}

FColor FAssetTypeActions_ITDSpatializationSettings::GetTypeColor() const
{
	return FColor(145, 145, 145);
}

UClass* FAssetTypeActions_ITDSpatializationSettings::GetSupportedClass() const
{
	return UITDSpatializationSourceSettings::StaticClass();
}

uint32 FAssetTypeActions_ITDSpatializationSettings::GetCategories()
{
	return EAssetTypeCategories::Sounds;
}

UITDSpatializationSettingsFactory::UITDSpatializationSettingsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UITDSpatializationSourceSettings::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UITDSpatializationSettingsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UITDSpatializationSourceSettings>(InParent, InName, Flags);
}

uint32 UITDSpatializationSettingsFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Sounds;
}