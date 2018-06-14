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
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/Formatters/JsonArchiveOutputFormatter.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ArchiveUObjectFromStructuredArchive.h"

DEFINE_LOG_CATEGORY(LogTextAsset);

UTextAssetCommandlet::UTextAssetCommandlet( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
}

bool HashFile(const TCHAR* InFilename, FSHAHash& OutHash)
{
	TArray<uint8> Bytes;
	if (FFileHelper::LoadFileToArray(Bytes, InFilename))
	{
		FSHA1::HashBuffer(&Bytes[0], Bytes.Num(), OutHash.Hash);
	}
	return false;
}

void FindMismatchedSerializers()
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (!It->HasAnyClassFlags(CLASS_MatchedSerializers))
		{
			UE_LOG(LogTextAsset, Display, TEXT("Class Mismatched Serializers: %s"), *It->GetName());
		}
	}
}

int32 UTextAssetCommandlet::Main(const FString& CmdLineParams)
{
	TArray<FString> Blacklist;

	FString ModeString = TEXT("ResaveText");
	FString IterationsString = TEXT("1");
	FString FilenameFilterString, OutputPathString;

	FParse::Value(*CmdLineParams, TEXT("mode="), ModeString);
	FParse::Value(*CmdLineParams, TEXT("filter="), FilenameFilterString);
	FParse::Value(*CmdLineParams, TEXT("outputpath="), OutputPathString);
	bool bVerifyJson = !FParse::Param(*CmdLineParams, TEXT("noverifyjson"));

	enum class EMode
	{
		ResaveText,
		ResaveBinary,
		RoundTrip,
		LoadText,
		FindMismatchedSerializers
	};

	TMap<FString, EMode> Modes;
	Modes.Add(TEXT("ResaveText"), EMode::ResaveText);
	Modes.Add(TEXT("ResaveBinary"), EMode::ResaveBinary);
	Modes.Add(TEXT("RoundTrip"), EMode::RoundTrip);
	Modes.Add(TEXT("LoadText"), EMode::LoadText);
	Modes.Add(TEXT("FindMismatchedSerializers"), EMode::FindMismatchedSerializers);

	check(Modes.Contains(ModeString));
	EMode Mode = Modes[ModeString];

	if (Mode == EMode::FindMismatchedSerializers)
	{
		FindMismatchedSerializers();
		return 0;
	}

	TArray<UObject*> Objects;	

	int32 NumSaveIterations = 1;
	FParse::Value(*CmdLineParams, TEXT("iterations="), NumSaveIterations);

	bool bIncludeEngineContent = FParse::Param(*CmdLineParams, TEXT("includeenginecontent"));

	TArray<FString> InputAssetFilenames;

	FString ProjectContentDir = *FPaths::ProjectContentDir();
	FString EngineContentDir = *FPaths::EngineContentDir();
	const FString Wildcard = TEXT("*");

	switch (Mode)
	{
	case EMode::ResaveBinary:
	case EMode::ResaveText:
	case EMode::RoundTrip:
	{
		IFileManager::Get().FindFilesRecursive(InputAssetFilenames, *ProjectContentDir, *(Wildcard + FPackageName::GetAssetPackageExtension()), true, false, true);
		IFileManager::Get().FindFilesRecursive(InputAssetFilenames, *ProjectContentDir, *(Wildcard + FPackageName::GetMapPackageExtension()), true, false, false);

		if (bIncludeEngineContent)
		{
			IFileManager::Get().FindFilesRecursive(InputAssetFilenames, *EngineContentDir, *(Wildcard + FPackageName::GetAssetPackageExtension()), true, false, false);
			IFileManager::Get().FindFilesRecursive(InputAssetFilenames, *EngineContentDir, *(Wildcard + FPackageName::GetMapPackageExtension()), true, false, false);
		}

		break;
	}

	case EMode::LoadText:
	{
		IFileManager::Get().FindFilesRecursive(InputAssetFilenames, *ProjectContentDir, *(Wildcard + FPackageName::GetTextAssetPackageExtension()), true, false, true);
		//IFileManager::Get().FindFilesRecursive(InputAssetFilenames, *BasePath, *(Wildcard + FPackageName::GetTextMapPackageExtension()), true, false, false);
		break;
	}
	}

	TArray<TTuple<FString, FString>> FilesToProcess;

	for (const FString& InputAssetFilename : InputAssetFilenames)
	{
		bool bIgnore = false;

		if (FilenameFilterString.Len() > 0 && !InputAssetFilename.Contains(FilenameFilterString))
		{
			bIgnore = true;
		}

		bIgnore = bIgnore || (InputAssetFilename.Contains(TEXT("_BuiltData")));

		for (const FString& BlacklistItem : Blacklist)
		{
			if (InputAssetFilename.Contains(BlacklistItem))
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
		FString DestinationFilename = InputAssetFilename;

		switch (Mode)
		{
		case EMode::ResaveBinary:
		{
			DestinationFilename = InputAssetFilename + TEXT(".tmp");
			break;
		}

		case EMode::ResaveText:
		{
			if (InputAssetFilename.EndsWith(FPackageName::GetAssetPackageExtension())) DestinationFilename = FPaths::ChangeExtension(InputAssetFilename, FPackageName::GetTextAssetPackageExtension());;
			if (InputAssetFilename.EndsWith(FPackageName::GetMapPackageExtension())) DestinationFilename = FPaths::ChangeExtension(InputAssetFilename, FPackageName::GetTextMapPackageExtension());;
			
			break;
		}

		case EMode::LoadText:
		{
			break;
		}
		}

		if (bShouldProcess)
		{
			FilesToProcess.Add(TTuple<FString, FString>(InputAssetFilename, DestinationFilename));
		}
	}

	TArray<FString> IntermediateFilenames;
	struct FVisitor : public IPlatformFile::FDirectoryVisitor
	{
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if (!bIsDirectory && FString(FilenameOrDirectory).Contains(TEXT(".txtassettemp")))
			{
				UE_LOG(LogTextAsset, Display, TEXT("Cleaning up old intermediate file %s"), FilenameOrDirectory);
				IFileManager::Get().Delete(FilenameOrDirectory);
			}
			return true;
		}
	} Visitor;

	IFileManager::Get().IterateDirectoryRecursively(*FPaths::ProjectContentDir(), Visitor);
	IFileManager::Get().IterateDirectoryRecursively(*FPaths::EngineContentDir(), Visitor);

	const FString FailedDiffsPath = FPaths::ProjectSavedDir() / TEXT("FailedDiffs");
	static const bool bKeepFailedDiffs = FParse::Param(FCommandLine::Get(), TEXT("keepfaileddiffs"));
	if (bKeepFailedDiffs)
	{
		IFileManager::Get().DeleteDirectory(*FailedDiffsPath, false, true);
	}

	float TotalPackageLoadTime = 0.0;
	float TotalPackageSaveTime = 0.0;

	for (int32 Iteration = 0; Iteration < NumSaveIterations; ++Iteration)
	{
		if (NumSaveIterations > 1)
		{
			UE_LOG(LogTextAsset, Display, TEXT("-----------------------------------------------------"));
			UE_LOG(LogTextAsset, Display, TEXT("Iteration %i/%i"), Iteration + 1, NumSaveIterations);
		}

		double MaxTime = FLT_MIN;
		double MinTime = FLT_MAX;
		double TotalTime = 0;
		int64 NumFiles = 0;
		FString MaxTimePackage;
		FString MinTimePackage;
		float IterationPackageLoadTime = 0.0;
		float IterationPackageSaveTime = 0.0;

		TArray<FString> PhaseSuccess;
		TArray<TArray<FString>> PhaseFails;
		PhaseFails.AddDefaulted(3);

		for (const TTuple<FString, FString>& FileToProcess : FilesToProcess)
		{
			FString SourceFilename = FileToProcess.Get<0>();
			FString SourceLongPackageName = FPackageName::FilenameToLongPackageName(SourceFilename);
			FString DestinationFilename = FileToProcess.Get<1>();

			IntermediateFilenames.Empty();

			double StartTime = FPlatformTime::Seconds();

			switch (Mode)
			{
			case EMode::RoundTrip:
			{
				const FString WorkingFilenames[2] = { SourceFilename, FPaths::ChangeExtension(SourceFilename, FPackageName::GetTextAssetPackageExtension()) };
				

				IFileManager::Get().Delete(*WorkingFilenames[1], false, false, true);

				FString SourceBackupFilename = SourceFilename + TEXT(".bak");
				if (IFileManager::Get().FileExists(*SourceBackupFilename))
				{
					IFileManager::Get().Delete(*SourceFilename, false, false, true);
					IFileManager::Get().Move(*SourceFilename, *SourceBackupFilename, true);
				}
				IFileManager::Get().Copy(*SourceBackupFilename, *SourceFilename, true);
				
				// Firstly, do a resave of the package
				UPackage* OriginalPackage = LoadPackage(nullptr, *SourceLongPackageName, LOAD_None);
				IFileManager::Get().Delete(*SourceFilename, false, true, true);
				SavePackageHelper(OriginalPackage, SourceFilename, RF_Standalone, GWarn, nullptr, SAVE_KeepGUID);
				CollectGarbage(RF_NoFlags, true);

				// Make a copy of the resaved source package which we can use as the base revision for each test
				FString BaseBinaryPackageBackup = SourceFilename + TEXT(".bak2");
				IFileManager::Get().Copy(*BaseBinaryPackageBackup, *SourceFilename, true);

				FSHAHash SourceHash;
				HashFile(*SourceBackupFilename, SourceHash);
				
				static const int32 NumPhases = 3;
				static const int32 NumTests = 3;

				static const TCHAR* PhaseNames[NumPhases] = { TEXT("Binary Only"), TEXT("Text Only"), TEXT("Alternating Binary/Text") };


				TArray<TArray<FSHAHash>> Hashes;

				CollectGarbage(RF_NoFlags, true);

				UE_LOG(LogTextAsset, Display, TEXT("Starting roundtrip test for '%s' [%d/%d]"), *SourceLongPackageName, NumFiles + 1, FilesToProcess.Num());
				UE_LOG(LogTextAsset, Display, TEXT("-----------------------------------------------------------------------------------------"));

				bool bPhasesMatched[NumPhases] = { true, true, true };
				TArray<FString> DiffFilenames;

				for (int32 Phase = 0; Phase < NumPhases; ++Phase)
				{
					IFileManager::Get().Delete(*SourceFilename, false, false, true);
					IFileManager::Get().Copy(*SourceFilename, *BaseBinaryPackageBackup, true);

					TArray<FSHAHash> PhaseHashes = Hashes[Hashes.AddDefaulted()];

					for (int32 i = 0; i < ((Phase == 2) ? NumTests * 2 : NumTests); ++i)
					{
						int32 Bucket;

						switch (Phase)
						{
						case 0: // binary only
						{
							Bucket = 0;
							break;
						}

						case 1: // text only
						{
							Bucket = 1;
							break;
						}

						case 2: // alternate
						{
							Bucket = i % 2;
							break;
						}

						default:
						{
							checkNoEntry();
							Bucket = 0;
						}
						};

						if (Phase == 2 && Bucket == 1)
						{
							// We're doing alternating text/binary saves, so we need to delete the text version as we have no way of forcing the load to choose between text and binary
							IFileManager::Get().Delete(*WorkingFilenames[1]);
						}

						UPackage* Package = LoadPackage(nullptr, *SourceLongPackageName, LOAD_None);
						SavePackageHelper(Package, *WorkingFilenames[Bucket], RF_Standalone, GWarn, nullptr, SAVE_KeepGUID);
						ResetLoaders(Package);
						CollectGarbage(RF_NoFlags, true);

						FSHAHash& Hash = PhaseHashes[PhaseHashes.AddDefaulted()];
						HashFile(*WorkingFilenames[Bucket], Hash);

						if (bKeepFailedDiffs)
						{ 
							FString TargetPath = WorkingFilenames[Bucket];
							FPaths::MakePathRelativeTo(TargetPath, *FPaths::ProjectContentDir());
							TargetPath = FailedDiffsPath / TargetPath;

							FString IntermediateFilename = FString::Printf(TEXT("%s_Phase%i_%03i%s"), *FPaths::ChangeExtension(TargetPath, TEXT("")), Phase, i + 1, *FPaths::GetExtension(WorkingFilenames[Bucket], true));
							IFileManager::Get().Copy(*IntermediateFilename, *WorkingFilenames[Bucket]);

							DiffFilenames.Add(IntermediateFilename);
						}
					}

					UE_LOG(LogTextAsset, Display, TEXT("Phase %i (%s) Results"), Phase + 1, PhaseNames[Phase]);
					int32 Pass = 1;
					FSHAHash Refs[2] = { PhaseHashes[0], PhaseHashes[1] };
					bool bTotalSuccess = true;
					for (const FSHAHash& Hash : PhaseHashes)
					{
						if (Phase == 2)
						{
							bPhasesMatched[Phase] = bPhasesMatched[Phase] && Hash == Refs[(Pass + 1) % 2];
						}
						else
						{
							bPhasesMatched[Phase] = bPhasesMatched[Phase] && Hash == Refs[0];
						}

						UE_LOG(LogTextAsset, Display, TEXT("\tPass %i [%s] %s"), Pass++, *Hash.ToString(), bPhasesMatched[Phase] ? TEXT("OK") : TEXT("FAILED"));
					}

					if (!bPhasesMatched[Phase])
					{
						UE_LOG(LogTextAsset, Display, TEXT("\tPhase %i (%s) failed for asset '%s'"), Phase + 1, PhaseNames[Phase], *SourceLongPackageName);
						bTotalSuccess = false;
					}

					if (Phase == 1)
					{
						IFileManager::Get().Delete(*WorkingFilenames[1], false, false, true);
					}

					if (bTotalSuccess)
					{
						for (const FString& DiffFilename : DiffFilenames)
						{
							IFileManager::Get().Delete(*DiffFilename, false, false, true);
						}
					}
				}

				static const bool bDisableCleanup = FParse::Param(FCommandLine::Get(), TEXT("disablecleanup"));
				CollectGarbage(RF_NoFlags, true);
				IFileManager::Get().Delete(*WorkingFilenames[1], false, true, true);
				IFileManager::Get().Delete(*BaseBinaryPackageBackup, false, true, true);
				IFileManager::Get().Delete(*SourceFilename, false, true, true);
				IFileManager::Get().Move(*SourceFilename, *SourceBackupFilename);

				if (!bDisableCleanup)
				{
					for (const FString& IntermediateFilename : IntermediateFilenames)
					{
						IFileManager::Get().Delete(*IntermediateFilename, false, true, true);
					}
				}

				if (!bPhasesMatched[0])
				{
					UE_LOG(LogTextAsset, Display, TEXT("-----------------------------------------------------------------------------------------"));
					UE_LOG(LogTextAsset, Warning, TEXT("Binary determinism tests failed, so we can't determine meaningful results for '%s'"), *SourceLongPackageName);
				}
				else if (!bPhasesMatched[1] || !bPhasesMatched[2])
				{
					UE_LOG(LogTextAsset, Display, TEXT("-----------------------------------------------------------------------------------------"));
					UE_LOG(LogTextAsset, Error, TEXT("Binary determinism tests succeeded, but text and/or alternating tests failed for asset '%s'"), *SourceLongPackageName);
				}

				bool bSuccess = true;
				for (int32 PhaseIndex = 0; PhaseIndex < NumPhases; ++PhaseIndex)
				{
					if (!bPhasesMatched[PhaseIndex])
					{
						bSuccess = false;
						PhaseFails[PhaseIndex].Add(SourceLongPackageName);
					}
				}

				if (bSuccess)
				{
					PhaseSuccess.Add(SourceLongPackageName);
				}

				UE_LOG(LogTextAsset, Display, TEXT("-----------------------------------------------------------------------------------------"));
				UE_LOG(LogTextAsset, Display, TEXT("Completed roundtrip test for '%s'"), *SourceLongPackageName);
				UE_LOG(LogTextAsset, Display, TEXT("-----------------------------------------------------------------------------------------"));
				
				break;
			}

			case EMode::ResaveBinary:
			case EMode::ResaveText:
			{
				UPackage* Package = nullptr;

				UE_LOG(LogTextAsset, Display, TEXT("Resaving asset %s"), *SourceFilename);

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

						IFileManager::Get().Delete(*DestinationFilename, false, true, true);
						SavePackageHelper(Package, *DestinationFilename, RF_Standalone, GWarn, nullptr, SAVE_KeepGUID);
					}
					TotalPackageSaveTime += Timer;
					IterationPackageSaveTime += Timer;
				}

				if (bVerifyJson)
				{
					FArchive* File = IFileManager::Get().CreateFileReader(*DestinationFilename);
					TSharedPtr< FJsonObject > RootObject;
					TSharedRef< TJsonReader<char> > Reader = TJsonReaderFactory<char>::Create(File);
					ensure(FJsonSerializer::Deserialize(Reader, RootObject));
					delete File;
				}

				if (OutputPathString.Len() > 0)
				{
					FString CopyFilename = DestinationFilename;
					FPaths::MakePathRelativeTo(CopyFilename, *FPaths::RootDir());
					CopyFilename = OutputPathString / CopyFilename;
					CopyFilename.RemoveFromEnd(TEXT(".tmp"));
					IFileManager::Get().MakeDirectory(*FPaths::GetPath(CopyFilename));
					IFileManager::Get().Move(*CopyFilename, *DestinationFilename);
				}

				break;
			}

			case EMode::LoadText:
			{
				UPackage* Package = nullptr;

				CollectGarbage(RF_NoFlags, true);
				double Timer = 0.0;
				{
					SCOPE_SECONDS_COUNTER(Timer);
					UE_LOG(LogTextAsset, Display, TEXT("Loading Text Asset '%s'"), *SourceFilename);
					Package = LoadPackage(nullptr, *SourceFilename, 0);
				}
				CollectGarbage(RF_NoFlags, true);
				IterationPackageLoadTime += Timer;
				TotalPackageLoadTime += Timer;

				Package = nullptr;

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

		if (Mode == EMode::RoundTrip)
		{
			UE_LOG(LogTextAsset, Display, TEXT("\t-----------------------------------------------------"));
			UE_LOG(LogTextAsset, Display, TEXT("\tRoundTrip Results"));
			UE_LOG(LogTextAsset, Display, TEXT("\tTotal Packages: %i"), FilesToProcess.Num());
			UE_LOG(LogTextAsset, Display, TEXT("\tNum Successful Packages: %i"), PhaseSuccess.Num());
			UE_LOG(LogTextAsset, Display, TEXT("\tPhase 0 Fails: %i (Binary Package Determinism Fails)"), PhaseFails[0].Num());
			UE_LOG(LogTextAsset, Display, TEXT("\tPhase 1 Fails: %i (Text Package Determinism Fails)"), PhaseFails[1].Num());
			UE_LOG(LogTextAsset, Display, TEXT("\tPhase 2 Fails: %i (Mixed Package Determinism Fails)"), PhaseFails[2].Num());
			UE_LOG(LogTextAsset, Display, TEXT("\t-----------------------------------------------------"));

			for (int32 PhaseIndex = 0; PhaseIndex < PhaseFails.Num(); ++PhaseIndex)
			{
				if (PhaseFails[PhaseIndex].Num() > 0)
				{
					UE_LOG(LogTextAsset, Display, TEXT("\tPhase %i Fails:"), PhaseIndex);
					for (const FString& PhaseFail : PhaseFails[PhaseIndex])
					{
						UE_LOG(LogTextAsset, Display, TEXT("\t\t%s"), *PhaseFail);
					}
					UE_LOG(LogTextAsset, Display, TEXT("\t-----------------------------------------------------"));
				}
			}
		}

		UE_LOG(LogTextAsset, Display, TEXT("\tTotal Time:\t%.2fs"), TotalTime);
		UE_LOG(LogTextAsset, Display, TEXT("\tAvg File Time:  \t%.2fms"), (TotalTime * 1000.0) / (double)NumFiles);
		UE_LOG(LogTextAsset, Display, TEXT("\tMin File Time:  \t%.2fms (%s)"), MinTime * 1000.0, *MinTimePackage);
		UE_LOG(LogTextAsset, Display, TEXT("\tMax File Time:  \t%.2fms (%s)"), MaxTime * 1000.0, *MaxTimePackage);
		UE_LOG(LogTextAsset, Display, TEXT("\tTotal Package Load Time:  \t%.2fs"), IterationPackageLoadTime);

		if (Mode != EMode::LoadText)
		{
			UE_LOG(LogTextAsset, Display, TEXT("\tTotal Package Save Time:  \t%.2fs"), IterationPackageSaveTime);
		}

		CollectGarbage(RF_NoFlags, true);
	}

	UE_LOG(LogTextAsset, Display, TEXT("-----------------------------------------------------"));
	UE_LOG(LogTextAsset, Display, TEXT("Text Asset Commandlet Completed!"));
	UE_LOG(LogTextAsset, Display, TEXT("\tTotal Files Processed:  \t%i"), FilesToProcess.Num());
	UE_LOG(LogTextAsset, Display, TEXT("\tAvg Iteration Package Load Time:  \t%.2fs"), TotalPackageLoadTime / (float)NumSaveIterations);

	if (Mode != EMode::LoadText)
	{
		UE_LOG(LogTextAsset, Display, TEXT("\tAvg Iteration Save Time:  \t%.2fs"), TotalPackageSaveTime / (float)NumSaveIterations);
	}
	
	UE_LOG(LogTextAsset, Display, TEXT("-----------------------------------------------------"));
	
	return 0;
}