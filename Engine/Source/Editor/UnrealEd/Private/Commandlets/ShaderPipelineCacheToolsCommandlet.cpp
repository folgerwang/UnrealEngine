// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/ShaderPipelineCacheToolsCommandlet.h"
#include "Misc/Paths.h"

#include "PipelineFileCache.h"
#include "ShaderCodeLibrary.h"
#include "Misc/FileHelper.h"
#include "ShaderPipelineCache.h"

DEFINE_LOG_CATEGORY_STATIC(LogShaderPipelineCacheTools, Log, All);


void ExpandWildcards(TArray<FString>& Parts)
{
	TArray<FString> NewParts;
	for (const FString& OldPart : Parts)
	{
		if (OldPart.Contains(TEXT("*")) || OldPart.Contains(TEXT("?")))
		{
			TArray<FString> ExpandedFiles;
			IFileManager::Get().FindFilesRecursive(ExpandedFiles, *FPaths::GetPath(OldPart), *FPaths::GetCleanFilename(OldPart), true, false);
			UE_CLOG(!ExpandedFiles.Num(), LogShaderPipelineCacheTools, Warning, TEXT("Expanding %s....did not match anything."), *OldPart);
			UE_CLOG(ExpandedFiles.Num(), LogShaderPipelineCacheTools, Log, TEXT("Expanding matched %4d files: %s"), ExpandedFiles.Num(), *OldPart);
			for (const FString& Item : ExpandedFiles)
			{
				UE_LOG(LogShaderPipelineCacheTools, Log, TEXT("                             : %s"), *Item);
				NewParts.Add(Item);
			}
		}
		else
		{
			NewParts.Add(OldPart);
		}
	}
	Parts = NewParts;
}


void LoadStableSCL(TMultiMap<FStableShaderKeyAndValue, FSHAHash>& StableMap, const FString& Filename)
{
	UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Loading %s...."), *Filename);
	TArray<FString> SourceFileContents;

	if (FFileHelper::LoadFileToStringArray(SourceFileContents, *Filename))
	{
		StableMap.Reserve(StableMap.Num() + SourceFileContents.Num() - 1);
		for (int32 Index = 1; Index < SourceFileContents.Num(); Index++)
		{
			FStableShaderKeyAndValue Item;
			FMemory::Memzero(Item);
			Item.ParseFromString(SourceFileContents[Index]);
			check(!(Item.OutputHash == FSHAHash()));
			StableMap.AddUnique(Item, Item.OutputHash);
		}
	}
	if (SourceFileContents.Num() < 1)
	{
		UE_LOG(LogShaderPipelineCacheTools, Fatal, TEXT("Could not load %s"), *Filename);
		return;
	}
	UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Loaded %d shader info lines"), SourceFileContents.Num() - 1);
}


static void PrintShaders(const TMap<FSHAHash, TArray<FString>>& InverseMap, const FSHAHash& Shader)
{
	if (Shader == FSHAHash())
	{
		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("    null"));
		return;
	}
	const TArray<FString>* Out = InverseMap.Find(Shader);
	if (!Out)
	{
		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("    No shaders found with hash %s"), *Shader.ToString());
		return;
	}

	for (const FString& Item : *Out)
	{
		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("    %s"), *Item);
	}
}

void CheckPSOStringInveribility(const FPipelineCacheFileFormatPSO& Item)
{
	FPipelineCacheFileFormatPSO TempItem(Item);
	TempItem.Hash = 0;

	FString StringRep;
	if (Item.Type == FPipelineCacheFileFormatPSO::DescriptorType::Compute)
	{
		StringRep = TempItem.ComputeDesc.ToString();
	}
	else
	{
		StringRep = TempItem.GraphicsDesc.ToString();
	}
	FPipelineCacheFileFormatPSO DupItem;
	FMemory::Memzero(DupItem.GraphicsDesc);
	DupItem.Type = Item.Type;
	if (Item.Type == FPipelineCacheFileFormatPSO::DescriptorType::Compute)
	{
		DupItem.ComputeDesc.FromString(StringRep);
	}
	else
	{
		DupItem.GraphicsDesc.FromString(StringRep);
	}
	UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("CheckPSOStringInveribility: %s"), *StringRep);

	check(DupItem == TempItem);
	check(GetTypeHash(DupItem) == GetTypeHash(TempItem));
}

int32 DumpPSOSC(FString& Token)
{
	TSet<FPipelineCacheFileFormatPSO> PSOs;

	UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Loading %s...."), *Token);
	if (!FPipelineFileCache::LoadPipelineFileCacheInto(Token, PSOs))
	{
		UE_LOG(LogShaderPipelineCacheTools, Error, TEXT("Could not load %s or it was empty."), *Token);
		return 1;
	}

	for (const FPipelineCacheFileFormatPSO& Item : PSOs)
	{
		FString StringRep;
		if (Item.Type == FPipelineCacheFileFormatPSO::DescriptorType::Compute)
		{
			check(!(Item.ComputeDesc.ComputeShader == FSHAHash()));
			StringRep = Item.ComputeDesc.ToString();
		}
		else
		{
			check(!(Item.GraphicsDesc.VertexShader == FSHAHash()));
			StringRep = Item.GraphicsDesc.ToString();
		}
		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("%s"), *StringRep);
	}
	UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("%s"), *FPipelineCacheFileFormatPSO::GraphicsDescriptor::HeaderLine());

	for (const FPipelineCacheFileFormatPSO& Item : PSOs)
	{
		CheckPSOStringInveribility(Item);
	}

	return 0;
}

static void PrintShaders(const TMap<FSHAHash, TArray<FStableShaderKeyAndValue>>& InverseMap, const FSHAHash& Shader, const TCHAR *Label)
{
	UE_LOG(LogShaderPipelineCacheTools, Display, TEXT(" -- %s"), Label);

	if (Shader == FSHAHash())
	{
		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("    null"));
		return;
	}
	const TArray<FStableShaderKeyAndValue>* Out = InverseMap.Find(Shader);
	if (!Out)
	{
		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("    No shaders found with hash %s"), *Shader.ToString());
		return;
	}
	for (const FStableShaderKeyAndValue& Item : *Out)
	{
		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("    %s"), *Item.ToString());
	}
}

static bool GetStableShadersAndZeroHash(const TMap<FSHAHash, TArray<FStableShaderKeyAndValue>>& InverseMap, const FSHAHash& Shader, TArray<FStableShaderKeyAndValue>& StableShaders, bool& bOutAnyActiveButMissing)
{
	if (Shader == FSHAHash())
	{
		return false;
	}
	const TArray<FStableShaderKeyAndValue>* Out = InverseMap.Find(Shader);
	if (!Out)
	{
		UE_LOG(LogShaderPipelineCacheTools, Warning, TEXT("No shaders found with hash %s"), *Shader.ToString());
		UE_LOG(LogShaderPipelineCacheTools, Warning, TEXT("If you can find the old .scl.csv file for this build, adding it will allow these PSOs to be usable."));
		bOutAnyActiveButMissing = true;
		return false;
	}
	StableShaders.Reserve(Out->Num());
	for (const FStableShaderKeyAndValue& Item : *Out)
	{
		FStableShaderKeyAndValue Temp = Item;
		Temp.OutputHash = FSHAHash();
		if (StableShaders.Contains(Temp))
		{
			UE_LOG(LogShaderPipelineCacheTools, Error, TEXT("Duplicate stable shader. This is bad because it means our stable key is not exhaustive."));
			UE_LOG(LogShaderPipelineCacheTools, Error, TEXT(" %s"), *Item.ToString());
			continue;
		}
		StableShaders.Add(Temp);
	}
	return true;
}

// return true if these two shaders could be part of the same stable PSO
// for example, if they come from two different vertex factories, we return false because that situation cannot occur
bool CouldBeUsedTogether(const FStableShaderKeyAndValue& A, const FStableShaderKeyAndValue& B)
{
	static FName NAME_FDeferredDecalVS("FDeferredDecalVS");
	static FName NAME_FWriteToSliceVS("FWriteToSliceVS");
	static FName NAME_FPostProcessVS("FPostProcessVS");
	if (
		A.ShaderType == NAME_FDeferredDecalVS || B.ShaderType == NAME_FDeferredDecalVS ||
		A.ShaderType == NAME_FWriteToSliceVS || B.ShaderType == NAME_FWriteToSliceVS ||
		A.ShaderType == NAME_FPostProcessVS || B.ShaderType == NAME_FPostProcessVS
		)
	{
		// oddball mix and match with any material shader.
		return true;
	}
	if (A.ShaderClass != B.ShaderClass)
	{
		return false;
	}
	if (A.VFType != B.VFType)
	{
		return false;
	}
	if (A.FeatureLevel != B.FeatureLevel)
	{
		return false;
	}
	if (A.QualityLevel != B.QualityLevel)
	{
		return false;
	}
	if (A.TargetPlatform != B.TargetPlatform)
	{
		return false;
	}
	if (!(A.ClassNameAndObjectPath == B.ClassNameAndObjectPath))
	{
		return false;
	}
	return true;
}

int32 DumpSCLCSV(const FString& Token)
{

	TMultiMap<FStableShaderKeyAndValue, FSHAHash> StableMap;
	LoadStableSCL(StableMap, Token);
	UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("    %s"), *FStableShaderKeyAndValue::HeaderLine());
	for (const auto& Pair : StableMap)
	{
		FStableShaderKeyAndValue Temp(Pair.Key);
		Temp.OutputHash = Pair.Value;
		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("    %s"), *Temp.ToString());
	}
	return 0;
}

void IntersectSets(TSet<FCompactFullName>& Intersect, const TSet<FCompactFullName>& ShaderAssets)
{
	if (!Intersect.Num() && ShaderAssets.Num())
	{
		Intersect = ShaderAssets;
	}
	else if (Intersect.Num() && ShaderAssets.Num())
	{
		Intersect  = Intersect.Intersect(ShaderAssets);
	}
}

struct FPermuation
{
	FStableShaderKeyAndValue Slots[SF_NumFrequencies];
};

void GeneratePermuations(TArray<FPermuation>& Permutations, FPermuation& WorkingPerm, int32 SlotIndex , const TArray<FStableShaderKeyAndValue> StableShadersPerSlot[SF_NumFrequencies], const bool ActivePerSlot[SF_NumFrequencies])
{
	check(SlotIndex >= 0 && SlotIndex <= SF_NumFrequencies);
	while (SlotIndex < SF_NumFrequencies && !ActivePerSlot[SlotIndex])
	{
		SlotIndex++;
	}
	if (SlotIndex >= SF_NumFrequencies)
	{
		Permutations.Add(WorkingPerm);
		return;
	}
	for (int32 StableIndex = 0; StableIndex < StableShadersPerSlot[SlotIndex].Num(); StableIndex++)
	{
		bool bKeep = true;
		// check compatibility with shaders in the working perm
		for (int32 SlotIndexInner = 0; SlotIndexInner < SlotIndex; SlotIndexInner++)
		{
			if (SlotIndex == SlotIndexInner || !ActivePerSlot[SlotIndexInner])
			{
				continue;
			}
			check(SlotIndex != SF_Compute && SlotIndexInner != SF_Compute); // there is never any matching with compute shaders
			if (!CouldBeUsedTogether(StableShadersPerSlot[SlotIndex][StableIndex], WorkingPerm.Slots[SlotIndexInner]))
			{
				bKeep = false;
				break;
			}
		}
		if (!bKeep)
		{
			continue;
		}
		WorkingPerm.Slots[SlotIndex] = StableShadersPerSlot[SlotIndex][StableIndex];
		GeneratePermuations(Permutations, WorkingPerm, SlotIndex + 1, StableShadersPerSlot, ActivePerSlot);
	}
}

int32 ExpandPSOSC(const TArray<FString>& Tokens)
{
	TMultiMap<FStableShaderKeyAndValue, FSHAHash> StableMap;
	check(Tokens.Last().EndsWith(TEXT(".stablepc.csv")));
	for (int32 Index = 0; Index < Tokens.Num() - 1; Index++)
	{
		if (Tokens[Index].EndsWith(TEXT(".scl.csv")))
		{
			LoadStableSCL(StableMap, Tokens[Index]);
		}
	}
	if (!StableMap.Num())
	{
		UE_LOG(LogShaderPipelineCacheTools, Warning, TEXT("No .scl.csv found or they were all empty. Nothing to do."));
		return 0;
	}
	if (UE_LOG_ACTIVE(LogShaderPipelineCacheTools, Verbose))
	{
		UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("    %s"), *FStableShaderKeyAndValue::HeaderLine());
		for (const auto& Pair : StableMap)
		{
			FStableShaderKeyAndValue Temp(Pair.Key);
			Temp.OutputHash = Pair.Value;
			UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("    %s"), *Temp.ToString());
		}
	}
	//self test
	for (const auto& Pair : StableMap)
	{
		FStableShaderKeyAndValue Item(Pair.Key);
		Item.OutputHash = Pair.Value;
		check(Pair.Value != FSHAHash());
		FString TestString = Item.ToString();
		FStableShaderKeyAndValue TestItem;
		TestItem.ParseFromString(TestString);
		check(Item == TestItem);
		check(GetTypeHash(Item) == GetTypeHash(TestItem));
		check(Item.OutputHash == TestItem.OutputHash);
	}
	// end self test
	UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Loaded %d unique shader info lines total."), StableMap.Num());

	TSet<FPipelineCacheFileFormatPSO> PSOs;

	for (int32 Index = 0; Index < Tokens.Num() - 1; Index++)
	{
		if (Tokens[Index].EndsWith(TEXT(".upipelinecache")))
		{
			UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Loading %s...."), *Tokens[Index]);
			TSet<FPipelineCacheFileFormatPSO> TempPSOs;
			if (!FPipelineFileCache::LoadPipelineFileCacheInto(Tokens[Index], TempPSOs))
			{
				UE_LOG(LogShaderPipelineCacheTools, Error, TEXT("Could not load %s or it was empty."), *Tokens[Index]);
				continue;
			}
			UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Loaded %d PSOs"), TempPSOs.Num());
			PSOs.Append(TempPSOs);

		}
		else
		{
			check(Tokens[Index].EndsWith(TEXT(".scl.csv")));
		}
	}
	if (!PSOs.Num())
	{
		UE_LOG(LogShaderPipelineCacheTools, Warning, TEXT("No .upipelinecache files found or they were all empty. Nothing to do."));
		return 0;
	}
	UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Loaded %d PSOs total."), PSOs.Num());

	//self test
	for (const FPipelineCacheFileFormatPSO& Item : PSOs)
	{
		CheckPSOStringInveribility(Item);
	}
	// end self test
	if (UE_LOG_ACTIVE(LogShaderPipelineCacheTools, Verbose))
	{
		TMap<FSHAHash, TArray<FString>> InverseMap;

		for (const auto& Pair : StableMap)
		{
			FStableShaderKeyAndValue Temp(Pair.Key);
			Temp.OutputHash = Pair.Value;
			InverseMap.FindOrAdd(Pair.Value).Add(Temp.ToString());
		}

		for (const FPipelineCacheFileFormatPSO& Item : PSOs)
		{
			if (Item.Type == FPipelineCacheFileFormatPSO::DescriptorType::Compute)
			{
				UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("ComputeShader"));
				PrintShaders(InverseMap, Item.ComputeDesc.ComputeShader);
			}
			else
			{
				UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("VertexShader"));
				PrintShaders(InverseMap, Item.GraphicsDesc.VertexShader);
				UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("FragmentShader"));
				PrintShaders(InverseMap, Item.GraphicsDesc.FragmentShader);
				UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("GeometryShader"));
				PrintShaders(InverseMap, Item.GraphicsDesc.GeometryShader);
				UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("HullShader"));
				PrintShaders(InverseMap, Item.GraphicsDesc.HullShader);
				UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("DomainShader"));
				PrintShaders(InverseMap, Item.GraphicsDesc.DomainShader);
			}
		}
	}
	TMap<FSHAHash, TArray<FStableShaderKeyAndValue>> InverseMap;

	for (const auto& Pair : StableMap)
	{
		FStableShaderKeyAndValue Item(Pair.Key);
		Item.OutputHash = Pair.Value;
		InverseMap.FindOrAdd(Item.OutputHash).AddUnique(Item);
	}

	int32 TotalStablePSOs = 0;

	struct FPermsPerPSO
	{
		const FPipelineCacheFileFormatPSO* PSO;
		bool ActivePerSlot[SF_NumFrequencies];
		TArray<FPermuation> Permutations;

		FPermsPerPSO()
			: PSO(nullptr)
		{
			for (int32 Index = 0; Index < SF_NumFrequencies; Index++)
			{
				ActivePerSlot[Index] = false;
			}
		}
	};

	TArray<FPermsPerPSO> StableResults;
	StableResults.Reserve(PSOs.Num());
	int32 NumSkipped = 0;
	int32 NumExamined = 0;

	for (const FPipelineCacheFileFormatPSO& Item : PSOs)
	{ 
		NumExamined++;
		check(SF_Vertex == 0 && SF_Compute == 5);
		TArray<FStableShaderKeyAndValue> StableShadersPerSlot[SF_NumFrequencies];
		bool ActivePerSlot[SF_NumFrequencies] = { false };

		bool OutAnyActiveButMissing = false;

		if (Item.Type == FPipelineCacheFileFormatPSO::DescriptorType::Compute)
		{
			ActivePerSlot[SF_Compute] = GetStableShadersAndZeroHash(InverseMap, Item.ComputeDesc.ComputeShader, StableShadersPerSlot[SF_Compute], OutAnyActiveButMissing);
		}
		else
		{
			ActivePerSlot[SF_Vertex] = GetStableShadersAndZeroHash(InverseMap, Item.GraphicsDesc.VertexShader, StableShadersPerSlot[SF_Vertex], OutAnyActiveButMissing);
			ActivePerSlot[SF_Pixel] = GetStableShadersAndZeroHash(InverseMap, Item.GraphicsDesc.FragmentShader, StableShadersPerSlot[SF_Pixel], OutAnyActiveButMissing);
			ActivePerSlot[SF_Geometry] = GetStableShadersAndZeroHash(InverseMap, Item.GraphicsDesc.GeometryShader, StableShadersPerSlot[SF_Geometry], OutAnyActiveButMissing);
			ActivePerSlot[SF_Hull] = GetStableShadersAndZeroHash(InverseMap, Item.GraphicsDesc.HullShader, StableShadersPerSlot[SF_Hull], OutAnyActiveButMissing);
			ActivePerSlot[SF_Domain] = GetStableShadersAndZeroHash(InverseMap, Item.GraphicsDesc.DomainShader, StableShadersPerSlot[SF_Domain], OutAnyActiveButMissing);
		}


		if (OutAnyActiveButMissing)
		{
			UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("PSO had an active shader slot that did not match any current shaders, ignored."));
			if (Item.Type == FPipelineCacheFileFormatPSO::DescriptorType::Compute)
			{
				PrintShaders(InverseMap, Item.ComputeDesc.ComputeShader, TEXT("ComputeShader"));
			}
			else
			{
				UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("   %s"), *Item.GraphicsDesc.StateToString());
				PrintShaders(InverseMap, Item.GraphicsDesc.VertexShader, TEXT("VertexShader"));
				PrintShaders(InverseMap, Item.GraphicsDesc.FragmentShader, TEXT("FragmentShader"));
				PrintShaders(InverseMap, Item.GraphicsDesc.GeometryShader, TEXT("GeometryShader"));
				PrintShaders(InverseMap, Item.GraphicsDesc.HullShader, TEXT("HullShader"));
				PrintShaders(InverseMap, Item.GraphicsDesc.DomainShader, TEXT("DomainShader"));
			}
			continue;
		}
		if (Item.Type != FPipelineCacheFileFormatPSO::DescriptorType::Compute)
		{
			check(!ActivePerSlot[SF_Compute]); // this is NOT a compute shader
			bool bRemovedAll = false;
			bool bAnyActive = false;
			// Quite the nested loop. It isn't clear if this could be made faster, but the thing to realize is that the same set of shaders will be used in multiple PSOs we could take advantage of that...we don't.
			for (int32 SlotIndex = 0; SlotIndex < SF_NumFrequencies; SlotIndex++)
			{
				if (!ActivePerSlot[SlotIndex])
				{
					check(!StableShadersPerSlot[SlotIndex].Num());
					continue;
				}
				bAnyActive = true;
				for (int32 StableIndex = 0; StableIndex < StableShadersPerSlot[SlotIndex].Num(); StableIndex++)
				{
					bool bKeep = true;
					for (int32 SlotIndexInner = 0; SlotIndexInner < SF_Compute; SlotIndexInner++) //SF_Compute here because this is NOT a compute shader
					{
						if (SlotIndex == SlotIndexInner || !ActivePerSlot[SlotIndexInner])
						{
							continue;
						}
						bool bFoundCompat = false;
						for (int32 StableIndexInner = 0; StableIndexInner < StableShadersPerSlot[SlotIndexInner].Num(); StableIndexInner++)
						{
							if (CouldBeUsedTogether(StableShadersPerSlot[SlotIndex][StableIndex], StableShadersPerSlot[SlotIndexInner][StableIndexInner]))
							{
								bFoundCompat = true;
								break;
							}
						}
						if (!bFoundCompat)
						{
							bKeep = false;
							break;
						}
					}
					if (!bKeep)
					{
						StableShadersPerSlot[SlotIndex].RemoveAt(StableIndex--);
					}
				}
				if (!StableShadersPerSlot[SlotIndex].Num())
				{
					bRemovedAll = true;
				}
			}
			if (!bAnyActive)
			{
				NumSkipped++;
				UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("PSO did not create any stable PSOs! (no active shader slots)"));
				UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("   %s"), *Item.GraphicsDesc.StateToString());
				continue;
			}
			if (bRemovedAll)
			{
				UE_LOG(LogShaderPipelineCacheTools, Warning, TEXT("PSO did not create any stable PSOs! (no cross shader slot compatibility)"));
				UE_LOG(LogShaderPipelineCacheTools, Warning, TEXT("   %s"), *Item.GraphicsDesc.StateToString());

				PrintShaders(InverseMap, Item.GraphicsDesc.VertexShader, TEXT("VertexShader"));
				PrintShaders(InverseMap, Item.GraphicsDesc.FragmentShader, TEXT("FragmentShader"));
				PrintShaders(InverseMap, Item.GraphicsDesc.GeometryShader, TEXT("GeometryShader"));
				PrintShaders(InverseMap, Item.GraphicsDesc.HullShader, TEXT("HullShader"));
				PrintShaders(InverseMap, Item.GraphicsDesc.DomainShader, TEXT("DomainShader"));

				continue;
			}
			// We could have done this on the fly, but that loop was already pretty complicated. Here we generate all plausible permutations and write them out
		}

		StableResults.AddDefaulted();
		FPermsPerPSO& Current = StableResults.Last();
		Current.PSO = &Item;

		for (int32 Index = 0; Index < SF_NumFrequencies; Index++)
		{
			Current.ActivePerSlot[Index] = ActivePerSlot[Index];
		}

		TArray<FPermuation>& Permutations(Current.Permutations);
		FPermuation WorkingPerm;
		GeneratePermuations(Permutations, WorkingPerm, 0, StableShadersPerSlot, ActivePerSlot);
		if (!Permutations.Num())
		{
			UE_LOG(LogShaderPipelineCacheTools, Error, TEXT("PSO did not create any stable PSOs! (somehow)"));
			// this is fatal because now we have a bogus thing in the list
			UE_LOG(LogShaderPipelineCacheTools, Fatal, TEXT("   %s"), *Item.GraphicsDesc.StateToString());
			continue;
		}

		UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("----- PSO created %d stable permutations --------------"), Permutations.Num());
		TotalStablePSOs += Permutations.Num();
	}
	UE_CLOG(NumSkipped > 0, LogShaderPipelineCacheTools, Warning, TEXT("%d/%d PSO did not create any stable PSOs! (no active shader slots)"), NumSkipped, NumExamined);
	UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Generated %d stable PSOs total"), TotalStablePSOs);
	if (!TotalStablePSOs || !StableResults.Num())
	{
		UE_LOG(LogShaderPipelineCacheTools, Error, TEXT("No stable PSOs created."));
		return 1;
	}

	TArray<FString> OutputLines;
	TSet<FString> DeDup;

	{
		FString PSOLine = FString::Printf(TEXT("\"%s\""), *FPipelineCacheFileFormatPSO::GraphicsDescriptor::StateHeaderLine());
		for (int32 SlotIndex = 0; SlotIndex < SF_Compute; SlotIndex++) // SF_Compute here because the stablepc.csv file format does not have a compute slot
		{
			PSOLine += FString::Printf(TEXT(",\"shaderslot%d: %s\""), SlotIndex, *FStableShaderKeyAndValue::HeaderLine());
		}
		OutputLines.Add(PSOLine);
	}

	for (const FPermsPerPSO& Item : StableResults)
	{
		if (UE_LOG_ACTIVE(LogShaderPipelineCacheTools, Verbose))
		{
			if (Item.PSO->Type == FPipelineCacheFileFormatPSO::DescriptorType::Compute)
			{
				UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT(" Compute"));
			}
			else
			{
				UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT(" %s"), *Item.PSO->GraphicsDesc.StateToString());
			}
			int32 PermIndex = 0;
			for (const FPermuation& Perm : Item.Permutations)
			{
				UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("  ----- perm %d"), PermIndex);
				for (int32 SlotIndex = 0; SlotIndex < SF_NumFrequencies; SlotIndex++)
				{
					if (!Item.ActivePerSlot[SlotIndex])
					{
						continue;
					}
					UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("   %s"), *Perm.Slots[SlotIndex].ToString());
				}
				PermIndex++;
			}

			UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("-----"));
		}
		for (const FPermuation& Perm : Item.Permutations)
		{
			// because it is a CSV, and for backward compat, compute shaders will just be a zeroed graphics desc with the shader in the hull shader slot.
			FString PSOLine;

			if (Item.PSO->Type == FPipelineCacheFileFormatPSO::DescriptorType::Compute)
			{
				FPipelineCacheFileFormatPSO::GraphicsDescriptor Zero;
				FMemory::Memzero(Zero);
				PSOLine = FString::Printf(TEXT("\"%s\""), *Zero.StateToString());
				for (int32 SlotIndex = 0; SlotIndex < SF_Compute; SlotIndex++)  // SF_Compute here because the stablepc.csv file format does not have a compute slot
				{
					check(!Item.ActivePerSlot[SlotIndex]); // none of these should be active for a compute shader
					if (SlotIndex == SF_Hull)
					{
						PSOLine += FString::Printf(TEXT(",\"%s\""), *Perm.Slots[SF_Compute].ToString());
					}
					else
					{
						PSOLine += FString::Printf(TEXT(",\"\""));
					}
				}
			}
			else
			{
				PSOLine = FString::Printf(TEXT("\"%s\""), *Item.PSO->GraphicsDesc.StateToString());
				for (int32 SlotIndex = 0; SlotIndex < SF_Compute; SlotIndex++) // SF_Compute here because the stablepc.csv file format does not have a compute slot
				{
					if (!Item.ActivePerSlot[SlotIndex])
					{
						PSOLine += FString::Printf(TEXT(",\"\""));
						continue;
					}
					PSOLine += FString::Printf(TEXT(",\"%s\""), *Perm.Slots[SlotIndex].ToString());
				}
			}


			if (!DeDup.Contains(PSOLine))
			{
				DeDup.Add(PSOLine); 
				OutputLines.Add(PSOLine);
			}
		}
	}

	if (IFileManager::Get().FileExists(*Tokens.Last()))
	{
		IFileManager::Get().Delete(*Tokens.Last(), false, true);
	}
	if (IFileManager::Get().FileExists(*Tokens.Last()))
	{
		UE_LOG(LogShaderPipelineCacheTools, Fatal, TEXT("Could not delete %s"), *Tokens.Last());
	}
	FFileHelper::SaveStringArrayToFile(OutputLines, *Tokens.Last());
	int64 Size = IFileManager::Get().FileSize(*Tokens.Last());
	if (Size < 1)
	{
		UE_LOG(LogShaderPipelineCacheTools, Fatal, TEXT("Failed to write %s"), *Tokens.Last());
	}
	UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Wrote stable PSOs, %d lines (%lldKB) to %s"), OutputLines.Num(), (Size + 1023) / 1024, *Tokens.Last());

	return 0;
}

void ParseQuoteComma(const FString& InLine, TArray<FString>& OutParts)
{
	FString Line = InLine;
	while (true)
	{
		int32 QuoteLoc = 0;
		if (!Line.FindChar(TCHAR('\"'), QuoteLoc))
		{
			break;
		}
		Line = Line.RightChop(QuoteLoc + 1);
		if (!Line.FindChar(TCHAR('\"'), QuoteLoc))
		{
			break;
		}
		OutParts.Add(Line.Left(QuoteLoc));
		Line = Line.RightChop(QuoteLoc + 1);
	}
}


int32 BuildPSOSC(const TArray<FString>& Tokens)
{
	TMultiMap<FStableShaderKeyAndValue, FSHAHash> StableMap;
	check(Tokens.Last().EndsWith(TEXT(".upipelinecache")));

	for (int32 Index = 0; Index < Tokens.Num() - 1; Index++)
	{
		if (Tokens[Index].EndsWith(TEXT(".scl.csv")))
		{
			LoadStableSCL(StableMap, Tokens[Index]);
		}
	}
	if (UE_LOG_ACTIVE(LogShaderPipelineCacheTools, Verbose))
	{
		UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("    %s"), *FStableShaderKeyAndValue::HeaderLine());
		for (const auto& Pair : StableMap)
		{
			FStableShaderKeyAndValue Temp(Pair.Key);
			Temp.OutputHash = Pair.Value;
			UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("    %s"), *Temp.ToString());
		}
	}
	//self test
	for (const auto& Pair : StableMap)
	{
		FStableShaderKeyAndValue Item(Pair.Key);
		Item.OutputHash = Pair.Value;
		check(Pair.Value != FSHAHash());
		FString TestString = Item.ToString();
		FStableShaderKeyAndValue TestItem;
		TestItem.ParseFromString(TestString);
		check(Item == TestItem);
		check(GetTypeHash(Item) == GetTypeHash(TestItem));
		check(Item.OutputHash == TestItem.OutputHash);
	}
	// end self test
	UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Loaded %d unique shader info lines total."), StableMap.Num());

	TSet<FPipelineCacheFileFormatPSO> PSOs;
	FName TargetPlatform;


	for (int32 TokenIndex = 0; TokenIndex < Tokens.Num() - 1; TokenIndex++)
	{
		if (!Tokens[TokenIndex].EndsWith(TEXT(".stablepc.csv")))
		{
			check(Tokens[TokenIndex].EndsWith(TEXT(".scl.csv")));
			continue;
		}

		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Loading %s...."), *Tokens[TokenIndex]);
		TArray<FString> SourceFileContents;

		if (!FFileHelper::LoadFileToStringArray(SourceFileContents, *Tokens[TokenIndex]) || SourceFileContents.Num() < 2)
		{
			UE_LOG(LogShaderPipelineCacheTools, Fatal, TEXT("Could not load %s"), *Tokens[TokenIndex]);
			return 1;
		}

		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Loaded %d stable PSO lines."), SourceFileContents.Num() - 1);

		for (int32 Index = 1; Index < SourceFileContents.Num(); Index++)
		{
			TArray<FString> Parts;
			ParseQuoteComma(SourceFileContents[Index], Parts);
			check(Parts.Num() == 1 + SF_Compute); // SF_Compute here because the stablepc.csv file format does not have a compute slot

			FPipelineCacheFileFormatPSO PSO;
			FMemory::Memzero(PSO);
			PSO.Type = FPipelineCacheFileFormatPSO::DescriptorType::Graphics; // we will change this to compute later if needed
			PSO.GraphicsDesc.StateFromString(Parts[0]);

			bool bValid = true;

			bool bLooksLikeAComputeShader = false;

			static FName NAME_SF_Compute("SF_Compute");
			// because it is a CSV, and for backward compat, compute shaders will just be a zeroed graphics desc with the shader in the hull shader slot.
			for (int32 SlotIndex = 0; SlotIndex < SF_Compute; SlotIndex++) // SF_Compute here because the stablepc.csv file format does not have a compute slot
			{
				if (!Parts[SlotIndex + 1].Len())
				{
					continue;
				}

				FStableShaderKeyAndValue Shader;
				Shader.ParseFromString(Parts[SlotIndex + 1]);

				if (SlotIndex == SF_Hull)
				{
					if (Shader.TargetFrequency == NAME_SF_Compute)
					{
						bLooksLikeAComputeShader = true;
					}
				}
				else
				{
					check(Shader.TargetFrequency != NAME_SF_Compute);
				}

				FSHAHash Match;
				int32 Count = 0;
				for (auto Iter = StableMap.CreateConstKeyIterator(Shader); Iter; ++Iter)
				{
					check(Iter.Value() != FSHAHash());
					Match = Iter.Value();
					if (TargetPlatform == NAME_None)
					{
						TargetPlatform = Iter.Key().TargetPlatform;
					}
					else
					{
						check(TargetPlatform == Iter.Key().TargetPlatform);
					}
					Count++;
				}

				if (!Count)
				{
					UE_LOG(LogShaderPipelineCacheTools, Log, TEXT("Stable PSO not found, rejecting %s"), *Shader.ToString());
					bValid = false;
					break;
				}

				if (Count > 1)
				{
					UE_LOG(LogShaderPipelineCacheTools, Error, TEXT("Stable PSO maps to multiple shaders. This is usually a bad thing and means you used .scl.csv files from multiple builds. Ignoring all but the last %s"), *Shader.ToString());
				}

				switch (SlotIndex)
				{
				case SF_Vertex:
					PSO.GraphicsDesc.VertexShader = Match;
					break;
				case SF_Pixel:
					PSO.GraphicsDesc.FragmentShader = Match;
					break;
				case SF_Geometry:
					PSO.GraphicsDesc.GeometryShader = Match;
					break;
				case SF_Hull:
					PSO.GraphicsDesc.HullShader = Match;
					break;
				case SF_Domain:
					PSO.GraphicsDesc.DomainShader = Match;
					break;
				}
			}
			if (bValid)
			{
				if (
					PSO.GraphicsDesc.VertexShader == FSHAHash() &&
					PSO.GraphicsDesc.FragmentShader == FSHAHash() &&
					PSO.GraphicsDesc.GeometryShader == FSHAHash() &&
					!(PSO.GraphicsDesc.HullShader == FSHAHash()) &&    // Compute shaders are stored in the hull slot
					PSO.GraphicsDesc.DomainShader == FSHAHash() &&
					bLooksLikeAComputeShader
					)
				{
					// this is a compute shader
					PSO.Type = FPipelineCacheFileFormatPSO::DescriptorType::Compute;
					PSO.ComputeDesc.ComputeShader = PSO.GraphicsDesc.HullShader;
					PSO.GraphicsDesc.HullShader = FSHAHash();
				}
				else
				{
					PSO.Type = FPipelineCacheFileFormatPSO::DescriptorType::Graphics;
					check(!bLooksLikeAComputeShader);
					if (PSO.GraphicsDesc.VertexShader == FSHAHash())
					{
						UE_LOG(LogShaderPipelineCacheTools, Warning, TEXT("Stable PSO with null vertex shader, ignored."));
						bValid = false;
					}
				}
			}

			if (bValid)
			{
				PSOs.Add(PSO);
			}
		}
	}
	UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Re-deduplicated into %d binary PSOs."), PSOs.Num());

	if (PSOs.Num() < 1)
	{
		UE_LOG(LogShaderPipelineCacheTools, Error, TEXT("No PSO were created!"));
		return 1;
	}

	if (UE_LOG_ACTIVE(LogShaderPipelineCacheTools, Verbose))
	{
		for (const FPipelineCacheFileFormatPSO& Item : PSOs)
		{
			FString StringRep;
			if (Item.Type == FPipelineCacheFileFormatPSO::DescriptorType::Compute)
			{
				check(!(Item.ComputeDesc.ComputeShader == FSHAHash()));
				StringRep = Item.ComputeDesc.ToString();
			}
			else
			{
				check(!(Item.GraphicsDesc.VertexShader == FSHAHash()));
				StringRep = Item.GraphicsDesc.ToString();
			}
			UE_LOG(LogShaderPipelineCacheTools, Verbose, TEXT("%s"), *StringRep);
		}
	}

	check(TargetPlatform != NAME_None);
	EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(TargetPlatform);
	check(Platform != SP_NumPlatforms);

	if (IsOpenGLPlatform(Platform))
	{
		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("OpenGL detected, reducing PSOs to be BSS only as OpenGL doesn't care about the state at all when compiling shaders."));

		TSet<FPipelineCacheFileFormatPSO> KeptPSOs;

		// N^2 not good. 
		for (const FPipelineCacheFileFormatPSO& Item : PSOs)
		{
			bool bMatchedKept = false;
			if (Item.Type == FPipelineCacheFileFormatPSO::DescriptorType::Graphics)
			{
				for (const FPipelineCacheFileFormatPSO& TestItem : KeptPSOs)
				{
					FSHAHash VertexShader;
					FSHAHash FragmentShader;
					FSHAHash GeometryShader;
					FSHAHash HullShader;
					FSHAHash DomainShader;
					if (TestItem.Type == FPipelineCacheFileFormatPSO::DescriptorType::Graphics)
					{
						if (
							TestItem.GraphicsDesc.VertexShader == Item.GraphicsDesc.VertexShader &&
							TestItem.GraphicsDesc.FragmentShader == Item.GraphicsDesc.FragmentShader &&
							TestItem.GraphicsDesc.GeometryShader == Item.GraphicsDesc.GeometryShader &&
							TestItem.GraphicsDesc.HullShader == Item.GraphicsDesc.HullShader &&
							TestItem.GraphicsDesc.DomainShader == Item.GraphicsDesc.DomainShader
							)
						{
							bMatchedKept = true;
							break;
						}
					}
				}
			}
			if (!bMatchedKept)
			{
				KeptPSOs.Add(Item);
			}
		}
		Exchange(PSOs, KeptPSOs);
		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("BSS only reduction produced %d binary PSOs."), PSOs.Num());

		if (PSOs.Num() < 1)
		{
			UE_LOG(LogShaderPipelineCacheTools, Error, TEXT("No PSO were created!"));
			return 1;
		}

	}

	if (IFileManager::Get().FileExists(*Tokens.Last()))
	{
		IFileManager::Get().Delete(*Tokens.Last(), false, true);
	}
	if (IFileManager::Get().FileExists(*Tokens.Last()))
	{
		UE_LOG(LogShaderPipelineCacheTools, Fatal, TEXT("Could not delete %s"), *Tokens.Last());
	}
	if (!FPipelineFileCache::SavePipelineFileCacheFrom(FShaderPipelineCache::GetGameVersionForPSOFileCache(), Platform, Tokens.Last(), PSOs))
	{
		UE_LOG(LogShaderPipelineCacheTools, Error, TEXT("Failed to save %s"), *Tokens.Last());
		return 1;
	}
	int64 Size = IFileManager::Get().FileSize(*Tokens.Last());
	if (Size < 1)
	{
		UE_LOG(LogShaderPipelineCacheTools, Fatal, TEXT("Failed to write %s"), *Tokens.Last());
	}
	UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Wrote binary PSOs, (%lldKB) to %s"), (Size + 1023) / 1024, *Tokens.Last());
	return 0;
}


int32 DiffStable(const TArray<FString>& Tokens)
{
	TArray<TSet<FString>> Sets;
	for (int32 TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++)
	{
		if (!Tokens[TokenIndex].EndsWith(TEXT(".stablepc.csv")))
		{
			check(0);
			continue;
		}

		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Loading %s...."), *Tokens[TokenIndex]);
		TArray<FString> SourceFileContents;

		if (!FFileHelper::LoadFileToStringArray(SourceFileContents, *Tokens[TokenIndex]) || SourceFileContents.Num() < 2)
		{
			UE_LOG(LogShaderPipelineCacheTools, Fatal, TEXT("Could not load %s"), *Tokens[TokenIndex]);
			return 1;
		}

		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("Loaded %d stable PSO lines."), SourceFileContents.Num() - 1);

		Sets.AddDefaulted();

		for (int32 Index = 1; Index < SourceFileContents.Num(); Index++)
		{
			Sets.Last().Add(SourceFileContents[Index]);
		}
	}
	TSet<FString> Inter;
	for (int32 TokenIndex = 0; TokenIndex < Sets.Num(); TokenIndex++)
	{
		if (TokenIndex)
		{
			Inter = Sets[TokenIndex];
		}
		else
		{
			Inter = Inter.Intersect(Sets[TokenIndex]);
		}
	}

	for (int32 TokenIndex = 0; TokenIndex < Sets.Num(); TokenIndex++)
	{
		TSet<FString> InterSet = Sets[TokenIndex].Difference(Inter);

		UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("********************* Loaded %d not in others %s"), InterSet.Num(), *Tokens[TokenIndex]);
		for (const FString& Item : InterSet)
		{
			UE_LOG(LogShaderPipelineCacheTools, Display, TEXT("    %s"), *Item);
		}
	}
	return 0;
}

UShaderPipelineCacheToolsCommandlet::UShaderPipelineCacheToolsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UShaderPipelineCacheToolsCommandlet::Main(const FString& Params)
{
	return StaticMain(Params);
}

int32 UShaderPipelineCacheToolsCommandlet::StaticMain(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	if (Tokens.Num() >= 1)
	{
		ExpandWildcards(Tokens);
		if (Tokens[0] == TEXT("Expand") && Tokens.Num() >= 4)
		{
			Tokens.RemoveAt(0);
			return ExpandPSOSC(Tokens);
		}
		else if (Tokens[0] == TEXT("Build") && Tokens.Num() >= 4)
		{
			Tokens.RemoveAt(0);
			return BuildPSOSC(Tokens);
		}
		else if (Tokens[0] == TEXT("Diff") && Tokens.Num() >= 3)
		{
			Tokens.RemoveAt(0);
			return DiffStable(Tokens);
		}
		else if (Tokens[0] == TEXT("Dump") && Tokens.Num() >= 2)
		{
			Tokens.RemoveAt(0);
			for (int32 Index = 0; Index < Tokens.Num(); Index++)
			{
				if (Tokens[Index].EndsWith(TEXT(".upipelinecache")))
				{
					return DumpPSOSC(Tokens[Index]);
				}
				if (Tokens[Index].EndsWith(TEXT(".scl.csv")))
				{
					return DumpSCLCSV(Tokens[Index]);
				}
			}
		}
	}
	
	UE_LOG(LogShaderPipelineCacheTools, Warning, TEXT("Usage: Dump ShaderCache1.upipelinecache SCLInfo2.scl.csv [...]]\n"));
	UE_LOG(LogShaderPipelineCacheTools, Warning, TEXT("Usage: Diff ShaderCache1.stablepc.csv ShaderCache1.stablepc.csv [...]]\n"));
	UE_LOG(LogShaderPipelineCacheTools, Warning, TEXT("Usage: Expand Input1.upipelinecache Dir2/*.upipelinecache InputSCLInfo1.scl.csv Dir2/*.scl.csv InputSCLInfo3.scl.csv [...] Output.stablepc.csv\n"));
	UE_LOG(LogShaderPipelineCacheTools, Warning, TEXT("Usage: Build Input.stablepc.csv InputDir2/*.stablepc.csv InputSCLInfo1.scl.csv Dir2/*.scl.csv InputSCLInfo3.scl.csv [...] Output.upipelinecache\n"));
	return 0;
}
