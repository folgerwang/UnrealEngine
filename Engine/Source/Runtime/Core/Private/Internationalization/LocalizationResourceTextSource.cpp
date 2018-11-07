// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/LocalizationResourceTextSource.h"
#include "Internationalization/TextLocalizationResource.h"
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
	OutLocalizedCultureNames.Append(LocalizedCultureNames);
}

void FLocalizationResourceTextSource::LoadLocalizedResources(const ELocalizationLoadFlags InLoadFlags, TArrayView<const FString> InPrioritizedCultures, FTextLocalizationResource& InOutNativeResource, FTextLocalizationResources& InOutLocalizedResources)
{
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

	// Load the native texts first to ensure we always apply translations to a consistent base
	if (PrioritizedNativePaths.Num() > 0)
	{
		for (const FString& LocalizationPath : PrioritizedNativePaths)
		{
			TArray<FString> LocMetaFilenames;
			IFileManager::Get().FindFiles(LocMetaFilenames, *(LocalizationPath / TEXT("*.locmeta")), true, false);

			// There should only be zero or one LocMeta file
			check(LocMetaFilenames.Num() <= 1);

			if (LocMetaFilenames.Num() == 1)
			{
				FTextLocalizationMetaDataResource LocMetaResource;
				if (LocMetaResource.LoadFromFile(LocalizationPath / LocMetaFilenames[0]))
				{
					// We skip loading the native text if we're transitioning to the native culture as there's no extra work that needs to be done
					if (!InPrioritizedCultures.Contains(LocMetaResource.NativeCulture))
					{
						InOutNativeResource.LoadFromFile(LocalizationPath / LocMetaResource.NativeLocRes);
					}
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
			TSharedRef<FTextLocalizationResource> TextLocalizationResource = InOutLocalizedResources.EnsureResource(InPrioritizedCultures[0]);
			for (const FString& LocalizationPath : GameNativePaths)
			{
				const FString CulturePath = LocalizationPath / NativeGameCulture;
				TextLocalizationResource->LoadFromDirectory(CulturePath);
			}
		}
	}

	// Read culture localization resources.
	for (const FString& PrioritizedCultureName : InPrioritizedCultures)
	{
		if (PrioritizedLocalizationPaths.Num() > 0)
		{
			TSharedRef<FTextLocalizationResource> TextLocalizationResource = InOutLocalizedResources.EnsureResource(PrioritizedCultureName);
			for (const FString& LocalizationPath : PrioritizedLocalizationPaths)
			{
				const FString CulturePath = LocalizationPath / PrioritizedCultureName;
				TextLocalizationResource->LoadFromDirectory(CulturePath);
			}
		}
	}
}
