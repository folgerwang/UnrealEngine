// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/PolyglotTextSource.h"
#include "Internationalization/TextLocalizationResource.h"

bool FPolyglotTextSource::GetNativeCultureName(const ELocalizedTextSourceCategory InCategory, FString& OutNativeCultureName)
{
	if (FCultureInfo* CultureInfo = AvailableCultureInfo.Find(InCategory))
	{
		for (const auto& NativeCulturePair : CultureInfo->NativeCultures)
		{
			OutNativeCultureName = NativeCulturePair.Key;
			return true;
		}
	}
	return false;
}

void FPolyglotTextSource::GetLocalizedCultureNames(const ELocalizationLoadFlags InLoadFlags, TSet<FString>& OutLocalizedCultureNames)
{
	auto AppendCulturesForCategory = [this, &OutLocalizedCultureNames](const ELocalizedTextSourceCategory InCategory)
	{
		auto AppendCulturesForMap = [&OutLocalizedCultureNames](const TMap<FString, int32>& CulturesMap)
		{
			TArray<FString> Cultures;
			CulturesMap.GenerateKeyArray(Cultures);
			OutLocalizedCultureNames.Append(Cultures);
		};

		if (FCultureInfo* CultureInfo = AvailableCultureInfo.Find(InCategory))
		{
			AppendCulturesForMap(CultureInfo->NativeCultures);
			AppendCulturesForMap(CultureInfo->LocalizedCultures);
		}
	};

	if (EnumHasAnyFlags(InLoadFlags, ELocalizationLoadFlags::Editor))
	{
		AppendCulturesForCategory(ELocalizedTextSourceCategory::Editor);
	}
	if (EnumHasAnyFlags(InLoadFlags, ELocalizationLoadFlags::Game))
	{
		AppendCulturesForCategory(ELocalizedTextSourceCategory::Game);
	}
	if (EnumHasAnyFlags(InLoadFlags, ELocalizationLoadFlags::Engine))
	{
		AppendCulturesForCategory(ELocalizedTextSourceCategory::Engine);
	}
}

void FPolyglotTextSource::LoadLocalizedResources(const ELocalizationLoadFlags InLoadFlags, TArrayView<const FString> InPrioritizedCultures, FTextLocalizationResource& InOutNativeResource, FTextLocalizationResources& InOutLocalizedResources)
{
	auto ShouldLoadLocalizedText = [InLoadFlags](const FPolyglotTextData& InPolyglotTextData) -> bool
	{
		switch (InPolyglotTextData.GetCategory())
		{
		case ELocalizedTextSourceCategory::Game:
			return ShouldLoadGame(InLoadFlags);
		case ELocalizedTextSourceCategory::Engine:
			return ShouldLoadEngine(InLoadFlags);
		case ELocalizedTextSourceCategory::Editor:
			return ShouldLoadEditor(InLoadFlags);
		default:
			checkf(false, TEXT("Unknown ELocalizedTextSourceCategory!"));
			break;
		}
		return false;
	};

	auto GetLocalizedStringForPolyglotData = [](const FPolyglotTextData& InPolyglotTextData, TArrayView<const FString> InCulturesToCheck, FString* OutLocalizedCulture = nullptr) -> FString
	{
		for (const FString& CultureName : InCulturesToCheck)
		{
			FString LocalizedString;
			if (InPolyglotTextData.GetLocalizedString(CultureName, LocalizedString))
			{
				if (OutLocalizedCulture)
				{
					*OutLocalizedCulture = CultureName;
				}
				return LocalizedString;
			}
		}

		if (OutLocalizedCulture)
		{
			*OutLocalizedCulture = InPolyglotTextData.ResolveNativeCulture();
		}
		return InPolyglotTextData.GetNativeString();
	};

	for (const auto& PolyglotTextDataPair : PolyglotTextDataMap)
	{
		const FPolyglotTextData& PolyglotTextData = PolyglotTextDataPair.Value;
		const FString NativeCulture = PolyglotTextData.ResolveNativeCulture();

		// We skip loading the native text if we're transitioning to the native culture as there's no extra work that needs to be done
		if (ShouldLoadNative(InLoadFlags) && !InPrioritizedCultures.Contains(NativeCulture))
		{
			InOutNativeResource.AddEntry(
				PolyglotTextData.GetNamespace(),
				PolyglotTextData.GetKey(),
				PolyglotTextData.GetNativeString(),
				GetLocalizedStringForPolyglotData(PolyglotTextData, TArrayView<const FString>(&NativeCulture, 1))
				);
		}

		if (ShouldLoadLocalizedText(PolyglotTextData))
		{
			if (PolyglotTextData.GetCategory() == ELocalizedTextSourceCategory::Game && ShouldLoadNativeGameData(InLoadFlags))
			{
				// The editor cheats and loads the native language's localizations for game data.
				TSharedRef<FTextLocalizationResource> TextLocalizationResource = InOutLocalizedResources.EnsureResource(InPrioritizedCultures[0]);
				TextLocalizationResource->AddEntry(
					PolyglotTextData.GetNamespace(),
					PolyglotTextData.GetKey(),
					PolyglotTextData.GetNativeString(),
					GetLocalizedStringForPolyglotData(PolyglotTextData, TArrayView<const FString>(&NativeCulture, 1))
					);
			}
			else
			{
				// Find culture localization resource.
				FString LocalizedCulture;
				const FString LocalizedString = GetLocalizedStringForPolyglotData(PolyglotTextData, InPrioritizedCultures, &LocalizedCulture);
				TSharedRef<FTextLocalizationResource> TextLocalizationResource = InOutLocalizedResources.EnsureResource(LocalizedCulture);
				TextLocalizationResource->AddEntry(
					PolyglotTextData.GetNamespace(),
					PolyglotTextData.GetKey(),
					PolyglotTextData.GetNativeString(),
					LocalizedString
					);
			}
		}
	}
}

void FPolyglotTextSource::RegisterPolyglotTextData(const FPolyglotTextData& InPolyglotTextData)
{
	check(InPolyglotTextData.IsValid());

	const FLocKey Identity = FString::Printf(TEXT("%s::%s"), *InPolyglotTextData.GetNamespace(), *InPolyglotTextData.GetKey());
	if (FPolyglotTextData* CurrentPolyglotData = PolyglotTextDataMap.Find(Identity))
	{
		UnregisterCultureNames(*CurrentPolyglotData);
		*CurrentPolyglotData = InPolyglotTextData;
		RegisterCultureNames(*CurrentPolyglotData);
	}
	else
	{
		PolyglotTextDataMap.Add(Identity, InPolyglotTextData);
		RegisterCultureNames(InPolyglotTextData);
	}
}

void FPolyglotTextSource::RegisterCultureNames(const FPolyglotTextData& InPolyglotTextData)
{
	auto IncrementCultureCount = [](TMap<FString, int32>& CulturesMap, const FString& CultureName)
	{
		if (!CultureName.IsEmpty())
		{
			int32& CultureCount = CulturesMap.FindOrAdd(CultureName);
			++CultureCount;
		}
	};

	FCultureInfo& CultureInfo = AvailableCultureInfo.FindOrAdd(InPolyglotTextData.GetCategory());

	const FString& NativeCulture = InPolyglotTextData.GetNativeCulture();
	IncrementCultureCount(CultureInfo.NativeCultures, NativeCulture);

	const TArray<FString> LocalizedCultures = InPolyglotTextData.GetLocalizedCultures();
	for (const FString& LocalizedCulture : LocalizedCultures)
	{
		IncrementCultureCount(CultureInfo.LocalizedCultures, LocalizedCulture);
	}
}

void FPolyglotTextSource::UnregisterCultureNames(const FPolyglotTextData& InPolyglotTextData)
{
	auto DecrementCultureCount = [](TMap<FString, int32>& CulturesMap, const FString& CultureName)
	{
		if (!CultureName.IsEmpty())
		{
			int32& CultureCount = CulturesMap.FindOrAdd(CultureName);
			--CultureCount;
			check(CultureCount >= 0);
			if (CultureCount == 0)
			{
				CulturesMap.Remove(CultureName);
			}
		}
	};

	FCultureInfo& CultureInfo = AvailableCultureInfo.FindOrAdd(InPolyglotTextData.GetCategory());

	const FString& NativeCulture = InPolyglotTextData.GetNativeCulture();
	DecrementCultureCount(CultureInfo.NativeCultures, NativeCulture);

	const TArray<FString> LocalizedCultures = InPolyglotTextData.GetLocalizedCultures();
	for (const FString& LocalizedCulture : LocalizedCultures)
	{
		DecrementCultureCount(CultureInfo.LocalizedCultures, LocalizedCulture);
	}
}
