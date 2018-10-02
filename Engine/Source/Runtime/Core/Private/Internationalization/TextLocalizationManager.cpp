// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Internationalization/ILocalizedTextSource.h"
#include "Internationalization/LocalizationResourceTextSource.h"
#include "Internationalization/PolyglotTextSource.h"
#include "Internationalization/StringTableRegistry.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/Parse.h"
#include "Templates/ScopedPointer.h"
#include "Misc/ScopeLock.h"
#include "Misc/CommandLine.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Stats/Stats.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "Templates/UniquePtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextLocalizationManager, Log, All);

const FString& GetAccessedStringBeforeLocLoadedErrorMsg()
{
	// Note: This is in a function to ensure it is initialized before we use it (eg, so that a file-scope static FText isn't being initialized before this string)
	static const FString AccessedStringBeforeLocLoadedErrorMsg = TEXT("Can't access string. Loc System hasn't been initialized yet!");
	return AccessedStringBeforeLocLoadedErrorMsg;
}

bool IsLocalizationLockedByConfig()
{
	bool bIsLocalizationLocked = false;
	if (!GConfig->GetBool(TEXT("Internationalization"), TEXT("LockLocalization"), bIsLocalizationLocked, GGameIni))
	{
		GConfig->GetBool(TEXT("Internationalization"), TEXT("LockLocalization"), bIsLocalizationLocked, GEngineIni);
	}
	return bIsLocalizationLocked;
}

void ApplyDefaultCultureSettings(const ELocalizationLoadFlags LocLoadFlags)
{
	const bool ShouldLoadNative = EnumHasAllFlags( LocLoadFlags, ELocalizationLoadFlags::Native);
	const bool ShouldLoadEditor = EnumHasAllFlags(LocLoadFlags, ELocalizationLoadFlags::Editor);
	const bool ShouldLoadGame = EnumHasAllFlags(LocLoadFlags, ELocalizationLoadFlags::Game);
	const bool ShouldLoadEngine = EnumHasAllFlags(LocLoadFlags, ELocalizationLoadFlags::Engine);
	const bool ShouldLoadAdditional = EnumHasAllFlags(LocLoadFlags, ELocalizationLoadFlags::Additional);

	FInternationalization& I18N = FInternationalization::Get();

	// Set culture according to configuration now that configs are available.
#if ENABLE_LOC_TESTING
	if (FCommandLine::IsInitialized() && FParse::Param(FCommandLine::Get(), TEXT("LEET")))
	{
		I18N.SetCurrentCulture(TEXT("LEET"));
	}
	else
#endif
	{
		FString RequestedLanguage;
		FString RequestedLocale;
		TArray<TTuple<FName, FString>> RequestedAssetGroups;

		auto ReadSettingsFromCommandLine = [&RequestedLanguage, &RequestedLocale]()
		{
			if (RequestedLanguage.IsEmpty() && FParse::Value(FCommandLine::Get(), TEXT("LANGUAGE="), RequestedLanguage))
			{
				UE_LOG(LogInit, Log, TEXT("Overriding language with language command-line option (%s)."), *RequestedLanguage);
			}

			if (RequestedLocale.IsEmpty() && FParse::Value(FCommandLine::Get(), TEXT("LOCALE="), RequestedLocale))
			{
				UE_LOG(LogInit, Log, TEXT("Overriding locale with locale command-line option (%s)."), *RequestedLocale);
			}

			FString CultureOverride;
			if (FParse::Value(FCommandLine::Get(), TEXT("CULTURE="), CultureOverride))
			{
				if (RequestedLanguage.IsEmpty())
				{
					RequestedLanguage = CultureOverride;
					UE_LOG(LogInit, Log, TEXT("Overriding language with culture command-line option (%s)."), *RequestedLanguage);
				}
				if (RequestedLocale.IsEmpty())
				{
					RequestedLocale = CultureOverride;
					UE_LOG(LogInit, Log, TEXT("Overriding locale with culture command-line option (%s)."), *RequestedLocale);
				}
			}
		};

		auto ReadSettingsFromConfig = [&RequestedLanguage, &RequestedLocale, &RequestedAssetGroups](const TCHAR* InConfigLogName, const FString& InConfigFilename)
		{
			if (RequestedLanguage.IsEmpty())
			{
				if (const FConfigSection* AssetGroupCulturesSection = GConfig->GetSectionPrivate(TEXT("Internationalization.AssetGroupCultures"), false, true, InConfigFilename))
				{
					for (const auto& SectionEntryPair : *AssetGroupCulturesSection)
					{
						const bool bAlreadyExists = RequestedAssetGroups.ContainsByPredicate([&](const TTuple<FName, FString>& InRequestedAssetGroup)
						{
							return InRequestedAssetGroup.Key == SectionEntryPair.Key;
						});

						if (!bAlreadyExists)
						{
							RequestedAssetGroups.Add(MakeTuple(SectionEntryPair.Key, SectionEntryPair.Value.GetValue()));
							UE_LOG(LogInit, Log, TEXT("Overriding asset group '%s' with %s configuration option (%s)."), *SectionEntryPair.Key.ToString(), InConfigLogName, *SectionEntryPair.Value.GetValue());
						}
					}
				}
			}

			if (RequestedLanguage.IsEmpty() && GConfig->GetString(TEXT("Internationalization"), TEXT("Language"), RequestedLanguage, InConfigFilename))
			{
				UE_LOG(LogInit, Log, TEXT("Overriding language with %s language configuration option (%s)."), InConfigLogName, *RequestedLanguage);
			}

			if (RequestedLocale.IsEmpty() && GConfig->GetString(TEXT("Internationalization"), TEXT("Locale"), RequestedLocale, InConfigFilename))
			{
				UE_LOG(LogInit, Log, TEXT("Overriding locale with %s locale configuration option (%s)."), InConfigLogName, *RequestedLocale);
			}

			FString CultureOverride;
			if (GConfig->GetString(TEXT("Internationalization"), TEXT("Culture"), CultureOverride, InConfigFilename))
			{
				if (RequestedLanguage.IsEmpty())
				{
					RequestedLanguage = CultureOverride;
					UE_LOG(LogInit, Log, TEXT("Overriding language with %s culture configuration option (%s)."), InConfigLogName, *RequestedLanguage);
				}
				if (RequestedLocale.IsEmpty())
				{
					RequestedLocale = CultureOverride;
					UE_LOG(LogInit, Log, TEXT("Overriding locale with %s culture configuration option (%s)."), InConfigLogName, *RequestedLocale);
				}
			}
		};

		auto ReadSettingsFromDefaults = [&RequestedLanguage, &RequestedLocale, &I18N]()
		{
			if (RequestedLanguage.IsEmpty())
			{
				RequestedLanguage = I18N.GetDefaultLanguage()->GetName();
				UE_LOG(LogInit, Log, TEXT("Using OS detected language (%s)."), *RequestedLanguage);
			}

			if (RequestedLocale.IsEmpty())
			{
				RequestedLocale = I18N.GetDefaultLocale()->GetName();
				UE_LOG(LogInit, Log, TEXT("Using OS detected locale (%s)."), *RequestedLocale);
			}
		};

		if (FParse::Value(FCommandLine::Get(), TEXT("CULTUREFORCOOKING="), RequestedLanguage))
		{
			RequestedLocale = RequestedLanguage;

			// Write the culture passed in if first install...
			if (FParse::Param(FCommandLine::Get(), TEXT("firstinstall")))
			{
				GConfig->SetString(TEXT("Internationalization"), TEXT("Language"), *RequestedLanguage, GEngineIni);
				GConfig->SetString(TEXT("Internationalization"), TEXT("Locale"), *RequestedLocale, GEngineIni);
			}

			UE_LOG(LogInit, Log, TEXT("Overriding language with culture cook command-line option (%s)."), *RequestedLanguage);
			UE_LOG(LogInit, Log, TEXT("Overriding locale with culture cook command-line option (%s)."), *RequestedLocale);
		}
		// Read setting override specified on commandline.
		ReadSettingsFromCommandLine();
#if WITH_EDITOR
		// Read setting specified in editor configuration.
		if (GIsEditor)
		{
			ReadSettingsFromConfig(TEXT("editor"), GEditorSettingsIni);
		}
#endif // WITH_EDITOR
		// Read setting specified in game configurations.
		if (!GIsEditor)
		{
			ReadSettingsFromConfig(TEXT("game user settings"), GGameUserSettingsIni);
			ReadSettingsFromConfig(TEXT("game"), GGameIni);
		}
		// Read setting specified in engine configuration.
		ReadSettingsFromConfig(TEXT("engine"), GEngineIni);
		// Read defaults
		ReadSettingsFromDefaults();

		auto ValidateRequestedCulture = [ShouldLoadEditor, ShouldLoadGame, ShouldLoadEngine, ShouldLoadAdditional, &I18N](const FString& InRequestedCulture, const FString& InFallbackCulture, const TCHAR* InLogDesc, const bool bWarnIfNotExactMatch) -> FString
		{
			FString TargetCultureName = InRequestedCulture;

#if ENABLE_LOC_TESTING
			if (TargetCultureName != TEXT("LEET"))
#endif
			{
				ELocalizationLoadFlags ValidationFlags = ELocalizationLoadFlags::None;
				if (ShouldLoadGame)
				{
					ValidationFlags |= ELocalizationLoadFlags::Game;
				}
				else
				{
					if (ShouldLoadEditor)
					{
						ValidationFlags |= ELocalizationLoadFlags::Editor;
					}
					if ( ShouldLoadEngine )
					{
						ValidationFlags |= ELocalizationLoadFlags::Engine;
					}
				}
				// before the game has initialized we may have initialized a plugin (specifically common for use of loading screens)
				// these can support more languages then the engine
				if (ShouldLoadAdditional)
				{
					ValidationFlags |= ELocalizationLoadFlags::Additional;
				}

				// Validate the locale has data or fallback to one that does.
				const TArray<FString> AvailableCultureNames = FTextLocalizationManager::Get().GetLocalizedCultureNames(ValidationFlags);
				auto ValidateCultureName = [&AvailableCultureNames, &I18N](const FString& InCultureToValidate) -> FString
				{
					const TArray<FString> PrioritizedCultureNames = I18N.GetPrioritizedCultureNames(InCultureToValidate);
					for (const FString& CultureName : PrioritizedCultureNames)
					{
						if (AvailableCultureNames.Contains(CultureName))
						{
							return CultureName;
						}
					}
					return FString();
				};

				const FString ValidCultureName = ValidateCultureName(InRequestedCulture);
				const FString ValidFallbackCultureName = ValidateCultureName(InFallbackCulture);

				if (!ValidCultureName.IsEmpty())
				{
					if (bWarnIfNotExactMatch && InRequestedCulture != ValidCultureName)
					{
						// Make the user aware that the localization data belongs to a parent culture.
						UE_LOG(LogTextLocalizationManager, Log, TEXT("No specific localization for '%s' exists, so the '%s' localization will be used."), *InRequestedCulture, *ValidCultureName, InLogDesc);
					}
				}
				else if (!ValidFallbackCultureName.IsEmpty())
				{
					TargetCultureName = ValidFallbackCultureName;
					UE_LOG(LogTextLocalizationManager, Log, TEXT("No localization for '%s' exists, so '%s' will be used for the %s."), *InRequestedCulture, *TargetCultureName, InLogDesc);
				}
				else
				{
					TargetCultureName = AvailableCultureNames.Num() > 0 ? AvailableCultureNames[0] : InFallbackCulture;
					UE_LOG(LogTextLocalizationManager, Log, TEXT("No localization for '%s' exists, so '%s' will be used for the %s."), *InRequestedCulture, *TargetCultureName, InLogDesc);
				}
			}

			return TargetCultureName;
		};

		FString FallbackLanguage = TEXT("en");
		if (ShouldLoadGame)
		{
			// If this is a game, use the native culture of the game as the fallback
			FString NativeGameCulture = FTextLocalizationManager::Get().GetNativeCultureName(ELocalizedTextSourceCategory::Game);
			if (!NativeGameCulture.IsEmpty())
			{
				FallbackLanguage = MoveTemp(NativeGameCulture);
			}
		}

		// Validate that we have translations for this language and locale
		// Note: We skip the locale check for the editor as we a limited number of translations, but want to allow locale correct display of numbers, dates, etc
		const FString TargetLanguage = ValidateRequestedCulture(RequestedLanguage, FallbackLanguage, TEXT("language"), true);
		const FString TargetLocale = GIsEditor ? RequestedLocale : ValidateRequestedCulture(RequestedLocale, TargetLanguage, TEXT("locale"), false);
		if (TargetLanguage == TargetLocale)
		{
			I18N.SetCurrentLanguageAndLocale(TargetLanguage);
		}
		else
		{
			I18N.SetCurrentLanguage(TargetLanguage);
			I18N.SetCurrentLocale(TargetLocale);
		}

		for (const auto& RequestedAssetGroupPair : RequestedAssetGroups)
		{
			const FString TargetAssetGroupCulture = ValidateRequestedCulture(RequestedAssetGroupPair.Value, TargetLanguage, *FString::Printf(TEXT("'%s' asset group"), *RequestedAssetGroupPair.Key.ToString()), false);
			if (TargetAssetGroupCulture != TargetLanguage)
			{
				I18N.SetCurrentAssetGroupCulture(RequestedAssetGroupPair.Key, TargetAssetGroupCulture);
			}
		}
	}
}

void BeginInitTextLocalization()
{
	// Initialize FInternationalization before we bind to OnCultureChanged, otherwise we can accidentally initialize
	// twice since FInternationalization::Initialize sets the culture.
	FInternationalization::Get();

	// Make sure the String Table Registry is initialized as it may trigger module loads
	FStringTableRegistry::Get();

	FInternationalization::Get().OnCultureChanged().AddRaw(&(FTextLocalizationManager::Get()), &FTextLocalizationManager::OnCultureChanged);
}

void InitEngineTextLocalization()
{
	double StartTime = FPlatformTime::Seconds();

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("EndInitEngineTextLocalization"), STAT_EndInitTextLocalization, STATGROUP_LoadTime);

	FStringTableRedirects::InitStringTableRedirects();

	ELocalizationLoadFlags LocLoadFlags = ELocalizationLoadFlags::None;
	LocLoadFlags |= (WITH_EDITOR ? ELocalizationLoadFlags::Editor : ELocalizationLoadFlags::None);
	LocLoadFlags |= ELocalizationLoadFlags::Engine;
	LocLoadFlags |= ELocalizationLoadFlags::Additional;
	
	ELocalizationLoadFlags ApplyLocLoadFlags = LocLoadFlags;
	ApplyLocLoadFlags |= FApp::IsGame() ? ELocalizationLoadFlags::Game : ELocalizationLoadFlags::None;

	ApplyDefaultCultureSettings(ApplyLocLoadFlags);

#if WITH_EDITOR
	FTextLocalizationManager::Get().GameLocalizationPreviewAutoEnableCount = 0;
	FTextLocalizationManager::Get().bIsGameLocalizationPreviewEnabled = false;
	FTextLocalizationManager::Get().bIsLocalizationLocked = IsLocalizationLockedByConfig();
#endif

	FInternationalization& I18N = FInternationalization::Get();

	// (re)cache the native cultures for the engine and editor
	{
		const bool bSkipCache = true;
		TextLocalizationResourceUtil::GetNativeEngineCultureName(bSkipCache);
#if WITH_EDITOR
		TextLocalizationResourceUtil::GetNativeEditorCultureName(bSkipCache);
#endif
	}

	FTextLocalizationManager::Get().LoadLocalizationResourcesForCulture(I18N.GetCurrentLanguage()->GetName(), LocLoadFlags);
	FTextLocalizationManager::Get().bIsInitialized = true;
	UE_CLOG(!IS_PROGRAM, LogStreaming, Display, TEXT("Took %6.3fs to InitEngineTextLocalization."), FPlatformTime::Seconds() - StartTime);
}

void InitGameTextLocalization()
{
	if ( !FApp::IsGame() )
	{
		// early out because we are not a game ;)
		return;
	}

	double StartTime = FPlatformTime::Seconds();

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("InitGameTextLocalization"), STAT_EndInitTextLocalization, STATGROUP_LoadTime);

	ELocalizationLoadFlags LocLoadFlags = ELocalizationLoadFlags::None;
	LocLoadFlags |= (FApp::IsGame() ? ELocalizationLoadFlags::Game : ELocalizationLoadFlags::None);

	ApplyDefaultCultureSettings(LocLoadFlags);

	FInternationalization& I18N = FInternationalization::Get();

	// (re)cache the native cultures for the game
	{
		const bool bSkipCache = true;
		TextLocalizationResourceUtil::GetNativeProjectCultureName(bSkipCache);
	}

	FTextLocalizationManager::Get().LoadLocalizationResourcesForCulture(I18N.GetCurrentLanguage()->GetName(), LocLoadFlags);
	FTextLocalizationManager::Get().bIsInitialized = true;
	UE_CLOG(!IS_PROGRAM, LogStreaming, Display, TEXT("Took %6.3fs to InitGameTextLocalization."), FPlatformTime::Seconds() - StartTime);
}


void FTextLocalizationManager::FDisplayStringLookupTable::Find(const FString& InNamespace, FKeysTable*& OutKeysTableForNamespace, const FString& InKey, FDisplayStringEntry*& OutDisplayStringEntry)
{
	// Find namespace's key table.
	OutKeysTableForNamespace = NamespacesTable.Find( InNamespace );

	// Find key table's entry.
	OutDisplayStringEntry = OutKeysTableForNamespace ? OutKeysTableForNamespace->Find( InKey ) : nullptr;
}

void FTextLocalizationManager::FDisplayStringLookupTable::Find(const FString& InNamespace, const FKeysTable*& OutKeysTableForNamespace, const FString& InKey, const FDisplayStringEntry*& OutDisplayStringEntry) const
{
	// Find namespace's key table.
	OutKeysTableForNamespace = NamespacesTable.Find( InNamespace );

	// Find key table's entry.
	OutDisplayStringEntry = OutKeysTableForNamespace ? OutKeysTableForNamespace->Find( InKey ) : nullptr;
}

FTextLocalizationManager& FTextLocalizationManager::Get()
{
	static FTextLocalizationManager* GTextLocalizationManager = nullptr;
	if( !GTextLocalizationManager )
	{
		GTextLocalizationManager = new FTextLocalizationManager();
	}

	return *GTextLocalizationManager;
}

FTextLocalizationManager::FTextLocalizationManager()
	: bIsInitialized(false)
	, SynchronizationObject()
	, TextRevisionCounter(0)
	, PolyglotTextSource(MakeShared<FPolyglotTextSource>())
{
	const bool bRefreshResources = false;
	RegisterTextSource(MakeShared<FLocalizationResourceTextSource>(), bRefreshResources);
	RegisterTextSource(PolyglotTextSource.ToSharedRef(), bRefreshResources);
}

FString FTextLocalizationManager::GetNativeCultureName(const ELocalizedTextSourceCategory InCategory) const
{
	FString NativeCultureName;
	for (const TSharedPtr<ILocalizedTextSource>& LocalizedTextSource : LocalizedTextSources)
	{
		if (LocalizedTextSource->GetNativeCultureName(InCategory, NativeCultureName))
		{
			break;
		}
	}
	return NativeCultureName;
}

TArray<FString> FTextLocalizationManager::GetLocalizedCultureNames(const ELocalizationLoadFlags InLoadFlags) const
{
	TSet<FString> LocalizedCultureNameSet;
	for (const TSharedPtr<ILocalizedTextSource>& LocalizedTextSource : LocalizedTextSources)
	{
		LocalizedTextSource->GetLocalizedCultureNames(InLoadFlags, LocalizedCultureNameSet);
	}

	TArray<FString> LocalizedCultureNames = LocalizedCultureNameSet.Array();
	LocalizedCultureNames.Sort();
	return LocalizedCultureNames;
}

void FTextLocalizationManager::RegisterTextSource(const TSharedRef<ILocalizedTextSource>& InLocalizedTextSource, const bool InRefreshResources)
{
	LocalizedTextSources.Add(InLocalizedTextSource);
	LocalizedTextSources.StableSort([](const TSharedPtr<ILocalizedTextSource>& InLocalizedTextSourceOne, const TSharedPtr<ILocalizedTextSource>& InLocalizedTextSourceTwo)
	{
		return InLocalizedTextSourceOne->GetPriority() > InLocalizedTextSourceTwo->GetPriority();
	});

	if (InRefreshResources)
	{
		RefreshResources();
	}
}

void FTextLocalizationManager::RegisterPolyglotTextData(const FPolyglotTextData& InPolyglotTextData, const bool InAddDisplayString)
{
	check(PolyglotTextSource.IsValid());

	PolyglotTextSource->RegisterPolyglotTextData(InPolyglotTextData);

	if (InAddDisplayString)
	{
		auto GetLocalizedStringForPolyglotData = [this, &InPolyglotTextData]() -> FString
		{
			// Work out which culture to use - this is typically the current language unless we're in the 
			// editor where the game localization preview affects the language we use for game text
			FString CultureName;
			if (InPolyglotTextData.GetCategory() != ELocalizedTextSourceCategory::Game || !GIsEditor)
			{
				CultureName = FInternationalization::Get().GetCurrentLanguage()->GetName();;
			}
#if WITH_EDITOR
			else if (bIsGameLocalizationPreviewEnabled)
			{
				CultureName = GetConfiguredGameLocalizationPreviewLanguage();
			}
#endif

			if (!CultureName.IsEmpty())
			{
				const TArray<FString> PrioritizedCultureNames = FInternationalization::Get().GetPrioritizedCultureNames(CultureName);
				for (const FString& PrioritizedCultureName : PrioritizedCultureNames)
				{
					FString LocalizedString;
					if (InPolyglotTextData.GetLocalizedString(PrioritizedCultureName, LocalizedString))
					{
						return LocalizedString;
					}
				}
			}

			return InPolyglotTextData.GetNativeString();
		};

		TSharedPtr<FTextLocalizationResource> TextLocalizationResource = MakeShared<FTextLocalizationResource>();
		TextLocalizationResource->AddEntry(
			InPolyglotTextData.GetNamespace(),
			InPolyglotTextData.GetKey(),
			InPolyglotTextData.GetNativeString(),
			GetLocalizedStringForPolyglotData()
			);

		UpdateFromLocalizations(TArrayView<const TSharedPtr<FTextLocalizationResource>>(&TextLocalizationResource, 1));
	}
}

FTextDisplayStringPtr FTextLocalizationManager::FindDisplayString( const FString& Namespace, const FString& Key, const FString* const SourceString )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	const FDisplayStringLookupTable::FKeysTable* LiveKeyTable = nullptr;
	const FDisplayStringLookupTable::FDisplayStringEntry* LiveEntry = nullptr;

	DisplayStringLookupTable.Find(Namespace, LiveKeyTable, Key, LiveEntry);

	if ( LiveEntry != nullptr && ( !SourceString || LiveEntry->SourceStringHash == FTextLocalizationResource::HashString(*SourceString) ) )
	{
		return LiveEntry->DisplayString;
	}

	return nullptr;
}

FTextDisplayStringRef FTextLocalizationManager::GetDisplayString(const FString& Namespace, const FString& Key, const FString* const SourceString)
{
	FScopeLock ScopeLock( &SynchronizationObject );

	// Hack fix for old assets that don't have namespace/key info.
	if (Namespace.IsEmpty() && Key.IsEmpty())
	{
		return MakeShared<FString, ESPMode::ThreadSafe>(SourceString ? *SourceString : FString());
	}

#if ENABLE_LOC_TESTING
	const bool bShouldLEETIFYAll = bIsInitialized && FInternationalization::Get().GetCurrentLanguage()->GetName() == TEXT("LEET");

	// Attempt to set bShouldLEETIFYUnlocalizedString appropriately, only once, after the commandline is initialized and parsed.
	static bool bShouldLEETIFYUnlocalizedString = false;
	{
		static bool bHasParsedCommandLine = false;
		if (!bHasParsedCommandLine && FCommandLine::IsInitialized())
		{
			bShouldLEETIFYUnlocalizedString = FParse::Param(FCommandLine::Get(), TEXT("LEETIFYUnlocalized"));
			bHasParsedCommandLine = true;
		}
	}
#endif

	const uint32 SourceStringHash = SourceString ? FTextLocalizationResource::HashString(*SourceString) : 0;

	FDisplayStringLookupTable::FKeysTable* LiveKeyTable = nullptr;
	FDisplayStringLookupTable::FDisplayStringEntry* LiveEntry = nullptr;
	DisplayStringLookupTable.Find(Namespace, LiveKeyTable, Key, LiveEntry);

	// In builds with stable keys enabled, we want to use the display string from the "clean" version of the text (if the sources match) as this is the only version that is translated
	const FString* DisplayString = SourceString;
	FDisplayStringLookupTable::FDisplayStringEntry* DisplayLiveEntry = nullptr;
#if USE_STABLE_LOCALIZATION_KEYS
	if (GIsEditor)
	{
		const FString DisplayNamespace = TextNamespaceUtil::StripPackageNamespace(Namespace);

		FDisplayStringLookupTable::FKeysTable* DisplayLiveKeyTable = nullptr;
		DisplayStringLookupTable.Find(DisplayNamespace, DisplayLiveKeyTable, Key, DisplayLiveEntry);

		if (DisplayLiveEntry)
		{
			if (!SourceString || DisplayLiveEntry->SourceStringHash == SourceStringHash)
			{
				DisplayString = &DisplayLiveEntry->DisplayString.Get();
			}
			else
			{
				DisplayLiveEntry = nullptr;
			}
		}
	}
#endif // USE_STABLE_LOCALIZATION_KEYS

	// Entry is present.
	if (LiveEntry)
	{
		// If the source string (hash) is different, the local source has changed and should override - can't be localized.
		if (SourceStringHash != LiveEntry->SourceStringHash && DisplayString)
		{
			LiveEntry->SourceStringHash = SourceStringHash;
			*LiveEntry->DisplayString = *DisplayString;
			DirtyLocalRevisionForDisplayString(LiveEntry->DisplayString);

#if ENABLE_LOC_TESTING
			if (bShouldLEETIFYAll || bShouldLEETIFYUnlocalizedString)
			{
				FInternationalization::Leetify(*LiveEntry->DisplayString);
				if (LiveEntry->DisplayString->Equals(*DisplayString, ESearchCase::CaseSensitive))
				{
					UE_LOG(LogTextLocalizationManager, Warning, TEXT("Leetify failed to alter a string (%s)."), **DisplayString);
				}
			}
#endif

			UE_LOG(LogTextLocalizationManager, Verbose, TEXT("An attempt was made to get a localized string (Namespace:%s, Key:%s), but the source string hash does not match - the source string (%s) will be used."), *Namespace, *Key, **LiveEntry->DisplayString);

#if ENABLE_LOC_TESTING
			LiveEntry->bIsLocalized = bShouldLEETIFYAll;
#else
			LiveEntry->bIsLocalized = false;
#endif
		}

		return LiveEntry->DisplayString;
	}
	// Entry is absent, but has a related entry to clone.
	else if (DisplayLiveEntry)
	{
		check(!SourceString || DisplayLiveEntry->SourceStringHash == SourceStringHash);
		check(DisplayString && &DisplayLiveEntry->DisplayString.Get() == DisplayString);

		// Clone the entry for the active ID, and assign it a new display string instance (as all entries must have a unique display string instance).
		FDisplayStringLookupTable::FDisplayStringEntry NewEntry(*DisplayLiveEntry);
		NewEntry.DisplayString = MakeShared<FString, ESPMode::ThreadSafe>(*DisplayString);

		if (!LiveKeyTable)
		{
			LiveKeyTable = &(DisplayStringLookupTable.NamespacesTable.Add(Namespace, FDisplayStringLookupTable::FKeysTable()));
		}

		LiveKeyTable->Add(Key, NewEntry);

		NamespaceKeyLookupTable.Add(NewEntry.DisplayString, FNamespaceKeyEntry(Namespace, Key));

		return NewEntry.DisplayString;
	}
	// Entry is absent.
	else
	{
		// Don't log warnings about unlocalized strings if the system hasn't been initialized - we simply don't have localization data yet.
		if (bIsInitialized)
		{
			UE_LOG(LogTextLocalizationManager, Verbose, TEXT("An attempt was made to get a localized string (Namespace:%s, Key:%s, Source:%s), but it did not exist."), *Namespace, *Key, SourceString ? **SourceString : TEXT(""));
		}

		const FTextDisplayStringRef UnlocalizedString = MakeShared<FString, ESPMode::ThreadSafe>(DisplayString ? *DisplayString : FString());

#if ENABLE_LOC_TESTING
		if ((bShouldLEETIFYAll || bShouldLEETIFYUnlocalizedString) && DisplayString)
		{
			FInternationalization::Leetify(*UnlocalizedString);
			if (UnlocalizedString->Equals(*DisplayString, ESearchCase::CaseSensitive))
			{
				UE_LOG(LogTextLocalizationManager, Warning, TEXT("Leetify failed to alter a string (%s)."), **DisplayString);
			}
		}
#endif

		if (UnlocalizedString->IsEmpty())
		{
			if (!bIsInitialized)
			{
				*(UnlocalizedString) = GetAccessedStringBeforeLocLoadedErrorMsg();
			}
		}

		// Make entries so that they can be updated when system is initialized or a culture swap occurs.
		FDisplayStringLookupTable::FDisplayStringEntry NewEntry(
#if ENABLE_LOC_TESTING
			bShouldLEETIFYAll					/*bIsLocalized*/
#else
			false								/*bIsLocalized*/
#endif
			, FTextLocalizationResourceId()		/*LocResID*/
			, SourceStringHash					/*SourceStringHash*/
			, UnlocalizedString					/*String*/
		);

		if (!LiveKeyTable)
		{
			LiveKeyTable = &(DisplayStringLookupTable.NamespacesTable.Add(Namespace, FDisplayStringLookupTable::FKeysTable()));
		}

		LiveKeyTable->Add(Key, NewEntry);

		NamespaceKeyLookupTable.Add(NewEntry.DisplayString, FNamespaceKeyEntry(Namespace, Key));

		return UnlocalizedString;
	}
}

bool FTextLocalizationManager::GetLocResID(const FString& Namespace, const FString& Key, FString& OutLocResId)
{
	FScopeLock ScopeLock(&SynchronizationObject);

	const FDisplayStringLookupTable::FKeysTable* LiveKeyTable = nullptr;
	const FDisplayStringLookupTable::FDisplayStringEntry* LiveEntry = nullptr;
	DisplayStringLookupTable.Find(Namespace, LiveKeyTable, Key, LiveEntry);

	if (LiveEntry != nullptr && !LiveEntry->LocResID.IsEmpty())
	{
		OutLocResId = LiveEntry->LocResID.GetString();
		return true;
	}

	return false;
}

bool FTextLocalizationManager::FindNamespaceAndKeyFromDisplayString(const FTextDisplayStringRef& InDisplayString, FString& OutNamespace, FString& OutKey)
{
	FScopeLock ScopeLock( &SynchronizationObject );

	FNamespaceKeyEntry* NamespaceKeyEntry = NamespaceKeyLookupTable.Find(InDisplayString);

	if (NamespaceKeyEntry)
	{
		OutNamespace = NamespaceKeyEntry->Namespace;
		OutKey = NamespaceKeyEntry->Key;
	}

	return NamespaceKeyEntry != nullptr;
}

uint16 FTextLocalizationManager::GetLocalRevisionForDisplayString(const FTextDisplayStringRef& InDisplayString)
{
	FScopeLock ScopeLock( &SynchronizationObject );

	uint16* FoundLocalRevision = LocalTextRevisions.Find(InDisplayString);
	return (FoundLocalRevision) ? *FoundLocalRevision : 0;
}

bool FTextLocalizationManager::AddDisplayString(const FTextDisplayStringRef& DisplayString, const FString& Namespace, const FString& Key)
{
	FScopeLock ScopeLock( &SynchronizationObject );

	// Try to find existing entries.
	FNamespaceKeyEntry* ReverseLiveTableEntry = NamespaceKeyLookupTable.Find(DisplayString);
	FDisplayStringLookupTable::FKeysTable* KeysTableForExistingNamespace = nullptr;
	FDisplayStringLookupTable::FDisplayStringEntry* ExistingDisplayStringEntry = nullptr;
	DisplayStringLookupTable.Find(Namespace, KeysTableForExistingNamespace, Key, ExistingDisplayStringEntry);

	// If there are any existing entries, they may cause a conflict, unless they're exactly the same as what we would be adding.
	if ( (ExistingDisplayStringEntry && ExistingDisplayStringEntry->DisplayString != DisplayString) || // Namespace and key mustn't be associated with a different display string.
		(ReverseLiveTableEntry && (ReverseLiveTableEntry->Namespace != Namespace || ReverseLiveTableEntry->Key != Key)) ) // Display string mustn't be associated with a different namespace and key.
	{
		return false;
	}

	// Add the necessary associations in both directions.
	FDisplayStringLookupTable::FKeysTable& KeysTableForNamespace = DisplayStringLookupTable.NamespacesTable.FindOrAdd(Namespace);
	KeysTableForNamespace.Add(Key, FDisplayStringLookupTable::FDisplayStringEntry(false, FTextLocalizationResourceId(), FTextLocalizationResource::HashString(*DisplayString), DisplayString));
	NamespaceKeyLookupTable.Add(DisplayString, FNamespaceKeyEntry(Namespace, Key));

	return true;
}

bool FTextLocalizationManager::UpdateDisplayString(const FTextDisplayStringRef& DisplayString, const FString& Value, const FString& Namespace, const FString& Key)
{
	FScopeLock ScopeLock( &SynchronizationObject );

	// Get entry from reverse live table. Contains current namespace and key values.
	FNamespaceKeyEntry& ReverseLiveTableEntry = NamespaceKeyLookupTable[DisplayString];

	// Copy old live table entry over as new live table entry and destroy old live table entry if the namespace or key has changed.
	if (ReverseLiveTableEntry.Namespace != Namespace || ReverseLiveTableEntry.Key != Key)
	{
		FDisplayStringLookupTable::FKeysTable& KeysTableForNewNamespace = DisplayStringLookupTable.NamespacesTable.FindOrAdd(Namespace);
		FDisplayStringLookupTable::FDisplayStringEntry* NewDisplayStringEntry = KeysTableForNewNamespace.Find(Key);
		if (NewDisplayStringEntry)
		{
			// Can not update, that namespace and key combination is already in use by another string.
			return false;
		}
		else
		{
			// Get old namespace's keys table and old live table entry under old key.
			FDisplayStringLookupTable::FKeysTable* KeysTableForOldNamespace = nullptr;
			FDisplayStringLookupTable::FDisplayStringEntry* OldDisplayStringEntry = nullptr;
			DisplayStringLookupTable.Find(ReverseLiveTableEntry.Namespace, KeysTableForOldNamespace, ReverseLiveTableEntry.Key, OldDisplayStringEntry);

			// Copy old live table entry to new key in the new namespace's key table.
			check(OldDisplayStringEntry);
			KeysTableForNewNamespace.Add(Key, *OldDisplayStringEntry);

			// Remove old live table entry and old key in the old namespace's key table.
			check(KeysTableForOldNamespace);
			KeysTableForOldNamespace->Remove(ReverseLiveTableEntry.Key);

			// Remove old namespace if empty.
			if(DisplayStringLookupTable.NamespacesTable[ReverseLiveTableEntry.Namespace].Num() == 0)
			{
				DisplayStringLookupTable.NamespacesTable.Remove(ReverseLiveTableEntry.Namespace);
			}
		}
	}

	// Update display string value.
	*DisplayString = Value;
	DirtyLocalRevisionForDisplayString(DisplayString);

	// Update entry from reverse live table.
	ReverseLiveTableEntry.Namespace = Namespace;
	ReverseLiveTableEntry.Key = Key;

	return true;
}

void FTextLocalizationManager::UpdateFromLocalizationResource(const FString& LocalizationResourceFilePath)
{
	TSharedPtr<FTextLocalizationResource> TextLocalizationResource = MakeShared<FTextLocalizationResource>();
	TextLocalizationResource->LoadFromFile(LocalizationResourceFilePath);
#if !UE_BUILD_SHIPPING
	TextLocalizationResource->DetectAndLogConflicts();
#endif

	UpdateFromLocalizations(TArrayView<const TSharedPtr<FTextLocalizationResource>>(&TextLocalizationResource, 1));
}

void FTextLocalizationManager::UpdateFromLocalizationResources(TArrayView<const TSharedPtr<FTextLocalizationResource>> TextLocalizationResources)
{
	UpdateFromLocalizations(TextLocalizationResources);
}

void FTextLocalizationManager::RefreshResources()
{
	ELocalizationLoadFlags LocLoadFlags = ELocalizationLoadFlags::None;
	LocLoadFlags |= (WITH_EDITOR ? ELocalizationLoadFlags::Editor : ELocalizationLoadFlags::None);
	LocLoadFlags |= (FApp::IsGame() ? ELocalizationLoadFlags::Game : ELocalizationLoadFlags::None);
	LocLoadFlags |= ELocalizationLoadFlags::Engine;
	LocLoadFlags |= ELocalizationLoadFlags::Native;
	LocLoadFlags |= ELocalizationLoadFlags::Additional;

	LoadLocalizationResourcesForCulture(FInternationalization::Get().GetCurrentLanguage()->GetName(), LocLoadFlags);
}

void FTextLocalizationManager::OnCultureChanged()
{
    if (!bIsInitialized)
	{
		// Ignore culture changes while the text localization manager is still being initialized
		// The correct data will be loaded by EndInitTextLocalization
		return;
	}

	ELocalizationLoadFlags LocLoadFlags = ELocalizationLoadFlags::None;
	LocLoadFlags |= (WITH_EDITOR ? ELocalizationLoadFlags::Editor : ELocalizationLoadFlags::None);
	LocLoadFlags |= (FApp::IsGame() ? ELocalizationLoadFlags::Game : ELocalizationLoadFlags::None);
	LocLoadFlags |= ELocalizationLoadFlags::Engine;
	LocLoadFlags |= ELocalizationLoadFlags::Native;
	LocLoadFlags |= ELocalizationLoadFlags::Additional;

	LoadLocalizationResourcesForCulture(FInternationalization::Get().GetCurrentLanguage()->GetName(), LocLoadFlags);
}

void FTextLocalizationManager::LoadLocalizationResourcesForCulture(const FString& CultureName, const ELocalizationLoadFlags LocLoadFlags)
{
    LLM_SCOPE(ELLMTag::Localization);

	// Can't load localization resources for a culture that doesn't exist, early-out.
	const FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureName);
	if (!Culture.IsValid())
	{
		return;
	}

	LoadLocalizationResourcesForPrioritizedCultures(FInternationalization::Get().GetPrioritizedCultureNames(CultureName), LocLoadFlags);
}

void FTextLocalizationManager::LoadLocalizationResourcesForPrioritizedCultures(TArrayView<const FString> PrioritizedCultureNames, const ELocalizationLoadFlags LocLoadFlags)
{
	LLM_SCOPE(ELLMTag::Localization);

	// Nothing to do?
	if (PrioritizedCultureNames.Num() == 0)
	{
		return;
	}

	// Load the resources from each text source
	FTextLocalizationResource NativeResource;
	FTextLocalizationResources LocalizedResources;
	for (const TSharedPtr<ILocalizedTextSource>& LocalizedTextSource : LocalizedTextSources)
	{
		LocalizedTextSource->LoadLocalizedResources(LocLoadFlags, PrioritizedCultureNames, NativeResource, LocalizedResources);
	}

	// When loc testing is enabled, UpdateFromNative also takes care of restoring non-localized text which is why the condition below is gated
#if !ENABLE_LOC_TESTING
	if (!NativeResource.IsEmpty())
#endif
	{
#if !UE_BUILD_SHIPPING
		NativeResource.DetectAndLogConflicts();
#endif
		UpdateFromNative(NativeResource);
	}

#if ENABLE_LOC_TESTING
	// The leet culture is fake. Just leet-ify existing strings.
	if (PrioritizedCultureNames[0] == TEXT("LEET"))
	{
		// Lock while updating the tables
		{
			FScopeLock ScopeLock(&SynchronizationObject);

			for (auto NamespaceIterator = DisplayStringLookupTable.NamespacesTable.CreateIterator(); NamespaceIterator; ++NamespaceIterator)
			{
				const FString& Namespace = NamespaceIterator.Key();
				FDisplayStringLookupTable::FKeysTable& LiveKeyTable = NamespaceIterator.Value();
				for (auto KeyIterator = LiveKeyTable.CreateIterator(); KeyIterator; ++KeyIterator)
				{
					const FString& Key = KeyIterator.Key();
					FDisplayStringLookupTable::FDisplayStringEntry& LiveStringEntry = KeyIterator.Value();
					LiveStringEntry.bIsLocalized = true;
					LiveStringEntry.NativeStringBackup = *LiveStringEntry.DisplayString;
					FInternationalization::Leetify(*LiveStringEntry.DisplayString);
				}
			}
		}

		// Early-out, there can be no localization resources to load for the fake leet culture.
		DirtyTextRevision();
	}
	else
#endif
	{
		// Apply any localized resources in the prioritized culture order
		TArray<TSharedPtr<FTextLocalizationResource>, TInlineAllocator<4>> LocalizedResourcesToApply;
		for (const FString& PrioritizedCultureName : PrioritizedCultureNames)
		{
			TSharedPtr<FTextLocalizationResource> LocalizedResource = LocalizedResources.FindResource(PrioritizedCultureName);
			if (LocalizedResource.IsValid() && !LocalizedResource->IsEmpty())
			{
#if !UE_BUILD_SHIPPING
				LocalizedResource->DetectAndLogConflicts();
#endif
				LocalizedResourcesToApply.Add(LocalizedResource);
			}
		}

		if (LocalizedResourcesToApply.Num() > 0)
		{
			// Replace localizations with those of the loaded localization resources.
			UpdateFromLocalizations(LocalizedResourcesToApply);
		}
	}
}

void FTextLocalizationManager::UpdateFromNative(const FTextLocalizationResource& TextLocalizationResource)
{
	// Lock while updating the tables
	{
		FScopeLock ScopeLock(&SynchronizationObject);

		// Note: This code doesn't handle "leet-ification" itself as it is resetting everything to a known "good" state ("leet-ification" happens later on the "good" native text)

		// Update existing entries to use the new native text
		for (auto& Namespace : DisplayStringLookupTable.NamespacesTable)
		{
			const FString& NamespaceName = Namespace.Key;
			FDisplayStringLookupTable::FKeysTable& LiveKeyTable = Namespace.Value;

			// In builds with stable keys enabled, we want to use the display string from the "clean" version of the text (if the sources match) as this is the only version that is translated
			const FString* NamespaceNamePtr = &NamespaceName;
#if USE_STABLE_LOCALIZATION_KEYS
			FString DisplayNamespace;
			if (GIsEditor)
			{
				DisplayNamespace = TextNamespaceUtil::StripPackageNamespace(NamespaceName);
				NamespaceNamePtr = &DisplayNamespace;
			}
#endif // USE_STABLE_LOCALIZATION_KEYS

			for (auto& Key : LiveKeyTable)
			{
				const FString& KeyName = Key.Key;
				FDisplayStringLookupTable::FDisplayStringEntry& LiveStringEntry = Key.Value;

				const FTextLocalizationResource::FEntry* SourceEntryForUpdate = nullptr;

				// Attempt to use resources in prioritized order until we find an entry.
				{
					const FTextLocalizationResource::FKeysTable* const UpdateKeyTable = TextLocalizationResource.Namespaces.Find(*NamespaceNamePtr);
					const FTextLocalizationResource::FEntryArray* const UpdateEntryArray = UpdateKeyTable ? UpdateKeyTable->Find(KeyName) : nullptr;
					const FTextLocalizationResource::FEntry* Entry = UpdateEntryArray && UpdateEntryArray->Num() ? &((*UpdateEntryArray)[0]) : nullptr;
					if (Entry)
					{
						SourceEntryForUpdate = Entry;
					}
				}

				// Update the display string with the new native string
				if (SourceEntryForUpdate && LiveStringEntry.SourceStringHash == SourceEntryForUpdate->SourceStringHash)
				{
					LiveStringEntry.bIsLocalized = true;
					LiveStringEntry.LocResID = SourceEntryForUpdate->LocResID;
					*LiveStringEntry.DisplayString = SourceEntryForUpdate->LocalizedString;
				}
				else
				{
					if (!LiveStringEntry.bIsLocalized && LiveStringEntry.DisplayString->Equals(GetAccessedStringBeforeLocLoadedErrorMsg(), ESearchCase::CaseSensitive))
					{
						*LiveStringEntry.DisplayString = FString();
					}

#if ENABLE_LOC_TESTING
					// Restore the pre-leet state (if any)
					if (!LiveStringEntry.NativeStringBackup.IsEmpty())
					{
						LiveStringEntry.bIsLocalized = false;
						LiveStringEntry.LocResID = FTextLocalizationResourceId();
						*LiveStringEntry.DisplayString = MoveTemp(LiveStringEntry.NativeStringBackup);
					}
#endif
				}

#if ENABLE_LOC_TESTING
				LiveStringEntry.NativeStringBackup.Reset();
#endif
			}
		}

		// Add new entries
		{
			for (const auto& Namespace : TextLocalizationResource.Namespaces)
			{
				const FString& NamespaceName = Namespace.Key;
				const FTextLocalizationResource::FKeysTable& NewKeyTable = Namespace.Value;
				FDisplayStringLookupTable::FKeysTable& LiveKeyTable = DisplayStringLookupTable.NamespacesTable.FindOrAdd(NamespaceName);
				for (const auto& Key : NewKeyTable)
				{
					const FString& KeyName = Key.Key;
					const FTextLocalizationResource::FEntryArray& NewEntryArray = Key.Value;
					const FTextLocalizationResource::FEntry& NewEntry = NewEntryArray[0];

					FDisplayStringLookupTable::FDisplayStringEntry* const LiveStringEntry = LiveKeyTable.Find(KeyName);
					// Note: Anything we find in the table has already been updated above.
					if (!LiveStringEntry)
					{
						FDisplayStringLookupTable::FDisplayStringEntry NewLiveEntry(
							true,																/*bIsLocalized*/
							NewEntry.LocResID,													/*LocResID*/
							NewEntry.SourceStringHash,											/*SourceStringHash*/
							MakeShared<FString, ESPMode::ThreadSafe>(NewEntry.LocalizedString)	/*String*/
						);
						LiveKeyTable.Add(KeyName, NewLiveEntry);

						NamespaceKeyLookupTable.Add(NewLiveEntry.DisplayString, FNamespaceKeyEntry(NamespaceName, KeyName));
					}
				}
			}
		}
	}

	DirtyTextRevision();
}

void FTextLocalizationManager::UpdateFromLocalizations(TArrayView<const TSharedPtr<FTextLocalizationResource>> TextLocalizationResources)
{
	// Lock while updating the tables
	{
		FScopeLock ScopeLock(&SynchronizationObject);

		// Update existing localized entries/flag existing newly-unlocalized entries.
		for (auto& Namespace : DisplayStringLookupTable.NamespacesTable)
		{
			const FString& NamespaceName = Namespace.Key;
			FDisplayStringLookupTable::FKeysTable& LiveKeyTable = Namespace.Value;

			// In builds with stable keys enabled, we want to use the display string from the "clean" version of the text (if the sources match) as this is the only version that is translated
			const FString* NamespaceNamePtr = &NamespaceName;
#if USE_STABLE_LOCALIZATION_KEYS
			FString DisplayNamespace;
			if (GIsEditor)
			{
				DisplayNamespace = TextNamespaceUtil::StripPackageNamespace(NamespaceName);
				NamespaceNamePtr = &DisplayNamespace;
			}
#endif // USE_STABLE_LOCALIZATION_KEYS

			for (auto& Key : LiveKeyTable)
			{
				const FString& KeyName = Key.Key;
				FDisplayStringLookupTable::FDisplayStringEntry& LiveStringEntry = Key.Value;

				const FTextLocalizationResource::FEntry* SourceEntryForUpdate = nullptr;

				// Attempt to use resources in prioritized order until we find an entry.
				for (const TSharedPtr<FTextLocalizationResource>& TextLocalizationResource : TextLocalizationResources)
				{
					check(TextLocalizationResource.IsValid());

					const FTextLocalizationResource::FKeysTable* const UpdateKeyTable = TextLocalizationResource->Namespaces.Find(*NamespaceNamePtr);
					const FTextLocalizationResource::FEntryArray* const UpdateEntryArray = UpdateKeyTable ? UpdateKeyTable->Find(KeyName) : nullptr;
					const FTextLocalizationResource::FEntry* Entry = UpdateEntryArray && UpdateEntryArray->Num() ? &((*UpdateEntryArray)[0]) : nullptr;
					if (Entry)
					{
						SourceEntryForUpdate = Entry;
						break;
					}
				}

				// If the source string hashes are are the same, we can replace the display string.
				// Otherwise, it would suggest the source string has changed and the new localization may be based off of an old source string.
				if (SourceEntryForUpdate && LiveStringEntry.SourceStringHash == SourceEntryForUpdate->SourceStringHash)
				{
					LiveStringEntry.bIsLocalized = true;
					LiveStringEntry.LocResID = SourceEntryForUpdate->LocResID;
					*(LiveStringEntry.DisplayString) = SourceEntryForUpdate->LocalizedString;
				}
				else
				{
					if (!LiveStringEntry.bIsLocalized && LiveStringEntry.DisplayString->Equals(GetAccessedStringBeforeLocLoadedErrorMsg(), ESearchCase::CaseSensitive))
					{
						*(LiveStringEntry.DisplayString) = FString();
					}

#if ENABLE_LOC_TESTING
					static const bool bShouldLEETIFYUnlocalizedString = FParse::Param(FCommandLine::Get(), TEXT("LEETIFYUnlocalized"));
					if (bShouldLEETIFYUnlocalizedString)
					{
						LiveStringEntry.bIsLocalized = false;
						LiveStringEntry.LocResID = FTextLocalizationResourceId();
						FInternationalization::Leetify(*LiveStringEntry.DisplayString);
					}
#endif
				}
			}
		}

		// Add new entries. 
		for (const TSharedPtr<FTextLocalizationResource>& TextLocalizationResource : TextLocalizationResources)
		{
			check(TextLocalizationResource.IsValid());

			for (const auto& Namespace : TextLocalizationResource->Namespaces)
			{
				const FString& NamespaceName = Namespace.Key;
				const FTextLocalizationResource::FKeysTable& NewKeyTable = Namespace.Value;
				FDisplayStringLookupTable::FKeysTable& LiveKeyTable = DisplayStringLookupTable.NamespacesTable.FindOrAdd(NamespaceName);
				for (const auto& Key : NewKeyTable)
				{
					const FString& KeyName = Key.Key;
					const FTextLocalizationResource::FEntryArray& NewEntryArray = Key.Value;
					const FTextLocalizationResource::FEntry& NewEntry = NewEntryArray[0];

					FDisplayStringLookupTable::FDisplayStringEntry* const LiveStringEntry = LiveKeyTable.Find(KeyName);
					// Note: Anything we find in the table has already been updated above.
					if (!LiveStringEntry)
					{
						FDisplayStringLookupTable::FDisplayStringEntry NewLiveEntry(
							true,																/*bIsLocalized*/
							NewEntry.LocResID,													/*LocResID*/
							NewEntry.SourceStringHash,											/*SourceStringHash*/
							MakeShared<FString, ESPMode::ThreadSafe>(NewEntry.LocalizedString)	/*String*/
						);
						LiveKeyTable.Add(KeyName, NewLiveEntry);

						NamespaceKeyLookupTable.Add(NewLiveEntry.DisplayString, FNamespaceKeyEntry(NamespaceName, KeyName));
					}
				}
			}
		}
	}

	DirtyTextRevision();
}

void FTextLocalizationManager::DirtyLocalRevisionForDisplayString(const FTextDisplayStringRef& InDisplayString)
{
	FScopeLock ScopeLock(&SynchronizationObject);

	uint16* FoundLocalRevision = LocalTextRevisions.Find(InDisplayString);
	if (FoundLocalRevision)
	{
		while (++(*FoundLocalRevision) == 0) {} // Zero is special, don't allow an overflow to stay at zero
	}
	else
	{
		LocalTextRevisions.Add(InDisplayString, 1);
	}
}

void FTextLocalizationManager::DirtyTextRevision()
{
	// Lock while updating the data
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		while (++TextRevisionCounter == 0) {} // Zero is special, don't allow an overflow to stay at zero
		LocalTextRevisions.Empty();
	}

	OnTextRevisionChangedEvent.Broadcast();
}

#if WITH_EDITOR
void FTextLocalizationManager::EnableGameLocalizationPreview()
{
	EnableGameLocalizationPreview(GetConfiguredGameLocalizationPreviewLanguage());
}

void FTextLocalizationManager::EnableGameLocalizationPreview(const FString& CultureName)
{
	// This only works in the editor
	if (!GIsEditor)
	{
		return;
	}

	// We need the native game culture to be available for this preview to work correctly
	const FString NativeGameCulture = GetNativeCultureName(ELocalizedTextSourceCategory::Game);
	if (NativeGameCulture.IsEmpty())
	{
		return;
	}

	const FString PreviewCulture = CultureName.IsEmpty() ? NativeGameCulture : CultureName;
	bIsGameLocalizationPreviewEnabled = PreviewCulture != NativeGameCulture;
	bIsLocalizationLocked = IsLocalizationLockedByConfig() || bIsGameLocalizationPreviewEnabled;

	TArray<FString> PrioritizedCultureNames;
	if (bIsGameLocalizationPreviewEnabled)
	{
		PrioritizedCultureNames = FInternationalization::Get().GetPrioritizedCultureNames(PreviewCulture);
	}
	else
	{
		PrioritizedCultureNames.Add(PreviewCulture);
	}

	ELocalizationLoadFlags LocLoadFlags = ELocalizationLoadFlags::Game | ELocalizationLoadFlags::ForceLocalizedGame;
	LocLoadFlags |= (bIsGameLocalizationPreviewEnabled ? ELocalizationLoadFlags::Native : ELocalizationLoadFlags::None);

	LoadLocalizationResourcesForPrioritizedCultures(PrioritizedCultureNames, LocLoadFlags);
}

void FTextLocalizationManager::DisableGameLocalizationPreview()
{
	EnableGameLocalizationPreview(GetNativeCultureName(ELocalizedTextSourceCategory::Game));
}

bool FTextLocalizationManager::IsGameLocalizationPreviewEnabled() const
{
	return bIsGameLocalizationPreviewEnabled;
}

void FTextLocalizationManager::PushAutoEnableGameLocalizationPreview()
{
	++GameLocalizationPreviewAutoEnableCount;
}

void FTextLocalizationManager::PopAutoEnableGameLocalizationPreview()
{
	checkf(GameLocalizationPreviewAutoEnableCount > 0, TEXT("Call to PopAutoEnableGameLocalizationPreview missing corresponding call to PushAutoEnableGameLocalizationPreview!"));
	--GameLocalizationPreviewAutoEnableCount;
}

bool FTextLocalizationManager::ShouldGameLocalizationPreviewAutoEnable() const
{
	return GameLocalizationPreviewAutoEnableCount > 0;
}

void FTextLocalizationManager::ConfigureGameLocalizationPreviewLanguage(const FString& CultureName)
{
	GConfig->SetString(TEXT("Internationalization"), TEXT("PreviewGameLanguage"), *CultureName, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

FString FTextLocalizationManager::GetConfiguredGameLocalizationPreviewLanguage() const
{
	return GConfig->GetStr(TEXT("Internationalization"), TEXT("PreviewGameLanguage"), GEditorPerProjectIni);
}

bool FTextLocalizationManager::IsLocalizationLocked() const
{
	return bIsLocalizationLocked;
}
#endif
