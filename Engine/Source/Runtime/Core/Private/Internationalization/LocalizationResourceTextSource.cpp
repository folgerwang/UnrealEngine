// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/LocalizationResourceTextSource.h"
#include "Internationalization/TextLocalizationResource.h"
#include "HAL/PlatformProperties.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ConfigCacheIni.h"

bool FLocalizationResourceTextSource::GetNativeCultureName(const ELocalizedTextSourceCategory InCategory, FString& OutNativeCultureName)
{
	FString NativeCultureName = TextLocalizationResourceUtil::GetNativeCultureName(InCategory);
	if (!NativeCultureName.IsEmpty())
	{
		OutNativeCultureName = MoveTemp(NativeCultureName);
		return true;
	}
	return false;
}

void FLocalizationResourceTextSource::GetLocalizedCultureNames(const ELocalizationLoadFlags InLoadFlags, TSet<FString>& OutLocalizedCultureNames)
{
	TArray<FString> LocalizationPaths;
	if (EnumHasAnyFlags(InLoadFlags, ELocalizationLoadFlags::Editor))
	{
		LocalizationPaths += FPaths::GetEditorLocalizationPaths();
	}
	if (EnumHasAnyFlags(InLoadFlags, ELocalizationLoadFlags::Game))
	{
		LocalizationPaths += FPaths::GetGameLocalizationPaths();
	}
	if (EnumHasAnyFlags(InLoadFlags, ELocalizationLoadFlags::Engine))
	{
		LocalizationPaths += FPaths::GetEngineLocalizationPaths();
	}
	if (EnumHasAnyFlags(InLoadFlags, ELocalizationLoadFlags::Additional))
	{
		FCoreDelegates::GatherAdditionalLocResPathsCallback.Broadcast(LocalizationPaths);
	}

	TArray<FString> LocalizedCultureNames = TextLocalizationResourceUtil::GetLocalizedCultureNames(LocalizationPaths);
	OutLocalizedCultureNames.Append(MoveTemp(LocalizedCultureNames));
}

void FLocalizationResourceTextSource::LoadLocalizedResources(const ELocalizationLoadFlags InLoadFlags, TArrayView<const FString> InPrioritizedCultures, FTextLocalizationResource& InOutNativeResource, FTextLocalizationResource& InOutLocalizedResource)
{
	const int32 BaseResourcePriority = GetPriority() * -1; // Flip the priority as larger text source priorities are more important, but smaller text resource priorities are more important

	// Collect the localization paths to load from.
	TArray<FString> GameNativePaths;
	TArray<FString> GameLocalizationPaths;
	if (ShouldLoadNativeGameData(InLoadFlags))
	{
		GameNativePaths += FPaths::GetGameLocalizationPaths();
	}
	else if (ShouldLoadGame(InLoadFlags))
	{
		GameLocalizationPaths += FPaths::GetGameLocalizationPaths();
	}

	TArray<FString> EditorNativePaths;
	TArray<FString> EditorLocalizationPaths;
	if (ShouldLoadEditor(InLoadFlags))
	{
		EditorLocalizationPaths += FPaths::GetEditorLocalizationPaths();
		EditorLocalizationPaths += FPaths::GetToolTipLocalizationPaths();

		bool bShouldUseLocalizedPropertyNames = false;
		if (!GConfig->GetBool(TEXT("Internationalization"), TEXT("ShouldUseLocalizedPropertyNames"), bShouldUseLocalizedPropertyNames, GEditorSettingsIni))
		{
			GConfig->GetBool(TEXT("Internationalization"), TEXT("ShouldUseLocalizedPropertyNames"), bShouldUseLocalizedPropertyNames, GEngineIni);
		}

		if (bShouldUseLocalizedPropertyNames)
		{
			EditorLocalizationPaths += FPaths::GetPropertyNameLocalizationPaths();
		}
		else
		{
			EditorNativePaths += FPaths::GetPropertyNameLocalizationPaths();
		}
	}

	TArray<FString> EngineLocalizationPaths;
	if (ShouldLoadEngine(InLoadFlags))
	{
		EngineLocalizationPaths += FPaths::GetEngineLocalizationPaths();
	}

	// Gather any additional paths that are unknown to the UE4 core (such as plugins)
	TArray<FString> AdditionalLocalizationPaths;
	if (ShouldLoadAdditional(InLoadFlags))
	{
		FCoreDelegates::GatherAdditionalLocResPathsCallback.Broadcast(AdditionalLocalizationPaths);
	}

	TArray<FString> PrioritizedLocalizationPaths;
	PrioritizedLocalizationPaths += GameLocalizationPaths;
	PrioritizedLocalizationPaths += EditorLocalizationPaths;
	PrioritizedLocalizationPaths += EngineLocalizationPaths;
	PrioritizedLocalizationPaths += AdditionalLocalizationPaths;

	TArray<FString> PrioritizedNativePaths;
	if (ShouldLoadNative(InLoadFlags))
	{
		PrioritizedNativePaths = PrioritizedLocalizationPaths;

		if (EditorNativePaths.Num() > 0)
		{
			for (const FString& LocalizationPath : EditorNativePaths)
			{
				PrioritizedNativePaths.AddUnique(LocalizationPath);
			}
		}
	}

	static const FString PlatformLocalizationFolderName = FPaths::GetPlatformLocalizationFolderName();
	static const FString PlatformName = ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName());
	auto LoadLocalizationResourcesForCulture = [](FTextLocalizationResource& InOutLocRes, const FString& InLocalizationPath, const FString& InCulture, const FString& InLocResFilename, const int32 InLocResPriority)
	{
		const FString PlatformAgnosticLocResFilename = InLocalizationPath / InCulture / InLocResFilename;
		InOutLocRes.LoadFromFile(PlatformAgnosticLocResFilename, InLocResPriority);

		const FString PlatformSpecificLocResFilename = InLocalizationPath / PlatformLocalizationFolderName / PlatformName / InCulture / InLocResFilename;
		if (FPaths::FileExists(PlatformSpecificLocResFilename))
		{
			InOutLocRes.LoadFromFile(PlatformSpecificLocResFilename, InLocResPriority);
		}
	};

	// Load the native texts first to ensure we always apply translations to a consistent base
	if (PrioritizedNativePaths.Num() > 0)
	{
		for (const FString& LocalizationPath : PrioritizedNativePaths)
		{
			if (!IFileManager::Get().DirectoryExists(*LocalizationPath))
			{
				continue;
			}

			const FString LocMetaFilename = FPaths::GetBaseFilename(LocalizationPath) + TEXT(".locmeta");

			FTextLocalizationMetaDataResource LocMetaResource;
			if (LocMetaResource.LoadFromFile(LocalizationPath / LocMetaFilename))
			{
				// We skip loading the native text if we're transitioning to the native culture as there's no extra work that needs to be done
				if (!InPrioritizedCultures.Contains(LocMetaResource.NativeCulture))
				{
					LoadLocalizationResourcesForCulture(InOutNativeResource, LocalizationPath, LocMetaResource.NativeCulture, FPaths::GetCleanFilename(LocMetaResource.NativeLocRes), BaseResourcePriority);
				}
			}
		}
	}

	// The editor cheats and loads the games native localizations.
	if (ShouldLoadNativeGameData(InLoadFlags) && GameNativePaths.Num() > 0)
	{
		const FString NativeGameCulture = TextLocalizationResourceUtil::GetNativeProjectCultureName();
		if (!NativeGameCulture.IsEmpty())
		{
			for (const FString& LocalizationPath : GameNativePaths)
			{
				if (!IFileManager::Get().DirectoryExists(*LocalizationPath))
				{
					continue;
				}

				const FString LocResFilename = FPaths::GetBaseFilename(LocalizationPath) + TEXT(".locres");
				LoadLocalizationResourcesForCulture(InOutLocalizedResource, LocalizationPath, NativeGameCulture, LocResFilename, BaseResourcePriority);
			}
		}
	}

	// Read culture localization resources.
	if (PrioritizedLocalizationPaths.Num() > 0)
	{
		for (int32 CultureIndex = 0; CultureIndex < InPrioritizedCultures.Num(); ++CultureIndex)
		{
			const FString& PrioritizedCultureName = InPrioritizedCultures[CultureIndex];
			for (const FString& LocalizationPath : PrioritizedLocalizationPaths)
			{
				if (!IFileManager::Get().DirectoryExists(*LocalizationPath))
				{
					continue;
				}

				const FString LocResFilename = FPaths::GetBaseFilename(LocalizationPath) + TEXT(".locres");
				LoadLocalizationResourcesForCulture(InOutLocalizedResource, LocalizationPath, PrioritizedCultureName, LocResFilename, BaseResourcePriority + CultureIndex);
			}
		}
	}
}
