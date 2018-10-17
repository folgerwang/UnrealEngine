// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/DiffAssetRegistriesCommandlet.h"

#include "AssetData.h"

#include "UObject/Class.h"
#include "PlatformInfo.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/ArrayReader.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY(LogDiffAssets);

int32 UDiffAssetRegistriesCommandlet::Main(const FString& FullCommandLine)
{
	UE_LOG(LogDiffAssets, Display, TEXT("--------------------------------------------------------------------------------------------"));
	UE_LOG(LogDiffAssets, Display, TEXT("Running DiffAssetRegistries Commandlet"));

	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Params;

	ParseCommandLine(*FullCommandLine, Tokens, Switches, Params);

	DiffChunkID = -1;
	bIsVerbose = Switches.Contains(TEXT("VERBOSE"));
	{
		bSaveCSV = Switches.Contains(TEXT("CSV"));
		FString CSVName;
		bSaveCSV |= FParse::Value(*FullCommandLine, TEXT("CSVName="), CSVName);
		FString CSVPath;
		bSaveCSV |= FParse::Value(*FullCommandLine, TEXT("CSVPath="), CSVPath);

		if (bSaveCSV)
		{
			if (CSVFilename.IsEmpty() && !CSVName.IsEmpty())
			{
				CSVFilename = FString::Printf(TEXT("%s%s"), *FPaths::DiffDir(), *CSVName);
			}
			if (CSVFilename.IsEmpty())
			{
				CSVFilename = CSVPath;
			}
			if (CSVFilename.IsEmpty())
			{
				CSVFilename = FPaths::Combine( *FPaths::DiffDir(), TEXT("AssetChanges.csv"));
			}
			if (FPaths::GetExtension(CSVFilename).IsEmpty())
			{
				CSVFilename += TEXT(".csv");
			}
		}
	}

	// options to ignore small changes/sizes
	FParse::Value(*FullCommandLine, TEXT("MinChanges="), MinChangeCount);
	FParse::Value(*FullCommandLine, TEXT("MinChangeSize="), MinChangeSizeMB);
	FParse::Value(*FullCommandLine, TEXT("ChunkID="), DiffChunkID);
	FParse::Value(*FullCommandLine, TEXT("WarnPercentage="), WarnPercentage);

	FString OldPath;
	FString NewPath;

	const bool bUseSourceGuid = Switches.Contains(TEXT("SOURCEGUID"));
	const bool bConsistency = Switches.Contains(TEXT("CONSISTENCY"));
	const bool bEnginePackages = Switches.Contains(TEXT("ENGINEPACKAGES"));

	FString SortOrder;
	FParse::Value(*FullCommandLine, TEXT("Sort="), SortOrder);

	if (SortOrder == TEXT("name"))
	{
		ReportedFileOrder = SortOrder::ByName;
	}
	else if (SortOrder == TEXT("size"))
	{
		ReportedFileOrder = SortOrder::BySize;
	}
	else if (SortOrder == TEXT("class"))
	{
		ReportedFileOrder = SortOrder::ByClass;
	}
	else if (SortOrder == TEXT("change"))
	{
		ReportedFileOrder = SortOrder::ByChange;
	}



	FString Branch;
	FString CL;
	{
		FString Spec;
		if (FParse::Value(*FullCommandLine, TEXT("Branch="), Spec))
		{
			Spec.Split(TEXT("@"), &Branch, &CL);
		}
	}

	FParse::Value(*FullCommandLine, TEXT("platform="), TargetPlatform);

	if (TargetPlatform.IsEmpty())
	{
		UE_LOG(LogDiffAssets, Error, TEXT("No platform specified on the commandline use \"-platform=<platform>\"."));
	}

	auto FindAssetRegistryPath = [&](const FString& PathVal, FString& OutPath) {
			for (const FString SearchPath : AssetRegistrySearchPath)
			{
				FString FinalSearchPath = SearchPath;
				FinalSearchPath.ReplaceInline(TEXT("[buildversion]"), *PathVal);
				FinalSearchPath.ReplaceInline(TEXT("[platform]"), *TargetPlatform);
				if (IFileManager::Get().FileExists(*FinalSearchPath))
				{
					OutPath = FinalSearchPath;
					return true;
				}
			}
			return false;
		};

	// const TCHAR* AssetRegistrySubPath = TEXT("/Metadata/DevelopmentAssetRegistry.bin");
	const FString* OldPathVal = Params.Find(FString(TEXT("OldPath")));
	if (OldPathVal)
	{
		FindAssetRegistryPath(*OldPathVal, OldPath);
	}

	const FString* NewPathVal = Params.Find(FString(TEXT("NewPath")));
	if (NewPathVal)
	{
		FindAssetRegistryPath(*NewPathVal, NewPath);

		const FString FortniteText = TEXT("++Fortnite+");

		if (NewPathVal->StartsWith(*FortniteText))
		{
			// might be able to figure out branch / cl from this
			FString TempBranchCL = NewPathVal->Right(NewPathVal->Len() - FortniteText.Len());
			TempBranchCL.Split(TEXT("-CL-"), &Branch, &CL);
		}
	}

	if (OldPath.IsEmpty())
	{
		UE_LOG(LogDiffAssets, Error, TEXT("No old path specified \"-oldpath=<>\", use full path to asset registry or build version."));
		return -1;
	}
	if (NewPath.IsEmpty())
	{
		UE_LOG(LogDiffAssets, Error, TEXT("No new path specified \"-newpath=<>\", use full path to asset registry or build version."));
		return -1;
	}

	bMatchChangelists = false;
	if (!Branch.IsEmpty() && !CL.IsEmpty())
	{
		bMatchChangelists = true;


		// skip the game packages if we're doing engine packages only
		if (!bEnginePackages)
		{
			FillChangelists(Branch, CL, TEXT("/FortniteGame/Content/"), TEXT("/Game/"));
		}
		FillChangelists(Branch, CL, TEXT("/Engine/Content/"), TEXT("/Engine/"));
	}


	FPaths::NormalizeFilename(NewPath);
	FPaths::NormalizeFilename(OldPath);

	// try to discern platform
	/*if (NewPath.Contains(AssetRegistrySubPath))
	{
		FString NewPlatformDir = NewPath.Left(NewPath.Find(AssetRegistrySubPath));
		FString PlatformPath = FPaths::GetCleanFilename(NewPlatformDir);

		int32 NumPlatforms;
		const PlatformInfo::FPlatformInfo* PlatformInfoArray = PlatformInfo::GetPlatformInfoArray(NumPlatforms);
		for (int32 i = 0; i < NumPlatforms; ++i)
		{
			const PlatformInfo::FPlatformInfo& PlatformInfo = PlatformInfoArray[i];
			if (PlatformPath == PlatformInfo.TargetPlatformName.ToString())
			{		
				TargetPlatform = PlatformPath;
				break;
			}
		}
	}*/

	if (bConsistency)
	{
		ConsistencyCheck(OldPath, NewPath);
	}
	else
	{
		DiffAssetRegistries(OldPath, NewPath, bUseSourceGuid, bEnginePackages);
	}
	UE_LOG(LogDiffAssets, Display, TEXT("Successfully finished running DiffAssetRegistries Commandlet"));
	UE_LOG(LogDiffAssets, Display, TEXT("--------------------------------------------------------------------------------------------"));
	return 0;
}

void UDiffAssetRegistriesCommandlet::FillChangelists(FString Branch, FString CL, FString BasePath, FString AssetPath)
{
	TArray<FString> Results;
	int32 ReturnCode = 0;
	if (LaunchP4(TEXT("files ") + FString(TEXT("//Fortnite/")) + Branch + BasePath + TEXT("...@") + CL, Results, ReturnCode))
	{
		if (ReturnCode == 0)
		{
			for (const FString& Result : Results)
			{
				FString DepotPathName;
				FString ExtraInfoAfterPound;
				if (Result.Split(TEXT("#"), &DepotPathName, &ExtraInfoAfterPound))
				{
					if (DepotPathName.EndsWith(TEXT(".uasset")) || DepotPathName.EndsWith(TEXT(".umap")))
					{
						FString PostContentPath;
						if (DepotPathName.Split(BasePath, nullptr, &PostContentPath))
						{
							if (!PostContentPath.IsEmpty() && !PostContentPath.StartsWith(TEXT("Cinematics")) && !PostContentPath.StartsWith(TEXT("Developers")) && !PostContentPath.StartsWith(TEXT("Maps/Test_Maps")))
							{
								const FString PostContentPathWithoutExtension = FPaths::GetBaseFilename(PostContentPath, false);
								const FString FullPackageName = AssetPath + PostContentPathWithoutExtension;
								
								TArray<FString> Chunks;

								ExtraInfoAfterPound.ParseIntoArray(Chunks, TEXT(" "), true);

								int32 Changelist;

								LexTryParseString<int32>(Changelist, *Chunks[4]);
								if (Changelist)
								{
									AssetPathToChangelist.FindOrAdd(FName(*FullPackageName)) = Changelist;
								}
							}
						}
					}
					// ignore non-assets
				}
			}
		}
	}
}

void rescale(int bytes, double &value, int &exp)
{
	value = bytes;
	value = fabs(value);

	while (value > 1024.0)
	{
		value /= 1024.0;
		exp++;
	}
}

void UDiffAssetRegistriesCommandlet::ConsistencyCheck(const FString& OldPath, const FString& NewPath)
{
	{
		FArrayReader SerializedAssetData;

		if (!IFileManager::Get().FileExists(*OldPath))
		{
			UE_LOG(LogDiffAssets, Error, TEXT("File '%s' does not exist."), *OldPath);
			return;
		}
		if (!FFileHelper::LoadFileToArray(SerializedAssetData, *OldPath))
		{
			UE_LOG(LogDiffAssets, Error, TEXT("Failed to load file '%s'."), *OldPath);
			return;
		}
		FAssetRegistrySerializationOptions Options;
		Options.ModifyForDevelopment();

		if (!OldState.Serialize(SerializedAssetData, Options))
		{
			UE_LOG(LogDiffAssets, Error, TEXT("Failed to parse file '%s' as asset registry."), *OldPath);
			return;
		}
	}
	{
		FArrayReader SerializedAssetData;

		if (!IFileManager::Get().FileExists(*NewPath))
		{
			UE_LOG(LogDiffAssets, Error, TEXT("File '%s' does not exist."), *NewPath);
			return;
		}
		if (!FFileHelper::LoadFileToArray(SerializedAssetData, *NewPath))
		{
			UE_LOG(LogDiffAssets, Error, TEXT("Failed to load file '%s'."), *NewPath);
			return;
		}
		FAssetRegistrySerializationOptions Options;
		Options.ModifyForDevelopment();

		if (!OldState.Serialize(SerializedAssetData, Options))
		{
			UE_LOG(LogDiffAssets, Error, TEXT("Failed to parse file '%s' as asset registry."), *NewPath);
			return;
		}
	}
	UE_LOG(LogDiffAssets, Display, TEXT("Comparing asset registries '%s' and '%s'."), *OldPath, *NewPath);
	UE_LOG(LogDiffAssets, Display, TEXT("Source vs Cooked Consistency Diff"));
	if (bIsVerbose)
	{
		UE_LOG(LogDiffAssets, Display, TEXT("Cooked files that differ, where source guids do not:"));
	}
	// We're looking for packages that the Cooked check says are modified, but that the Guid check says are not
	// We're ignoring new packages for this, as those are obviously going to change
	TSet<FName> GuidModified, CookModified;
	TSet<FName> New;

	for (const TPair<FName, const FAssetPackageData*>& Pair : NewState.GetAssetPackageDataMap())
	{
		FName Name = Pair.Key;
		const FAssetPackageData* Data = Pair.Value;
		const FAssetPackageData* PrevData = OldState.GetAssetPackageData(Name);

		if (!PrevData)
		{
			New.Add(Name);
		}
		else
		{
			if (Data->PackageGuid != PrevData->PackageGuid)
			{
				GuidModified.Add(Name);
			}
			if (Data->CookedHash != PrevData->CookedHash)
			{
				CookModified.Add(Name);
			}
		}
	}

	// recurse through the referencer lists to fill out GuidModified
	TArray<FName> Recurse = GuidModified.Array();
	
	for (int32 RecurseIndex = 0; RecurseIndex < Recurse.Num(); RecurseIndex++)
	{
		FName Package = Recurse[RecurseIndex];
		TArray<FAssetIdentifier> Referencers;
		NewState.GetReferencers(Package, Referencers, EAssetRegistryDependencyType::Hard);
		
		for (const FAssetIdentifier& Referencer : Referencers)
		{
			FName ReferencerPackage = Referencer.PackageName;
			if (!New.Contains(ReferencerPackage) && !GuidModified.Contains(ReferencerPackage))
			{
				GuidModified.Add(ReferencerPackage);
				Recurse.Add(ReferencerPackage);
			}
		}
	}

	int64 changes = 0;
	int64 change_bytes = 0;

	// find all entries of CookModified that do not exist in GuidModified
	const TMap<FName, const FAssetPackageData*>& PackageMap = NewState.GetAssetPackageDataMap();
	for (FName const &Package : CookModified)
	{
		const FAssetPackageData* Data = PackageMap[Package];

		if (!GuidModified.Contains(Package))
		{
			++changes;
			change_bytes += Data->DiskSize;
			if (bIsVerbose)
			{
				UE_LOG(LogDiffAssets, Display, TEXT("%s : %d bytes"), *Package.ToString(), Data->DiskSize);
			}
		}
	}

	double change_value = 0.0;
	int change_exp = 0;

	rescale(change_bytes, change_value, change_exp);

	UE_LOG(LogDiffAssets, Display, TEXT("Summary:"));
	UE_LOG(LogDiffAssets, Display, TEXT("%d nondeterministic cooks, %8.3f %cB"), changes, change_value, " KMGTP"[change_exp]);
}

bool	UDiffAssetRegistriesCommandlet::IsInRelevantChunk(FAssetRegistryState& InRegistryState, FName InAssetPath)
{
	if (DiffChunkID == -1)
	{
		return true;

	}
	TArray<const FAssetData*> Assets = InRegistryState.GetAssetsByPackageName(InAssetPath);

	if (Assets.Num() && Assets[0]->ChunkIDs.Num())
	{
		return Assets[0]->ChunkIDs.Contains(DiffChunkID);
	}

	return true;
}

FName UDiffAssetRegistriesCommandlet::GetClassName(FAssetRegistryState& InRegistryState, FName InAssetPath)
{
	if (AssetPathToClassName.Contains(InAssetPath) == false)
	{
		TArray<const FAssetData*> Assets = InRegistryState.GetAssetsByPackageName(InAssetPath);

		FName NewName = NAME_None;
		if (Assets.Num() > 0)
		{
			NewName = Assets[0]->AssetClass;
		}
		else
		{
			if (InAssetPath.ToString().StartsWith(TEXT("/Script/")))
			{
				NewName = NAME_Class;
			}
		}

		if (NewName == NAME_None)
		{
			UE_LOG(LogDiffAssets, Warning, TEXT("Unable to find class type of asset %s"), *InAssetPath.ToString());
		}
		AssetPathToClassName.Add(InAssetPath) = NewName;
	}

	return AssetPathToClassName[InAssetPath];
}

void UDiffAssetRegistriesCommandlet::RecordAdd(FName InAssetPath, const FAssetPackageData& InNewData)
{
	FChangeInfo AssetChange;

	++AssetChange.Adds;
	if (InNewData.DiskSize > 0)
	{
		AssetChange.AddedBytes += InNewData.DiskSize;
	}

	FName ClassName = GetClassName(NewState, InAssetPath);

	ChangeInfoByAsset.FindOrAdd(InAssetPath) = AssetChange;
	ChangeSummaryByClass.FindOrAdd(ClassName) += AssetChange;
	ChangeSummary += AssetChange;
}

void UDiffAssetRegistriesCommandlet::RecordEdit(FName InAssetPath, const FAssetPackageData& InNewData, const FAssetPackageData& InOldData)
{
	FChangeInfo AssetChange;

	if (InNewData.DiskSize > 0)
	{
		++AssetChange.Changes;
		AssetChange.ChangedBytes += InNewData.DiskSize;
	}

	FName ClassName = GetClassName(NewState, InAssetPath);

	ChangeInfoByAsset.FindOrAdd(InAssetPath) = AssetChange;
	ChangeSummaryByClass.FindOrAdd(ClassName) += AssetChange;
	ChangeSummary += AssetChange;
}

void UDiffAssetRegistriesCommandlet::RecordDelete(FName InAssetPath, const FAssetPackageData& InData)
{
	FChangeInfo AssetChange;

	++AssetChange.Deletes;

	if (InData.DiskSize >= 0)
	{
		AssetChange.DeletedBytes += InData.DiskSize;
	}

	FName ClassName = GetClassName(OldState, InAssetPath);

	ChangeInfoByAsset.FindOrAdd(InAssetPath) = AssetChange;
	ChangeSummaryByClass.FindOrAdd(ClassName) += AssetChange;
	ChangeSummary += AssetChange;
}

void UDiffAssetRegistriesCommandlet::RecordNoChange(FName InAssetPath, const FAssetPackageData& InData)
{
	FChangeInfo AssetChange;

	AssetChange.Unchanged++;

	if (InData.DiskSize >= 0)
	{
		AssetChange.UnchangedBytes += InData.DiskSize;
	}

	FName ClassName = GetClassName(NewState, InAssetPath);

	ChangeInfoByAsset.FindOrAdd(InAssetPath) = AssetChange;
	ChangeSummaryByClass.FindOrAdd(ClassName) += AssetChange;
	ChangeSummary += AssetChange;
}

void UDiffAssetRegistriesCommandlet::LogChangedFiles()
{
	if (!bIsVerbose && !bSaveCSV)
	{
		return;
	}

	TArray<FName> AssetPaths;
	ChangeInfoByAsset.GetKeys(AssetPaths);

	// sort by size
	if (ReportedFileOrder == SortOrder::BySize)
	{
		// Sort by size of change
		AssetPaths.Sort([this](const FName& Lhs, const FName& Rhs) {
			return ChangeInfoByAsset[Lhs].GetTotalChangeSize() > ChangeInfoByAsset[Rhs].GetTotalChangeSize();
		});
	}
	// Sort by class type then size size
	else if (ReportedFileOrder == SortOrder::ByClass)
	{
		AssetPaths.Sort([this](const FName& Lhs, const FName& Rhs) {
			FString LhsName = GetClassName(NewState, Lhs).ToString();
			FString RhsName = GetClassName(NewState, Rhs).ToString();

			if (LhsName != RhsName)
			{
				return LhsName < RhsName;
			}

			return ChangeInfoByAsset[Lhs].GetTotalChangeSize() > ChangeInfoByAsset[Rhs].GetTotalChangeSize();
		});
	}
	// sort by change type then size
	else if (ReportedFileOrder == SortOrder::ByChange)
	{
		AssetPaths.Sort([this](const FName& Lhs, const FName& Rhs) {

			int32 LHSChanges = ChangeInfoByAsset[Lhs].GetChangeFlags();
			int32 RHSChanges = ChangeInfoByAsset[Rhs].GetChangeFlags();

			if (LHSChanges != RHSChanges)
			{
				return LHSChanges > RHSChanges;
			}

			// sort by size
			return ChangeInfoByAsset[Lhs].GetTotalChangeSize() > ChangeInfoByAsset[Rhs].GetTotalChangeSize();
		});
	}
	// sort by name
	else if (ReportedFileOrder == SortOrder::ByName)
	{
		AssetPaths.Sort([this](const FName& Lhs, const FName& Rhs) {
			return Lhs.ToString() < Rhs.ToString();
		});
	}

	FArchive* CSVFile = nullptr;

	if (bSaveCSV)
	{
		CSVFile = IFileManager::Get().CreateFileWriter(*CSVFilename);
		CSVFile->Logf(TEXT("Modification,Name,Class,NewSize,OldSize,Changelist"));

		UE_LOG(LogDiffAssets, Display, TEXT("Saving CSV results to %s"), *CSVFilename);
	}

	for (const FName AssetPath : AssetPaths)
	{
		const FChangeInfo& ChangeInfo = ChangeInfoByAsset[AssetPath];

		int Changelist = bMatchChangelists ? AssetPathToChangelist.FindOrAdd(AssetPath) : 0;
		
		FName ClassName;

		if (ChangeInfo.Deletes)
		{
			ClassName = GetClassName(OldState, AssetPath);
		}
		else
		{
			ClassName = GetClassName(NewState, AssetPath);
		}

		if (ChangeInfo.Adds)
		{
			if (CSVFile)
			{
				CSVFile->Logf(TEXT("a,%s,%s,%d,0,%d"), *AssetPath.ToString(), *ClassName.ToString(), ChangeInfo.AddedBytes, Changelist);
			}

			if (bIsVerbose)
			{
				UE_LOG(LogDiffAssets, Display, TEXT("a %s : (Class=%s,NewSize=%d bytes)"), *AssetPath.ToString(), *ClassName.ToString(), ChangeInfo.AddedBytes);
			}
		}
		else if (ChangeInfo.Changes)
		{
			const FAssetPackageData* PrevData = OldState.GetAssetPackageData(AssetPath);

			char classification;

			// classify the asset change by the flags
			int32 flags = AssetPathFlags.FindOrAdd(AssetPath);
			{
				bool hash = (flags & EAssetFlags::HashChange) != 0;
				bool guid = (flags & EAssetFlags::GuidChange) != 0;
				bool dephash = (flags & EAssetFlags::DepHashChange) != 0;
				bool depguid = (flags & EAssetFlags::DepGuidChange) != 0;

				if (!hash)
					classification = 'x'; // shouldn't see this in here, no binary change
				else 
				{
					if (guid)
						classification = 'e'; // explicit edit
					else if (dephash & depguid)
						classification = 'd'; // dependency edit
					else if (dephash & !depguid)
						classification = 'n'; // nondeterministic dependency
					else
						classification = 'c'; // nondeterministic
				}
			}

			if (CSVFile)
			{
				CSVFile->Logf(TEXT("%c,%s,%s,%d,%d,%d"), classification, *AssetPath.ToString(), *ClassName.ToString(), ChangeInfo.ChangedBytes, PrevData->DiskSize, Changelist);
			}

			if (bIsVerbose)
			{
				UE_LOG(LogDiffAssets, Display, TEXT("%c %s : (Class=%s,NewSize=%d bytes,OldSize=%d bytes)"), classification, *AssetPath.ToString(), *ClassName.ToString(), ChangeInfo.ChangedBytes, PrevData->DiskSize);
			}
			//UE_LOG(LogDiffAssets, Display, TEXT("Source file changed: %s"), (flags & EAssetFlags::GuidChange) ? TEXT("true") : TEXT("false"));
			if ((flags & EAssetFlags::GuidChange) && bMatchChangelists)
			{
				UE_LOG(LogDiffAssets, Display, TEXT("Last change: %d"), Changelist);
			}
#if 0			
			TArray<FAssetIdentifier> Dependencies;
			NewState.GetDependencies(AssetPath, Dependencies, EAssetRegistryDependencyType::Hard);
			if (Dependencies.Num() > 0)
			{
				UE_LOG(LogDiffAssets, Display, TEXT("Dependency changes:"));

				for (const FAssetIdentifier& Dependency : Dependencies)
				{
					if (AssetPathToSourceChanged.FindOrAdd(Dependency.PackageName))
					{
						UE_LOG(LogDiffAssets, Display, TEXT("  %s: %d"), *Dependency.PackageName.ToString(), AssetPathToChangelist.FindOrAdd(Dependency.PackageName));
					}
				}
			}
#endif
		}
		else if (ChangeInfo.Deletes)
		{
			const FAssetPackageData* PrevData = OldState.GetAssetPackageData(AssetPath);

			if (CSVFile)
			{
				CSVFile->Logf(TEXT("r,%s,%s,0,%d,0"), *AssetPath.ToString(), *ClassName.ToString(), PrevData->DiskSize);
			}

			if (bIsVerbose)
			{
				UE_LOG(LogDiffAssets, Display, TEXT("r %s : (Class=%s,OldSize=%d bytes)"), *AssetPath.ToString(), *ClassName.ToString(), PrevData->DiskSize);
			}
		}
	}

	delete CSVFile;
}

void UDiffAssetRegistriesCommandlet::DiffAssetRegistries(const FString& OldPath, const FString& NewPath, bool bUseSourceGuid, bool bEnginePackagesOnly)
{
	{
		FArrayReader SerializedAssetData;

		if (!IFileManager::Get().FileExists(*OldPath))
		{
			UE_LOG(LogDiffAssets, Error, TEXT("File '%s' does not exist."), *OldPath);
			return;
		}
		if (!FFileHelper::LoadFileToArray(SerializedAssetData, *OldPath))
		{
			UE_LOG(LogDiffAssets, Error, TEXT("Failed to load file '%s'."), *OldPath);
			return;
		}
		FAssetRegistrySerializationOptions Options;
		Options.ModifyForDevelopment();

		if (!OldState.Serialize(SerializedAssetData, Options))
		{
			UE_LOG(LogDiffAssets, Error, TEXT("Failed to parse file '%s' as asset registry."), *OldPath);
			return;
		}
	}
	{
		FArrayReader SerializedAssetData;

		if (!IFileManager::Get().FileExists(*NewPath))
		{
			UE_LOG(LogDiffAssets, Error, TEXT("File '%s' does not exist."), *NewPath);
			return;
		}
		if (!FFileHelper::LoadFileToArray(SerializedAssetData, *NewPath))
		{
			UE_LOG(LogDiffAssets, Error, TEXT("Failed to load file '%s'."), *NewPath);
			return;
		}
		FAssetRegistrySerializationOptions Options;
		Options.ModifyForDevelopment();

		if (!NewState.Serialize(SerializedAssetData, Options))
		{
			UE_LOG(LogDiffAssets, Error, TEXT("Failed to parse file '%s' as asset registry."), *NewPath);
			return;
		}
	}

	
	int64 newtotal = 0;
	int64 oldtotal = 0;
	int64 newuncooked = 0;
	int64 olduncooked = 0;
	int64 newassets = 0;
	int64 oldassets = 0;

	UE_LOG(LogDiffAssets, Display, TEXT("Comparing asset registries '%s' and '%s'."), *OldPath, *NewPath);
	if (bUseSourceGuid)
	{
		UE_LOG(LogDiffAssets, Display, TEXT("Source Package Diff"));
	}
	else
	{
		UE_LOG(LogDiffAssets, Display, TEXT("Cooked Package Diff"));
	}
	if (bIsVerbose)
	{
		UE_LOG(LogDiffAssets, Display, TEXT("Package changes:"));
	}

	TSet<FName> Modified;
	TSet<FName> New;

	if (bUseSourceGuid)
	{
		for (const TPair<FName, const FAssetPackageData*>& Pair : NewState.GetAssetPackageDataMap())
		{
			FName Name = Pair.Key;

			FString NameString = Name.ToString();

			if (bEnginePackagesOnly && !NameString.StartsWith(TEXT("/Engine/")))
			{
				continue;
			}

			if (!IsInRelevantChunk(NewState, Name))
			{
				continue;
			}

			const FAssetPackageData* Data = Pair.Value;
			const FAssetPackageData* PrevData = OldState.GetAssetPackageData(Name);
			
			if (Data->DiskSize < 0)
			{
				newuncooked++;
			}
			
			newassets += NewState.GetAssetsByPackageName(Name).Num();
			
			if (!PrevData)
			{
				New.Add(Name);
				RecordAdd(Name, *Data);
			}
			else if (Data->PackageGuid != PrevData->PackageGuid)
			{
				Modified.Add(Name);
			}
			else
			{
				RecordNoChange(Name, *Data);
			}
			++newtotal;
		}

		TArray<FName> Recurse = Modified.Array();

		for (int32 RecurseIndex = 0; RecurseIndex < Recurse.Num(); RecurseIndex++)
		{
			FName Package = Recurse[RecurseIndex];
			TArray<FAssetIdentifier> Referencers;
			NewState.GetReferencers(Package, Referencers, EAssetRegistryDependencyType::Hard);

			for (const FAssetIdentifier& Referencer : Referencers)
			{
				FName ReferencerPackage = Referencer.PackageName;
				if (!New.Contains(ReferencerPackage) && !Modified.Contains(ReferencerPackage))
				{
					Modified.Add(ReferencerPackage);
					Recurse.Add(ReferencerPackage);
				}
			}
		}

		const TMap<FName, const FAssetPackageData*>& PackageMap = NewState.GetAssetPackageDataMap();
		for (FName const &Package : Modified)
		{
			const FAssetPackageData* Data = PackageMap[Package];
			const FAssetPackageData* PrevData = OldState.GetAssetPackageData(Package);

			RecordEdit(Package, *Data,*PrevData);
		}
	}
	else
	{
		for (const TPair<FName, const FAssetPackageData*>& Pair : NewState.GetAssetPackageDataMap())
		{
			FName Name = Pair.Key;

			if (bEnginePackagesOnly && !Name.ToString().StartsWith(TEXT("/Engine/")))
			{
				continue;
			}

			if (!IsInRelevantChunk(NewState, Name))
			{
				continue;
			}

			const FAssetPackageData* Data = Pair.Value;
			const FAssetPackageData* PrevData = OldState.GetAssetPackageData(Name);
			
			if (Data->DiskSize < 0)
			{
				newuncooked++;
			}
			
			newassets += NewState.GetAssetsByPackageName(Name).Num();
			
			if (!PrevData)
			{
				RecordAdd(Name, *Data);
				AssetPathFlags.FindOrAdd(Name) |= EAssetFlags::Add;
			}
			else if (Data->CookedHash != PrevData->CookedHash)
			{
				RecordEdit(Name, *Data, *PrevData);
				AssetPathFlags.FindOrAdd(Name) |= EAssetFlags::HashChange;
				if (Data->PackageGuid != PrevData->PackageGuid)
				{
					AssetPathFlags.FindOrAdd(Name) |= EAssetFlags::GuidChange;
				}
			}
			else
			{
				RecordNoChange(Name, *Data);
			}
			newtotal++;
		}
	}

	for (const TPair<FName, const FAssetPackageData*>& Pair : OldState.GetAssetPackageDataMap())
	{
		FName Name = Pair.Key;

		FString NameString = Name.ToString();

		if (bEnginePackagesOnly && !NameString.StartsWith(TEXT("/Engine/")))
		{
			continue;
		}

		if (!IsInRelevantChunk(OldState, Name))
		{
			continue;
		}

		const FAssetPackageData* PrevData = Pair.Value;
		const FAssetPackageData* Data = NewState.GetAssetPackageData(Name);

		if (PrevData->DiskSize < 0)
		{
			olduncooked++;
		}

		oldassets += OldState.GetAssetsByPackageName(Name).Num();

		if (!Data)
		{
			RecordDelete(Name, *PrevData);
			AssetPathFlags.FindOrAdd(Name) |= EAssetFlags::Remove;
		}
		oldtotal++;
	}

	// Propagate hash/guid changes down through referencers
	{
		TArray<FName> Recurse;
		AssetPathFlags.GetKeys(Recurse);

		for (int32 RecurseIndex = 0; RecurseIndex < Recurse.Num(); RecurseIndex++)
		{
			FName Package = Recurse[RecurseIndex];
			
			TArray<FAssetIdentifier> Referencers;
			
			// grab the hash/guid change flags, shift up to the dependency ones
			int32 NewFlags = (AssetPathFlags.FindOrAdd(Package) & 0x0C) << 2;
			
			// don't bother touching anything if this asset didn't change
			if (NewFlags)
			{
				NewState.GetReferencers(Package, Referencers, EAssetRegistryDependencyType::Hard);

				for (const FAssetIdentifier& Referencer : Referencers)
				{
					FName RefPackage = Referencer.PackageName;
					
					// merge new dependency flags in, add to the list if something changed
					int32& RefFlags = AssetPathFlags.FindOrAdd(RefPackage);
					int32 OldFlags = RefFlags;
					RefFlags |= NewFlags;
					if (RefFlags != OldFlags)
					{
						Recurse.Add(RefPackage);
					}
				}
			}
		}
	}

	LogChangedFiles();

	// start summary
	UE_LOG(LogDiffAssets, Display, TEXT("Summary:"));
	UE_LOG(LogDiffAssets, Display, TEXT("Old AssetRegistry: %s"), *OldPath);
	UE_LOG(LogDiffAssets, Display, TEXT("%d packages total, %d uncooked, %d cooked assets"), oldtotal, olduncooked, oldassets);
	UE_LOG(LogDiffAssets, Display, TEXT("New AssetRegistry: %s"), *NewPath);

	const float InvToMB = 1.0 / (1024 * 1024);

	// show class totals first
	TArray<FName> ClassNames;
	ChangeSummaryByClass.GetKeys(ClassNames);

	// Sort keys by the desired order
	if (ReportedFileOrder == SortOrder::ByName || ReportedFileOrder == SortOrder::ByClass)
	{
		ClassNames.Sort([](const FName& Lhs, const FName& Rhs) {
			return Lhs.ToString() < Rhs.ToString();
		});
		
	}
	else // Default to size for everything else for class list
	{
		// sort by size of changes (number can also be a big impact on patch size but depends on datalayout and patch algo..)
		ClassNames.Sort([this](const FName& Lhs, const FName& Rhs) {
			const FChangeInfo& LHSChanges = ChangeSummaryByClass[Lhs];
			const FChangeInfo& RHSChanges = ChangeSummaryByClass[Rhs];
			return LHSChanges.GetTotalChangeSize() > RHSChanges.GetTotalChangeSize();
		});
	}
	
	for (FName ClassName : ClassNames)
	{
		const FChangeInfo& Changes = ChangeSummaryByClass[ClassName];

		if (Changes.GetTotalChangeSize() == 0)
		{
			continue;
		}

		if (Changes.GetTotalChangeCount() < MinChangeCount || Changes.GetTotalChangeSize() < (MinChangeSizeMB*1024*1024))
		{
			continue;
		}

		// log summary & change
		UE_LOG(LogDiffAssets, Display, TEXT("%s: %.02f%% changes (%.02f MB Total)"),
			*ClassName.ToString(), Changes.GetChangePercentage() * 100.0, Changes.GetTotalChangeSize() * InvToMB);

		if (Changes.Adds)
		{
			UE_LOG(LogDiffAssets, Display, TEXT("\t%d packages added,    %8.3f MB"), Changes.Adds, Changes.AddedBytes * InvToMB);
		}

		if (Changes.Changes)
		{
			UE_LOG(LogDiffAssets, Display, TEXT("\t%d packages modified, %8.3f MB"), Changes.Changes, Changes.ChangedBytes * InvToMB);
		}

		if (Changes.Deletes)
		{
			UE_LOG(LogDiffAssets, Display, TEXT("\t%d packages removed,  %8.3f MB"), Changes.Deletes, Changes.DeletedBytes * InvToMB);
		}

		UE_LOG(LogDiffAssets, Display, TEXT("\t%d packages unchanged,  %8.3f MB"), Changes.Unchanged, Changes.UnchangedBytes * InvToMB);
		
		// Warn on a certain % of changes if that's enabled
		if (Changes.Changes >= 10
			&& WarnPercentage > 0
			&& Changes.GetChangePercentage() * 100.0 > WarnPercentage)
		{
			UE_LOG(LogDiffAssets, Warning, TEXT("\t%s Assets for %s are %.02f%% changed. (%.02f MB of data)"),
				   *TargetPlatform, *ClassName.ToString(), Changes.GetChangePercentage() * 100.0, Changes.ChangedBytes * InvToMB);
		}
	}
#if 0
	TArray<int32> Changelists;
	ChangeSummaryByChangelist.GetKeys(Changelists);
	
	Changelists.Sort([this](const int32& Lhs, const int32& Rhs) {
			const FChangeInfo& LHSChanges = ChangeSummaryByChangelist[Lhs];
			const FChangeInfo& RHSChanges = ChangeSummaryByChangelist[Rhs];
			return LHSChanges.GetTotalChangeSize() > RHSChanges.GetTotalChangeSize();
		});
	
	for (int32 Changelist : Changelists)
	{
		const FChangeInfo& Changes = ChangeSummaryByChangelist[Changelist];
		
		if (Changes.GetTotalChangeSize() == 0)
		{
			continue;
		}
		
		if (Changes.GetTotalChangeCount() < MinChangeCount || Changes.GetTotalChangeSize() < (MinChangeSizeMB*1024*1024))
		{
			continue;
		}
		
		if (Changelist)
		{
			UE_LOG(LogDiffAssets, Display, TEXT("%d: %.02f%% changes (%.02f MB Total)"),
				   Changelist, Changes.GetChangePercentage() * 100.0, Changes.GetTotalChangeSize() * InvToMB);
		}
		else
		{
			UE_LOG(LogDiffAssets, Display, TEXT("Unattributable (nondeterministic?): %.02f%% changes (%.02f MB Total)"),
				   Changes.GetChangePercentage() * 100.0, Changes.GetTotalChangeSize() * InvToMB);
		}
		
		if (Changes.Adds)
		{
			UE_LOG(LogDiffAssets, Display, TEXT("\t%d packages added,    %8.3f MB"), Changes.Adds, Changes.AddedBytes * InvToMB);
		}
		
		if (Changes.Changes)
		{
			UE_LOG(LogDiffAssets, Display, TEXT("\t%d packages modified, %8.3f MB"), Changes.Changes, Changes.ChangedBytes * InvToMB);
		}
		
		if (Changes.Deletes)
		{
			UE_LOG(LogDiffAssets, Display, TEXT("\t%d packages removed,  %8.3f MB"), Changes.Deletes, Changes.DeletedBytes * InvToMB);
		}
	}
#endif
	// these are parsed by scripts, so please don't modify :)
	UE_LOG(LogDiffAssets, Display, TEXT("%d total packages, %d uncooked, %d cooked assets"), newtotal, newuncooked, newassets);
	UE_LOG(LogDiffAssets, Display, TEXT("%d total unchanged,         %8.3f MB"), ChangeSummary.Unchanged, ChangeSummary.UnchangedBytes * InvToMB);
	UE_LOG(LogDiffAssets, Display, TEXT("%d total packages added,    %8.3f MB"), ChangeSummary.Adds, ChangeSummary.AddedBytes * InvToMB);
	UE_LOG(LogDiffAssets, Display, TEXT("%d total packages modified, %8.3f MB"), ChangeSummary.Changes, ChangeSummary.ChangedBytes * InvToMB);
	UE_LOG(LogDiffAssets, Display, TEXT("%d total packages removed,  %8.3f MB"), ChangeSummary.Deletes, ChangeSummary.DeletedBytes * InvToMB);
}

bool UDiffAssetRegistriesCommandlet::LaunchP4(const FString& Args, TArray<FString>& Output, int32& OutReturnCode) const
{
	void* PipeRead = nullptr;
	void* PipeWrite = nullptr;

	verify(FPlatformProcess::CreatePipe(PipeRead, PipeWrite));

	bool bInvoked = false;
	OutReturnCode = -1;
	FString StringOutput;
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(TEXT("p4.exe"), *Args, false, true, true, nullptr, 0, nullptr, PipeWrite);
	if (ProcHandle.IsValid())
	{
		while (FPlatformProcess::IsProcRunning(ProcHandle))
		{
			StringOutput += FPlatformProcess::ReadPipe(PipeRead);
			FPlatformProcess::Sleep(0.1f);
		}

		StringOutput += FPlatformProcess::ReadPipe(PipeRead);
		FPlatformProcess::GetProcReturnCode(ProcHandle, &OutReturnCode);
		bInvoked = true;
	}
	else
	{
		UE_LOG(LogDiffAssets, Error, TEXT("Failed to launch p4."));
	}

	FPlatformProcess::ClosePipe(PipeRead, PipeWrite);

	StringOutput.ParseIntoArrayLines(Output);

	return bInvoked;
}
