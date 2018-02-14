// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextAssetCommandlet.cpp: Commandlet for batch conversion and testing of
	text asset formats
=============================================================================*/

#include "Commandlets/TextAssetCommandlet.h"
#include "PackageHelperFunctions.h"
#include "Engine/Texture.h"
#include "Logging/LogMacros.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectIterator.h"
#include "Stats/StatsMisc.h"

DEFINE_LOG_CATEGORY(LogTextAsset);

UTextAssetCommandlet::UTextAssetCommandlet( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
}

int32 UTextAssetCommandlet::Main(const FString& CmdLineParams)
{
	TArray<FString> Blacklist;

	FString ModeString = TEXT("loadsave");
	FString OutputFormatString = TEXT("text");
	FString IterationsString = TEXT("1");

	FString FilenameFilterString, OutputPathString;
	FParse::Value(*CmdLineParams, TEXT("mode="), ModeString);
	FParse::Value(*CmdLineParams, TEXT("filter="), FilenameFilterString);
	FParse::Value(*CmdLineParams, TEXT("outputpath="), OutputPathString);
	FParse::Value(*CmdLineParams, TEXT("outputFormat="), OutputFormatString);

	enum class EMode
	{
		LoadSave,
	};

	enum class EOutputFormat
	{
		Text,
		Binary
	};

	TMap<FString, EMode> Modes;
	Modes.Add(TEXT("loadsave"), EMode::LoadSave);

	TMap<FString, EOutputFormat> OutputFormats;
	OutputFormats.Add(TEXT("text"), EOutputFormat::Text);
	OutputFormats.Add(TEXT("binary"), EOutputFormat::Binary);

	check(Modes.Contains(ModeString));
	EMode Mode = Modes[ModeString];

	check(OutputFormats.Contains(OutputFormatString));
	EOutputFormat OutputFormat = OutputFormats[OutputFormatString];
	
	TArray<UPackage*> Packages;
	TArray<UObject*> Objects;	

	int32 NumSaveIterations = 1;
	FParse::Value(*CmdLineParams, TEXT("iterations="), NumSaveIterations);

	TArray<FString> Files;

	FString BasePath = *FPaths::ProjectContentDir();
	IFileManager::Get().FindFilesRecursive(Files, *BasePath, TEXT("*.uasset"), true, false, true);
	IFileManager::Get().FindFilesRecursive(Files, *BasePath, TEXT("*.umap"), true, false, false);

	TArray<TTuple<FString, FString>> FilesToProcess;

	TMap<EOutputFormat, FString> OutputFormatExtensions;
	OutputFormatExtensions.Add(EOutputFormat::Binary, FPackageName::GetAssetPackageExtension());
	OutputFormatExtensions.Add(EOutputFormat::Text, FPackageName::GetTextAssetPackageExtension());

	for (const FString& SrcFilename : Files)
	{
		bool bIgnore = false;

		FString Filename = SrcFilename;
		if (FilenameFilterString.Len() > 0 && !Filename.Contains(FilenameFilterString))
		{
			bIgnore = true;
		}

		bIgnore = bIgnore || (Filename.Contains(TEXT("_BuiltData")));

		for (const FString& BlacklistItem : Blacklist)
		{
			if (Filename.Contains(BlacklistItem))
			{
				bIgnore = true;
				break;
			}
		}

		if (bIgnore)
		{
			continue;
		}

		bool bShouldProcess = true;

		FString DestinationFilename = FPaths::ChangeExtension(Filename, OutputFormatExtensions[OutputFormat]);

		if (Filename == DestinationFilename)
		{
			DestinationFilename += TEXT(".tmp");
		}

		if (bShouldProcess)
		{
			FilesToProcess.Add(TTuple<FString, FString>(Filename, DestinationFilename));
		}
	}

	float TotalPackageLoadTime = 0.0;
	float TotalPackageSaveTime = 0.0;

	UE_LOG(LogTextAsset, Log, TEXT("-----------------------------------------------------"));

	for (int32 Iteration = 0; Iteration < NumSaveIterations; ++Iteration)
	{
		if (NumSaveIterations > 1)
		{
			UE_LOG(LogTextAsset, Log, TEXT("Iteration %i Started"), Iteration + 1);
			UE_LOG(LogTextAsset, Log, TEXT("-----------------------------------------------------"));
		}

		double MaxTime = FLT_MIN;
		double MinTime = FLT_MAX;
		double TotalTime = 0;
		int64 NumFiles = 0;
		FString MaxTimePackage;
		FString MinTimePackage;
		float IterationPackageLoadTime = 0.0;
		float IterationPackageSaveTime = 0.0;

		for (const TTuple<FString, FString>& FileToProcess : FilesToProcess)
		{
			FString SourceFilename = FPackageName::FilenameToLongPackageName(FileToProcess.Get<0>());
			FString DestinationFilename = FileToProcess.Get<1>();

			double StartTime = FPlatformTime::Seconds();

			switch (Mode)
			{
			case EMode::LoadSave:
			{
				UPackage* Package = nullptr;

				double Timer = 0.0;
				{
					SCOPE_SECONDS_COUNTER(Timer);
					Package = LoadPackage(nullptr, *SourceFilename, 0);
				}
				IterationPackageLoadTime += Timer;
				TotalPackageLoadTime += Timer;

				if (Package)
				{
					{
						SCOPE_SECONDS_COUNTER(Timer);

						IFileManager::Get().Delete(*DestinationFilename, false, true);
						SavePackageHelper(Package, *DestinationFilename, RF_Standalone, GWarn, nullptr, SAVE_KeepGUID);
					}
					TotalPackageSaveTime += Timer;
					IterationPackageSaveTime += Timer;
				}

				if (OutputPathString.Len() > 0)
				{
					FString CopyFilename = DestinationFilename;
					FPaths::MakePathRelativeTo(CopyFilename, *FPaths::ProjectContentDir());
					CopyFilename = OutputPathString / FApp::GetProjectName() / CopyFilename;
					CopyFilename.RemoveFromEnd(TEXT(".tmp"));
					IFileManager::Get().MakeDirectory(*FPaths::GetPath(CopyFilename));
					IFileManager::Get().Move(*CopyFilename, *DestinationFilename);
				}

				break;
			}
			}

			double EndTime = FPlatformTime::Seconds();
			double Time = EndTime - StartTime;

			if (Time > MaxTime)
			{
				MaxTime = Time;
				MaxTimePackage = SourceFilename;
			}

			if (Time < MinTime)
			{
				MinTime = Time;
				MinTimePackage = SourceFilename;
			}

			TotalTime += Time;
			NumFiles++;
		}

		if (NumSaveIterations > 1)
		{
			UE_LOG(LogTextAsset, Log, TEXT("Iteration %i Completed"), Iteration + 1);
		}
		
		UE_LOG(LogTextAsset, Log, TEXT("\tTotal Time:\t%.2fs"), TotalTime);
		UE_LOG(LogTextAsset, Log, TEXT("\tAvg File Time:  \t%.2fms"), (TotalTime * 1000.0) / (double)NumFiles);
		UE_LOG(LogTextAsset, Log, TEXT("\tMin File Time:  \t%.2fms (%s)"), MinTime * 1000.0, *MinTimePackage);
		UE_LOG(LogTextAsset, Log, TEXT("\tMax File Time:  \t%.2fms (%s)"), MaxTime * 1000.0, *MaxTimePackage);
		UE_LOG(LogTextAsset, Log, TEXT("\tTotal Package Load Time:  \t%.2fs"), IterationPackageLoadTime);
		UE_LOG(LogTextAsset, Log, TEXT("\tTotal Package Save Time:  \t%.2fs"), IterationPackageSaveTime);

		Packages.Empty();
		CollectGarbage(RF_NoFlags, true);
	}

	UE_LOG(LogTextAsset, Log, TEXT("-----------------------------------------------------"));
	UE_LOG(LogTextAsset, Log, TEXT("Text Asset Commandlet Completed!"));
	UE_LOG(LogTextAsset, Log, TEXT("\tTotal Files Saved:  \t%i"), FilesToProcess.Num());
	UE_LOG(LogTextAsset, Log, TEXT("\tAvg Iteration Package Load Time:  \t%.2fs"), TotalPackageLoadTime / (float)NumSaveIterations);
	UE_LOG(LogTextAsset, Log, TEXT("\tAvg Iteration Save Time:  \t%.2fs"), TotalPackageSaveTime / (float)NumSaveIterations);
	UE_LOG(LogTextAsset, Log, TEXT("-----------------------------------------------------"));
	
	return 0;
}