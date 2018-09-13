// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ShaderCodeLibrary.cpp: Bound shader state cache implementation.
=============================================================================*/

#include "ShaderCodeLibrary.h"
#include "Shader.h"
#include "Misc/SecureHash.h"
#include "Misc/Paths.h"
#include "Math/UnitConversion.h"
#include "HAL/FileManagerGeneric.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "Async/AsyncFileHandle.h"
#include "PipelineFileCache.h"
#include "Interfaces/IPluginManager.h"

#include "Interfaces/IShaderFormatArchive.h"
#include "ShaderPipelineCache.h"
#include "Misc/FileHelper.h"

#if WITH_EDITORONLY_DATA
#include "Modules/ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#endif

	DEFINE_LOG_CATEGORY(LogShaderLibrary);

static const ECompressionFlags ShaderLibraryCompressionFlag = ECompressionFlags::COMPRESS_ZLIB;

static uint32 GShaderCodeArchiveVersion = 1;
static uint32 GShaderPipelineArchiveVersion = 1;

static FString ShaderExtension = TEXT(".ushaderbytecode");
static FString StableExtension = TEXT(".scl.csv");
static FString PipelineExtension = TEXT(".ushaderpipelines");

static FString GetCodeArchiveFilename(const FString& BaseDir, const FString& LibraryName, FName Platform)
{
	return BaseDir / FString::Printf(TEXT("ShaderArchive-%s-"), *LibraryName) + Platform.ToString() + ShaderExtension;
}

static FString GetStableInfoArchiveFilename(const FString& BaseDir, const FString& LibraryName, FName Platform)
{
	return BaseDir / FString::Printf(TEXT("ShaderStableInfo-%s-"), *LibraryName) + Platform.ToString() + StableExtension;
}

static FString GetPipelinesArchiveFilename(const FString& BaseDir, const FString& LibraryName, FName Platform)
{
	return BaseDir / FString::Printf(TEXT("ShaderArchive-%s-"), *LibraryName) + Platform.ToString() + PipelineExtension;
}

static FString GetShaderCodeFilename(const FString& BaseDir, const FString& LibraryName, FName Platform)
{
	return BaseDir / FString::Printf(TEXT("ShaderCode-%s-"), *LibraryName) + Platform.ToString() + ShaderExtension;
}

static FString GetShaderDebugFolder(const FString& BaseDir, const FString& LibraryName, FName Platform)
{
	return BaseDir / FString::Printf(TEXT("ShaderDebug-%s-"), *LibraryName) + Platform.ToString();
}

static TArray<uint8>& FShaderLibraryHelperUncompressCode(EShaderPlatform Platform, int32 UncompressedSize, TArray<uint8>& Code, TArray<uint8>& UncompressedCode)
{
	if (Code.Num() != UncompressedSize)
	{
		UncompressedCode.SetNum(UncompressedSize);
		bool bSucceed = FCompression::UncompressMemory(ShaderLibraryCompressionFlag, UncompressedCode.GetData(), UncompressedSize, Code.GetData(), Code.Num());
		check(bSucceed);
		return UncompressedCode;
	}
	else
	{
		return Code;
	}
}

static void FShaderLibraryHelperCompressCode(EShaderPlatform Platform, const TArray<uint8>& UncompressedCode, TArray<uint8>& CompressedCode)
{
	int32 CompressedSize = UncompressedCode.Num() * 4.f / 3.f;
	CompressedCode.SetNumUninitialized(CompressedSize); // Allocate large enough buffer for compressed code

	if (FCompression::CompressMemory(ShaderLibraryCompressionFlag, CompressedCode.GetData(), CompressedSize, UncompressedCode.GetData(), UncompressedCode.Num()))
	{
		CompressedCode.SetNum(CompressedSize);
	}
	else
	{
		CompressedCode = UncompressedCode;
	}
	CompressedCode.Shrink();
}

FString FCompactFullName::ToString() const
{
	FString RetString;
	if (!ObjectClassAndPath.Num())
	{
		RetString += TEXT("empty");
	}
	else
	{
		for (int32 NameIdx = 0; NameIdx < ObjectClassAndPath.Num(); NameIdx++)
		{
			RetString += ObjectClassAndPath[NameIdx].ToString();
			if (NameIdx == 0)
			{
				RetString += TEXT(" ");
			}
			else if (NameIdx < ObjectClassAndPath.Num() - 1)
			{
				if (NameIdx == ObjectClassAndPath.Num() - 2)
				{
					RetString += TEXT(".");
				}
				else
				{
					RetString += TEXT("/");
				}
			}
		}
	}
	return RetString;
}

void FCompactFullName::ParseFromString(const FString& InSrc)
{
	FString Src = InSrc;
	Src.ReplaceInline(TEXT("\t"), TEXT(" "));
	Src.ReplaceInline(TEXT("."), TEXT(" "));
	Src.ReplaceInline(TEXT("/"), TEXT(" "));
	TArray<FString> Fields;
	Src.TrimStartAndEnd().ParseIntoArray(Fields, TEXT(" "), true);
	if (Fields.Num() == 1 && Fields[0] == TEXT("empty"))
	{
		Fields.Empty();
	}
	ObjectClassAndPath.Empty(Fields.Num());
	for (const FString& Item : Fields)
	{
		ObjectClassAndPath.Add(FName(*Item));
	}
}

uint32 GetTypeHash(const FCompactFullName& A)
{
	uint32 Hash = 0;
	for (const FName& Name : A.ObjectClassAndPath)
	{
		Hash = HashCombine(Hash, GetTypeHash(Name));
	}
	return Hash;
}

void FStableShaderKeyAndValue::ComputeKeyHash()
{
	KeyHash = GetTypeHash(ClassNameAndObjectPath);

	KeyHash = HashCombine(KeyHash, GetTypeHash(ShaderType));
	KeyHash = HashCombine(KeyHash, GetTypeHash(ShaderClass));
	KeyHash = HashCombine(KeyHash, GetTypeHash(MaterialDomain));
	KeyHash = HashCombine(KeyHash, GetTypeHash(FeatureLevel));

	KeyHash = HashCombine(KeyHash, GetTypeHash(QualityLevel));
	KeyHash = HashCombine(KeyHash, GetTypeHash(TargetFrequency));
	KeyHash = HashCombine(KeyHash, GetTypeHash(TargetPlatform));

	KeyHash = HashCombine(KeyHash, GetTypeHash(VFType));
	KeyHash = HashCombine(KeyHash, GetTypeHash(PermutationId));
}

void FStableShaderKeyAndValue::ParseFromString(const FString& Src)
{
	TArray<FString> Fields;
	Src.TrimStartAndEnd().ParseIntoArray(Fields, TEXT(","), false);
	if (Fields.Num() > 11)
	{
		// hack fix for unsanitized names, should not occur anymore.

		FString NewSrc(Src);

		int32 ParenOpen = -1;
		int32 ParenClose = -1;

		if (NewSrc.FindChar(TCHAR('('), ParenOpen) && NewSrc.FindChar(TCHAR(')'), ParenClose) && ParenOpen < ParenClose && ParenOpen >= 0 && ParenClose >= 0)
		{
			for (int32 Index = ParenOpen + 1; Index < ParenClose; Index++)
			{
				if (NewSrc[Index] == TCHAR(','))
				{
					NewSrc[Index] = ' ';
				}
			}
			Fields.Empty();
			NewSrc.TrimStartAndEnd().ParseIntoArray(Fields, TEXT(","), false);
			check(Fields.Num() == 11);
		}
	}


	check(Fields.Num() == 11);

	int32 Index = 0;
	ClassNameAndObjectPath.ParseFromString(Fields[Index++]);

	ShaderType = FName(*Fields[Index++]);
	ShaderClass = FName(*Fields[Index++]);
	MaterialDomain = FName(*Fields[Index++]);
	FeatureLevel = FName(*Fields[Index++]);

	QualityLevel = FName(*Fields[Index++]);
	TargetFrequency = FName(*Fields[Index++]);
	TargetPlatform = FName(*Fields[Index++]);

	VFType = FName(*Fields[Index++]);
	PermutationId = FName(*Fields[Index++]);

	OutputHash.FromString(Fields[Index++]);

	check(Index == 11);

	ComputeKeyHash();
}

FString FStableShaderKeyAndValue::ToString() const
{
	FString Result;
	Result.Reserve(2048);

	const FString Delim(TEXT(","));

	Result += ClassNameAndObjectPath.ToString().Replace(TEXT(","), TEXT(" "));
	Result += Delim;

	Result += ShaderType.ToString().Replace(TEXT(","), TEXT(" "));
	Result += Delim;
	Result += ShaderClass.ToString().Replace(TEXT(","), TEXT(" "));
	Result += Delim;
	Result += MaterialDomain.ToString();
	Result += Delim;
	Result += FeatureLevel.ToString();
	Result += Delim;

	Result += QualityLevel.ToString();
	Result += Delim;
	Result += TargetFrequency.ToString();
	Result += Delim;
	Result += TargetPlatform.ToString();
	Result += Delim;

	Result += VFType.ToString();
	Result += Delim;
	Result += PermutationId.ToString();
	Result += Delim;

	Result += OutputHash.ToString();

	return Result;
}

FString FStableShaderKeyAndValue::HeaderLine()
{
	FString Result;
	Result.Reserve(2048);

	const FString Delim(TEXT(","));

	Result += TEXT("ClassNameAndObjectPath");
	Result += Delim;

	Result += TEXT("ShaderType");
	Result += Delim;
	Result += TEXT("ShaderClass");
	Result += Delim;
	Result += TEXT("MaterialDomain");
	Result += Delim;
	Result += TEXT("FeatureLevel");
	Result += Delim;

	Result += TEXT("QualityLevel");
	Result += Delim;
	Result += TEXT("TargetFrequency");
	Result += Delim;
	Result += TEXT("TargetPlatform");
	Result += Delim;

	Result += TEXT("VFType");
	Result += Delim;
	Result += TEXT("Permutation");
	Result += Delim;

	Result += TEXT("OutputHash");

	return Result;
}


struct FShaderCodeEntry
{
	// Serialized
	uint32 Size;
	uint64 Offset;
	uint32 UncompressedSize;
	uint8 Frequency;

	// Transient
	TArray<uint8> LoadedCode;
	int32 NumRefs;
	TWeakPtr<IAsyncReadRequest, ESPMode::ThreadSafe> ReadRequest;
#if DO_CHECK	
	volatile int32 bReadCompleted;
#endif

	FShaderCodeEntry()
		: Size(0)
		, Offset(0)
		, UncompressedSize(0)
		, Frequency(0)
		, NumRefs(0)
#if DO_CHECK
		, bReadCompleted(0)
#endif
	{}
};

static FArchive& operator <<(FArchive& Ar, FShaderCodeEntry& Ref)
{
	return Ar << Ref.Offset << Ref.Size << Ref.UncompressedSize << Ref.Frequency;
}

class FShaderCodeArchive final : public FShaderFactoryInterface
{
public:
	FShaderCodeArchive(EShaderPlatform InPlatform, const FString& InLibraryDir, const FString& InLibraryName)
		: FShaderFactoryInterface(InPlatform, InLibraryName)
		, LibraryDir(InLibraryDir)
		, LibraryCodeOffset(0)
		, LibraryAsyncFileHandle(nullptr)
	{
		FName PlatformName = LegacyShaderPlatformToShaderFormat(InPlatform);
		FString DestFilePath = GetCodeArchiveFilename(LibraryDir, LibraryName, PlatformName);

		FArchive* Ar = IFileManager::Get().CreateFileReader(*DestFilePath);
		if (Ar)
		{
			uint32 Version = 0;
			*Ar << Version;

			if (Version == GShaderCodeArchiveVersion)
			{
				*Ar << Shaders;
				LibraryCodeOffset = Ar->Tell();
			}
			Ar->Close();
			delete Ar;

			// Open library for async reads
			LibraryAsyncFileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*DestFilePath);

			UE_LOG(LogShaderLibrary, Display, TEXT("Using %s for material shader code. Total %d unique shaders."), *DestFilePath, Shaders.Num());
		}
	}

	virtual ~FShaderCodeArchive()
	{
	}

	virtual bool IsLibraryNativeFormat() const { return false; }

	TArray<uint8>* LookupShaderCode(uint8 Frequency, const FSHAHash& Hash, int32& OutSize)
	{
		FShaderCodeEntry* Entry = Shaders.Find(Hash);
		if (Entry)
		{
			FScopeLock ScopeLock(&ReadRequestLock);

			if (Entry->NumRefs == 0 && Entry->LoadedCode.Num() == 0)
			{
				// Someone has asked for a shader without previously invoking RequestEntry, we cannot afford to crash because this happens all too frequently.
				UE_LOG(LogShaderLibrary, Warning, TEXT("Synchronously loading shader %s from library: %s - caller should have invoked FShaderCodeLibrary::RequestShaderCode first!"), *Hash.ToString(), *GetName());
				RequestEntry(Hash, nullptr);
			}

			check(Entry->NumRefs > 0);
			check(Entry->LoadedCode.Num() != 0);
			check(Entry->bReadCompleted == 1);

			OutSize = Entry->UncompressedSize;
			return &Entry->LoadedCode;
		}
		return nullptr;
	}

	virtual bool ContainsEntry(const FSHAHash& Hash) final override
	{
		FShaderCodeEntry* Entry = Shaders.Find(Hash);
		return (Entry != nullptr);
	}

	virtual bool RequestEntry(const FSHAHash& Hash, FArchive* Ar) final override
	{
		FShaderCodeEntry* Entry = Shaders.Find(Hash);
		if (Entry)
		{
			FScopeLock ScopeLock(&ReadRequestLock);

			int32 CodeNumRefs = Entry->NumRefs++;
			TSharedPtr<IAsyncReadRequest, ESPMode::ThreadSafe> LocalReadRequest = Entry->ReadRequest.Pin();
			bool bHasReadRequest = LocalReadRequest.IsValid();

			if (CodeNumRefs == 0 && !bHasReadRequest)
			{
				// should not have allocated mem for code if there is no active read request
				check(Entry->LoadedCode.Num() == 0);

				int64 ReadSize = Entry->Size;
				int64 ReadOffset = LibraryCodeOffset + Entry->Offset;
				Entry->LoadedCode.SetNumUninitialized(ReadSize);
				LocalReadRequest = MakeShareable(LibraryAsyncFileHandle->ReadRequest(ReadOffset, ReadSize, AIOP_Normal, nullptr, Entry->LoadedCode.GetData()));
				Entry->ReadRequest = LocalReadRequest;
				bHasReadRequest = true;
			}

			if (bHasReadRequest)
			{
				FExternalReadCallback ExternalReadCallback = [this, Entry, LocalReadRequest](double ReaminingTime)
				{
					return this->OnExternalReadCallback(LocalReadRequest, Entry, ReaminingTime);
				};

				if (!Ar || !Ar->AttachExternalReadDependency(ExternalReadCallback))
				{
					// Archive does not support async loading 
					// do a blocking load
					ExternalReadCallback(0.0);
				}
			}
			else
			{
				// already loaded
				check(Entry->LoadedCode.Num() != 0);
				check(Entry->bReadCompleted == 1);
			}

			return true;
		}

		return false;
	}

	bool OnExternalReadCallback(const TSharedPtr<IAsyncReadRequest, ESPMode::ThreadSafe>& AsyncReadRequest, FShaderCodeEntry* Entry, double RemainingTime)
	{
		if (RemainingTime < 0.0 && !AsyncReadRequest->PollCompletion())
		{
			return false;
		}
		else if (RemainingTime >= 0.0 && !AsyncReadRequest->WaitCompletion(RemainingTime))
		{
			return false;
		}
#if DO_CHECK
		Entry->bReadCompleted = 1;
#endif
		return true;
	}

	void ReleaseShaderCode(const FSHAHash& Hash)
	{
		FShaderCodeEntry* Entry = Shaders.Find(Hash);
		if (Entry)
		{
			FScopeLock ScopeLock(&ReadRequestLock);

			Entry->NumRefs--;
			if (Entry->NumRefs == 0)
			{
				// should not attempt to release shader code while it's loading
				check(Entry->ReadRequest.IsValid() == false);

				// free code mem
				Entry->LoadedCode.Empty();
#if DO_CHECK
				Entry->bReadCompleted = 0;
#endif
			}
		}
	}

	FPixelShaderRHIRef CreatePixelShader(const FSHAHash& Hash) override final
	{
		FPixelShaderRHIRef Shader;

		int32 Size = 0;
		TArray<uint8>* Code = LookupShaderCode(SF_Pixel, Hash, Size);
		if (Code)
		{
			TArray<uint8> UCode;
			TArray<uint8>& UncompressedCode = FShaderLibraryHelperUncompressCode(Platform, Size, *Code, UCode);
			Shader = RHICreatePixelShader(UncompressedCode);
		}
		return Shader;
	}

	FVertexShaderRHIRef CreateVertexShader(const FSHAHash& Hash) override final
	{
		FVertexShaderRHIRef Shader;

		int32 Size = 0;
		TArray<uint8>* Code = LookupShaderCode(SF_Vertex, Hash, Size);
		if (Code)
		{
			TArray<uint8> UCode;
			TArray<uint8>& UncompressedCode = FShaderLibraryHelperUncompressCode(Platform, Size, *Code, UCode);
			Shader = RHICreateVertexShader(UncompressedCode);
		}
		return Shader;
	}

	FHullShaderRHIRef CreateHullShader(const FSHAHash& Hash) override final
	{
		FHullShaderRHIRef Shader;

		int32 Size = 0;
		TArray<uint8>* Code = LookupShaderCode(SF_Hull, Hash, Size);
		if (Code)
		{
			TArray<uint8> UCode;
			TArray<uint8>& UncompressedCode = FShaderLibraryHelperUncompressCode(Platform, Size, *Code, UCode);
			Shader = RHICreateHullShader(UncompressedCode);
		}
		return Shader;
	}

	FDomainShaderRHIRef CreateDomainShader(const FSHAHash& Hash) override final
	{
		FDomainShaderRHIRef Shader;

		int32 Size = 0;
		TArray<uint8>* Code = LookupShaderCode(SF_Domain, Hash, Size);
		if (Code)
		{
			TArray<uint8> UCode;
			TArray<uint8>& UncompressedCode = FShaderLibraryHelperUncompressCode(Platform, Size, *Code, UCode);
			Shader = RHICreateDomainShader(UncompressedCode);
		}
		return Shader;
	}

	FGeometryShaderRHIRef CreateGeometryShader(const FSHAHash& Hash) override final
	{
		FGeometryShaderRHIRef Shader;

		int32 Size = 0;
		TArray<uint8>* Code = LookupShaderCode(SF_Geometry, Hash, Size);
		if (Code)
		{
			TArray<uint8> UCode;
			TArray<uint8>& UncompressedCode = FShaderLibraryHelperUncompressCode(Platform, Size, *Code, UCode);
			Shader = RHICreateGeometryShader(UncompressedCode);
		}
		return Shader;
	}

	FGeometryShaderRHIRef CreateGeometryShaderWithStreamOutput(const FSHAHash& Hash, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream) override final
	{
		FGeometryShaderRHIRef Shader;

		int32 Size = 0;
		TArray<uint8>* Code = LookupShaderCode(SF_Geometry, Hash, Size);
		if (Code)
		{
			TArray<uint8> UCode;
			TArray<uint8>& UncompressedCode = FShaderLibraryHelperUncompressCode(Platform, Size, *Code, UCode);
			Shader = RHICreateGeometryShaderWithStreamOutput(UncompressedCode, ElementList, NumStrides, Strides, RasterizedStream);
		}
		return Shader;
	}

	FComputeShaderRHIRef CreateComputeShader(const FSHAHash& Hash) override final
	{
		FComputeShaderRHIRef Shader;

		int32 Size = 0;
		TArray<uint8>* Code = LookupShaderCode(SF_Compute, Hash, Size);
		if (Code)
		{
			TArray<uint8> UCode;
			TArray<uint8>& UncompressedCode = FShaderLibraryHelperUncompressCode(Platform, Size, *Code, UCode);
			Shader = RHICreateComputeShader(UncompressedCode);
		}
		return Shader;
	}

	class FShaderCodeLibraryIterator : public FRHIShaderLibrary::FShaderLibraryIterator
	{
	public:
		FShaderCodeLibraryIterator(FShaderCodeArchive* Owner, EShaderPlatform Plat, TMap<FSHAHash, FShaderCodeEntry>::TIterator It)
			: FRHIShaderLibrary::FShaderLibraryIterator(Owner)
			, Platform(Plat)
			, IteratorImpl(It)
		{}

		virtual bool IsValid() const final override
		{
			return !!IteratorImpl;
		}

		virtual FRHIShaderLibrary::FShaderLibraryEntry operator*() const final override
		{
			FRHIShaderLibrary::FShaderLibraryEntry Entry;
			TPair<FSHAHash, FShaderCodeEntry> const& Pair = *IteratorImpl;
			Entry.Hash = Pair.Key;
			Entry.Frequency = (EShaderFrequency)Pair.Value.Frequency;
			Entry.Platform = Platform;
			return Entry;
		}

		virtual FRHIShaderLibrary::FShaderLibraryIterator& operator++() final override
		{
			++IteratorImpl;
			return *this;
		}

	private:
		EShaderPlatform Platform;
		TMap<FSHAHash, FShaderCodeEntry>::TIterator IteratorImpl;
	};

	virtual TRefCountPtr<FRHIShaderLibrary::FShaderLibraryIterator> CreateIterator(void) override final
	{
		return new FShaderCodeLibraryIterator(this, Platform, Shaders.CreateIterator());
	}

	TSet<FShaderCodeLibraryPipeline> const* GetShaderPipelines(EShaderPlatform InPlatform)
	{
		if (Pipelines.Num() == 0)
		{
			FName PlatformName = LegacyShaderPlatformToShaderFormat(InPlatform);
			FString DestFilePath = GetPipelinesArchiveFilename(LibraryDir, LibraryName, PlatformName);

			FArchive* Ar = IFileManager::Get().CreateFileReader(*DestFilePath);
			if (Ar)
			{
				uint32 Version = 0;
				*Ar << Version;

				if (Version == GShaderPipelineArchiveVersion)
				{
					*Ar << Pipelines;
				}

				Ar->Close();
				delete Ar;
			}
		}

		return &Pipelines;
	}

	virtual uint32 GetShaderCount(void) const override final
	{
		return Shaders.Num();
	}

private:
	// Library directory
	FString LibraryDir;

	// Offset at where shader code starts in a code library
	int64 LibraryCodeOffset;

	// Library file handle for async reads
	IAsyncReadFileHandle* LibraryAsyncFileHandle;
	FCriticalSection ReadRequestLock;

	// The shader code present in the library
	TMap<FSHAHash, FShaderCodeEntry> Shaders;

	// De-serialised pipeline map
	TSet<FShaderCodeLibraryPipeline> Pipelines;

};

#if WITH_EDITOR
struct FEditorShaderCodeArchive
{
	FEditorShaderCodeArchive(FName InFormat)
		: FormatName(InFormat)
		, Format(nullptr)
	{
		Format = GetTargetPlatformManagerRef().FindShaderFormat(InFormat);
		check(Format);
	}

	~FEditorShaderCodeArchive() {}

	const IShaderFormat* GetFormat() const
	{
		return Format;
	}

	void OpenLibrary(FString const& Name)
	{
		check(LibraryName.Len() == 0);
		check(Name.Len() > 0);
		LibraryName = Name;
		Offset = 0;
		Shaders.Empty();
		Pipelines.Empty();
	}

	void CloseLibrary(FString const& Name)
	{
		check(LibraryName == Name);
		LibraryName = TEXT("");
	}

	bool HasShader(const FSHAHash& Hash) const
	{
		return Shaders.Contains(Hash);
	}

	bool AddShader(uint8 Frequency, const FSHAHash& Hash, TArray<uint8> const& InCode, int32 const UncompressedSize)
	{
		bool bAdd = false;
		if (!Shaders.Contains(Hash))
		{
#if DO_CHECK
			uint8 Count = 0;
			for (uint8 i : InCode)
			{
				Count |= i;
			}
			check(Count > 0);
#endif

			FShaderCodeEntry Entry;
			Entry.Size = InCode.Num();
			Entry.Offset = Offset;
			Entry.UncompressedSize = UncompressedSize;
			Entry.Frequency = Frequency;
			Entry.LoadedCode = InCode;

			Offset += Entry.Size;

			Shaders.Add(Hash, Entry);
			bAdd = true;
		}
		return bAdd;
	}

	bool AddPipeline(FShaderPipeline* Pipeline)
	{
		check(LibraryName.Len() != 0);
		EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(FormatName);

		FShaderCodeLibraryPipeline LibraryPipeline;
		if (IsValidRef(Pipeline->VertexShader))
		{
			LibraryPipeline.VertexShader = Pipeline->VertexShader->GetOutputHash();
		}
		if (IsValidRef(Pipeline->GeometryShader))
		{
			LibraryPipeline.GeometryShader = Pipeline->GeometryShader->GetOutputHash();
		}
		if (IsValidRef(Pipeline->HullShader))
		{
			LibraryPipeline.HullShader = Pipeline->HullShader->GetOutputHash();
		}
		if (IsValidRef(Pipeline->DomainShader))
		{
			LibraryPipeline.DomainShader = Pipeline->DomainShader->GetOutputHash();
		}
		if (IsValidRef(Pipeline->PixelShader))
		{
			LibraryPipeline.PixelShader = Pipeline->PixelShader->GetOutputHash();
		}
		if (!Pipelines.Contains(LibraryPipeline))
		{
			Pipelines.Add(LibraryPipeline);
			return true;
		}
		return false;
	}

	void AddExistingShaderCodeLibrary(FString const& OutputDir)
	{
		check(LibraryName.Len() > 0);

		const FString ShaderIntermediateLocation = FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatName.ToString() / LibraryName;

		TArray<FString> ShaderFiles;
		IFileManager::Get().FindFiles(ShaderFiles, *ShaderIntermediateLocation, *ShaderExtension);

		for (const FString& ShaderFileName : ShaderFiles)
		{
			if (ShaderFileName.Contains(LibraryName + TEXT("_") + FormatName.ToString() + TEXT(".")))
			{
				FArchive* PrevCookedAr = IFileManager::Get().CreateFileReader(*GetCodeArchiveFilename(OutputDir, LibraryName, FormatName));

				if (PrevCookedAr)
				{
					uint32 Version = 0;
					*PrevCookedAr << Version;

					if (Version == GShaderCodeArchiveVersion)
					{
						TMap<FSHAHash, FShaderCodeEntry> PrevCookedShaders;

						*PrevCookedAr << PrevCookedShaders;
						int64 PrevCookedShadersCodeStart = PrevCookedAr->Tell();

						for (TMap<FSHAHash, FShaderCodeEntry>::TIterator It(PrevCookedShaders); It; ++It)
						{
							FSHAHash& Hash = It.Key();

							if (!Shaders.Contains(Hash))
							{
								// Shader not in list - lazy load shader code
								FShaderCodeEntry& CodeEntry = It.Value();

								int64 ReadSize = CodeEntry.Size;
								int64 ReadOffset = PrevCookedShadersCodeStart + CodeEntry.Offset;

								CodeEntry.LoadedCode.SetNumUninitialized(ReadSize);

								// Read shader code from archive and add shader to set
								PrevCookedAr->Seek(ReadOffset);
								PrevCookedAr->Serialize(CodeEntry.LoadedCode.GetData(), ReadSize);

								AddShader(CodeEntry.Frequency, Hash, CodeEntry.LoadedCode, CodeEntry.UncompressedSize);
							}
						}
					}

					PrevCookedAr->Close();
					delete PrevCookedAr;
				}
			}
		}

		TArray<FString> PipelineFiles;
		IFileManager::Get().FindFiles(PipelineFiles, *ShaderIntermediateLocation, *PipelineExtension);

		for (const FString& ShaderFileName : PipelineFiles)
		{
			if (ShaderFileName.Contains(LibraryName + TEXT("_") + FormatName.ToString() + TEXT(".")))
			{
				FArchive* PrevCookedAr = IFileManager::Get().CreateFileReader(*GetPipelinesArchiveFilename(OutputDir, LibraryName, FormatName));

				if (PrevCookedAr)
				{
					uint32 Version = 0;
					*PrevCookedAr << Version;

					if (Version == GShaderPipelineArchiveVersion)
					{
						TSet<FShaderCodeLibraryPipeline> PrevCookedPipelines;

						*PrevCookedAr << PrevCookedPipelines;
						int64 PrevCookedShadersCodeStart = PrevCookedAr->Tell();

						Pipelines.Append(PrevCookedPipelines);
					}

					PrevCookedAr->Close();
					delete PrevCookedAr;
				}
			}
		}
	}

	bool Finalize(FString OutputDir, bool bNativeFormat, bool bMasterCooker)
	{
		check(LibraryName.Len() > 0);

		if (bMasterCooker)
		{
			AddExistingShaderCodeLibrary(OutputDir);
		}

		bool bSuccess = IFileManager::Get().MakeDirectory(*OutputDir, true);

		EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(FormatName);

		// Shader library
		if (bSuccess && Shaders.Num() > 0)
		{
			// Write to a intermediate file
			FString IntermediateFormatPath = GetShaderCodeFilename(FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatName.ToString(), LibraryName, FormatName);
			FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*IntermediateFormatPath, FILEWRITE_NoFail);

			if (FileWriter)
			{
				check(Format);

				*FileWriter << GShaderCodeArchiveVersion;

				if (Format->CanStripShaderCode(bNativeFormat) && !bNativeFormat)
				{
					FString DebugPlatformDir = GetShaderDebugFolder(FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatName.ToString(), LibraryName, FormatName);
					IFileManager::Get().MakeDirectory(*DebugPlatformDir, true);

					TMap<FSHAHash, FShaderCodeEntry> StrippedShaders;
					uint64 TotalSize = 0;
					for (const auto& Pair : Shaders)
					{
						TArray<uint8> CompressedCode;
						CompressedCode.Append(Pair.Value.LoadedCode.GetData(), Pair.Value.Size);

						int32 UncompressedSize = Pair.Value.UncompressedSize;

						TArray<uint8> UCode;
						TArray<uint8>& UncompressedCode = FShaderLibraryHelperUncompressCode(Platform, UncompressedSize, CompressedCode, UCode);

						if (!Format->StripShaderCode(UncompressedCode, DebugPlatformDir, bNativeFormat))
						{
							bSuccess = false;
						}

						FShaderLibraryHelperCompressCode(Platform, UncompressedCode, CompressedCode);

						FShaderCodeEntry StrippedEntry;
						StrippedEntry.Size = CompressedCode.Num();
						StrippedEntry.Offset = TotalSize;
						StrippedEntry.UncompressedSize = UncompressedCode.Num();
						StrippedEntry.Frequency = Pair.Value.Frequency;
						StrippedEntry.LoadedCode = CompressedCode;

						TotalSize += StrippedEntry.Size;

						StrippedShaders.Add(Pair.Key, StrippedEntry);
					}

					// Write stripped shader library
					*FileWriter << StrippedShaders;
					for (auto& Pair : StrippedShaders)
					{
						FileWriter->Serialize(Pair.Value.LoadedCode.GetData(), Pair.Value.Size);
					}

					// Delete the temp dir
					IFileManager::Get().DeleteDirectory(*DebugPlatformDir, false, true);
				}
				else
				{
					// Write shader library
					*FileWriter << Shaders;
					for (auto& Pair : Shaders)
					{
						FileWriter->Serialize(Pair.Value.LoadedCode.GetData(), Pair.Value.Size);
					}
				}

				FileWriter->Close();
				delete FileWriter;

				// Only the master cooker needs to write to the output directory, child cookers only write to the Saved directory
				if (bMasterCooker)
				{
					FString OutputFilePath = GetCodeArchiveFilename(OutputDir, LibraryName, FormatName);

					// Copy to output location - support for iterative native library cooking
					IFileManager::Get().Copy(*OutputFilePath, *IntermediateFormatPath, true, true);
				}
			}
		}

		// Pipelines
		if (bSuccess && Pipelines.Num() > 0)
		{
			// Write to a temporary file
			FString TempFilePath = GetPipelinesArchiveFilename(FPaths::ProjectSavedDir() / TEXT("Shaders"), LibraryName, FormatName);
			FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*TempFilePath, FILEWRITE_NoFail);

			*FileWriter << GShaderPipelineArchiveVersion;

			*FileWriter << Pipelines;

			FileWriter->Close();
			delete FileWriter;

			// Only the master cooker needs to write to the output directory, child cookers only write to the Saved directory
			if (bMasterCooker)
			{
				FString OutputFilePath = GetPipelinesArchiveFilename(OutputDir, LibraryName, FormatName);

				// Copy to output location - support for iterative native library cooking
				IFileManager::Get().Copy(*OutputFilePath, *TempFilePath, true, true);
			}
		}

		return bSuccess;
	}

	bool PackageNativeShaderLibrary(const FString& ShaderCodeDir)
	{
		if (Shaders.Num() == 0)
		{
			return true;
		}

		bool bOK = false;

		FString IntermediateFormatPath = GetShaderDebugFolder(FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatName.ToString(), LibraryName, FormatName);
		FString TempPath = IntermediateFormatPath / TEXT("NativeLibrary");

		IFileManager::Get().MakeDirectory(*TempPath, true);
		IFileManager::Get().MakeDirectory(*ShaderCodeDir, true);

		EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(FormatName);
		IShaderFormatArchive* Archive = Format->CreateShaderArchive(LibraryName, FormatName, TempPath);
		if (Archive)
		{
			bOK = true;

			// Add the shaders to the archive.
			for (auto& Pair : Shaders)
			{
				FSHAHash& Hash = Pair.Key;
				FShaderCodeEntry& Entry = Pair.Value;

				TArray<uint8> UCode;
				TArray<uint8>& UncompressedCode = FShaderLibraryHelperUncompressCode(Platform, Entry.UncompressedSize, Entry.LoadedCode, UCode);

				if (Format->CanStripShaderCode(true))
				{
					if (!Format->StripShaderCode(UncompressedCode, IntermediateFormatPath, true))
					{
						bOK = false;
						break;
					}
				}

				if (!Archive->AddShader(Entry.Frequency, Hash, UncompressedCode))
				{
					bOK = false;
					break;
				}
			}

			if (bOK)
			{
				bOK = Archive->Finalize(ShaderCodeDir, IntermediateFormatPath, nullptr);

				// Delete Shader code library / pipelines as we now have native versions
				{
					FString OutputFilePath = GetCodeArchiveFilename(ShaderCodeDir, LibraryName, FormatName);
					IFileManager::Get().Delete(*OutputFilePath);
				}
				{
					FString OutputFilePath = GetPipelinesArchiveFilename(ShaderCodeDir, LibraryName, FormatName);
					IFileManager::Get().Delete(*OutputFilePath);
				}
			}
		}

		// Clean up the saved directory of temporary files
		IFileManager::Get().DeleteDirectory(*IntermediateFormatPath, false, true);
		IFileManager::Get().DeleteDirectory(*TempPath, false, true);

		return bOK;
	}

private:
	FName FormatName;
	FString LibraryName;
	TMap<FSHAHash, FShaderCodeEntry> Shaders;
	TSet<FShaderCodeLibraryPipeline> Pipelines;
	uint64 Offset;
	const IShaderFormat* Format;
};

struct FEditorShaderStableInfo
{
	FEditorShaderStableInfo(FName InFormat)
		: FormatName(InFormat)
	{
	}

	void OpenLibrary(FString const& Name)
	{
		check(LibraryName.Len() == 0);
		check(Name.Len() > 0);
		LibraryName = Name;
		Offset = 0;
		StableMap.Empty();
	}

	void CloseLibrary(FString const& Name)
	{
		check(LibraryName == Name);
		LibraryName = TEXT("");
	}

	void AddShader(FStableShaderKeyAndValue& StableKeyValue)
	{
		FStableShaderKeyAndValue* Existing = StableMap.Find(StableKeyValue);
		if (Existing && Existing->OutputHash != StableKeyValue.OutputHash)
		{
			UE_LOG(LogShaderLibrary, Warning, TEXT("Duplicate key in stable shader library, but different keys, skipping new item:"));
			UE_LOG(LogShaderLibrary, Warning, TEXT("    Existing: %s"), *Existing->ToString());
			UE_LOG(LogShaderLibrary, Warning, TEXT("    New     : %s"), *StableKeyValue.ToString());
			return;
		}
		StableMap.Add(StableKeyValue);
	}

	void AddExistingShaderCodeLibrary(FString const& OutputDir)
	{
		check(LibraryName.Len() > 0);

		const FString ShaderIntermediateLocation = FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatName.ToString() / LibraryName;

		TArray<FString> ShaderFiles;
		IFileManager::Get().FindFiles(ShaderFiles, *ShaderIntermediateLocation, *ShaderExtension);

		for (const FString& ShaderFileName : ShaderFiles)
		{
			if (ShaderFileName.Contains(LibraryName + TEXT("_") + FormatName.ToString() + TEXT(".")))
			{
				TArray<FString> SourceFileContents;
				if (FFileHelper::LoadFileToStringArray(SourceFileContents, *GetStableInfoArchiveFilename(OutputDir, LibraryName, FormatName)))
				{
					for (int32 Index = 1; Index < SourceFileContents.Num(); Index++)
					{
						FStableShaderKeyAndValue Item;
						Item.ParseFromString(SourceFileContents[Index]);
						AddShader(Item);
					}
				}
			}
		}
	}

	bool Finalize(FString OutputDir, bool bNativeFormat, bool bMasterCooker, FString& OutSCLCSVPath)
	{
		check(LibraryName.Len() > 0);
		OutSCLCSVPath = FString();

		if (bMasterCooker)
		{
			AddExistingShaderCodeLibrary(OutputDir);
		}

		bool bSuccess = IFileManager::Get().MakeDirectory(*OutputDir, true);

		EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(FormatName);

		// Shader library
		if (bSuccess && StableMap.Num() > 0)
		{
			// Write to a intermediate file
			FString IntermediateFormatPath = GetStableInfoArchiveFilename(FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatName.ToString(), LibraryName, FormatName);

			TArray<FString> FileContents;

			FileContents.Add(FStableShaderKeyAndValue::HeaderLine());

			for (const FStableShaderKeyAndValue& Item : StableMap)
			{
				FileContents.Add(Item.ToString());
			}
			FFileHelper::SaveStringArrayToFile(FileContents, *IntermediateFormatPath);

			// Only the master cooker needs to write to the output directory, child cookers only write to the Saved directory
			if (bMasterCooker)
			{
				FString OutputFilePath = GetStableInfoArchiveFilename(OutputDir, LibraryName, FormatName);

				// Copy to output location - support for iterative native library cooking
				IFileManager::Get().Copy(*OutputFilePath, *IntermediateFormatPath, true, true);
				OutSCLCSVPath = OutputFilePath;
			}
		}

		return bSuccess;
	}

private:
	FName FormatName;
	FString LibraryName;
	TSet<FStableShaderKeyAndValue> StableMap;
	uint64 Offset;
};

struct FShaderCodeStats
{
	int64 ShadersSize;
	int64 ShadersUniqueSize;
	int32 NumShaders;
	int32 NumUniqueShaders;
	int32 NumPipelines;
	int32 NumUniquePipelines;
};
#endif //WITH_EDITOR

class FShaderCodeLibraryImpl
{
	// At runtime, shader code collection for current shader platform
	TArray<FRHIShaderLibraryRef> ShaderCodeArchiveStack;
	TSet<FShaderCodeLibraryPipeline> Pipelines;
	EShaderPlatform ShaderPlatform;
	uint64 ShaderCount;
	FRWLock LibraryMutex;
#if WITH_EDITOR
	FCriticalSection ShaderCodeCS;
	// At cook time, shader code collection for each shader platform
	FEditorShaderCodeArchive* EditorShaderCodeArchive[EShaderPlatform::SP_NumPlatforms];
	// At cook time, shader code collection for each shader platform
	FEditorShaderStableInfo* EditorShaderStableInfo[EShaderPlatform::SP_NumPlatforms];
	// At cook time, shader stats for each shader platform
	FShaderCodeStats EditorShaderCodeStats[EShaderPlatform::SP_NumPlatforms];
	// At cook time, whether the shader archive supports pipelines (only OpenGL should)
	bool EditorArchivePipelines[EShaderPlatform::SP_NumPlatforms];
#endif //WITH_EDITOR
	bool bSupportsPipelines;
	bool bNativeFormat;

	class FShaderCodeLibraryIterator : public FRHIShaderLibrary::FShaderLibraryIterator
	{
	public:
		FShaderCodeLibraryIterator(TArray<FRHIShaderLibraryRef>& Stack, FRWLock& InLibraryMutex)
			: FShaderLibraryIterator(nullptr)
			, LibraryMutex(InLibraryMutex, SLT_ReadOnly)
			, IteratorImpl(Stack.CreateIterator())
		{
			if (Stack.Num() > 0)
			{
				Current = (*IteratorImpl)->CreateIterator();
				ShaderLibrarySource = *IteratorImpl;
			}
		}

		virtual bool IsValid() const final override
		{
			return IsValidRef(Current) && Current->IsValid();
		}

		virtual FRHIShaderLibrary::FShaderLibraryEntry operator*() const final override
		{
			check(IsValid());
			return *(*Current);
		}
		virtual FShaderLibraryIterator& operator++() final override
		{
			++(*Current);
			if (!Current->IsValid())
			{
				++IteratorImpl;
				if (!!IteratorImpl)
				{
					Current = (*IteratorImpl)->CreateIterator();
					ShaderLibrarySource = *IteratorImpl;
				}
			}
			return *this;
		}

	private:
		FRWScopeLock LibraryMutex;
		TArray<FRHIShaderLibraryRef>::TIterator IteratorImpl;
		TRefCountPtr<FRHIShaderLibrary::FShaderLibraryIterator> Current;
	};

public:
	static FShaderCodeLibraryImpl* Impl;

	FShaderCodeLibraryImpl(bool bInNativeFormat)
		: ShaderPlatform(SP_NumPlatforms)
		, ShaderCount(0)
		, bSupportsPipelines(false)
		, bNativeFormat(bInNativeFormat)
	{
#if WITH_EDITOR
		FMemory::Memzero(EditorShaderCodeArchive);
		FMemory::Memzero(EditorShaderStableInfo);
		FMemory::Memzero(EditorShaderCodeStats);
		FMemory::Memzero(EditorArchivePipelines);
#endif
	}

	~FShaderCodeLibraryImpl()
	{
#if WITH_EDITOR
		for (uint32 i = 0; i < EShaderPlatform::SP_NumPlatforms; i++)
		{
			if (EditorShaderCodeArchive[i])
			{
				delete EditorShaderCodeArchive[i];
			}
			if (EditorShaderStableInfo[i])
			{
				delete EditorShaderStableInfo[i];
			}
		}
		FMemory::Memzero(EditorShaderCodeArchive);
		FMemory::Memzero(EditorShaderStableInfo);
#endif
	}

	void OpenLibrary(FString const& Name, FString const& Directory)
	{
		if (ShaderPlatform < SP_NumPlatforms)
		{
			if (OpenShaderCode(Directory, ShaderPlatform, Name))
			{
				// Attempt to open the shared-cooked override code library if there is one.
				// This is probably not ideal, but it should get shared-cooks working.
				OpenShaderCode(Directory, ShaderPlatform, Name + TEXT("_SC"));

				// Inform the pipeline cache that the state of loaded libraries has changed
				FShaderPipelineCache::ShaderLibraryStateChanged(FShaderPipelineCache::Opened, ShaderPlatform, Name);
			}
			else
			{
				FName PlatformName = LegacyShaderPlatformToShaderFormat(ShaderPlatform);
				UE_LOG(LogShaderLibrary, Error, TEXT("Cooked Context: Failed to load Shared Shader Library %s from %s for %s"), *Name, *Directory, *PlatformName.GetPlainNameString());
			}
		}

#if WITH_EDITOR
		for (uint32 i = 0; i < EShaderPlatform::SP_NumPlatforms; i++)
		{
			FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[i];
			if (CodeArchive)
			{
				CodeArchive->OpenLibrary(Name);
			}
		}
		for (uint32 i = 0; i < EShaderPlatform::SP_NumPlatforms; i++)
		{
			FEditorShaderStableInfo* StableArchive = EditorShaderStableInfo[i];
			if (StableArchive)
			{
				StableArchive->OpenLibrary(Name);
			}
		}
#endif
	}

	void CloseLibrary(FString const& Name)
	{
		{
			FRWScopeLock(LibraryMutex, SLT_Write);
			for (uint32 i = ShaderCodeArchiveStack.Num(); i > 0; i--)
			{
				FRHIShaderLibraryParamRef ShaderCodeArchive = ShaderCodeArchiveStack[i - 1];
				if (ShaderCodeArchive->GetName() == Name)
				{
					ShaderCodeArchiveStack.RemoveAt(i - 1);
					break;
				}
			}
		}

		// Inform the pipeline cache that the state of loaded libraries has changed
		FShaderPipelineCache::ShaderLibraryStateChanged(FShaderPipelineCache::Closed, ShaderPlatform, Name);

#if WITH_EDITOR
		for (uint32 i = 0; i < EShaderPlatform::SP_NumPlatforms; i++)
		{
			if (EditorShaderCodeArchive[i])
			{
				EditorShaderCodeArchive[i]->CloseLibrary(Name);
			}
			if (EditorShaderStableInfo[i])
			{
				EditorShaderStableInfo[i]->CloseLibrary(Name);
			}
		}
#endif
	}

	// At runtime, open shader code collection for specified shader platform
	bool OpenShaderCode(const FString& ShaderCodeDir, EShaderPlatform InShaderPlatform, FString const& Library)
	{
		check(ShaderPlatform == SP_NumPlatforms || InShaderPlatform == ShaderPlatform);

		ShaderPlatform = InShaderPlatform;

		FRHIShaderLibraryRef ShaderCodeArchive = new FShaderCodeArchive(ShaderPlatform, ShaderCodeDir, Library);
		if (ShaderCodeArchive->GetShaderCount() > 0)
		{
			bSupportsPipelines = true;
			UE_LOG(LogShaderLibrary, Display, TEXT("Cooked Context: Using Shared Shader Library %s"), *Library);
		}
		else if (RHISupportsNativeShaderLibraries(ShaderPlatform))
		{
			ShaderCodeArchive = RHICreateShaderLibrary(ShaderPlatform, ShaderCodeDir, Library);

			if (ShaderCodeArchive.IsValid())
			{
				bNativeFormat = true;

				UE_LOG(LogShaderLibrary, Display, TEXT("Cooked Context: Loaded Native Shared Shader Library %s"), *Library);
			}
			else
			{
				UE_LOG(LogShaderLibrary, Display, TEXT("Failed to load Native Shared Shader Library: %s."), *Library);
			}
		}
		else
		{
			UE_LOG(LogShaderLibrary, Display, TEXT("Failed to load Shared Shader Library: %s and no native library supported."), *Library);
		}

		bool const bOK = IsValidRef(ShaderCodeArchive);
		if (bOK)
		{
			FRWScopeLock(LibraryMutex, SLT_Write);

			ShaderCodeArchiveStack.Add(ShaderCodeArchive);

			ShaderCount += ShaderCodeArchive->GetShaderCount();

			if (bSupportsPipelines && !bNativeFormat)
			{
				TSet<FShaderCodeLibraryPipeline> const* NewPipelines = ((FShaderCodeArchive*)ShaderCodeArchive.GetReference())->GetShaderPipelines(ShaderPlatform);
				if (NewPipelines)
				{
					Pipelines.Append(*NewPipelines);
				}
			}
		}
		return bOK;
	}

	FVertexShaderRHIRef CreateVertexShader(EShaderPlatform Platform, FSHAHash Hash)
	{
		checkSlow(Platform == GetRuntimeShaderPlatform());

		FVertexShaderRHIRef Result;
		FRHIShaderLibraryParamRef ShaderCodeArchive = FindShaderLibrary(Hash);
		if (ShaderCodeArchive)
		{
			if (bNativeFormat)
				Result = RHICreateVertexShader(ShaderCodeArchive, Hash);
			else
				Result = ((FShaderCodeArchive*)ShaderCodeArchive)->CreateVertexShader(Hash);
		}
		return Result;
	}

	FPixelShaderRHIRef CreatePixelShader(EShaderPlatform Platform, FSHAHash Hash)
	{
		checkSlow(Platform == GetRuntimeShaderPlatform());

		FPixelShaderRHIRef Result;
		FRHIShaderLibraryParamRef ShaderCodeArchive = FindShaderLibrary(Hash);
		if (ShaderCodeArchive)
		{
			if (bNativeFormat)
				Result = RHICreatePixelShader(ShaderCodeArchive, Hash);
			else
				Result = ((FShaderCodeArchive*)ShaderCodeArchive)->CreatePixelShader(Hash);
		}
		return Result;
	}

	FGeometryShaderRHIRef CreateGeometryShader(EShaderPlatform Platform, FSHAHash Hash)
	{
		checkSlow(Platform == GetRuntimeShaderPlatform());

		FGeometryShaderRHIRef Result;
		FRHIShaderLibraryParamRef ShaderCodeArchive = FindShaderLibrary(Hash);
		if (ShaderCodeArchive)
		{
			if (bNativeFormat)
				Result = RHICreateGeometryShader(ShaderCodeArchive, Hash);
			else
				Result = ((FShaderCodeArchive*)ShaderCodeArchive)->CreateGeometryShader(Hash);
		}
		return Result;
	}

	FGeometryShaderRHIRef CreateGeometryShaderWithStreamOutput(EShaderPlatform Platform, FSHAHash Hash, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
	{
		checkSlow(Platform == GetRuntimeShaderPlatform());

		FGeometryShaderRHIRef Result;
		FRHIShaderLibraryParamRef ShaderCodeArchive = FindShaderLibrary(Hash);
		if (ShaderCodeArchive)
		{
			if (bNativeFormat)
				Result = RHICreateGeometryShaderWithStreamOutput(ElementList, NumStrides, Strides, RasterizedStream, ShaderCodeArchive, Hash);
			else
				Result = ((FShaderCodeArchive*)ShaderCodeArchive)->CreateGeometryShaderWithStreamOutput(Hash, ElementList, NumStrides, Strides, RasterizedStream);
		}
		return Result;
	}

	FHullShaderRHIRef CreateHullShader(EShaderPlatform Platform, FSHAHash Hash)
	{
		checkSlow(Platform == GetRuntimeShaderPlatform());

		FHullShaderRHIRef Result;
		FRHIShaderLibraryParamRef ShaderCodeArchive = FindShaderLibrary(Hash);
		if (ShaderCodeArchive)
		{
			if (bNativeFormat)
				Result = RHICreateHullShader(ShaderCodeArchive, Hash);
			else
				Result = ((FShaderCodeArchive*)ShaderCodeArchive)->CreateHullShader(Hash);
		}
		return Result;
	}

	FDomainShaderRHIRef CreateDomainShader(EShaderPlatform Platform, FSHAHash Hash)
	{
		checkSlow(Platform == GetRuntimeShaderPlatform());

		FDomainShaderRHIRef Result;
		FRHIShaderLibraryParamRef ShaderCodeArchive = FindShaderLibrary(Hash);
		if (ShaderCodeArchive)
		{
			if (bNativeFormat)
				Result = RHICreateDomainShader(ShaderCodeArchive, Hash);
			else
				Result = ((FShaderCodeArchive*)ShaderCodeArchive)->CreateDomainShader(Hash);
		}
		return Result;
	}

	FComputeShaderRHIRef CreateComputeShader(EShaderPlatform Platform, FSHAHash Hash)
	{
		checkSlow(Platform == GetRuntimeShaderPlatform());

		FComputeShaderRHIRef Result;
		FRHIShaderLibraryParamRef ShaderCodeArchive = FindShaderLibrary(Hash);
		if (ShaderCodeArchive)
		{
			if (bNativeFormat)
				Result = RHICreateComputeShader(ShaderCodeArchive, Hash);
			else
				Result = ((FShaderCodeArchive*)ShaderCodeArchive)->CreateComputeShader(Hash);
		}
		return Result;
	}

	TRefCountPtr<FRHIShaderLibrary::FShaderLibraryIterator> CreateIterator(void)
	{
		return new FShaderCodeLibraryIterator(ShaderCodeArchiveStack, LibraryMutex);
	}

	uint32 GetShaderCount(void)
	{
		return ShaderCount;
	}

	EShaderPlatform GetRuntimeShaderPlatform(void)
	{
		return ShaderPlatform;
	}

	TSet<FShaderCodeLibraryPipeline> const* GetShaderPipelines(EShaderPlatform Platform)
	{
		if (bSupportsPipelines)
		{
			FRWScopeLock(LibraryMutex, SLT_ReadOnly);
			checkSlow(Platform == GetRuntimeShaderPlatform());
			return &Pipelines;
		}
		return nullptr;
	}

	FRHIShaderLibraryParamRef FindShaderLibrary(const FSHAHash& Hash)
	{
		FRWScopeLock(LibraryMutex, SLT_ReadOnly);
		FRHIShaderLibraryParamRef Result = nullptr;

		// Search in library opened order
		for (int32 i = 0; i < ShaderCodeArchiveStack.Num(); ++i)
		{
			FRHIShaderLibraryParamRef ShaderCodeArchive = ShaderCodeArchiveStack[i];
			if (ShaderCodeArchive->ContainsEntry(Hash))
			{
				Result = ShaderCodeArchive;
				break;
			}
		}
		return Result;
	}

	bool ContainsShaderCode(const FSHAHash& Hash)
	{
		FRHIShaderLibraryParamRef ShaderCodeArchive = FindShaderLibrary(Hash);
		if (ShaderCodeArchive)
			return true;
		else
			return false;
	}

	bool RequestShaderCode(const FSHAHash& Hash, FArchive* Ar)
	{
		FRHIShaderLibraryParamRef ShaderCodeArchive = FindShaderLibrary(Hash);
		if (ShaderCodeArchive)
			return ShaderCodeArchive->RequestEntry(Hash, Ar);
		else
			return false;
	}

	void ReleaseShaderCode(const FSHAHash& Hash)
	{
		if (!bNativeFormat)
		{
			FRHIShaderLibraryParamRef ShaderCodeArchive = FindShaderLibrary(Hash);
			if (ShaderCodeArchive)
				((FShaderCodeArchive*)ShaderCodeArchive)->ReleaseShaderCode(Hash);
		}
	}

#if WITH_EDITOR
	void CleanDirectories(TArray<FName> const& ShaderFormats)
	{
		for (FName const& Format : ShaderFormats)
		{
			FString ShaderIntermediateLocation = FPaths::ProjectSavedDir() / TEXT("Shaders") / Format.ToString();
			IFileManager::Get().DeleteDirectory(*ShaderIntermediateLocation, false, true);
		}
	}

	void CookShaderFormats(TArray<FName> const& ShaderFormats)
	{
		for (FName const& Format : ShaderFormats)
		{
			EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(Format);
			FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[Platform];
			if (!CodeArchive)
			{
				CodeArchive = new FEditorShaderCodeArchive(Format);
				EditorShaderCodeArchive[Platform] = CodeArchive;
				EditorArchivePipelines[Platform] = !bNativeFormat;
			}
			check(CodeArchive);
		}
		for (FName const& Format : ShaderFormats)
		{
			EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(Format);
			FEditorShaderStableInfo* StableArchive = EditorShaderStableInfo[Platform];
			if (!StableArchive)
			{
				StableArchive = new FEditorShaderStableInfo(Format);
				EditorShaderStableInfo[Platform] = StableArchive;
			}
			check(StableArchive);
		}
	}

	void AddShaderCode(EShaderPlatform Platform, EShaderFrequency Frequency, const FSHAHash& Hash, const TArray<uint8>& InCode, uint32 const UncompressedSize)
	{
		FScopeLock ScopeLock(&ShaderCodeCS);
		FShaderCodeStats& CodeStats = EditorShaderCodeStats[Platform];
		CodeStats.NumShaders++;
		CodeStats.ShadersSize += InCode.Num();

		FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[Platform];
		check(CodeArchive);

		if (CodeArchive->AddShader((uint8)Frequency, Hash, InCode, UncompressedSize))
		{
			CodeStats.NumUniqueShaders++;
			CodeStats.ShadersUniqueSize += InCode.Num();
		}
	}

	void AddShaderStableKeyValue(EShaderPlatform InShaderPlatform, FStableShaderKeyAndValue& StableKeyValue)
	{
		FScopeLock ScopeLock(&ShaderCodeCS);

		StableKeyValue.ComputeKeyHash();

		FEditorShaderStableInfo* StableArchive = EditorShaderStableInfo[InShaderPlatform];
		check(StableArchive);

		StableArchive->AddShader(StableKeyValue);
	}

	bool AddShaderPipeline(FShaderPipeline* Pipeline)
	{
		check(Pipeline);

		EShaderPlatform SPlatform = SP_NumPlatforms;
		for (uint8 Freq = 0; Freq < SF_Compute; Freq++)
		{
			FShader* Shader = Pipeline->GetShader((EShaderFrequency)Freq);
			if (Shader)
			{
				if (SPlatform == SP_NumPlatforms)
				{
					SPlatform = (EShaderPlatform)Shader->GetTarget().Platform;
				}
				else
				{
					check(SPlatform == (EShaderPlatform)Shader->GetTarget().Platform);
				}
			}
		}

		FScopeLock ScopeLock(&ShaderCodeCS);
		FShaderCodeStats& CodeStats = EditorShaderCodeStats[SPlatform];
		CodeStats.NumPipelines++;

		FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[SPlatform];
		check(CodeArchive);

		bool bAdded = false;
		if (EditorArchivePipelines[SPlatform] && ((FEditorShaderCodeArchive*)CodeArchive)->AddPipeline(Pipeline))
		{
			CodeStats.NumUniquePipelines++;
			bAdded = true;
		}
		return bAdded;
	}

	bool SaveShaderCode(const FString& ShaderCodeDir, const FString& MetaOutputDir, const TArray<FName>& ShaderFormats, bool bMaster, FString& OutSCLCSVPath)
	{
		bool bOk = ShaderFormats.Num() > 0;

		FScopeLock ScopeLock(&ShaderCodeCS);

		for (int32 i = 0; i < ShaderFormats.Num(); ++i)
		{
			FName ShaderFormatName = ShaderFormats[i];
			EShaderPlatform SPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);
			{
				FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[SPlatform];
				if (CodeArchive)
				{
					bOk &= CodeArchive->Finalize(ShaderCodeDir, bNativeFormat, bMaster);
				}
			}
			{
				FEditorShaderStableInfo* StableArchive = EditorShaderStableInfo[SPlatform];
				if (StableArchive)
				{
					bOk &= StableArchive->Finalize(MetaOutputDir, bNativeFormat, bMaster, OutSCLCSVPath);
				}
			}
		}

		return bOk;
	}

	bool PackageNativeShaderLibrary(const FString& ShaderCodeDir, const TArray<FName>& ShaderFormats)
	{
		bool bOK = true;
		for (int32 i = 0; i < ShaderFormats.Num(); ++i)
		{
			FName ShaderFormatName = ShaderFormats[i];
			EShaderPlatform SPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);
			FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[SPlatform];

			if (CodeArchive && CodeArchive->GetFormat()->SupportsShaderArchives())
			{
				bOK &= CodeArchive->PackageNativeShaderLibrary(ShaderCodeDir);
			}
		}
		return bOK;
	}

	void DumpShaderCodeStats()
	{
		int32 PlatformId = 0;
		for (const FShaderCodeStats& CodeStats : EditorShaderCodeStats)
		{
			if (CodeStats.NumShaders > 0)
			{
				float UniqueSize = CodeStats.ShadersUniqueSize;
				float UniqueSizeMB = FUnitConversion::Convert(UniqueSize, EUnit::Bytes, EUnit::Megabytes);
				float TotalSize = CodeStats.ShadersSize;
				float TotalSizeMB = FUnitConversion::Convert(TotalSize, EUnit::Bytes, EUnit::Megabytes);

				UE_LOG(LogShaderLibrary, Display, TEXT(""));
				UE_LOG(LogShaderLibrary, Display, TEXT("Shader Code Stats: %s"), *LegacyShaderPlatformToShaderFormat((EShaderPlatform)PlatformId).ToString());
				UE_LOG(LogShaderLibrary, Display, TEXT("================="));
				UE_LOG(LogShaderLibrary, Display, TEXT("Unique Shaders: %d, Total Shaders: %d"), CodeStats.NumUniqueShaders, CodeStats.NumShaders);
				UE_LOG(LogShaderLibrary, Display, TEXT("Unique Shaders Size: %.2fmb, Total Shader Size: %.2fmb"), UniqueSizeMB, TotalSizeMB);
				UE_LOG(LogShaderLibrary, Display, TEXT("================="));
			}

			PlatformId++;
		}
	}
#endif// WITH_EDITOR
};

FShaderCodeLibraryImpl* FShaderCodeLibraryImpl::Impl = nullptr;

static void FShaderCodeLibraryPluginMountedCallback(IPlugin& Plugin)
{
	if (Plugin.CanContainContent() && Plugin.IsEnabled())
	{
		FShaderCodeLibrary::OpenLibrary(Plugin.GetName(), Plugin.GetBaseDir());
		FShaderCodeLibrary::OpenLibrary(Plugin.GetName(), Plugin.GetContentDir());
	}
}

void FShaderCodeLibrary::InitForRuntime(EShaderPlatform ShaderPlatform)
{
	check(FPlatformProperties::RequiresCookedData());

	if (FShaderCodeLibraryImpl::Impl != nullptr)
	{
		//cooked, can't change shader platform on the fly
		check(FShaderCodeLibraryImpl::Impl->GetRuntimeShaderPlatform() == ShaderPlatform);
		return;
	}

	// Cannot be enabled by the server, pointless if we can't ever render and not compatible with cook-on-the-fly
	bool bEnable = !FPlatformProperties::IsServerOnly() && FApp::CanEverRender();
#if !UE_BUILD_SHIPPING
	FString FileHostIP;
	const bool bCookOnTheFly = FParse::Value(FCommandLine::Get(), TEXT("filehostip"), FileHostIP);
	bEnable &= !bCookOnTheFly;
#endif

	if (bEnable)
	{
		FShaderCodeLibraryImpl::Impl = new FShaderCodeLibraryImpl(false);
		if (FShaderCodeLibraryImpl::Impl->OpenShaderCode(FPaths::ProjectContentDir(), ShaderPlatform, TEXT("Global")))
		{
			IPluginManager::Get().OnNewPluginMounted().AddStatic(&FShaderCodeLibraryPluginMountedCallback);
		
#if !UE_BUILD_SHIPPING
			// support shared cooked builds by also opening the shared cooked build shader code file
			FShaderCodeLibraryImpl::Impl->OpenShaderCode(FPaths::ProjectContentDir(), ShaderPlatform, TEXT("Global_SC"));
#endif

			auto Plugins = IPluginManager::Get().GetEnabledPluginsWithContent();
			for (auto Plugin : Plugins)
			{
				FShaderCodeLibraryPluginMountedCallback(*Plugin);
			}

		}
		else
		{
			Shutdown();
		}
	}
}

void FShaderCodeLibrary::Shutdown()
{
	if (FShaderCodeLibraryImpl::Impl)
	{
#if WITH_EDITOR
		DumpShaderCodeStats();
#endif
		delete FShaderCodeLibraryImpl::Impl;
		FShaderCodeLibraryImpl::Impl = nullptr;
	}
}

bool FShaderCodeLibrary::IsEnabled()
{
	return FShaderCodeLibraryImpl::Impl != nullptr;
}

bool FShaderCodeLibrary::ContainsShaderCode(const FSHAHash& Hash)
{
	if (FShaderCodeLibraryImpl::Impl)
	{
		return FShaderCodeLibraryImpl::Impl->ContainsShaderCode(Hash);
	}
	return false;
}

bool FShaderCodeLibrary::RequestShaderCode(const FSHAHash& Hash, FArchive* Ar)
{
	if (FShaderCodeLibraryImpl::Impl)
	{
		return FShaderCodeLibraryImpl::Impl->RequestShaderCode(Hash, Ar);
	}
	return false;
}

void FShaderCodeLibrary::ReleaseShaderCode(const FSHAHash& Hash)
{
	if (FShaderCodeLibraryImpl::Impl)
	{
		return FShaderCodeLibraryImpl::Impl->ReleaseShaderCode(Hash);
	}
}

FVertexShaderRHIRef FShaderCodeLibrary::CreateVertexShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FVertexShaderRHIRef Shader;
	if (FShaderCodeLibraryImpl::Impl && FPlatformProperties::RequiresCookedData())
	{
		Shader = FShaderCodeLibraryImpl::Impl->CreateVertexShader(Platform, Hash);
	}
	if (!IsValidRef(Shader))
	{
		Shader = RHICreateVertexShader(Code);
	}
	SafeAssignHash(Shader, Hash);
	return Shader;
}

FPixelShaderRHIRef FShaderCodeLibrary::CreatePixelShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FPixelShaderRHIRef Shader;
	if (FShaderCodeLibraryImpl::Impl && FPlatformProperties::RequiresCookedData())
	{
		Shader = FShaderCodeLibraryImpl::Impl->CreatePixelShader(Platform, Hash);
	}
	if (!IsValidRef(Shader))
	{
		Shader = RHICreatePixelShader(Code);
	}
	SafeAssignHash(Shader, Hash);
	return Shader;
}

FGeometryShaderRHIRef FShaderCodeLibrary::CreateGeometryShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FGeometryShaderRHIRef Shader;
	if (FShaderCodeLibraryImpl::Impl && FPlatformProperties::RequiresCookedData())
	{
		Shader = FShaderCodeLibraryImpl::Impl->CreateGeometryShader(Platform, Hash);
	}
	if (!IsValidRef(Shader))
	{
		Shader = RHICreateGeometryShader(Code);
	}
	SafeAssignHash(Shader, Hash);
	return Shader;
}

FGeometryShaderRHIRef FShaderCodeLibrary::CreateGeometryShaderWithStreamOutput(EShaderPlatform Platform, FSHAHash Hash, const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	FGeometryShaderRHIRef Shader;
	if (FShaderCodeLibraryImpl::Impl && FPlatformProperties::RequiresCookedData())
	{
		Shader = FShaderCodeLibraryImpl::Impl->CreateGeometryShaderWithStreamOutput(Platform, Hash, ElementList, NumStrides, Strides, RasterizedStream);
	}
	if (!IsValidRef(Shader))
	{
		Shader = RHICreateGeometryShaderWithStreamOutput(Code, ElementList, NumStrides, Strides, RasterizedStream);
	}
	SafeAssignHash(Shader, Hash);
	return Shader;
}

FHullShaderRHIRef FShaderCodeLibrary::CreateHullShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FHullShaderRHIRef Shader;
	if (FShaderCodeLibraryImpl::Impl && FPlatformProperties::RequiresCookedData())
	{
		Shader = FShaderCodeLibraryImpl::Impl->CreateHullShader(Platform, Hash);
	}
	if (!IsValidRef(Shader))
	{
		Shader = RHICreateHullShader(Code);
	}
	SafeAssignHash(Shader, Hash);
	return Shader;
}

FDomainShaderRHIRef FShaderCodeLibrary::CreateDomainShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FDomainShaderRHIRef Shader;
	if (FShaderCodeLibraryImpl::Impl && FPlatformProperties::RequiresCookedData())
	{
		Shader = FShaderCodeLibraryImpl::Impl->CreateDomainShader(Platform, Hash);
	}
	if (!IsValidRef(Shader))
	{
		Shader = RHICreateDomainShader(Code);
	}
	SafeAssignHash(Shader, Hash);
	return Shader;
}

FComputeShaderRHIRef FShaderCodeLibrary::CreateComputeShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FComputeShaderRHIRef Shader;
	if (FShaderCodeLibraryImpl::Impl && FPlatformProperties::RequiresCookedData())
	{
		Shader = FShaderCodeLibraryImpl::Impl->CreateComputeShader(Platform, Hash);
	}
	if (!IsValidRef(Shader))
	{
		Shader = RHICreateComputeShader(Code);
	}
	SafeAssignHash(Shader, Hash);
	FPipelineFileCache::CacheComputePSO(GetTypeHash(Shader.GetReference()), Shader.GetReference());
	Shader->SetStats(FPipelineFileCache::RegisterPSOStats(GetTypeHash(Shader.GetReference())));
	return Shader;
}

TRefCountPtr<FRHIShaderLibrary::FShaderLibraryIterator> FShaderCodeLibrary::CreateIterator(void)
{
	TRefCountPtr<FRHIShaderLibrary::FShaderLibraryIterator> It;
	if (FShaderCodeLibraryImpl::Impl && FPlatformProperties::RequiresCookedData())
	{
		It = FShaderCodeLibraryImpl::Impl->CreateIterator();
	}
	return It;
}

uint32 FShaderCodeLibrary::GetShaderCount(void)
{
	uint32 Num = 0;
	if (FShaderCodeLibraryImpl::Impl && FPlatformProperties::RequiresCookedData())
	{
		Num = FShaderCodeLibraryImpl::Impl->GetShaderCount();
	}
	return Num;
}

TSet<FShaderCodeLibraryPipeline> const* FShaderCodeLibrary::GetShaderPipelines(EShaderPlatform Platform)
{
	TSet<FShaderCodeLibraryPipeline> const* Pipelines = nullptr;
	if (FShaderCodeLibraryImpl::Impl && FPlatformProperties::RequiresCookedData())
	{
		Pipelines = FShaderCodeLibraryImpl::Impl->GetShaderPipelines(Platform);
	}
	return Pipelines;
}

EShaderPlatform FShaderCodeLibrary::GetRuntimeShaderPlatform(void)
{
	EShaderPlatform Platform = SP_NumPlatforms;
	if (FShaderCodeLibraryImpl::Impl && FPlatformProperties::RequiresCookedData())
	{
		Platform = FShaderCodeLibraryImpl::Impl->GetRuntimeShaderPlatform();
	}
	return Platform;
}

void FShaderCodeLibrary::OpenLibrary(FString const& Name, FString const& Directory)
{
	if (FShaderCodeLibraryImpl::Impl)
	{
		FShaderCodeLibraryImpl::Impl->OpenLibrary(Name, Directory);
	}
}

void FShaderCodeLibrary::CloseLibrary(FString const& Name)
{
	if (FShaderCodeLibraryImpl::Impl)
	{
		FShaderCodeLibraryImpl::Impl->CloseLibrary(Name);
	}
}

#if WITH_EDITOR
void FShaderCodeLibrary::InitForCooking(bool bNativeFormat)
{
	FShaderCodeLibraryImpl::Impl = new FShaderCodeLibraryImpl(bNativeFormat);
}

void FShaderCodeLibrary::CleanDirectories(TArray<FName> const& ShaderFormats)
{
	if (FShaderCodeLibraryImpl::Impl)
	{
		FShaderCodeLibraryImpl::Impl->CleanDirectories(ShaderFormats);
	}
}

void FShaderCodeLibrary::CookShaderFormats(TArray<FName> const& ShaderFormats)
{
	if (FShaderCodeLibraryImpl::Impl)
	{
		FShaderCodeLibraryImpl::Impl->CookShaderFormats(ShaderFormats);
	}
}

bool FShaderCodeLibrary::AddShaderCode(EShaderPlatform ShaderPlatform, EShaderFrequency Frequency, const FSHAHash& Hash, const TArray<uint8>& InCode, uint32 const UncompressedSize)
{
#if WITH_EDITOR
	if (FShaderCodeLibraryImpl::Impl)
	{
		FShaderCodeLibraryImpl::Impl->AddShaderCode(ShaderPlatform, Frequency, Hash, InCode, UncompressedSize);
		return true;
	}
#endif// WITH_EDITOR

	return false;
}

bool FShaderCodeLibrary::NeedsShaderStableKeys()
{
#if WITH_EDITOR
	if (FShaderCodeLibraryImpl::Impl)
	{
		return true;
	}
#endif// WITH_EDITOR
	return false;
}

void FShaderCodeLibrary::AddShaderStableKeyValue(EShaderPlatform ShaderPlatform, FStableShaderKeyAndValue& StableKeyValue)
{
#if WITH_EDITOR
	if (FShaderCodeLibraryImpl::Impl)
	{
		FShaderCodeLibraryImpl::Impl->AddShaderStableKeyValue(ShaderPlatform, StableKeyValue);
	}
#endif// WITH_EDITOR
}

bool FShaderCodeLibrary::AddShaderPipeline(FShaderPipeline* Pipeline)
{
#if WITH_EDITOR
	if (FShaderCodeLibraryImpl::Impl && Pipeline)
	{
		FShaderCodeLibraryImpl::Impl->AddShaderPipeline(Pipeline);
		return true;
	}
#endif// WITH_EDITOR

	return false;
}

bool FShaderCodeLibrary::SaveShaderCodeMaster(const FString& OutputDir, const FString& MetaOutputDir, const TArray<FName>& ShaderFormats, FString& OutSCLCSVPath)
{
	if (FShaderCodeLibraryImpl::Impl)
	{
		return FShaderCodeLibraryImpl::Impl->SaveShaderCode(OutputDir, MetaOutputDir, ShaderFormats, true, OutSCLCSVPath);
	}

	return false;
}

bool FShaderCodeLibrary::SaveShaderCodeChild(const FString& OutputDir, const FString& MetaOutputDir, const TArray<FName>& ShaderFormats)
{
	if (FShaderCodeLibraryImpl::Impl)
	{
		FString OutSCLCSVPathJunk;
		return FShaderCodeLibraryImpl::Impl->SaveShaderCode(OutputDir, MetaOutputDir, ShaderFormats, false, OutSCLCSVPathJunk);
	}

	return false;
}

bool FShaderCodeLibrary::PackageNativeShaderLibrary(const FString& ShaderCodeDir, const TArray<FName>& ShaderFormats)
{
	if (FShaderCodeLibraryImpl::Impl)
	{
		return FShaderCodeLibraryImpl::Impl->PackageNativeShaderLibrary(ShaderCodeDir, ShaderFormats);
	}

	return false;
}

void FShaderCodeLibrary::DumpShaderCodeStats()
{
	if (FShaderCodeLibraryImpl::Impl)
	{
		FShaderCodeLibraryImpl::Impl->DumpShaderCodeStats();
	}
}
#endif// WITH_EDITOR

void FShaderCodeLibrary::SafeAssignHash(FRHIShader* InShader, const FSHAHash& Hash)
{
	if (InShader)
	{
		InShader->SetHash(Hash);
	}
}
