// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Internationalization/ILocalizedTextSource.h"
#include "Internationalization/PolyglotTextData.h"
#include "Internationalization/LocKeyFuncs.h"

/**
 * Implementation of a localized text source that stores literal polyglot data registered at runtime.
 */
class FPolyglotTextSource : public ILocalizedTextSource
{
public:
	//~ ILocalizedTextSource interface
	virtual int32 GetPriority() const override { return ELocalizedTextSourcePriority::Highest; }
	virtual bool GetNativeCultureName(const ELocalizedTextSourceCategory InCategory, FString& OutNativeCultureName) override;
	virtual void GetLocalizedCultureNames(const ELocalizationLoadFlags InLoadFlags, TSet<FString>& OutLocalizedCultureNames) override;
	virtual void LoadLocalizedResources(const ELocalizationLoadFlags InLoadFlags, TArrayView<const FString> InPrioritizedCultures, FTextLocalizationResource& InOutNativeResource, FTextLocalizationResource& InOutLocalizedResource) override;

	/**
	 * Register a polyglot text data with this localized text source.
	 */
	void RegisterPolyglotTextData(const FPolyglotTextData& InPolyglotTextData);

private:
	/**
	 * Register the culture names associated with the given polyglot data (called as it is added to the map).
	 */
	void RegisterCultureNames(const FPolyglotTextData& InPolyglotTextData);

	/**
	 * Unregister the culture names associated with the given polyglot data (called as it is removed from the map).
	 */
	void UnregisterCultureNames(const FPolyglotTextData& InPolyglotTextData);

	struct FCultureInfo
	{
		TMap<FString, int32> NativeCultures;
		TMap<FString, int32> LocalizedCultures;
	};

	/**
	 * Mapping from a localization category to the currently available culture information.
	 */
	TMap<ELocalizedTextSourceCategory, FCultureInfo> AvailableCultureInfo;

	/**
	 * Mapping from a "{Namespace}::{Key}" string to a polyglot text data instance.
	 */
	TMap<FLocKey, FPolyglotTextData> PolyglotTextDataMap;
};
