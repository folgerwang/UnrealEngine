// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Misc/Crc.h"
#include "Containers/UnrealString.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Delegates/Delegate.h"
#include "Internationalization/TextKey.h"
#include "Internationalization/LocTesting.h"
#include "Internationalization/LocKeyFuncs.h"
#include "Internationalization/LocalizedTextSourceTypes.h"

struct FPolyglotTextData;
class ILocalizedTextSource;
class FTextLocalizationResource;

typedef TSharedRef<FString, ESPMode::ThreadSafe> FTextDisplayStringRef;
typedef TSharedPtr<FString, ESPMode::ThreadSafe> FTextDisplayStringPtr;

/** Singleton class that manages display strings for FText. */
class CORE_API FTextLocalizationManager
{
	friend CORE_API void BeginInitTextLocalization();
	friend CORE_API void InitEngineTextLocalization();
	friend CORE_API void InitGameTextLocalization();

private:

	/** Data struct for tracking a display string. */
	struct FDisplayStringEntry
	{
		FTextDisplayStringRef DisplayString;
#if WITH_EDITORONLY_DATA
		FTextKey LocResID;
#endif
#if ENABLE_LOC_TESTING
		FString NativeStringBackup;
#endif
		uint32 SourceStringHash;
		bool bIsLocalized;

		FDisplayStringEntry(const bool InIsLocalized, const FTextKey& InLocResID, const uint32 InSourceStringHash, const FTextDisplayStringRef& InDisplayString)
			: DisplayString(InDisplayString)
#if WITH_EDITORONLY_DATA
			, LocResID(InLocResID)
#endif
			, SourceStringHash(InSourceStringHash)
			, bIsLocalized(InIsLocalized)
		{
		}
	};

	/** Manages the currently loaded or registered text localizations. */
	typedef TMap<FTextId, FDisplayStringEntry> FDisplayStringLookupTable;

	/** Manages the identity associated with a display string, for use in looking up namespace and key from a display string. */
	typedef TMap<FTextDisplayStringRef, FTextId> FNamespaceKeyLookupTable;

private:
	bool bIsInitialized;

	FCriticalSection SynchronizationObject;
	FDisplayStringLookupTable DisplayStringLookupTable;
	FNamespaceKeyLookupTable NamespaceKeyLookupTable;
	TMap<FTextDisplayStringRef, uint16> LocalTextRevisions;
	uint16 TextRevisionCounter;

#if WITH_EDITOR
	uint8 GameLocalizationPreviewAutoEnableCount;
	bool bIsGameLocalizationPreviewEnabled;
	bool bIsLocalizationLocked;
#endif

private:
	FTextLocalizationManager();

public:

	/** Singleton accessor */
	static FTextLocalizationManager& Get();

	void DumpMemoryInfo();
	void CompactDataStructures();

	/**
	 * Given a localization category, get the native culture for the category (if known).
	 * @return The native culture for the given localization category, or an empty string if the native culture is unknown.
	 */
	FString GetNativeCultureName(const ELocalizedTextSourceCategory InCategory) const;

	/**
	 * Get a list of culture names that we have localized resource data for (ELocalizationLoadFlags controls which resources should be checked).
	 */
	TArray<FString> GetLocalizedCultureNames(const ELocalizationLoadFlags InLoadFlags) const;

	/**
	 * Register a localized text source with the text localization manager.
	 */
	void RegisterTextSource(const TSharedRef<ILocalizedTextSource>& InLocalizedTextSource, const bool InRefreshResources = true);

	/**
	 * Register a polyglot text data with the text localization manager.
	 */
	void RegisterPolyglotTextData(const FPolyglotTextData& InPolyglotTextData, const bool InAddDisplayString = true);
	void RegisterPolyglotTextData(TArrayView<const FPolyglotTextData> InPolyglotTextDataArray, const bool InAddDisplayStrings = true);

	/**	Finds and returns the display string with the given namespace and key, if it exists.
	 *	Additionally, if a source string is specified and the found localized display string was not localized from that source string, null will be returned. */
	FTextDisplayStringPtr FindDisplayString(const FTextKey& Namespace, const FTextKey& Key, const FString* const SourceString = nullptr);

	/**	Returns a display string with the given namespace and key.
	 *	If no display string exists, it will be created using the source string or an empty string if no source string is provided.
	 *	If a display string exists ...
	 *		... but it was not localized from the specified source string, the display string will be set to the specified source and returned.
	 *		... and it was localized from the specified source string (or none was provided), the display string will be returned.
	*/
	FTextDisplayStringRef GetDisplayString(const FTextKey& Namespace, const FTextKey& Key, const FString* const SourceString);

#if WITH_EDITORONLY_DATA
	/** If an entry exists for the specified namespace and key, returns true and provides the localization resource identifier from which it was loaded. Otherwise, returns false. */
	bool GetLocResID(const FTextKey& Namespace, const FTextKey& Key, FString& OutLocResId);
#endif
	/**	Finds the namespace and key associated with the specified display string.
	 *	Returns true if found and sets the out parameters. Otherwise, returns false.
	 */
	bool FindNamespaceAndKeyFromDisplayString(const FTextDisplayStringRef& InDisplayString, FString& OutNamespace, FString& OutKey);
	
	/**
	 * Attempts to find a local revision history for the given display string.
	 * This will only be set if the display string has been changed since the localization manager version has been changed (eg, if it has been edited while keeping the same key).
	 * @return The local revision, or 0 if there have been no changes since a global history change.
	 */
	uint16 GetLocalRevisionForDisplayString(const FTextDisplayStringRef& InDisplayString);

	/**	Attempts to register the specified display string, associating it with the specified namespace and key.
	 *	Returns true if the display string has been or was already associated with the namespace and key.
	 *	Returns false if the display string was already associated with another namespace and key or the namespace and key are already in use by another display string.
	 */
	bool AddDisplayString(const FTextDisplayStringRef& DisplayString, const FTextKey& Namespace, const FTextKey& Key);

	/**
	 * Updates the underlying value of a display string and associates it with a specified namespace and key, then returns true.
	 * If the namespace and key are already in use by another display string, no changes occur and false is returned.
	 */
	bool UpdateDisplayString(const FTextDisplayStringRef& DisplayString, const FString& Value, const FTextKey& Namespace, const FTextKey& Key);

	/** Updates display string entries and adds new display string entries based on localizations found in a specified localization resource. */
	void UpdateFromLocalizationResource(const FString& LocalizationResourceFilePath);
	void UpdateFromLocalizationResource(const FTextLocalizationResource& TextLocalizationResource);

	/** Reloads resources for the current culture. */
	void RefreshResources();

	/**	Returns the current text revision number. This value can be cached when caching information from the text localization manager.
	 *	If the revision does not match, cached information may be invalid and should be recached. */
	uint16 GetTextRevision() const { return TextRevisionCounter; }

#if WITH_EDITOR
	/**
	 * Enable the game localization preview using the current "preview language" setting, or the native culture if no "preview language" is set.
	 * @note This is the same as calling EnableGameLocalizationPreview with the current "preview language" setting.
	 */
	void EnableGameLocalizationPreview();

	/**
	 * Enable the game localization preview using the given language, or the native language if the culture name is empty.
	 * @note This will also lockdown localization editing if the given language is a non-native game language (to avoid accidentally baking out translations as source data in assets).
	 */
	void EnableGameLocalizationPreview(const FString& CultureName);

	/**
	 * Disable the game localization preview.
	 * @note This is the same as calling EnableGameLocalizationPreview with the native game language (or an empty string).
	 */
	void DisableGameLocalizationPreview();

	/**
	 * Is the game localization preview enabled for a non-native language?
	 */
	bool IsGameLocalizationPreviewEnabled() const;

	/**
	 * Notify that the game localization preview should automatically enable itself under certain circumstances 
	 * (such as changing the preview language via the UI) due to a state change (such as PIE starting).
	 * @note This must be paired with a call to PopAutoEnableGameLocalizationPreview.
	 */
	void PushAutoEnableGameLocalizationPreview();

	/**
	 * Notify that the game localization preview should no longer automatically enable itself under certain circumstances 
	 * (such as changing the preview language via the UI) due to a state change (such as PIE ending).
	 * @note This must be paired with a call to PushAutoEnableGameLocalizationPreview.
	 */
	void PopAutoEnableGameLocalizationPreview();

	/**
	 * Should the game localization preview automatically enable itself under certain circumstances?
	 */
	bool ShouldGameLocalizationPreviewAutoEnable() const;

	/**
	 * Configure the "preview language" setting used for the game localization preview.
	 */
	void ConfigureGameLocalizationPreviewLanguage(const FString& CultureName);

	/**
	 * Get the configured "preview language" setting used for the game localization preview (if any).
	 */
	FString GetConfiguredGameLocalizationPreviewLanguage() const;

	/**
	 * Is the localization of this game currently locked? (ie, can it be edited in the UI?).
	 */
	bool IsLocalizationLocked() const;
#endif

	/** Event type for immediately reacting to changes in display strings for text. */
	DECLARE_EVENT(FTextLocalizationManager, FTextRevisionChangedEvent)
	FTextRevisionChangedEvent OnTextRevisionChangedEvent;

private:
	/** Callback for changes in culture. Loads the new culture's localization resources. */
	void OnCultureChanged();

	/** Loads localization resources for the specified culture, optionally loading localization resources that are editor-specific or game-specific. */
	void LoadLocalizationResourcesForCulture(const FString& CultureName, const ELocalizationLoadFlags LocLoadFlags);

	/** Loads localization resources for the specified prioritized cultures, optionally loading localization resources that are editor-specific or game-specific. */
	void LoadLocalizationResourcesForPrioritizedCultures(TArrayView<const FString> PrioritizedCultureNames, const ELocalizationLoadFlags LocLoadFlags);

	/** Updates display string entries and adds new display string entries based on provided native text. */
	void UpdateFromNative(FTextLocalizationResource&& TextLocalizationResource, const bool bDirtyTextRevision = true);

	/** Updates display string entries and adds new display string entries based on provided localizations. */
	void UpdateFromLocalizations(FTextLocalizationResource&& TextLocalizationResource, const bool bDirtyTextRevision = true);

	/** Dirties the local revision counter for the given display string by incrementing it (or adding it) */
	void DirtyLocalRevisionForDisplayString(const FTextDisplayStringRef& InDisplayString);

	/** Dirties the text revision counter by incrementing it, causing a revision mismatch for any information cached before this happens.  */
	void DirtyTextRevision();

	/** Array of registered localized text sources, sorted by priority (@see RegisterTextSource) */
	TArray<TSharedPtr<ILocalizedTextSource>> LocalizedTextSources;

	/** The polyglot text source (this is also added to LocalizedTextSources, but we keep a pointer to it directly so we can add new polyglot data to it at runtime) */
	TSharedPtr<class FPolyglotTextSource> PolyglotTextSource;
};
