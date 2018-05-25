// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/ILocalizedTextSource.h"

/**
 * Implementation of a localized text source that loads data from Localization Resource (LocRes) files.
 */
class FLocalizationResourceTextSource : public ILocalizedTextSource
{
public:
	//~ ILocalizedTextSource interface
	virtual bool GetNativeCultureName(const ELocalizedTextSourceCategory InCategory, FString& OutNativeCultureName) override;
	virtual void GetLocalizedCultureNames(const ELocalizationLoadFlags InLoadFlags, TSet<FString>& OutLocalizedCultureNames) override;
	virtual void LoadLocalizedResources(const ELocalizationLoadFlags InLoadFlags, TArrayView<const FString> InPrioritizedCultures, FTextLocalizationResource& InOutNativeResource, FTextLocalizationResources& InOutLocalizedResources) override;
};
