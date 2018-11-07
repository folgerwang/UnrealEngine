// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PakFileUtilities.h"
#include "IPlatformFilePak.h"
#include "Misc/SecureHash.h"
#include "Math/BigInt.h"
#include "SignedArchiveWriter.h"
#include "KeyGenerator.h"
#include "Misc/AES.h"
#include "Templates/UniquePtr.h"
#include "Serialization/LargeMemoryWriter.h"
#include "ProfilingDebugging/DiagnosticTable.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "Misc/Compression.h"
#include "Features/IModularFeatures.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformFilemanager.h"
#include "Async/ParallelFor.h"

struct FNamedAESKey
{
	FString Name;
	FGuid Guid;
	FAES::FAESKey Key;

	bool IsValid() const
	{
		return Key.IsValid();
	}
};

struct FPakCommandLineParameters
{
	FPakCommandLineParameters()
		: CompressionBlockSize(64*1024)
		, CompressionBitWindow(DEFAULT_ZLIB_BIT_WINDOW)
		, FileSystemBlockSize(0)
		, PatchFilePadAlign(0)
		, GeneratePatch(false)
		, EncryptIndex(false)
		, UseCustomCompressor(false)
		, OverridePlatformCompressor(false)
	{}

	int32  CompressionBlockSize;
	int32  CompressionBitWindow;
	int64  FileSystemBlockSize;
	int64  PatchFilePadAlign;
	bool   GeneratePatch;
	FString SourcePatchPakFilename;
	FString SourcePatchDiffDirectory;
	bool EncryptIndex;
	bool UseCustomCompressor;
	bool OverridePlatformCompressor;
};

struct FPakEntryPair
{
	FString Filename;
	FPakEntry Info;
};

struct FPakInputPair
{
	FString Source;
	FString Dest;
	uint64 SuggestedOrder; 
	bool bNeedsCompression;
	bool bNeedEncryption;
	bool bIsDeleteRecord;	// This is used for patch PAKs when a file is deleted from one patch to the next

	FPakInputPair()
		: SuggestedOrder(MAX_uint64)
		, bNeedsCompression(false)
		, bNeedEncryption(false)
		, bIsDeleteRecord(false)
	{}

	FPakInputPair(const FString& InSource, const FString& InDest)
		: Source(InSource)
		, Dest(InDest)
		, bNeedsCompression(false)
		, bNeedEncryption(false)
		, bIsDeleteRecord(false)
	{}

	FORCEINLINE bool operator==(const FPakInputPair& Other) const
	{
		return Source == Other.Source;
	}
};

struct FPakEntryOrder
{
	FPakEntryOrder() : Order(MAX_uint64) {}
	FString Filename;
	uint64  Order;
};

struct FCompressedFileBuffer
{
	FCompressedFileBuffer()
		: OriginalSize(0)
		,TotalCompressedSize(0)
		,FileCompressionBlockSize(0)
		,CompressedBufferSize(0)
	{

	}

	void Reinitialize(FArchive* File,ECompressionFlags CompressionMethod,int64 CompressionBlockSize)
	{
		OriginalSize = File->TotalSize();
		TotalCompressedSize = 0;
		FileCompressionBlockSize = 0;
		FileCompressionMethod = CompressionMethod;
		CompressedBlocks.Reset();
		CompressedBlocks.AddUninitialized((OriginalSize+CompressionBlockSize-1)/CompressionBlockSize);
	}

	void EnsureBufferSpace(int64 RequiredSpace)
	{
		if(RequiredSpace > CompressedBufferSize)
		{
			TUniquePtr<uint8[]> NewCompressedBuffer = MakeUnique<uint8[]>(RequiredSpace);
			FMemory::Memcpy(NewCompressedBuffer.Get(), CompressedBuffer.Get(), CompressedBufferSize);
			CompressedBuffer = MoveTemp(NewCompressedBuffer);
			CompressedBufferSize = RequiredSpace;
		}
	}

	bool CompressFileToWorkingBuffer(const FPakInputPair& InFile,uint8*& InOutPersistentBuffer,int64& InOutBufferSize,ECompressionFlags CompressionMethod,const int32 CompressionBlockSize,const int32 CompressionBitWindow);

	int64				OriginalSize;
	int64				TotalCompressedSize;
	int32				FileCompressionBlockSize;
	ECompressionFlags		FileCompressionMethod;
	TArray<FPakCompressedBlock>	CompressedBlocks;
	int64				CompressedBufferSize;
	TUniquePtr<uint8[]>		CompressedBuffer;
};

FString GetLongestPath(TArray<FPakInputPair>& FilesToAdd)
{
	FString LongestPath;
	int32 MaxNumDirectories = 0;

	for (int32 FileIndex = 0; FileIndex < FilesToAdd.Num(); FileIndex++)
	{
		FString& Filename = FilesToAdd[FileIndex].Dest;
		int32 NumDirectories = 0;
		for (int32 Index = 0; Index < Filename.Len(); Index++)
		{
			if (Filename[Index] == '/')
			{
				NumDirectories++;
			}
		}
		if (NumDirectories > MaxNumDirectories)
		{
			LongestPath = Filename;
			MaxNumDirectories = NumDirectories;
		}
	}
	return FPaths::GetPath(LongestPath) + TEXT("/");
}

FString GetCommonRootPath(TArray<FPakInputPair>& FilesToAdd)
{
	FString Root = GetLongestPath(FilesToAdd);
	for (int32 FileIndex = 0; FileIndex < FilesToAdd.Num() && Root.Len(); FileIndex++)
	{
		FString Filename(FilesToAdd[FileIndex].Dest);
		FString Path = FPaths::GetPath(Filename) + TEXT("/");
		int32 CommonSeparatorIndex = -1;
		int32 SeparatorIndex = Path.Find(TEXT("/"), ESearchCase::CaseSensitive);
		while (SeparatorIndex >= 0)
		{
			if (FCString::Strnicmp(*Root, *Path, SeparatorIndex + 1) != 0)
			{
				break;
			}
			CommonSeparatorIndex = SeparatorIndex;
			if (CommonSeparatorIndex + 1 < Path.Len())
			{
				SeparatorIndex = Path.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, CommonSeparatorIndex + 1);
			}
			else
			{
				break;
			}
		}
		if ((CommonSeparatorIndex + 1) < Root.Len())
		{
			Root = Root.Mid(0, CommonSeparatorIndex + 1);
		}
	}
	return Root;
}

bool FCompressedFileBuffer::CompressFileToWorkingBuffer(const FPakInputPair& InFile, uint8*& InOutPersistentBuffer, int64& InOutBufferSize, ECompressionFlags CompressionMethod, const int32 CompressionBlockSize, const int32 CompressionBitWindow)
{
	TUniquePtr<FArchive> FileHandle(IFileManager::Get().CreateFileReader(*InFile.Source));
	if(!FileHandle)
	{
		TotalCompressedSize = 0;
		return false;
	}

	Reinitialize(FileHandle.Get(), CompressionMethod, CompressionBlockSize);
	const int64 FileSize = OriginalSize;
	const int64 PaddedEncryptedFileSize = Align(FileSize,FAES::AESBlockSize);
	if(InOutBufferSize < PaddedEncryptedFileSize)
	{
		InOutPersistentBuffer = (uint8*)FMemory::Realloc(InOutPersistentBuffer,PaddedEncryptedFileSize);
		InOutBufferSize = FileSize;
	}

	// Load to buffer
	FileHandle->Serialize(InOutPersistentBuffer,FileSize);

	// Build buffers for working
	int64 UncompressedSize = FileSize;
	int32 CompressionBufferSize = Align(FCompression::CompressMemoryBound(CompressionMethod,CompressionBlockSize,CompressionBitWindow),FAES::AESBlockSize);
	EnsureBufferSpace(Align(FCompression::CompressMemoryBound(CompressionMethod,FileSize,CompressionBitWindow),FAES::AESBlockSize));


	TotalCompressedSize = 0;
	int64 UncompressedBytes = 0;
	int32 CurrentBlock = 0;
	while(UncompressedSize)
	{
		int32 BlockSize = (int32)FMath::Min<int64>(UncompressedSize,CompressionBlockSize);
		int32 MaxCompressedBlockSize = FCompression::CompressMemoryBound(CompressionMethod, BlockSize, CompressionBitWindow);
		int32 CompressedBlockSize = FMath::Max<int32>(CompressionBufferSize, MaxCompressedBlockSize);
		FileCompressionBlockSize = FMath::Max<uint32>(BlockSize, FileCompressionBlockSize);
		EnsureBufferSpace(Align(TotalCompressedSize+CompressedBlockSize,FAES::AESBlockSize));
		if(!FCompression::CompressMemory(CompressionMethod,CompressedBuffer.Get()+TotalCompressedSize,CompressedBlockSize,InOutPersistentBuffer+UncompressedBytes,BlockSize,CompressionBitWindow))
		{
			return false;
		}
		UncompressedSize -= BlockSize;
		UncompressedBytes += BlockSize;

		CompressedBlocks[CurrentBlock].CompressedStart = TotalCompressedSize;
		CompressedBlocks[CurrentBlock].CompressedEnd = TotalCompressedSize+CompressedBlockSize;
		++CurrentBlock;

		TotalCompressedSize += CompressedBlockSize;

		if(InFile.bNeedEncryption)
		{
			int32 EncryptionBlockPadding = Align(TotalCompressedSize,FAES::AESBlockSize);
			for(int64 FillIndex=TotalCompressedSize; FillIndex < EncryptionBlockPadding; ++FillIndex)
			{
				// Fill the trailing buffer with bytes from file. Note that this is now from a fixed location
				// rather than a random one so that we produce deterministic results
				CompressedBuffer.Get()[FillIndex] = CompressedBuffer.Get()[FillIndex % TotalCompressedSize];
			}
			TotalCompressedSize += EncryptionBlockPadding - TotalCompressedSize;
		}
	}

	return true;
}

bool PrepareCopyFileToPak(const FString& InMountPoint, const FPakInputPair& InFile, uint8*& InOutPersistentBuffer, int64& InOutBufferSize, FPakEntryPair& OutNewEntry, uint8*& OutDataToWrite, int64& OutSizeToWrite, const FNamedAESKey& InEncryptionKey)
{	
	TUniquePtr<FArchive> FileHandle(IFileManager::Get().CreateFileReader(*InFile.Source));
	bool bFileExists = FileHandle.IsValid();
	if (bFileExists)
	{
		const int64 FileSize = FileHandle->TotalSize();
		const int64 PaddedEncryptedFileSize = Align(FileSize, FAES::AESBlockSize); 
		OutNewEntry.Filename = InFile.Dest.Mid(InMountPoint.Len());
		OutNewEntry.Info.Offset = 0; // Don't serialize offsets here.
		OutNewEntry.Info.Size = FileSize;
		OutNewEntry.Info.UncompressedSize = FileSize;
		OutNewEntry.Info.CompressionMethod = COMPRESS_None;
		OutNewEntry.Info.SetEncrypted( InFile.bNeedEncryption && InEncryptionKey.IsValid() );
		OutNewEntry.Info.SetDeleteRecord(false);

		if (InOutBufferSize < PaddedEncryptedFileSize)
		{
			InOutPersistentBuffer = (uint8*)FMemory::Realloc(InOutPersistentBuffer, PaddedEncryptedFileSize);
			InOutBufferSize = FileSize;
		}

		// Load to buffer
		FileHandle->Serialize(InOutPersistentBuffer, FileSize);

		{
			OutSizeToWrite = FileSize;
			if (InFile.bNeedEncryption && InEncryptionKey.IsValid())
			{
				for(int64 FillIndex=FileSize; FillIndex < PaddedEncryptedFileSize && InFile.bNeedEncryption; ++FillIndex)
				{
					// Fill the trailing buffer with bytes from file. Note that this is now from a fixed location
					// rather than a random one so that we produce deterministic results
					InOutPersistentBuffer[FillIndex] = InOutPersistentBuffer[FillIndex % FileSize];
				}

				//Encrypt the buffer before writing it to disk
				FAES::EncryptData(InOutPersistentBuffer, PaddedEncryptedFileSize, InEncryptionKey.Key);
				// Update the size to be written
				OutSizeToWrite = PaddedEncryptedFileSize;
				OutNewEntry.Info.SetEncrypted( true );
			}

			// Calculate the buffer hash value
			FSHA1::HashBuffer(InOutPersistentBuffer,FileSize,OutNewEntry.Info.Hash);			
			OutDataToWrite = InOutPersistentBuffer;
		}
	}
	return bFileExists;
}

void FinalizeCopyCompressedFileToPak(FArchive& InPak, const FCompressedFileBuffer& CompressedFile, FPakEntryPair& OutNewEntry)
{
	check(CompressedFile.TotalCompressedSize != 0);

	check(OutNewEntry.Info.CompressionBlocks.Num() == CompressedFile.CompressedBlocks.Num());
	check(OutNewEntry.Info.CompressionMethod == CompressedFile.FileCompressionMethod);

	int64 TellPos = OutNewEntry.Info.GetSerializedSize(FPakInfo::PakFile_Version_Latest);
	const TArray<FPakCompressedBlock>& Blocks = CompressedFile.CompressedBlocks;
	for (int32 BlockIndex = 0, BlockCount = Blocks.Num(); BlockIndex < BlockCount; ++BlockIndex)
	{
		OutNewEntry.Info.CompressionBlocks[BlockIndex].CompressedStart = Blocks[BlockIndex].CompressedStart + TellPos;
		OutNewEntry.Info.CompressionBlocks[BlockIndex].CompressedEnd = Blocks[BlockIndex].CompressedEnd + TellPos;
	}
}

bool PrepareCopyCompressedFileToPak(const FString& InMountPoint, const FPakInputPair& InFile, const FCompressedFileBuffer& CompressedFile, FPakEntryPair& OutNewEntry, uint8*& OutDataToWrite, int64& OutSizeToWrite, const FNamedAESKey& InEncryptionKey)
{
	if (CompressedFile.TotalCompressedSize == 0)
	{
		return false;
	}

	OutNewEntry.Info.CompressionMethod = CompressedFile.FileCompressionMethod;
	OutNewEntry.Info.CompressionBlocks.AddZeroed(CompressedFile.CompressedBlocks.Num());

	if (InFile.bNeedEncryption && InEncryptionKey.IsValid())
	{
		FAES::EncryptData(CompressedFile.CompressedBuffer.Get(), CompressedFile.TotalCompressedSize, InEncryptionKey.Key);
	}

	//Hash the final buffer thats written
	FSHA1 Hash;
	Hash.Update(CompressedFile.CompressedBuffer.Get(), CompressedFile.TotalCompressedSize);
	Hash.Final();

	// Update file size & Hash
	OutNewEntry.Info.CompressionBlockSize = CompressedFile.FileCompressionBlockSize;
	OutNewEntry.Info.UncompressedSize = CompressedFile.OriginalSize;
	OutNewEntry.Info.Size = CompressedFile.TotalCompressedSize;
	Hash.GetHash(OutNewEntry.Info.Hash);

	//	Write the header, then the data
	OutNewEntry.Filename = InFile.Dest.Mid(InMountPoint.Len());
	OutNewEntry.Info.Offset = 0; // Don't serialize offsets here.
	OutNewEntry.Info.SetEncrypted( InFile.bNeedEncryption && InEncryptionKey.IsValid() );
	OutNewEntry.Info.SetDeleteRecord(false);
	OutSizeToWrite = CompressedFile.TotalCompressedSize;
	OutDataToWrite = CompressedFile.CompressedBuffer.Get();
	//OutNewEntry.Info.Serialize(InPak,FPakInfo::PakFile_Version_Latest);	
	//InPak.Serialize(CompressedFile.CompressedBuffer.Get(), CompressedFile.TotalCompressedSize);

	return true;
}

void PrepareDeleteRecordForPak(const FString& InMountPoint, const FPakInputPair InDeletedFile, FPakEntryPair& OutNewEntry)
{
	OutNewEntry.Filename = InDeletedFile.Dest.Mid(InMountPoint.Len());
	OutNewEntry.Info.SetDeleteRecord(true);
}

bool ProcessOrderFile(const TCHAR* ResponseFile, TMap<FString, uint64>& OrderMap)
{
	// List of all items to add to pak file
	FString Text;
	UE_LOG(LogPakFile, Display, TEXT("Loading pak order file %s..."), ResponseFile);
	if (FFileHelper::LoadFileToString(Text, ResponseFile))
	{
		// Read all lines
		TArray<FString> Lines;
		Text.ParseIntoArray(Lines, TEXT("\n"), true);
		for (int32 EntryIndex = 0; EntryIndex < Lines.Num(); EntryIndex++)
		{
			Lines[EntryIndex].ReplaceInline(TEXT("\r"), TEXT(""));
			Lines[EntryIndex].ReplaceInline(TEXT("\n"), TEXT(""));
			int32 OpenOrderNumber = EntryIndex;
			if (Lines[EntryIndex].FindLastChar('"', OpenOrderNumber))
			{
				FString ReadNum = Lines[EntryIndex].RightChop(OpenOrderNumber+1);
				Lines[EntryIndex] = Lines[EntryIndex].Left(OpenOrderNumber+1);
				ReadNum.TrimStartInline();
				if (ReadNum.IsNumeric())
				{
					OpenOrderNumber = FCString::Atoi(*ReadNum);
				}
			}
			Lines[EntryIndex] = Lines[EntryIndex].TrimQuotes();
			FString Path=FString::Printf(TEXT("%s"), *Lines[EntryIndex]);
			FPaths::NormalizeFilename(Path);
			Path = Path.ToLower();
#if 0
			if (Path.EndsWith("uexp"))
			{
				OpenOrderNumber += (1 << 29);
			}
			if (Path.EndsWith("ubulk"))
			{
				OpenOrderNumber += (1 << 30);
			}
#endif
			OrderMap.Add(Path, OpenOrderNumber);
		}
		UE_LOG(LogPakFile, Display, TEXT("Finished loading pak order file %s."), ResponseFile);
		return true;
	}
	else 
	{
		UE_LOG(LogPakFile, Error, TEXT("Unable to load pak order file %s."), ResponseFile);
		return false;
	}
}

static void CommandLineParseHelper(const TCHAR* InCmdLine, TArray<FString>& Tokens, TArray<FString>& Switches)
{
	FString NextToken;
	while(FParse::Token(InCmdLine,NextToken,false))
	{
		if((**NextToken == TCHAR('-')))
		{
			new(Switches)FString(NextToken.Mid(1));
		}
		else
		{
			new(Tokens)FString(NextToken);
		}
	}
}

void PreProcessCommandline(const TCHAR* CmdLine, FPakCommandLineParameters& CmdLineParameters)
{

	FString CompressorFileName;
	if (FParse::Value(CmdLine, TEXT("customcompressor="), CompressorFileName))
	{
		FPlatformProcess::AddDllDirectory(*FPaths::GetPath(CompressorFileName));

		void* CustomCompressorDll = FPlatformProcess::GetDllHandle(*CompressorFileName);
		if (CustomCompressorDll == nullptr)
		{
			UE_LOG(LogPakFile, Error, TEXT("Unable to load custom compressor from %s"), *CompressorFileName);
			return;
		}

		UE_LOG(LogPakFile, Display, TEXT("Loaded custom compressor from %s."), *CompressorFileName);

		static const TCHAR* CreateCustomCompressorExport = TEXT("CreateCustomCompressor");
		typedef ICustomCompressor* (CreateCustomCompressorFunc)(const TCHAR*);
		CreateCustomCompressorFunc* CreateCustomCompressor = reinterpret_cast<CreateCustomCompressorFunc*>(FPlatformProcess::GetDllExport(CustomCompressorDll, CreateCustomCompressorExport));
		if (CreateCustomCompressor == nullptr)
		{
			UE_LOG(LogPakFile, Error, TEXT("Unable to find exported symbol '%s' in '%s'"), CreateCustomCompressorExport, *CompressorFileName);
			return;
		}

		ICustomCompressor* Compressor = (*CreateCustomCompressor)(CmdLine);
		if (Compressor == nullptr)
		{
			UE_LOG(LogPakFile, Error, TEXT("Failed to create custom compressor from '%s'"), *CompressorFileName);
			return;
		}

		IModularFeatures::Get().RegisterModularFeature(CUSTOM_COMPRESSOR_FEATURE_NAME, Compressor);
		CmdLineParameters.UseCustomCompressor = true;
	}

}

void ProcessCommandLine(const TCHAR* CmdLine, const TArray<FString>& NonOptionArguments, TArray<FPakInputPair>& Entries, FPakCommandLineParameters& CmdLineParameters)
{
	// List of all items to add to pak file
	FString ResponseFile;
	FString ClusterSizeString;

	if (FParse::Value(CmdLine, TEXT("-blocksize="), ClusterSizeString) && 
		FParse::Value(CmdLine, TEXT("-blocksize="), CmdLineParameters.FileSystemBlockSize))
	{
		if (ClusterSizeString.EndsWith(TEXT("MB")))
		{
			CmdLineParameters.FileSystemBlockSize *= 1024*1024;
		}
		else if (ClusterSizeString.EndsWith(TEXT("KB")))
		{
			CmdLineParameters.FileSystemBlockSize *= 1024;
		}
	}
	else
	{
		CmdLineParameters.FileSystemBlockSize = 0;
	}

	FString CompBlockSizeString;
	if (FParse::Value(CmdLine, TEXT("-compressionblocksize="), CompBlockSizeString) &&
		FParse::Value(CmdLine, TEXT("-compressionblocksize="), CmdLineParameters.CompressionBlockSize))
	{
		if (CompBlockSizeString.EndsWith(TEXT("MB")))
		{
			CmdLineParameters.CompressionBlockSize *= 1024 * 1024;
		}
		else if (CompBlockSizeString.EndsWith(TEXT("KB")))
		{
			CmdLineParameters.CompressionBlockSize *= 1024;
		}
	}

	if (!FParse::Value(CmdLine, TEXT("-bitwindow="), CmdLineParameters.CompressionBitWindow))
	{
		CmdLineParameters.CompressionBitWindow = DEFAULT_ZLIB_BIT_WINDOW;
	}

	if (!FParse::Value(CmdLine, TEXT("-patchpaddingalign="), CmdLineParameters.PatchFilePadAlign))
	{
		CmdLineParameters.PatchFilePadAlign = 0;
	}

	if (FParse::Param(CmdLine, TEXT("encryptindex")))
	{
		CmdLineParameters.EncryptIndex = true;
	}

	if (FParse::Param(CmdLine, TEXT("overrideplatformcompressor")))
	{
		CmdLineParameters.OverridePlatformCompressor = true;
	}

	if (FParse::Value(CmdLine, TEXT("-create="), ResponseFile))
	{
		TArray<FString> Lines;

		CmdLineParameters.GeneratePatch = FParse::Value(CmdLine, TEXT("-generatepatch="), CmdLineParameters.SourcePatchPakFilename);

		bool bCompress = FParse::Param(CmdLine, TEXT("compress"));
		bool bEncrypt = FParse::Param(CmdLine, TEXT("encrypt"));

		bool bParseLines = true;
		if (IFileManager::Get().DirectoryExists(*ResponseFile))
		{
			IFileManager::Get().FindFilesRecursive(Lines, *ResponseFile, TEXT("*"), true, false);
			bParseLines = false;
		}
		else
		{
			FString Text;
			UE_LOG(LogPakFile, Display, TEXT("Loading response file %s"), *ResponseFile);
			if (FFileHelper::LoadFileToString(Text, *ResponseFile))
			{
				// Remove all carriage return characters.
				Text.ReplaceInline(TEXT("\r"), TEXT(""));
				// Read all lines
				Text.ParseIntoArray(Lines, TEXT("\n"), true);
			}
			else
			{
				UE_LOG(LogPakFile, Error, TEXT("Failed to load %s"), *ResponseFile);
			}
		}

		for (int32 EntryIndex = 0; EntryIndex < Lines.Num(); EntryIndex++)
		{
			TArray<FString> SourceAndDest;
			TArray<FString> Switches;
			if (bParseLines)
			{
				Lines[EntryIndex].TrimStartInline();
				CommandLineParseHelper(*Lines[EntryIndex], SourceAndDest, Switches);
			}
			else
			{
				SourceAndDest.Add(Lines[EntryIndex]);
			}
			if( SourceAndDest.Num() == 0)
			{
				continue;
			}
			FPakInputPair Input;

			Input.Source = SourceAndDest[0];
			FPaths::NormalizeFilename(Input.Source);
			if (SourceAndDest.Num() > 1)
			{
				Input.Dest = FPaths::GetPath(SourceAndDest[1]);
			}
			else
			{
				Input.Dest = FPaths::GetPath(Input.Source);
			}
			FPaths::NormalizeFilename(Input.Dest);
			FPakFile::MakeDirectoryFromPath(Input.Dest);

			//check for compression switches
			for (int32 Index = 0; Index < Switches.Num(); ++Index)
			{
				if (Switches[Index] == TEXT("compress"))
				{
					Input.bNeedsCompression = true;
				}
				if (Switches[Index] == TEXT("encrypt"))
				{
					Input.bNeedEncryption = true;
				}
			}
			Input.bNeedsCompression |= bCompress;
			Input.bNeedEncryption |= bEncrypt;

			UE_LOG(LogPakFile, Log, TEXT("Added file Source: %s Dest: %s"), *Input.Source, *Input.Dest);
			Entries.Add(Input);
		}			
	}
	else
	{
		// Override destination path.
		FString MountPoint;
		FParse::Value(CmdLine, TEXT("-dest="), MountPoint);
		FPaths::NormalizeFilename(MountPoint);
		FPakFile::MakeDirectoryFromPath(MountPoint);

		// Parse comand line params. The first param after the program name is the created pak name
		for (int32 Index = 1; Index < NonOptionArguments.Num(); Index++)
		{
			// Skip switches and add everything else to the Entries array
			FPakInputPair Input;
			Input.Source = *NonOptionArguments[Index];
			FPaths::NormalizeFilename(Input.Source);
			if (MountPoint.Len() > 0)
			{
				FString SourceDirectory( FPaths::GetPath(Input.Source) );
				FPakFile::MakeDirectoryFromPath(SourceDirectory);
				Input.Dest = Input.Source.Replace(*SourceDirectory, *MountPoint, ESearchCase::IgnoreCase);
			}
			else
			{
				Input.Dest = FPaths::GetPath(Input.Source);
				FPakFile::MakeDirectoryFromPath(Input.Dest);
			}
			FPaths::NormalizeFilename(Input.Dest);
			Entries.Add(Input);
		}
	}
	UE_LOG(LogPakFile, Display, TEXT("Added %d entries to add to pak file."), Entries.Num());
}

void CollectFilesToAdd(TArray<FPakInputPair>& OutFilesToAdd, const TArray<FPakInputPair>& InEntries, const TMap<FString, uint64>& OrderMap)
{
	UE_LOG(LogPakFile, Display, TEXT("Collecting files to add to pak file..."));
	const double StartTime = FPlatformTime::Seconds();

	// Start collecting files
	TSet<FString> AddedFiles;	
	for (int32 Index = 0; Index < InEntries.Num(); Index++)
	{
		const FPakInputPair& Input = InEntries[Index];
		const FString& Source = Input.Source;
		bool bCompression = Input.bNeedsCompression;
		bool bEncryption = Input.bNeedEncryption;


		FString Filename = FPaths::GetCleanFilename(Source);
		FString Directory = FPaths::GetPath(Source);
		FPaths::MakeStandardFilename(Directory);
		FPakFile::MakeDirectoryFromPath(Directory);

		if (Filename.IsEmpty())
		{
			Filename = TEXT("*.*");
		}
		if ( Filename.Contains(TEXT("*")) )
		{
			// Add multiple files
			TArray<FString> FoundFiles;
			IFileManager::Get().FindFilesRecursive(FoundFiles, *Directory, *Filename, true, false);

			for (int32 FileIndex = 0; FileIndex < FoundFiles.Num(); FileIndex++)
			{
				FPakInputPair FileInput;
				FileInput.Source = FoundFiles[FileIndex];
				FPaths::MakeStandardFilename(FileInput.Source);
				FileInput.Dest = FileInput.Source.Replace(*Directory, *Input.Dest, ESearchCase::IgnoreCase);
				const uint64* FoundOrder = OrderMap.Find(FileInput.Dest.ToLower());
				if(FoundOrder)
				{
					FileInput.SuggestedOrder = *FoundOrder;
				}
				else
				{
					// we will put all unordered files at 1 << 28 so that they are before any uexp or ubulk files we assign orders to here
					FileInput.SuggestedOrder = (1 << 28);
					// if this is a cook order or an old order it will not have uexp files in it, so we put those in the same relative order after all of the normal files, but before any ubulk files
					if (FileInput.Dest.EndsWith(TEXT("uexp")) || FileInput.Dest.EndsWith(TEXT("ubulk")))
					{
						FoundOrder = OrderMap.Find(FPaths::GetBaseFilename(FileInput.Dest.ToLower(), false) + TEXT(".uasset"));
						if (!FoundOrder)
						{
							FoundOrder = OrderMap.Find(FPaths::GetBaseFilename(FileInput.Dest.ToLower(), false) + TEXT(".umap"));
						}
						if (FileInput.Dest.EndsWith(TEXT("uexp")))
						{
							FileInput.SuggestedOrder = (FoundOrder ? *FoundOrder : 0) + (1 << 29);
						}
						else
						{
							FileInput.SuggestedOrder = (FoundOrder ? *FoundOrder : 0) + (1 << 30);
						}
					}
				}
				FileInput.bNeedsCompression = bCompression;
				FileInput.bNeedEncryption = bEncryption;
				if (!AddedFiles.Contains(FileInput.Source))
				{
					OutFilesToAdd.Add(FileInput);
					AddedFiles.Add(FileInput.Source);
				}
				else
				{
					int32 FoundIndex;
					OutFilesToAdd.Find(FileInput,FoundIndex);
					OutFilesToAdd[FoundIndex].bNeedEncryption |= bEncryption;
					OutFilesToAdd[FoundIndex].bNeedsCompression |= bCompression;
					OutFilesToAdd[FoundIndex].SuggestedOrder = FMath::Min<uint64>(OutFilesToAdd[FoundIndex].SuggestedOrder, FileInput.SuggestedOrder);
				}
			}
		}
		else
		{
			// Add single file
			FPakInputPair FileInput;
			FileInput.Source = Input.Source;
			FPaths::MakeStandardFilename(FileInput.Source);
			FileInput.Dest = FileInput.Source.Replace(*Directory, *Input.Dest, ESearchCase::IgnoreCase);
			const uint64* FoundOrder = OrderMap.Find(FileInput.Dest.ToLower());
			if (FoundOrder)
			{
				FileInput.SuggestedOrder = *FoundOrder;
			}
			FileInput.bNeedEncryption = bEncryption;
			FileInput.bNeedsCompression = bCompression;

			if (AddedFiles.Contains(FileInput.Source))
			{
				int32 FoundIndex;
				OutFilesToAdd.Find(FileInput, FoundIndex);
				OutFilesToAdd[FoundIndex].bNeedEncryption |= bEncryption;
				OutFilesToAdd[FoundIndex].bNeedsCompression |= bCompression;
				OutFilesToAdd[FoundIndex].SuggestedOrder = FMath::Min<uint64>(OutFilesToAdd[FoundIndex].SuggestedOrder, FileInput.SuggestedOrder);
			}
			else
			{
				OutFilesToAdd.Add(FileInput);
				AddedFiles.Add(FileInput.Source);
			}
		}
	}

	// Sort by suggested order then alphabetically
	struct FInputPairSort
	{
		FORCEINLINE bool operator()(const FPakInputPair& A, const FPakInputPair& B) const
		{
			return A.SuggestedOrder == B.SuggestedOrder ? A.Dest < B.Dest : A.SuggestedOrder < B.SuggestedOrder;
		}
	};
	OutFilesToAdd.Sort(FInputPairSort());
	UE_LOG(LogPakFile, Display, TEXT("Collected %d files in %.2lfs."), OutFilesToAdd.Num(), FPlatformTime::Seconds() - StartTime);
}

bool BufferedCopyFile(FArchive& Dest, FArchive& Source, const FPakEntry& Entry, void* Buffer, int64 BufferSize, const FNamedAESKey& Key)
{	
	// Align down
	BufferSize = BufferSize & ~(FAES::AESBlockSize-1);
	int64 RemainingSizeToCopy = Entry.Size;
	while (RemainingSizeToCopy > 0)
	{
		const int64 SizeToCopy = FMath::Min(BufferSize, RemainingSizeToCopy);
		// If file is encrypted so we need to account for padding
		int64 SizeToRead = Entry.IsEncrypted() ? Align(SizeToCopy,FAES::AESBlockSize) : SizeToCopy;

		Source.Serialize(Buffer,SizeToRead);
		if (Entry.IsEncrypted())
		{
			FAES::DecryptData((uint8*)Buffer, SizeToRead, Key.Key);
		}
		Dest.Serialize(Buffer, SizeToCopy);
		RemainingSizeToCopy -= SizeToRead;
	}
	return true;
}

bool UncompressCopyFile(FArchive& Dest, FArchive& Source, const FPakEntry& Entry, uint8*& PersistentBuffer, int64& BufferSize, const FNamedAESKey& Key, const FPakFile& PakFile)
{
	if (Entry.UncompressedSize == 0)
	{
		return false;
	}

	// The compression block size depends on the bit window that the PAK file was originally created with. Since this isn't stored in the PAK file itself,
	// we can use FCompression::CompressMemoryBound as a guideline for the max expected size to avoid unncessary reallocations, but we need to make sure
	// that we check if the actual size is not actually greater (eg. UE-59278).
	int32 MaxCompressionBlockSize = FCompression::CompressMemoryBound((ECompressionFlags)Entry.CompressionMethod, Entry.CompressionBlockSize);
	for (const FPakCompressedBlock& Block : Entry.CompressionBlocks)
	{
		MaxCompressionBlockSize = FMath::Max<int32>(MaxCompressionBlockSize, Block.CompressedEnd - Block.CompressedStart);
	}

	int64 WorkingSize = Entry.CompressionBlockSize + MaxCompressionBlockSize;
	if (BufferSize < WorkingSize)
	{
		PersistentBuffer = (uint8*)FMemory::Realloc(PersistentBuffer, WorkingSize);
		BufferSize = WorkingSize;
	}

	uint8* UncompressedBuffer = PersistentBuffer+MaxCompressionBlockSize;

	for (uint32 BlockIndex=0, BlockIndexNum=Entry.CompressionBlocks.Num(); BlockIndex < BlockIndexNum; ++BlockIndex)
	{
		uint32 CompressedBlockSize = Entry.CompressionBlocks[BlockIndex].CompressedEnd - Entry.CompressionBlocks[BlockIndex].CompressedStart;
		uint32 UncompressedBlockSize = (uint32)FMath::Min<int64>(Entry.UncompressedSize - Entry.CompressionBlockSize*BlockIndex, Entry.CompressionBlockSize);
		Source.Seek(Entry.CompressionBlocks[BlockIndex].CompressedStart + (PakFile.GetInfo().HasRelativeCompressedChunkOffsets() ? Entry.Offset : 0));
		uint32 SizeToRead = Entry.IsEncrypted() ? Align(CompressedBlockSize, FAES::AESBlockSize) : CompressedBlockSize;
		Source.Serialize(PersistentBuffer, SizeToRead);

		if (Entry.IsEncrypted())
		{
			FAES::DecryptData(PersistentBuffer, SizeToRead, Key.Key);
		}

		if(!FCompression::UncompressMemory((ECompressionFlags)Entry.CompressionMethod,UncompressedBuffer,UncompressedBlockSize,PersistentBuffer,CompressedBlockSize))
		{
			return false;
		}
		Dest.Serialize(UncompressedBuffer,UncompressedBlockSize);
	}

	return true;
}

TEncryptionInt ParseEncryptionIntFromJson(TSharedPtr<FJsonObject> InObj, const TCHAR* InName)
{
	FString Base64;
	if (InObj->TryGetStringField(InName, Base64))
	{
		TArray<uint8> Bytes;
		FBase64::Decode(Base64, Bytes);
		check(Bytes.Num() == sizeof(TEncryptionInt));
		return TEncryptionInt((uint32*)&Bytes[0]);
	}
	else
	{
		return TEncryptionInt();
	}
}

void PrepareEncryptionAndSigningKeysFromCryptoKeyCache(const FString& InFilename, FKeyPair& OutSigningKey, FNamedAESKey& OutAESKey, TArray<FNamedAESKey>& OutSecondaryEncryptionKeys)
{
	FArchive* File = IFileManager::Get().CreateFileReader(*InFilename);
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<char>> Reader = TJsonReaderFactory<char>::Create(File);
	if (FJsonSerializer::Deserialize(Reader, RootObject))
	{
		const bool bDataCryptoRequired = RootObject->GetBoolField("bDataCryptoRequired");

		if (bDataCryptoRequired)
		{
			const TSharedPtr<FJsonObject>* EncryptionKeyObject;
			if (RootObject->TryGetObjectField(TEXT("EncryptionKey"), EncryptionKeyObject))
			{
				FString EncryptionKeyBase64;
				if ((*EncryptionKeyObject)->TryGetStringField(TEXT("Key"), EncryptionKeyBase64))
				{
					if (EncryptionKeyBase64.Len() > 0)
					{
						TArray<uint8> Key;
						FBase64::Decode(EncryptionKeyBase64, Key);
						check(Key.Num() == sizeof(FAES::FAESKey::Key));
						FMemory::Memcpy(OutAESKey.Key.Key, &Key[0], sizeof(FAES::FAESKey::Key));
					}
				}
			}

			bool bEnablePakSigning = false;
			if (RootObject->TryGetBoolField(TEXT("bEnablePakSigning"), bEnablePakSigning))
			{
				const TSharedPtr<FJsonObject>* SigningKey = nullptr;
				if (bEnablePakSigning && RootObject->TryGetObjectField(TEXT("SigningKey"), SigningKey))
				{
					TSharedPtr<FJsonObject> PublicKey = (*SigningKey)->GetObjectField(TEXT("PublicKey"));
					TSharedPtr<FJsonObject> PrivateKey = (*SigningKey)->GetObjectField(TEXT("PrivateKey"));
					OutSigningKey.PublicKey.Exponent = ParseEncryptionIntFromJson(PublicKey, TEXT("Exponent"));
					OutSigningKey.PublicKey.Modulus = ParseEncryptionIntFromJson(PublicKey, TEXT("Modulus"));
					OutSigningKey.PrivateKey.Exponent = ParseEncryptionIntFromJson(PrivateKey, TEXT("Exponent"));
					OutSigningKey.PrivateKey.Modulus = ParseEncryptionIntFromJson(PrivateKey, TEXT("Modulus"));
					check(OutSigningKey.PublicKey.Modulus == OutSigningKey.PrivateKey.Modulus);
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* SecondaryEncryptionKeyArray = nullptr;
		if (RootObject->TryGetArrayField(TEXT("SecondaryEncryptionKeys"), SecondaryEncryptionKeyArray))
		{
			OutSecondaryEncryptionKeys.Empty(SecondaryEncryptionKeyArray->Num());

			for (TSharedPtr<FJsonValue> EncryptionKeyValue : *SecondaryEncryptionKeyArray)
			{
				FNamedAESKey& NewKey = OutSecondaryEncryptionKeys[OutSecondaryEncryptionKeys.Add(FNamedAESKey())];
				TSharedPtr<FJsonObject> SecondaryEncryptionKeyObject = EncryptionKeyValue->AsObject();
				FGuid::Parse(SecondaryEncryptionKeyObject->GetStringField(TEXT("Guid")), NewKey.Guid);
				NewKey.Name = SecondaryEncryptionKeyObject->GetStringField(TEXT("Name"));
				FString KeyBase64 = SecondaryEncryptionKeyObject->GetStringField(TEXT("Key"));

				TArray<uint8> Key;
				FBase64::Decode(KeyBase64, Key);
				check(Key.Num() == sizeof(FAES::FAESKey::Key));
				FMemory::Memcpy(NewKey.Key.Key, &Key[0], sizeof(FAES::FAESKey::Key));
			}
		}
	}
	delete File;
}

void PrepareEncryptionAndSigningKeys(const TCHAR* CmdLine, FKeyPair& OutSigningKey, FNamedAESKey& OutEncryptionKey)
{
	OutSigningKey.PrivateKey.Exponent.Zero();
	OutSigningKey.PrivateKey.Modulus.Zero();
	OutSigningKey.PublicKey.Exponent.Zero();
	OutSigningKey.PublicKey.Modulus.Zero();
	OutEncryptionKey.Key.Reset();
	TArray<FNamedAESKey> SecondaryEncryptionKeys;

	// First, try and parse the keys from a supplied crypto key cache file
	FString CryptoKeysCacheFilename;
	if (FParse::Value(CmdLine, TEXT("cryptokeys="), CryptoKeysCacheFilename))
	{
		UE_LOG(LogPakFile, Display, TEXT("Parsing crypto keys from a crypto key cache file"));
		PrepareEncryptionAndSigningKeysFromCryptoKeyCache(CryptoKeysCacheFilename, OutSigningKey, OutEncryptionKey, SecondaryEncryptionKeys);
	}
	else if (FParse::Param(CmdLine, TEXT("encryptionini")))
	{
		FString ProjectDir, EngineDir, Platform;

		if (FParse::Value(CmdLine, TEXT("projectdir="), ProjectDir, false)
			&& FParse::Value(CmdLine, TEXT("enginedir="), EngineDir, false)
			&& FParse::Value(CmdLine, TEXT("platform="), Platform, false))
		{
			FConfigFile EngineConfig;

			FConfigCacheIni::LoadExternalIniFile(EngineConfig, TEXT("Engine"), *FPaths::Combine(EngineDir, TEXT("Config\\")), *FPaths::Combine(ProjectDir, TEXT("Config/")), true, *Platform);
			bool bDataCryptoRequired = false;
			EngineConfig.GetBool(TEXT("PlatformCrypto"), TEXT("PlatformRequiresDataCrypto"), bDataCryptoRequired);

			if (!bDataCryptoRequired)
			{
				return;
			}

			FConfigFile ConfigFile;
			FConfigCacheIni::LoadExternalIniFile(ConfigFile, TEXT("Crypto"), *FPaths::Combine(EngineDir, TEXT("Config\\")), *FPaths::Combine(ProjectDir, TEXT("Config/")), true, *Platform);
			bool bSignPak = false;
			bool bEncryptPakIniFiles = false;
			bool bEncryptPakIndex = false;
			bool bEncryptAssets = false;
			bool bEncryptPak = false;

			if (ConfigFile.Num())
			{
				UE_LOG(LogPakFile, Display, TEXT("Using new format crypto.ini files for crypto configuration"));

				static const TCHAR* SectionName = TEXT("/Script/CryptoKeys.CryptoKeysSettings");

				ConfigFile.GetBool(SectionName, TEXT("bEnablePakSigning"), bSignPak);
				ConfigFile.GetBool(SectionName, TEXT("bEncryptPakIniFiles"), bEncryptPakIniFiles);
				ConfigFile.GetBool(SectionName, TEXT("bEncryptPakIndex"), bEncryptPakIndex);
				ConfigFile.GetBool(SectionName, TEXT("bEncryptAssets"), bEncryptAssets);
				bEncryptPak = bEncryptPakIniFiles || bEncryptPakIndex || bEncryptAssets;

				if (bSignPak)
				{
					FString PublicExpBase64, PrivateExpBase64, ModulusBase64;
					ConfigFile.GetString(SectionName, TEXT("SigningPublicExponent"), PublicExpBase64);
					ConfigFile.GetString(SectionName, TEXT("SigningPrivateExponent"), PrivateExpBase64);
					ConfigFile.GetString(SectionName, TEXT("SigningModulus"), ModulusBase64);

					TArray<uint8> PublicExp, PrivateExp, Modulus;
					FBase64::Decode(PublicExpBase64, PublicExp);
					FBase64::Decode(PrivateExpBase64, PrivateExp);
					FBase64::Decode(ModulusBase64, Modulus);

					OutSigningKey.PrivateKey.Exponent = TEncryptionInt((uint32*)&PrivateExp[0]);
					OutSigningKey.PrivateKey.Modulus = TEncryptionInt((uint32*)&Modulus[0]);
					OutSigningKey.PublicKey.Exponent = TEncryptionInt((uint32*)&PublicExp[0]);
					OutSigningKey.PublicKey.Modulus = OutSigningKey.PrivateKey.Modulus;

					UE_LOG(LogPakFile, Display, TEXT("Parsed signature keys from config files."));
				}

				if (bEncryptPak)
				{
					FString EncryptionKeyString;
					ConfigFile.GetString(SectionName, TEXT("EncryptionKey"), EncryptionKeyString);

					if (EncryptionKeyString.Len() > 0)
					{
						TArray<uint8> Key;
						FBase64::Decode(EncryptionKeyString, Key);
						check(Key.Num() == sizeof(FAES::FAESKey::Key));
						FMemory::Memcpy(OutEncryptionKey.Key.Key, &Key[0], sizeof(FAES::FAESKey::Key));
						UE_LOG(LogPakFile, Display, TEXT("Parsed AES encryption key from config files."));
					}
				}
			}
			else
			{
				static const TCHAR* SectionName = TEXT("Core.Encryption");

				UE_LOG(LogPakFile, Display, TEXT("Using old format encryption.ini files for crypto configuration"));

				FConfigCacheIni::LoadExternalIniFile(ConfigFile, TEXT("Encryption"), *FPaths::Combine(EngineDir, TEXT("Config\\")), *FPaths::Combine(ProjectDir, TEXT("Config/")), true, *Platform);
				ConfigFile.GetBool(SectionName, TEXT("SignPak"), bSignPak);
				ConfigFile.GetBool(SectionName, TEXT("EncryptPak"), bEncryptPak);

				if (bSignPak)
				{
					FString RSAPublicExp, RSAPrivateExp, RSAModulus;
					ConfigFile.GetString(SectionName, TEXT("rsa.publicexp"), RSAPublicExp);
					ConfigFile.GetString(SectionName, TEXT("rsa.privateexp"), RSAPrivateExp);
					ConfigFile.GetString(SectionName, TEXT("rsa.modulus"), RSAModulus);

					OutSigningKey.PrivateKey.Exponent.Parse(RSAPrivateExp);
					OutSigningKey.PrivateKey.Modulus.Parse(RSAModulus);
					OutSigningKey.PublicKey.Exponent.Parse(RSAPublicExp);
					OutSigningKey.PublicKey.Modulus = OutSigningKey.PrivateKey.Modulus;

					UE_LOG(LogPakFile, Display, TEXT("Parsed signature keys from config files."));
				}

				if (bEncryptPak)
				{
					FString EncryptionKeyString;
					ConfigFile.GetString(SectionName, TEXT("aes.key"), EncryptionKeyString);

					if (EncryptionKeyString.Len() == 32 && TCString<TCHAR>::IsPureAnsi(*EncryptionKeyString))
					{
						for (int32 Index = 0; Index < 32; ++Index)
						{
							OutEncryptionKey.Key.Key[Index] = (uint8)EncryptionKeyString[Index];
						}

						UE_LOG(LogPakFile, Display, TEXT("Parsed AES encryption key from config files."));
					}
				}
			}
		}
	}
	else
	{
		UE_LOG(LogPakFile, Display, TEXT("Using command line for crypto configuration"));

		FString EncryptionKeyString;
		FParse::Value(CmdLine, TEXT("aes="), EncryptionKeyString, false);

		if (EncryptionKeyString.Len() > 0)
		{
			const uint32 RequiredKeyLength = sizeof(OutEncryptionKey.Key);

			// Error checking
			if (EncryptionKeyString.Len() < RequiredKeyLength)
			{
				UE_LOG(LogPakFile, Fatal, TEXT("AES encryption key must be %d characters long"), RequiredKeyLength);
			}

			if (EncryptionKeyString.Len() > RequiredKeyLength)
			{
				UE_LOG(LogPakFile, Warning, TEXT("AES encryption key is more than %d characters long, so will be truncated!"), RequiredKeyLength);
				EncryptionKeyString = EncryptionKeyString.Left(RequiredKeyLength);
			}

			if (!FCString::IsPureAnsi(*EncryptionKeyString))
			{
				UE_LOG(LogPakFile, Fatal, TEXT("AES encryption key must be a pure ANSI string!"));
			}

			ANSICHAR* AsAnsi = TCHAR_TO_ANSI(*EncryptionKeyString);
			check(TCString<ANSICHAR>::Strlen(AsAnsi) == RequiredKeyLength);
			FMemory::Memcpy(OutEncryptionKey.Key.Key, AsAnsi, RequiredKeyLength);
			UE_LOG(LogPakFile, Display, TEXT("Parsed AES encryption key from command line."));
		}

		FString KeyFilename;
		if (FParse::Value(CmdLine, TEXT("sign="), KeyFilename, false))
		{
			if (KeyFilename.StartsWith(TEXT("0x")))
			{
				TArray<FString> KeyValueText;
				int32 NumParts = KeyFilename.ParseIntoArray(KeyValueText, TEXT("+"), true);
				if (NumParts == 3)
				{
					OutSigningKey.PrivateKey.Exponent.Parse(KeyValueText[0]);
					OutSigningKey.PrivateKey.Modulus.Parse(KeyValueText[1]);
					OutSigningKey.PublicKey.Exponent.Parse(KeyValueText[2]);
					OutSigningKey.PublicKey.Modulus = OutSigningKey.PrivateKey.Modulus;

					UE_LOG(LogPakFile, Display, TEXT("Parsed signature keys from command line."));
				}
				else
				{
					UE_LOG(LogPakFile, Error, TEXT("Expected 3, got %d, when parsing %s"), KeyValueText.Num(), *KeyFilename);
					OutSigningKey.PrivateKey.Exponent.Zero();
				}
			}
			else if (!ReadKeysFromFile(*KeyFilename, OutSigningKey))
			{
				UE_LOG(LogPakFile, Error, TEXT("Unable to load signature keys %s."), *KeyFilename);
			}
		}
	}

	if (OutSigningKey.IsValid())
	{
		if (!TestKeys(OutSigningKey))
		{
			UE_LOG(LogPakFile, Fatal, TEXT("Pak signing keys are invalid"));
			OutSigningKey.PrivateKey.Exponent.Zero();
		}
	}

	FString MasterEncryptionKeyOverrideGuidString;
	if (FParse::Value(CmdLine, TEXT("-EncryptionKeyOverrideGuid="), MasterEncryptionKeyOverrideGuidString))
	{
		FGuid MasterEncryptionKeyOverrideGuid;
		if (FGuid::Parse(MasterEncryptionKeyOverrideGuidString, MasterEncryptionKeyOverrideGuid))
		{
			for (FNamedAESKey& NamedKey : SecondaryEncryptionKeys)
			{
				if (NamedKey.Guid == MasterEncryptionKeyOverrideGuid)
				{
					OutEncryptionKey = NamedKey;
					UE_LOG(LogPakFile, Display, TEXT("Using encryption key override from command line (%s [%s])"), *NamedKey.Name, *MasterEncryptionKeyOverrideGuidString);
					break;
				}
			}

			if (OutEncryptionKey.Guid != MasterEncryptionKeyOverrideGuid)
			{
				UE_LOG(LogPakFile, Fatal, TEXT("Failed to find specified encryption key override guid (%s)"), *MasterEncryptionKeyOverrideGuidString);
			}
		}
		else
		{
			UE_LOG(LogPakFile, Fatal, TEXT("Failed to parse encryption key override guid from command line (%s)"), *MasterEncryptionKeyOverrideGuidString);
		}
	}
	else
	{
		if (OutEncryptionKey.IsValid())
		{
			UE_LOG(LogPakFile, Display, TEXT("Using embedded encryption key"));
		}
	}
	
	if (OutEncryptionKey.IsValid())
	{
		FCoreDelegates::GetPakEncryptionKeyDelegate().BindLambda([OutEncryptionKey](uint8 OutKey[32]) { FMemory::Memcpy(OutKey, OutEncryptionKey.Key.Key, sizeof(OutEncryptionKey.Key)); });
	}

	for (const FNamedAESKey& Key : SecondaryEncryptionKeys)
	{
		FCoreDelegates::GetRegisterEncryptionKeyDelegate().ExecuteIfBound(Key.Guid, Key.Key);
	}
}

/**
 * Creates a pak file writer. This can be a signed writer if the encryption keys are specified in the command line
 */
FArchive* CreatePakWriter(const TCHAR* Filename, const FKeyPair& SigningKey)
{
	FArchive* Writer = IFileManager::Get().CreateFileWriter(Filename);
	FString KeyFilename;
	bool bSigningEnabled = false;
	
	if (Writer)
	{
		if (SigningKey.IsValid())
		{
			UE_LOG(LogPakFile, Display, TEXT("Creating signed pak %s."), Filename);
			Writer = new FSignedArchiveWriter(*Writer, Filename, SigningKey.PublicKey, SigningKey.PrivateKey);
		}
	}

	return Writer;
}

bool CreatePakFile(const TCHAR* Filename, TArray<FPakInputPair>& FilesToAdd, const FPakCommandLineParameters& CmdLineParameters, const FKeyPair& SigningKey, const FNamedAESKey& EncryptionKey)
{	
	const double StartTime = FPlatformTime::Seconds();

	// Create Pak
	TUniquePtr<FArchive> PakFileHandle(CreatePakWriter(Filename, SigningKey));
	if (!PakFileHandle)
	{
		UE_LOG(LogPakFile, Error, TEXT("Unable to create pak file \"%s\"."), Filename);
		return false;
	}

	FPakInfo Info;
	Info.bEncryptedIndex = (EncryptionKey.IsValid() && CmdLineParameters.EncryptIndex);
	Info.EncryptionKeyGuid = EncryptionKey.Guid;

	TArray<FPakEntryPair> Index;
	FString MountPoint = GetCommonRootPath(FilesToAdd);
	uint8* ReadBuffer = NULL;
	int64 BufferSize = 0;
	ECompressionFlags CompressionMethod = COMPRESS_None;
	FCompressedFileBuffer CompressedFileBuffer;

	uint8* PaddingBuffer = nullptr;
	int64 PaddingBufferSize = 0;
	if (CmdLineParameters.PatchFilePadAlign > 0)
	{
		PaddingBufferSize = CmdLineParameters.PatchFilePadAlign;
		PaddingBuffer = (uint8*)FMemory::Malloc(PaddingBufferSize);
		FMemory::Memset(PaddingBuffer, 0, PaddingBufferSize);
	}

	// Some platforms provide patch download size reduction by diffing the patch files.  However, they often operate on specific block
	// sizes when dealing with new data within the file.  Pad files out to the given alignment to work with these systems more nicely.
	// We also want to combine smaller files into the same padding size block so we don't waste as much space. i.e. grouping 64 1k files together
	// rather than padding each out to 64k.
	const uint32 RequiredPatchPadding = CmdLineParameters.PatchFilePadAlign;

	uint64 ContiguousTotalSizeSmallerThanBlockSize = 0;
	uint64 ContiguousFilesSmallerThanBlockSize = 0;

	uint64 TotalUncompressedSize = 0;
	uint64 TotalCompressedSize = 0;

	uint64 TotalRequestedEncryptedFiles = 0;
	uint64 TotalEncryptedFiles = 0;
	uint64 TotalEncryptedDataSize = 0;

	for (int32 FileIndex = 0; FileIndex < FilesToAdd.Num(); FileIndex++)
	{
		bool bDeleted = FilesToAdd[FileIndex].bIsDeleteRecord;
		bool bIsUAssetUExpPairUAsset = false;
		bool bIsUAssetUExpPairUExp = false;

		if (FileIndex)
		{
			if (FPaths::GetBaseFilename(FilesToAdd[FileIndex - 1].Dest, false) == FPaths::GetBaseFilename(FilesToAdd[FileIndex].Dest, false) &&
				FPaths::GetExtension(FilesToAdd[FileIndex - 1].Dest, true) == TEXT(".uasset") && 
				FPaths::GetExtension(FilesToAdd[FileIndex].Dest, true) == TEXT(".uexp")
				)
			{
				bIsUAssetUExpPairUExp = true;
			}
		}
		if (!bIsUAssetUExpPairUExp && FileIndex + 1 < FilesToAdd.Num())
		{
			if (FPaths::GetBaseFilename(FilesToAdd[FileIndex].Dest, false) == FPaths::GetBaseFilename(FilesToAdd[FileIndex + 1].Dest, false) &&
				FPaths::GetExtension(FilesToAdd[FileIndex].Dest, true) == TEXT(".uasset") &&
				FPaths::GetExtension(FilesToAdd[FileIndex + 1].Dest, true) == TEXT(".uexp")
				)
			{
				bIsUAssetUExpPairUAsset = true;
			}
		}

		//  Remember the offset but don't serialize it with the entry header.
		int64 NewEntryOffset = PakFileHandle->Tell();
		FPakEntryPair NewEntry;

		if (!bDeleted)
		{
			//check if this file requested to be compression
			int64 OriginalFileSize = IFileManager::Get().FileSize(*FilesToAdd[FileIndex].Source);
			int64 RealFileSize = OriginalFileSize + NewEntry.Info.GetSerializedSize(FPakInfo::PakFile_Version_Latest);
			if (FilesToAdd[FileIndex].bNeedsCompression && OriginalFileSize > 0)
			{
				if (CmdLineParameters.UseCustomCompressor)
				{
					CompressionMethod = COMPRESS_Custom;
				}
				else
				{
					CompressionMethod = COMPRESS_Default;
				}

				if (CmdLineParameters.OverridePlatformCompressor)
				{
					CompressionMethod = (ECompressionFlags)(CompressionMethod | COMPRESS_OverridePlatform);
				}
			}
			else
			{
				CompressionMethod = COMPRESS_None;
			}

			if (CompressionMethod != COMPRESS_None)
			{
				if (CompressedFileBuffer.CompressFileToWorkingBuffer(FilesToAdd[FileIndex], ReadBuffer, BufferSize, CompressionMethod, CmdLineParameters.CompressionBlockSize, CmdLineParameters.CompressionBitWindow))
				{
					// Check the compression ratio, if it's too low just store uncompressed. Also take into account read size
					// if we still save 64KB it's probably worthwhile compressing, as that saves a file read operation in the runtime.
					// TODO: drive this threashold from the command line
					float PercentLess = ((float)CompressedFileBuffer.TotalCompressedSize / (OriginalFileSize / 100.f));
					if (PercentLess > 90.f && (OriginalFileSize - CompressedFileBuffer.TotalCompressedSize) < 65536)
					{
						CompressionMethod = COMPRESS_None;
					}
					else
					{
						NewEntry.Info.CompressionMethod = CompressionMethod;
						NewEntry.Info.CompressionBlocks.AddUninitialized(CompressedFileBuffer.CompressedBlocks.Num());
						RealFileSize = CompressedFileBuffer.TotalCompressedSize + NewEntry.Info.GetSerializedSize(FPakInfo::PakFile_Version_Latest);
						NewEntry.Info.CompressionBlocks.Reset();
					}
				}
				else
				{
					// Compression failed. Include file uncompressed and warn the user.
					UE_LOG(LogPakFile, Warning, TEXT("File \"%s\" failed compression. File will be saved uncompressed."), *FilesToAdd[FileIndex].Source);
					CompressionMethod = COMPRESS_None;
				}
			}

			// Account for file system block size, which is a boundary we want to avoid crossing.
			if (!bIsUAssetUExpPairUExp && // don't split uexp / uasset pairs
				CmdLineParameters.FileSystemBlockSize > 0 && OriginalFileSize != INDEX_NONE && RealFileSize <= CmdLineParameters.FileSystemBlockSize)
			{
				if ((NewEntryOffset / CmdLineParameters.FileSystemBlockSize) != ((NewEntryOffset + RealFileSize) / CmdLineParameters.FileSystemBlockSize))
				{
					//File crosses a block boundary, so align it to the beginning of the next boundary
					int64 OldOffset = NewEntryOffset;
					NewEntryOffset = AlignArbitrary(NewEntryOffset, CmdLineParameters.FileSystemBlockSize);
					int64 PaddingRequired = NewEntryOffset - OldOffset;

					if (PaddingRequired > 0)
					{
						// If we don't already have a padding buffer, create one
						if (PaddingBuffer == nullptr)
						{
							PaddingBufferSize = 64 * 1024;
							PaddingBuffer = (uint8*)FMemory::Malloc(PaddingBufferSize);
							FMemory::Memset(PaddingBuffer, 0, PaddingBufferSize);
						}

						UE_LOG(LogPakFile, Verbose, TEXT("%14llu - %14llu : %14llu padding."), PakFileHandle->Tell(), PakFileHandle->Tell() + PaddingRequired, PaddingRequired);
						while (PaddingRequired > 0)
						{
							int64 AmountToWrite = FMath::Min(PaddingRequired, PaddingBufferSize);
							PakFileHandle->Serialize(PaddingBuffer, AmountToWrite);
							PaddingRequired -= AmountToWrite;
						}

						check(PakFileHandle->Tell() == NewEntryOffset);
					}
				}
			}
		}

		bool bCopiedToPak;
		int64 SizeToWrite = 0;
		uint8* DataToWrite = nullptr;
		if (bDeleted)
		{
			PrepareDeleteRecordForPak(MountPoint, FilesToAdd[FileIndex], NewEntry);
			bCopiedToPak = false;

			// Directly add the new entry to the index, no more work to do
			Index.Add(NewEntry);
		}
		else if (FilesToAdd[FileIndex].bNeedsCompression && CompressionMethod != COMPRESS_None)
		{
			bCopiedToPak = PrepareCopyCompressedFileToPak(MountPoint, FilesToAdd[FileIndex], CompressedFileBuffer, NewEntry, DataToWrite, SizeToWrite, EncryptionKey);
			DataToWrite = CompressedFileBuffer.CompressedBuffer.Get();
		}
		else
		{
			bCopiedToPak = PrepareCopyFileToPak(MountPoint, FilesToAdd[FileIndex], ReadBuffer, BufferSize, NewEntry, DataToWrite, SizeToWrite, EncryptionKey);
			DataToWrite = ReadBuffer;
		}		

		int64 TotalSizeToWrite = SizeToWrite + NewEntry.Info.GetSerializedSize(FPakInfo::PakFile_Version_Latest);
		if (bCopiedToPak)
		{
			
			if (RequiredPatchPadding > 0)
			{
				//if the next file is going to cross a patch-block boundary then pad out the current set of files with 0's
				//and align the next file up.
				bool bCrossesBoundary = AlignArbitrary(NewEntryOffset, RequiredPatchPadding) != AlignArbitrary(NewEntryOffset + TotalSizeToWrite - 1, RequiredPatchPadding);
				bool bPatchPadded = false;
				if (!bIsUAssetUExpPairUExp) // never patch-pad the uexp of a uasset/uexp pair
				{
					bool bPairProbablyCrossesBoundary = false; // we don't consider compression because we have not compressed the uexp yet.
					if (bIsUAssetUExpPairUAsset)
					{
						int64 UExpFileSize = IFileManager::Get().FileSize(*FilesToAdd[FileIndex + 1].Source) / 2; // assume 50% compression
						bPairProbablyCrossesBoundary = AlignArbitrary(NewEntryOffset, RequiredPatchPadding) != AlignArbitrary(NewEntryOffset + TotalSizeToWrite + UExpFileSize - 1, RequiredPatchPadding);
					}
					if (TotalSizeToWrite >= RequiredPatchPadding || // if it exactly the padding size and by luck does not cross a boundary, we still consider it "over" because it can't be packed with anything else
						bCrossesBoundary || bPairProbablyCrossesBoundary)
					{
						NewEntryOffset = AlignArbitrary(NewEntryOffset, RequiredPatchPadding);
						int64 CurrentLoc = PakFileHandle->Tell();
						int64 PaddingSize = NewEntryOffset - CurrentLoc;
						check(PaddingSize >= 0);
						if (PaddingSize)
						{
							UE_LOG(LogPakFile, Verbose, TEXT("%14llu - %14llu : %14llu patch padding."), PakFileHandle->Tell(), PakFileHandle->Tell() + PaddingSize, PaddingSize);
							check(PaddingSize <= PaddingBufferSize);

							//have to pad manually with 0's.  File locations skipped by Seek and never written are uninitialized which would defeat the whole purpose
							//of padding for certain platforms patch diffing systems.
							PakFileHandle->Serialize(PaddingBuffer, PaddingSize);
						}
						check(PakFileHandle->Tell() == NewEntryOffset);
						bPatchPadded = true;
					}
				}

				//if the current file is bigger than a patch block then we will always have to pad out the previous files.
				//if there were a large set of contiguous small files behind us then this will be the natural stopping point for a possible pathalogical patching case where growth in the small files causes a cascade 
				//to dirty up all the blocks prior to this one.  If this could happen let's warn about it.
				if (bPatchPadded ||
					FileIndex + 1 == FilesToAdd.Num()) // also check the last file, this won't work perfectly if we don't end up adding the last file for some reason
				{
					const uint64 ContiguousGroupedFilePatchWarningThreshhold = 50 * 1024 * 1024;
					if (ContiguousTotalSizeSmallerThanBlockSize > ContiguousGroupedFilePatchWarningThreshhold)
					{
						UE_LOG(LogPakFile, Display, TEXT("%i small files (%i) totaling %llu contiguous bytes found before first 'large' file.  Changes to any of these files could cause the whole group to be 'dirty' in a per-file binary diff based patching system."), ContiguousFilesSmallerThanBlockSize, RequiredPatchPadding, ContiguousTotalSizeSmallerThanBlockSize);
					}
					ContiguousTotalSizeSmallerThanBlockSize = 0;
					ContiguousFilesSmallerThanBlockSize = 0;
				}
				else
				{
					ContiguousTotalSizeSmallerThanBlockSize += TotalSizeToWrite;
					ContiguousFilesSmallerThanBlockSize++;				
				}
			}
			if (FilesToAdd[FileIndex].bNeedsCompression && CompressionMethod != COMPRESS_None)
			{
				FinalizeCopyCompressedFileToPak(*PakFileHandle, CompressedFileBuffer, NewEntry);
			}

			// Write to file
			int64 Offset = PakFileHandle->Tell();
			NewEntry.Info.Serialize(*PakFileHandle, FPakInfo::PakFile_Version_Latest);
			PakFileHandle->Serialize(DataToWrite, SizeToWrite);	
			int64 EndOffset = PakFileHandle->Tell();

			UE_LOG(LogPakFile, Verbose, TEXT("%14llu - %14llu : %14llu header+file %s."), Offset, EndOffset, EndOffset - Offset, *NewEntry.Filename);

			// Update offset now and store it in the index (and only in index)
			NewEntry.Info.Offset = NewEntryOffset;
			Index.Add(NewEntry);
			const TCHAR* EncryptedString = TEXT("");

			if (FilesToAdd[FileIndex].bNeedEncryption)
			{
				TotalRequestedEncryptedFiles++;

				if (EncryptionKey.IsValid())
				{
					TotalEncryptedFiles++;
					TotalEncryptedDataSize += SizeToWrite;
					EncryptedString = TEXT("encrypted ");
				}
			}

			if (FilesToAdd[FileIndex].bNeedsCompression && CompressionMethod != COMPRESS_None)
			{
				TotalCompressedSize += NewEntry.Info.Size;
				TotalUncompressedSize += NewEntry.Info.UncompressedSize;
				float PercentLess = ((float)NewEntry.Info.Size / (NewEntry.Info.UncompressedSize / 100.f));
				if (FilesToAdd[FileIndex].SuggestedOrder < MAX_uint64)
				{
					UE_LOG(LogPakFile, Log, TEXT("Added compressed %sfile \"%s\", %.2f%% of original size. Compressed Size %lld bytes, Original Size %lld bytes (order %llu)."), EncryptedString, *NewEntry.Filename, PercentLess, NewEntry.Info.Size, NewEntry.Info.UncompressedSize, FilesToAdd[FileIndex].SuggestedOrder);
				}
				else
				{
					UE_LOG(LogPakFile, Log, TEXT("Added compressed %sfile \"%s\", %.2f%% of original size. Compressed Size %lld bytes, Original Size %lld bytes (no order given)."), EncryptedString, *NewEntry.Filename, PercentLess, NewEntry.Info.Size, NewEntry.Info.UncompressedSize);
				}
			}
			else
			{
				if (FilesToAdd[FileIndex].SuggestedOrder < MAX_uint64)
				{
					UE_LOG(LogPakFile, Log, TEXT("Added %sfile \"%s\", %lld bytes (order %llu)."), EncryptedString, *NewEntry.Filename, NewEntry.Info.Size, FilesToAdd[FileIndex].SuggestedOrder);
				}
				else
				{
					UE_LOG(LogPakFile, Log, TEXT("Added %sfile \"%s\", %lld bytes (no order given)."), EncryptedString, *NewEntry.Filename, NewEntry.Info.Size);
				}
			}
		}
		else
		{
			if (bDeleted)
			{
				UE_LOG(LogPakFile, Log, TEXT("Created delete record for file \"%s\"."), *FilesToAdd[FileIndex].Source);
			}
			else
			{
				UE_LOG(LogPakFile, Warning, TEXT("Missing file \"%s\" will not be added to PAK file."), *FilesToAdd[FileIndex].Source);
			}
		}
	}

	FMemory::Free(PaddingBuffer);
	FMemory::Free(ReadBuffer);
	ReadBuffer = NULL;

	// Remember IndexOffset
	Info.IndexOffset = PakFileHandle->Tell();

	// Serialize Pak Index at the end of Pak File
	TArray<uint8> IndexData;
	FMemoryWriter IndexWriter(IndexData);
	IndexWriter.SetByteSwapping(PakFileHandle->ForceByteSwapping());
	int32 NumEntries = Index.Num();
	IndexWriter << MountPoint;
	IndexWriter << NumEntries;
	for (int32 EntryIndex = 0; EntryIndex < Index.Num(); EntryIndex++)
	{
		FPakEntryPair& Entry = Index[EntryIndex];
		IndexWriter << Entry.Filename;
		Entry.Info.Serialize(IndexWriter, Info.Version);

		if (RequiredPatchPadding > 0)
		{
			int64 EntrySize = Entry.Info.GetSerializedSize(FPakInfo::PakFile_Version_Latest);
			int64 TotalSizeToWrite = Entry.Info.Size + EntrySize;
			if (TotalSizeToWrite >= RequiredPatchPadding)
			{
				int64 RealStart = Entry.Info.Offset;
				if ((RealStart % RequiredPatchPadding) != 0 && 
					!Entry.Filename.EndsWith(TEXT("uexp"))) // these are export sections of larger files and may be packed with uasset/umap and so we don't need a warning here
				{
					UE_LOG(LogPakFile, Warning, TEXT("File at offset %lld of size %lld not aligned to patch size %i"), RealStart, Entry.Info.Size, RequiredPatchPadding);
				}
			}
		}
	}

	if (Info.bEncryptedIndex)
	{
		int32 OriginalSize = IndexData.Num();
		int32 AlignedSize = Align(OriginalSize, FAES::AESBlockSize);

		for (int32 PaddingIndex = IndexData.Num(); PaddingIndex < AlignedSize; ++PaddingIndex)
		{
			uint8 Byte = IndexData[PaddingIndex % OriginalSize];
			IndexData.Add(Byte);
		}
	}

	FSHA1::HashBuffer(IndexData.GetData(), IndexData.Num(), Info.IndexHash);

	if (Info.bEncryptedIndex)
	{
		FAES::EncryptData(IndexData.GetData(), IndexData.Num(), EncryptionKey.Key);
		TotalEncryptedDataSize += IndexData.Num();
	}

	PakFileHandle->Serialize(IndexData.GetData(), IndexData.Num());

	Info.IndexSize = IndexData.Num();

	// Save trailer (offset, size, hash value)
	Info.Serialize(*PakFileHandle, FPakInfo::PakFile_Version_Latest);

	UE_LOG(LogPakFile, Display, TEXT("Added %d files, %lld bytes total, time %.2lfs."), Index.Num(), PakFileHandle->TotalSize(), FPlatformTime::Seconds() - StartTime);
	if (TotalUncompressedSize)
	{
		float PercentLess = ((float)TotalCompressedSize / (TotalUncompressedSize / 100.f));
		UE_LOG(LogPakFile, Display, TEXT("Compression summary: %.2f%% of original size. Compressed Size %lld bytes, Original Size %lld bytes. "), PercentLess, TotalCompressedSize, TotalUncompressedSize);
	}

	if (TotalEncryptedDataSize)
	{
		UE_LOG(LogPakFile, Display, TEXT("Encryption - ENABLED"));
		UE_LOG(LogPakFile, Display, TEXT("  Files: %d"), TotalEncryptedFiles);

		if (Info.bEncryptedIndex)
		{
			UE_LOG(LogPakFile, Display, TEXT("  Index: Encrypted (%d bytes, %.2fMB)"), Info.IndexSize, (float)Info.IndexSize / 1024.0f / 1024.0f);
		}
		else
		{
			UE_LOG(LogPakFile, Display, TEXT("  Index: Unencrypted"));
		}
		

		UE_LOG(LogPakFile, Display, TEXT("  Total: %d bytes (%.2fMB)"), TotalEncryptedDataSize, (float)TotalEncryptedDataSize / 1024.0f / 1024.0f);
	}
	else
	{
		UE_LOG(LogPakFile, Display, TEXT("Encryption - DISABLED"));
	}

	if (TotalEncryptedFiles < TotalRequestedEncryptedFiles)
	{
		UE_LOG(LogPakFile, Display, TEXT("%d files requested encryption, but no AES key was supplied! Encryption was skipped for these files"), TotalRequestedEncryptedFiles);
	}

	PakFileHandle->Close();
	PakFileHandle.Reset();

	return true;
}

bool TestPakFile(const TCHAR* Filename, bool bSigned)
{	
	FPakFile PakFile(&FPlatformFileManager::Get().GetPlatformFile(), Filename, bSigned);
	if (PakFile.IsValid())
	{
		return PakFile.Check();
	}
	else
	{
		UE_LOG(LogPakFile, Error, TEXT("Unable to open pak file \"%s\"."), Filename);
		return false;
	}
}

bool ListFilesInPak(const TCHAR * InPakFilename, int64 SizeFilter, bool bIncludeDeleted, const FString& CSVFilename, bool bExtractToMountPoint, bool bSigned)
{
	FPakFile PakFile(&FPlatformFileManager::Get().GetPlatformFile(), InPakFilename, bSigned);
	int32 FileCount = 0;
	int64 FileSize = 0;
	int64 FilteredSize = 0;

	if (PakFile.IsValid())
	{
		UE_LOG(LogPakFile, Display, TEXT("Mount point %s"), *PakFile.GetMountPoint());

		TArray<FPakFile::FFileIterator> Records;

		for (FPakFile::FFileIterator It(PakFile,bIncludeDeleted); It; ++It)
		{
			Records.Add(It);
		}

		struct FOffsetSort
		{
			FORCEINLINE bool operator()(const FPakFile::FFileIterator& A, const FPakFile::FFileIterator& B) const
			{
				return A.Info().Offset < B.Info().Offset;
			}
		};

		Records.Sort(FOffsetSort());

		const FString MountPoint = bExtractToMountPoint ? PakFile.GetMountPoint() : TEXT("");

		if (CSVFilename.Len() > 0)
		{
			
			TArray<FString> Lines;
			Lines.Empty(Records.Num()+2);
			Lines.Add(TEXT("Filename, Offset, Size, Hash, Deleted"));
			for (auto It : Records)
			{
				const FPakEntry& Entry = It.Info();

				Lines.Add( FString::Printf(
					TEXT("%s%s, %d, %d, %s, %s"),
					*MountPoint, *It.Filename(),
					Entry.Offset, Entry.Size,
					*BytesToHex(Entry.Hash, sizeof(Entry.Hash)),
					Entry.IsDeleteRecord() ? TEXT("true") : TEXT("false")) );
			}

			if (FFileHelper::SaveStringArrayToFile(Lines, *CSVFilename) == false)
			{
				UE_LOG(LogPakFile, Display, TEXT("Failed to save CSV file %s"), *CSVFilename);
			}
			else
			{
				UE_LOG(LogPakFile, Display, TEXT("Saved CSV file to %s"), *CSVFilename);
			}
		}

		for (auto It : Records)
		{
			const FPakEntry& Entry = It.Info();
			if (Entry.IsDeleteRecord())
			{
				UE_LOG(LogPakFile, Display, TEXT("\"%s%s\" deleted."), *MountPoint, *It.Filename());
				continue;
			}
			if (Entry.Size >= SizeFilter)
			{
				UE_LOG(LogPakFile, Display, TEXT("\"%s%s\" offset: %lld, size: %d bytes, sha1: %s."), *MountPoint, *It.Filename(), Entry.Offset, Entry.Size, *BytesToHex(Entry.Hash, sizeof(Entry.Hash)));
				FilteredSize += Entry.Size;
			}
			FileSize += Entry.Size;
			FileCount++;
		}
		UE_LOG(LogPakFile, Display, TEXT("%d files (%lld bytes), (%lld filtered bytes)."), FileCount, FileSize, FilteredSize);

		return true;
	}
	else
	{
		UE_LOG(LogPakFile, Error, TEXT("Unable to open pak file \"%s\"."), InPakFilename);
		return false;
	}
}

int32 GetPakPriorityFromFilename( const FString& PakFilename )
{
	// Parse the pak file index, the base pak file is index -1
	int32 PakPriority = -1;
	if (PakFilename.EndsWith("_P.pak"))
	{
		FString PakIndexFromFilename = PakFilename.LeftChop(6);
		int32 PakIndexStart = INDEX_NONE;
		PakIndexFromFilename.FindLastChar('_', PakIndexStart);
		if (PakIndexStart != INDEX_NONE)
		{
			PakIndexFromFilename = PakIndexFromFilename.RightChop(PakIndexStart + 1);
			if (PakIndexFromFilename.IsNumeric())
			{
				PakPriority = FCString::Atoi(*PakIndexFromFilename);
			}
		}
	}

	return PakPriority;
}

int32 GetPakChunkIndexFromFilename( const FString& PakFilePath )
{
	const TCHAR* PakChunkPrefix = TEXT("pakchunk");
	const int32 PakChunkPrefixLength = 8;//FCString::Strlen(PakChunkPrefix);

	int32 PakChunkIndex = -1;
	FString PakFilename = FPaths::GetCleanFilename(PakFilePath);
	if (PakFilename.StartsWith(PakChunkPrefix))
	{
		int32 ChunkIndexStart = INDEX_NONE;
		if( PakFilename.FindChar(TEXT('-'), ChunkIndexStart ) )
		{
			FString PakChunkFromFilename = PakFilename.Mid( PakChunkPrefixLength, ChunkIndexStart - PakChunkPrefixLength );
			if( PakChunkFromFilename.IsNumeric() )
			{
				PakChunkIndex = FCString::Atoi(*PakChunkFromFilename);
			}
		}
	}

	return PakChunkIndex;
}

bool AuditPakFiles( const FString& InputPath, bool bOnlyDeleted, const FString& CSVFilename, bool bSigned )
{
	//collect all pak files
	FString PakFileDirectory;
	TArray<FString> PakFileList;
	if (FPaths::DirectoryExists(InputPath))
	{
		//InputPath is a directory
		IFileManager::Get().FindFiles(PakFileList, *InputPath, TEXT(".pak") );
		PakFileDirectory = InputPath;
	}
	else
	{
		//InputPath is a search wildcard (or a directory that doesn't exist...)
		IFileManager::Get().FindFiles(PakFileList, *InputPath, true, false);
		PakFileDirectory = FPaths::GetPath(InputPath);
	}
	if (PakFileList.Num() == 0)
	{
		UE_LOG(LogPakFile, Error, TEXT("No pak files found searching \"%s\"."), *InputPath);
		return false;
	}
	
	struct FFilePakRevision
	{
		FString PakFilename;
		int32 PakPriority;
		int32 Size;
	};
	TMap<FString, FFilePakRevision> FileRevisions;
	TMap<FString, FFilePakRevision> DeletedRevisions;
	int32 HighestPakPriority = -1;

	//build lookup tables for the newest revision of all files and all deleted files
	for (int32 PakFileIndex = 0; PakFileIndex < PakFileList.Num(); PakFileIndex++)
	{
		FString PakFilename = PakFileDirectory + "\\" + PakFileList[PakFileIndex];
		int32 PakPriority = GetPakPriorityFromFilename(PakFilename);
		HighestPakPriority = FMath::Max( HighestPakPriority, PakPriority );

		FPakFile PakFile(&FPlatformFileManager::Get().GetPlatformFile(), *PakFilename, bSigned);
		if (PakFile.IsValid())
		{
			FString PakMountPoint = PakFile.GetMountPoint().Replace(TEXT("../../../"), TEXT(""));

			const bool bIncludeDeleted = true;
			for (FPakFile::FFileIterator It(PakFile,bIncludeDeleted); It; ++It)
			{
				FString AssetName = PakMountPoint;
				if (!AssetName.IsEmpty() && !AssetName.EndsWith("/"))
				{
					AssetName += "/";
				}
				AssetName += It.Filename();

				FFilePakRevision Revision;
				Revision.PakFilename = PakFileList[PakFileIndex];
				Revision.PakPriority = PakPriority;
				Revision.Size = It.Info().Size;

				//add or update the entry for the appropriate revision, depending on whether this is a delete record or not
				TMap<FString, FFilePakRevision>& AppropriateRevisions = (It.Info().IsDeleteRecord()) ? DeletedRevisions : FileRevisions;
				if (!AppropriateRevisions.Contains(AssetName))
				{
					AppropriateRevisions.Add(AssetName, Revision);
				}
				else if (AppropriateRevisions[AssetName].PakPriority < Revision.PakPriority)
				{
					AppropriateRevisions[AssetName] = Revision;
				}
			}
		}
		else
		{
			UE_LOG(LogPakFile, Error, TEXT("Unable to open pak file \"%s\"."), *PakFilename);
			return false;
		}
	}

	//open CSV file, if requested
	FArchive* CSVFileWriter = nullptr;
	if( !CSVFilename.IsEmpty() )
	{
		CSVFileWriter = IFileManager::Get().CreateFileWriter(*CSVFilename);
		if (CSVFileWriter == nullptr)
		{
			UE_LOG(LogPakFile, Error, TEXT("Unable to open csv file \"%s\"."), *CSVFilename);
			return false;
		}
	}

	//helper lambda for writing line depending on whether there's a CSV file or not
	auto WriteCSVLine = [CSVFileWriter]( const FString& Text )
	{
		if( CSVFileWriter )
		{
			CSVFileWriter->Logf( TEXT("%s"), *Text );
		}
		else
		{
			UE_LOG(LogPakFile, Display, TEXT("%s"), *Text );
		}
	};

	//log every file, sorted alphabetically
	FileRevisions.KeySort([]( const FString& A, const FString& B )
	{
		return A.Compare(B, ESearchCase::IgnoreCase) < 0;
	});
	DeletedRevisions.KeySort([]( const FString& A, const FString& B )
	{
		return A.Compare(B, ESearchCase::IgnoreCase) < 0;
	});

	WriteCSVLine( TEXT("AssetName,State,Pak,Prev.Pak,Rev,Prev.Rev,Size,AssetPath") );
	for (auto Itr : FileRevisions)
	{
		const FString& AssetPath = Itr.Key;
		const FString AssetName = FPaths::GetCleanFilename(AssetPath);
		const FFilePakRevision* DeletedRevision = DeletedRevisions.Find(AssetPath);
		if (DeletedRevision == nullptr)
		{
			if (bOnlyDeleted)
			{
				//skip
			}
			else if (Itr.Value.PakPriority == HighestPakPriority)
			{
				WriteCSVLine( FString::Printf( TEXT("%s,Fresh,%s,,%d,,%d,%s"), *AssetName, *Itr.Value.PakFilename, Itr.Value.PakPriority, Itr.Value.Size, *AssetPath ) );
			}
			else
			{
				WriteCSVLine( FString::Printf( TEXT("%s,Inherited,%s,,%d,,%d,%s"), *AssetName, *Itr.Value.PakFilename, Itr.Value.PakPriority, Itr.Value.Size, *AssetPath ) );
			}
		}
		else if (DeletedRevision->PakPriority == Itr.Value.PakPriority)
		{
			WriteCSVLine( FString::Printf( TEXT("%s,Moved,%s,%s,%d,,%d,%s"), *AssetName, *Itr.Value.PakFilename, *DeletedRevision->PakFilename, Itr.Value.PakPriority, Itr.Value.Size, *AssetPath ) );
		}
		else if (DeletedRevision->PakPriority > Itr.Value.PakPriority)
		{
			WriteCSVLine( FString::Printf( TEXT("%s,Deleted,%s,%s,%d,%d,,%s"), *AssetName, *DeletedRevision->PakFilename, *Itr.Value.PakFilename, DeletedRevision->PakPriority, Itr.Value.PakPriority, *AssetPath ) );
		}
		else if (DeletedRevision->PakPriority < Itr.Value.PakPriority)
		{
			WriteCSVLine( FString::Printf( TEXT("%s,Restored,%s,%s,%d,%d,%d,%s"), *AssetName, *Itr.Value.PakFilename, *DeletedRevision->PakFilename, Itr.Value.PakPriority, DeletedRevision->PakPriority, Itr.Value.Size, *AssetPath ) );
		}
	}

	//check for deleted assets where there is no previous revision (missing pak files?)
	for (auto Itr : DeletedRevisions)
	{
		const FString& AssetPath = Itr.Key;
		const FFilePakRevision* Revision = FileRevisions.Find(AssetPath);
		if (Revision == nullptr)
		{
			const FString AssetName = FPaths::GetCleanFilename(AssetPath);
			WriteCSVLine( FString::Printf( TEXT("%s,Deleted,%s,Error,%d,,,%s"), *AssetName, *Itr.Value.PakFilename, Itr.Value.PakPriority, *AssetPath ) );
		}
	}

	//clean up CSV writer
	if (CSVFileWriter)
	{
		CSVFileWriter->Close();
		delete CSVFileWriter;
		CSVFileWriter = NULL;
	}

	return true;
}

bool ListFilesAtOffset( const TCHAR* InPakFileName, const TArray<int64>& InOffsets, bool bSigned )
{
	if( InOffsets.Num() == 0 )
	{
		UE_LOG(LogPakFile, Error, TEXT("No offsets specified") );
		return false;
	}

	FPakFile PakFile(&FPlatformFileManager::Get().GetPlatformFile(), InPakFileName, bSigned);
	if (!PakFile.IsValid())
	{
		UE_LOG(LogPakFile, Error, TEXT("Failed to open %s"), *InPakFileName );
		return false;
	}

	UE_LOG( LogPakFile, Display, TEXT("%-12s%-12s%-12s%s"), TEXT("Offset"), TEXT("File Offset"), TEXT("File Size"), TEXT("File Name") );

	TArray<int64> OffsetsToCheck = InOffsets;
	FArchive& PakReader = *PakFile.GetSharedReader(NULL);
	for (FPakFile::FFileIterator It(PakFile); It; ++It)
	{
		const FPakEntry& Entry = It.Info();

		//see if this file is on of the ones in the offset range we want
		int64 FoundOffset = INDEX_NONE;
		for( int64 Offset : OffsetsToCheck )
		{
			if( Offset >= Entry.Offset && Offset <= Entry.Offset+Entry.Size )
			{
				UE_LOG( LogPakFile, Display, TEXT("%-12lld%-12lld%-12d%s"), Offset, Entry.Offset, Entry.Size, *It.Filename() );
				FoundOffset = Offset;
				break;
			}
		}

		//remove it from the list if we found a match
		if( FoundOffset != INDEX_NONE )
		{
			OffsetsToCheck.Remove(FoundOffset);
		}
	}

	//list out any that we didn't find a match for
	for( int64 InvalidOffset : OffsetsToCheck )
	{
		UE_LOG(LogPakFile, Display, TEXT("%-12lld - invalid offset"), InvalidOffset );
	}

	return true;
}


struct FFileInfo
{
	uint64 FileSize;
	int32 PatchIndex;
	bool bIsDeleteRecord;
	bool bForceInclude;
	uint8 Hash[16];
};

bool ExtractFilesFromPak(const TCHAR* InPakFilename, TMap<FString, FFileInfo>& InFileHashes, const TCHAR* InDestPath, bool bUseMountPoint, const FNamedAESKey& InEncryptionKey, bool bSigned, TArray<FPakInputPair>* OutEntries = nullptr, TArray<FPakInputPair>* OutDeletedEntries = nullptr, TMap<FString, uint64>* OutOrderMap = nullptr)
{
	// Gather all patch versions of the requested pak file and run through each separately
	TArray<FString> PakFileList;
	FString PakFileDirectory = FPaths::GetPath(InPakFilename);
	// If file doesn't exist try using it as a search string, it may contain wild cards
	if (IFileManager::Get().FileExists(InPakFilename))
	{
		PakFileList.Add(*FPaths::GetCleanFilename(InPakFilename));
	}
	else
	{
		IFileManager::Get().FindFiles(PakFileList, *PakFileDirectory, *FPaths::GetCleanFilename(InPakFilename));
	}

	bool bIncludeDeleted = (OutDeletedEntries != nullptr);

	for (int32 PakFileIndex = 0; PakFileIndex < PakFileList.Num(); PakFileIndex++)
	{
		FString PakFilename = PakFileDirectory + "\\" + PakFileList[PakFileIndex];
		int32 PakPriority = GetPakPriorityFromFilename(PakFilename);

		FPakFile PakFile(&FPlatformFileManager::Get().GetPlatformFile(), *PakFilename, bSigned);
		if (PakFile.IsValid())
		{
			FString DestPath(InDestPath);
			FArchive& PakReader = *PakFile.GetSharedReader(NULL);
			const int64 BufferSize = 8 * 1024 * 1024; // 8MB buffer for extracting
			void* Buffer = FMemory::Malloc(BufferSize);
			int64 CompressionBufferSize = 0;
			uint8* PersistantCompressionBuffer = NULL;
			int32 ErrorCount = 0;
			int32 FileCount = 0;
			int32 ExtractedCount = 0;

			FString PakMountPoint = bUseMountPoint ? PakFile.GetMountPoint().Replace(TEXT("../../../"), TEXT("")) : TEXT("");

			for (FPakFile::FFileIterator It(PakFile,bIncludeDeleted); It; ++It, ++FileCount)
			{
				// Extract only the most recent version of a file when present in multiple paks
				FFileInfo* HashFileInfo = InFileHashes.Find(It.Filename());
				if (HashFileInfo == nullptr || HashFileInfo->PatchIndex == PakPriority)
				{
					FString DestFilename(DestPath / PakMountPoint / It.Filename());

					const FPakEntry& Entry = It.Info();
					if (Entry.IsDeleteRecord())
					{
						UE_LOG(LogPakFile, Display, TEXT("Found delete record for \"%s\"."), *It.Filename() );

						FPakInputPair DeleteRecord;
						DeleteRecord.bIsDeleteRecord = true;
						DeleteRecord.Source = DestFilename;
						DeleteRecord.Dest = PakFile.GetMountPoint() / It.Filename();
						OutDeletedEntries->Add(DeleteRecord);
						continue;
					}

					PakReader.Seek(Entry.Offset);
					uint32 SerializedCrcTest = 0;
					FPakEntry EntryInfo;
					EntryInfo.Serialize(PakReader, PakFile.GetInfo().Version);
					if (EntryInfo == Entry)
					{
						TUniquePtr<FArchive> FileHandle(IFileManager::Get().CreateFileWriter(*DestFilename));
						if (FileHandle)
						{
							if (Entry.CompressionMethod == COMPRESS_None)
							{
								BufferedCopyFile(*FileHandle, PakReader, Entry, Buffer, BufferSize, InEncryptionKey);
							}
							else
							{
								UncompressCopyFile(*FileHandle, PakReader, Entry, PersistantCompressionBuffer, CompressionBufferSize, InEncryptionKey, PakFile);
							}
							UE_LOG(LogPakFile, Display, TEXT("Extracted \"%s\" to \"%s\"."), *It.Filename(), *DestFilename);
							ExtractedCount++;

							if (OutOrderMap != nullptr)
							{
								OutOrderMap->Add(DestFilename, OutOrderMap->Num());
							}

							if (OutEntries != nullptr)
							{
								FPakInputPair Input;

								Input.Source = DestFilename;
								FPaths::NormalizeFilename(Input.Source);

								Input.Dest = PakFile.GetMountPoint() + FPaths::GetPath(It.Filename());
								FPaths::NormalizeFilename(Input.Dest);
								FPakFile::MakeDirectoryFromPath(Input.Dest);

								Input.bNeedsCompression = Entry.CompressionMethod != 0;
								Input.bNeedEncryption = Entry.IsEncrypted();
	
								OutEntries->Add(Input);
							}
						}
						else
						{
							UE_LOG(LogPakFile, Error, TEXT("Unable to create file \"%s\"."), *DestFilename);
							ErrorCount++;
						}
					}
					else
					{
						UE_LOG(LogPakFile, Error, TEXT("Serialized hash mismatch for \"%s\"."), *It.Filename());
						ErrorCount++;
					}
				}
			}
			FMemory::Free(Buffer);
			FMemory::Free(PersistantCompressionBuffer);

			UE_LOG(LogPakFile, Log, TEXT("Finished extracting %d (including %d errors)."), ExtractedCount, ErrorCount);
		}
		else
		{
			UE_LOG(LogPakFile, Error, TEXT("Unable to open pak file \"%s\"."), *PakFilename);
			return false;
		}
	}

	return true;
}

void CreateDiffRelativePathMap(TArray<FString>& FileNames, const FString& RootPath, TMap<FName, FString>& OutMap)
{
	for (int32 i = 0; i < FileNames.Num(); ++i)
	{
		const FString& FullPath = FileNames[i];
		FString RelativePath = FullPath.Mid(RootPath.Len());
		OutMap.Add(FName(*RelativePath), FullPath);
	}
}

bool DiffFilesInPaks(const FString& InPakFilename1, const FString& InPakFilename2, const bool bLogUniques1, const bool bLogUniques2, const FNamedAESKey& InEncryptionKey, const bool bSigned)
{
	int32 NumUniquePAK1 = 0;
	int32 NumUniquePAK2 = 0;
	int32 NumDifferentContents = 0;
	int32 NumEqualContents = 0;

	TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);
	UE_LOG(LogPakFile, Log, TEXT("FileEventType, FileName, Size1, Size2"));

	FPakFile PakFile1(&FPlatformFileManager::Get().GetPlatformFile(), *InPakFilename1, bSigned);
	FPakFile PakFile2(&FPlatformFileManager::Get().GetPlatformFile(), *InPakFilename2, bSigned);
	if (PakFile1.IsValid() && PakFile2.IsValid())
	{		
		FArchive& PakReader1 = *PakFile1.GetSharedReader(NULL);
		FArchive& PakReader2 = *PakFile2.GetSharedReader(NULL);

		const int64 BufferSize = 8 * 1024 * 1024; // 8MB buffer for extracting
		void* Buffer = FMemory::Malloc(BufferSize);
		int64 CompressionBufferSize = 0;
		uint8* PersistantCompressionBuffer = NULL;
		int32 ErrorCount = 0;
		int32 FileCount = 0;		
		
		//loop over pak1 entries.  compare against entry in pak2.
		for (FPakFile::FFileIterator It(PakFile1); It; ++It, ++FileCount)
		{
			const FString& PAK1FileName = It.Filename();

			//double check entry info and move pakreader into place
			const FPakEntry& Entry1 = It.Info();
			PakReader1.Seek(Entry1.Offset);

			FPakEntry EntryInfo1;
			EntryInfo1.Serialize(PakReader1, PakFile1.GetInfo().Version);

			if (EntryInfo1 != Entry1)
			{
				UE_LOG(LogPakFile, Log, TEXT("PakEntry1Invalid, %s, 0, 0"), *PAK1FileName);
				continue;
			}
			
			//see if entry exists in other pak							
			FPakEntry Entry2;
			FPakFile::EFindResult FoundEntry2 = PakFile2.Find(PakFile1.GetMountPoint() / PAK1FileName, &Entry2);
			if (FoundEntry2 != FPakFile::EFindResult::Found)
			{
				++NumUniquePAK1;
				if (bLogUniques1)
				{
					UE_LOG(LogPakFile, Log, TEXT("UniqueToFirstPak, %s, %i, 0"), *PAK1FileName, EntryInfo1.UncompressedSize);
				}
				continue;
			}

			//double check entry info and move pakreader into place
			PakReader2.Seek(Entry2.Offset);
			FPakEntry EntryInfo2;
			EntryInfo2.Serialize(PakReader2, PakFile2.GetInfo().Version);
			if (EntryInfo2 != Entry2)
			{
				UE_LOG(LogPakFile, Log, TEXT("PakEntry2Invalid, %s, 0, 0"), *PAK1FileName);
				continue;;
			}

			//check sizes first as quick compare.
			if (EntryInfo1.UncompressedSize != EntryInfo2.UncompressedSize)
			{
				UE_LOG(LogPakFile, Log, TEXT("FilesizeDifferent, %s, %i, %i"), *PAK1FileName, EntryInfo1.UncompressedSize, EntryInfo2.UncompressedSize);
				continue;
			}
			
			//serialize and memcompare the two entries
			{
				FLargeMemoryWriter PAKWriter1(EntryInfo1.UncompressedSize);
				FLargeMemoryWriter PAKWriter2(EntryInfo2.UncompressedSize);

				if (EntryInfo1.CompressionMethod == COMPRESS_None)
				{
					BufferedCopyFile(PAKWriter1, PakReader1, Entry1, Buffer, BufferSize, InEncryptionKey);
				}
				else
				{
					UncompressCopyFile(PAKWriter1, PakReader1, Entry1, PersistantCompressionBuffer, CompressionBufferSize, InEncryptionKey, PakFile1);
				}

				if (EntryInfo2.CompressionMethod == COMPRESS_None)
				{
					BufferedCopyFile(PAKWriter2, PakReader2, Entry2, Buffer, BufferSize, InEncryptionKey);
				}
				else
				{
					UncompressCopyFile(PAKWriter2, PakReader2, Entry2, PersistantCompressionBuffer, CompressionBufferSize, InEncryptionKey, PakFile2);
				}

				if (FMemory::Memcmp(PAKWriter1.GetData(), PAKWriter2.GetData(), EntryInfo1.UncompressedSize) != 0)
				{
					++NumDifferentContents;
					UE_LOG(LogPakFile, Log, TEXT("ContentsDifferent, %s, %i, %i"), *PAK1FileName, EntryInfo1.UncompressedSize, EntryInfo2.UncompressedSize);
				}
				else
				{
					++NumEqualContents;
				}
			}			
		}
		
		//check for files unique to the second pak.
		for (FPakFile::FFileIterator It(PakFile2); It; ++It, ++FileCount)
		{
			const FPakEntry& Entry2 = It.Info();
			PakReader2.Seek(Entry2.Offset);

			FPakEntry EntryInfo2;
			EntryInfo2.Serialize(PakReader2, PakFile2.GetInfo().Version);

			if (EntryInfo2 == Entry2)
			{
				const FString& PAK2FileName = It.Filename();
				FPakEntry Entry1;
				FPakFile::EFindResult FoundEntry1 = PakFile1.Find(PakFile2.GetMountPoint() / PAK2FileName, &Entry1);
				if (FoundEntry1 != FPakFile::EFindResult::Found)
				{
					++NumUniquePAK2;
					if (bLogUniques2)
					{
						UE_LOG(LogPakFile, Log, TEXT("UniqueToSecondPak, %s, 0, %i"), *PAK2FileName, Entry2.UncompressedSize);
					}
					continue;
				}
			}
		}

		FMemory::Free(Buffer);
		Buffer = nullptr;
	}

	UE_LOG(LogPakFile, Log, TEXT("Comparison complete"));
	UE_LOG(LogPakFile, Log, TEXT("Unique to first pak: %i, Unique to second pak: %i, Num Different: %i, NumEqual: %i"), NumUniquePAK1, NumUniquePAK2, NumDifferentContents, NumEqualContents);	
	return true;
}

void GenerateHashForFile(uint8* ByteBuffer, uint64 TotalSize, FFileInfo& FileHash)
{
	FMD5 FileHasher;
	FileHasher.Update(ByteBuffer, TotalSize);
	FileHasher.Final(FileHash.Hash);
	FileHash.FileSize = TotalSize;
}

bool GenerateHashForFile( FString Filename, FFileInfo& FileHash)
{
	FArchive* File = IFileManager::Get().CreateFileReader(*Filename);

	if ( File == NULL )
		return false;

	uint64 TotalSize = File->TotalSize();

	uint8* ByteBuffer = new uint8[TotalSize];

	File->Serialize(ByteBuffer, TotalSize);

	delete File;
	File = NULL;

	GenerateHashForFile(ByteBuffer, TotalSize, FileHash);
	
	delete[] ByteBuffer;
	return true;
}

bool GenerateHashesFromPak(const TCHAR* InPakFilename, const TCHAR* InDestPakFilename, TMap<FString, FFileInfo>& FileHashes, bool bUseMountPoint, const FNamedAESKey& InEncryptionKey, int32& OutLowestSourcePakVersion, const bool bSigned )
{
	OutLowestSourcePakVersion = FPakInfo::PakFile_Version_Initial-1;

	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, InPakFilename, true, false);
	if (FoundFiles.Num() == 0)
	{
		return false;
	}

	// Gather all patch pak files and run through them one at a time
	TArray<FString> PakFileList;
	FString PakFileDirectory = FPaths::GetPath(InPakFilename);
	IFileManager::Get().FindFiles(PakFileList, *PakFileDirectory, *FPaths::GetCleanFilename(InPakFilename));
	for (int32 PakFileIndex = 0; PakFileIndex < PakFileList.Num(); PakFileIndex++)
	{
		FString PakFilename = PakFileDirectory + "\\" + PakFileList[PakFileIndex];
		// Skip the destination pak file so we can regenerate an existing patch level
		if (PakFilename.Equals(InDestPakFilename))
		{
			continue;
		}
		int32 PakPriority = GetPakPriorityFromFilename(PakFilename);
		int32 PakChunkIndex = GetPakChunkIndexFromFilename(PakFilename);

		FPakFile PakFile(&FPlatformFileManager::Get().GetPlatformFile(), *PakFilename, bSigned);
		if (PakFile.IsValid())
		{
			FArchive& PakReader = *PakFile.GetSharedReader(NULL);
			const int64 BufferSize = 8 * 1024 * 1024; // 8MB buffer for extracting
			void* Buffer = FMemory::Malloc(BufferSize);
			int64 CompressionBufferSize = 0;
			uint8* PersistantCompressionBuffer = NULL;
			int32 ErrorCount = 0;
			int32 FileCount = 0;

			//remember the lowest pak version for any patch paks
			if( PakChunkIndex != -1 )
			{
				OutLowestSourcePakVersion = FMath::Min( OutLowestSourcePakVersion, PakFile.GetInfo().Version );
			}

			FString PakMountPoint = bUseMountPoint ? PakFile.GetMountPoint().Replace(TEXT("../../../"), TEXT("")) : TEXT("");

			const bool bIncludeDeleted = true;
			for (FPakFile::FFileIterator It(PakFile,bIncludeDeleted); It; ++It, ++FileCount)
			{
				const FPakEntry& Entry = It.Info();
				FFileInfo FileHash = {};
				bool bEntryValid = false;

				FString FullFilename = PakMountPoint;
				if (!FullFilename.IsEmpty() && !FullFilename.EndsWith("/"))
				{
					FullFilename += "/";
				}
				FullFilename += It.Filename();

				if (Entry.IsDeleteRecord())
				{
					FileHash.PatchIndex = PakPriority;
					FileHash.bIsDeleteRecord = true;
					FileHash.bForceInclude = false;
					bEntryValid = true;
				}
				else
				{
				    PakReader.Seek(Entry.Offset);
				    uint32 SerializedCrcTest = 0;
				    FPakEntry EntryInfo;
				    EntryInfo.Serialize(PakReader, PakFile.GetInfo().Version);
				    if (EntryInfo == Entry)
				    {
					    // TAutoPtr<FArchive> FileHandle(IFileManager::Get().CreateFileWriter(*DestFilename));
					    TArray<uint8> Bytes;
					    FMemoryWriter MemoryFile(Bytes);
					    FArchive* FileHandle = &MemoryFile;
					    // if (FileHandle.IsValid())
					    {
						    if (Entry.CompressionMethod == COMPRESS_None)
						    {
							    BufferedCopyFile(*FileHandle, PakReader, Entry, Buffer, BufferSize, InEncryptionKey);
						    }
						    else
						    {
							    UncompressCopyFile(*FileHandle, PakReader, Entry, PersistantCompressionBuffer, CompressionBufferSize, InEncryptionKey, PakFile);
						    }
    
						    UE_LOG(LogPakFile, Display, TEXT("Generated hash for \"%s\""), *FullFilename);
						    GenerateHashForFile(Bytes.GetData(), Bytes.Num(), FileHash);
						    FileHash.PatchIndex = PakPriority;
						    FileHash.bIsDeleteRecord = false;
							FileHash.bForceInclude = false;
							bEntryValid = true;
						}
						/*else
						{
						UE_LOG(LogPakFile, Error, TEXT("Unable to create file \"%s\"."), *DestFilename);
						ErrorCount++;
						}*/

					}
					else
					{
						UE_LOG(LogPakFile, Error, TEXT("Serialized hash mismatch for \"%s\"."), *It.Filename());
						ErrorCount++;
					}
				}

				if (bEntryValid)
				{
					// Keep only the hash of the most recent version of a file (across multiple pak patch files)
					if (!FileHashes.Contains(FullFilename))
					{
						FileHashes.Add(FullFilename, FileHash);
					}
					else if (FileHashes[FullFilename].PatchIndex < FileHash.PatchIndex)
					{
						FileHashes[FullFilename] = FileHash;
					}
				}
			}
			FMemory::Free(Buffer);
			FMemory::Free(PersistantCompressionBuffer);

			UE_LOG(LogPakFile, Log, TEXT("Finished extracting %d files (including %d errors)."), FileCount, ErrorCount);
		}
		else
		{
			UE_LOG(LogPakFile, Error, TEXT("Unable to open pak file \"%s\"."), *PakFilename);
			return false;
		}
	}

	return true;
}

bool FileIsIdentical(FString SourceFile, FString DestFilename, const FFileInfo* Hash)
{
	int64 SourceTotalSize = Hash ? Hash->FileSize : IFileManager::Get().FileSize(*SourceFile);
	int64 DestTotalSize = IFileManager::Get().FileSize(*DestFilename);

	if (SourceTotalSize != DestTotalSize)
	{
		// file size doesn't match 
		UE_LOG(LogPakFile, Display, TEXT("Source file size for %s %d bytes doesn't match %s %d bytes, did find %d"), *SourceFile, SourceTotalSize, *DestFilename, DestTotalSize, Hash ? 1 : 0);
		return false;
	}

	FFileInfo SourceFileHash;
	if (!Hash)
	{
		if (GenerateHashForFile(SourceFile, SourceFileHash) == false)
		{
			// file size doesn't match 
			UE_LOG(LogPakFile, Display, TEXT("Source file size %s doesn't exist will be included in build"), *SourceFile);
			return false;;
		}
		else
		{
			UE_LOG(LogPakFile, Warning, TEXT("Generated hash for file %s but it should have been in the FileHashes array"), *SourceFile);
		}
	}
	else
	{
		SourceFileHash = *Hash;
	}

	FFileInfo DestFileHash;
	if (GenerateHashForFile(DestFilename, DestFileHash) == false)
	{
		// destination file was removed don't really care about it
		UE_LOG(LogPakFile, Display, TEXT("File was removed from destination cooked content %s not included in patch"), *DestFilename);
		return false;
	}

	int32 Diff = FMemory::Memcmp(&SourceFileHash.Hash, &DestFileHash.Hash, sizeof(DestFileHash.Hash));
	if (Diff != 0)
	{
		UE_LOG(LogPakFile, Display, TEXT("Source file hash for %s doesn't match dest file hash %s and will be included in patch"), *SourceFile, *DestFilename);
		return false;
	}

	return true;
}

void RemoveIdenticalFiles( TArray<FPakInputPair>& FilesToPak, const FString& SourceDirectory, const TMap<FString, FFileInfo>& FileHashes )
{
	FString HashFilename = SourceDirectory / TEXT("Hashes.txt");

	if (IFileManager::Get().FileExists(*HashFilename) )
	{
		FString EntireFile;
		FFileHelper::LoadFileToString(EntireFile, *HashFilename);
	}

	TArray<FString> FilesToRemove;

	for ( int I = FilesToPak.Num()-1; I >= 0; --I )
	{
		const auto& NewFile = FilesToPak[I]; 
		if( NewFile.bIsDeleteRecord )
		{
			continue;
		}

		FString SourceFileNoMountPoint =  NewFile.Dest.Replace(TEXT("../../../"), TEXT(""));
		FString SourceFilename = SourceDirectory / NewFile.Dest.Replace(TEXT("../../../"), TEXT(""));
		
		const FFileInfo* FoundFileHash = FileHashes.Find(SourceFileNoMountPoint);
		if (!FoundFileHash)
		{
			FoundFileHash = FileHashes.Find(NewFile.Dest);
		}
		
 		if ( !FoundFileHash )
 		{
 			UE_LOG(LogPakFile, Display, TEXT("Didn't find hash for %s No mount %s"), *SourceFilename, *SourceFileNoMountPoint);
 		}
 
		// uexp files are always handled with their corresponding uasset file
		if (!FPaths::GetExtension(SourceFilename).Equals("uexp", ESearchCase::IgnoreCase))
		{
			bool bForceInclude = FoundFileHash && FoundFileHash->bForceInclude;

			FString DestFilename = NewFile.Source;
			if (!bForceInclude && FileIsIdentical(SourceFilename, DestFilename, FoundFileHash))
			{
				// Check for uexp files only for uasset files
				FString Ext(FPaths::GetExtension(SourceFilename));
				if (Ext.Equals("uasset", ESearchCase::IgnoreCase) || Ext.Equals("umap", ESearchCase::IgnoreCase))
				{
					FString UexpSourceFilename = FPaths::ChangeExtension(SourceFilename, "uexp");
					FString UexpSourceFileNoMountPoint = FPaths::ChangeExtension(SourceFileNoMountPoint, "uexp");

					const FFileInfo* UexpFoundFileHash = FileHashes.Find(UexpSourceFileNoMountPoint);
					if (!UexpFoundFileHash)
					{
						UexpFoundFileHash = FileHashes.Find(FPaths::ChangeExtension(NewFile.Dest, "uexp"));
					}

					if (!UexpFoundFileHash)
					{
						UE_LOG(LogPakFile, Display, TEXT("Didn't find hash for %s No mount %s"), *UexpSourceFilename, *UexpSourceFileNoMountPoint);
					}

					if (UexpFoundFileHash || IFileManager::Get().FileExists(*UexpSourceFilename))
					{

						FString UexpDestFilename = FPaths::ChangeExtension(NewFile.Source, "uexp");
						if (!FileIsIdentical(UexpSourceFilename, UexpDestFilename, UexpFoundFileHash))
						{
							UE_LOG(LogPakFile, Display, TEXT("%s not identical for %s. Including both files in patch."), *UexpSourceFilename, *SourceFilename);
							continue;
						}
						// Add this file to the list to be removed from FilesToPak after we finish processing (since this file was found at random within 
						// the list we cannot remove it or we'll mess up our containing for loop)
						FilesToRemove.Add(UexpDestFilename);
					}
				}

				UE_LOG(LogPakFile, Display, TEXT("Source file %s matches dest file %s and will not be included in patch"), *SourceFilename, *DestFilename);
				// remove from the files to pak list
				FilesToPak.RemoveAt(I);
			}
		}
	}

	// Clean up uexp files that were marked for removal, assume files may only be listed one in FilesToPak
	for (int FileIndexToRemove = 0; FileIndexToRemove < FilesToRemove.Num(); FileIndexToRemove++)
	{
		const FPakInputPair FileSourceToRemove(FilesToRemove[FileIndexToRemove], "");
		FilesToPak.RemoveSingle(FileSourceToRemove);
	}
}

void ProcessLegacyFileMoves( TArray<FPakInputPair>& InDeleteRecords, TMap<FString, FFileInfo>& InExistingPackagedFileHashes, const FString& InInputPath, const TArray<FPakInputPair>& InFilesToPak, int32 CurrentPatchChunkIndex, bool bSigned )
{
	double StartTime = FPlatformTime::Seconds();


	TArray<FString> PakFileList;
	IFileManager::Get().FindFiles(PakFileList, *InInputPath, TEXT(".pak") );
	if( PakFileList.Num() == 0 )
	{
		UE_LOG( LogPakFile, Error, TEXT("No pak files searching \"%s\""), *InInputPath );
		return;
	}

	struct FFileChunkRevisionInfo
	{
		FString PakFilename;
		int32 PakPriority;
		int32 PakChunkIndex;
		int32 PakVersion;
	};
	TMap<FString, FFileChunkRevisionInfo> DeletedFileRevisions;
	TMap<FString, FFileChunkRevisionInfo> RequiredFileRevisions;

	TSet<FString> DeleteRecordSourceNames;
	for (const FPakInputPair& DeleteRecord : InDeleteRecords)
	{
		DeleteRecordSourceNames.Add(DeleteRecord.Source);
	}

	TSet<FString> FilesToPakDestNames;
	for (const FPakInputPair& FileToPak : InFilesToPak)
	{
		FilesToPakDestNames.Add(FileToPak.Dest);
	}

	for (int32 PakFileIndex = 0; PakFileIndex < PakFileList.Num(); PakFileIndex++)
	{
		FString PakFilename = InInputPath + "\\" + PakFileList[PakFileIndex];
		int32 PakPriority = GetPakPriorityFromFilename(PakFilename);
		int32 PakChunkIndex = GetPakChunkIndexFromFilename(PakFilename);

		UE_LOG(LogPakFile, Display, TEXT("Checking old pak file \"%s\" Pri:%d Chunk:%d."), *PakFilename, PakPriority, PakChunkIndex );


		FPakFile PakFile(&FPlatformFileManager::Get().GetPlatformFile(), *PakFilename, bSigned);
		if (PakFile.IsValid())
		{
			FString PakMountPoint = PakFile.GetMountPoint().Replace(TEXT("../../../"), TEXT(""));

			const bool bIncludeDeleted = true;
			for (FPakFile::FFileIterator It(PakFile,bIncludeDeleted); It; ++It)
			{
				FString AssetName = PakMountPoint;
				if (!AssetName.IsEmpty() && !AssetName.EndsWith("/"))
				{
					AssetName += "/";
				}
				AssetName += It.Filename();

				bool bHasNewDeleteRecord = DeleteRecordSourceNames.Contains(AssetName);

				FFileChunkRevisionInfo Revision;
				Revision.PakFilename = PakFileList[PakFileIndex];
				Revision.PakPriority = PakPriority;
				Revision.PakChunkIndex = PakChunkIndex;
				Revision.PakVersion = PakFile.GetInfo().Version;

				TMap<FString, FFileChunkRevisionInfo>* DestList = nullptr;

				if( bHasNewDeleteRecord )
				{
					DestList = &DeletedFileRevisions;
				}
				else if( InExistingPackagedFileHashes.Contains(AssetName) )
				{
					FString DestAssetName = TEXT("../../../") + AssetName;
					bool bRequiredFile = FilesToPakDestNames.Contains(DestAssetName);

					if(bRequiredFile)
					{
						DestList = &RequiredFileRevisions;
					}
				}

				if( DestList != nullptr )
				{
					if( !DestList->Contains(AssetName) )
					{
						DestList->Add(AssetName,Revision);
					}
					else if( (*DestList)[AssetName].PakPriority < PakPriority )
					{
						(*DestList)[AssetName] = Revision;
					}
				}

			}
		}
	}

	//prevent delete records being created for files that have historically been moved
	for (auto Itr : DeletedFileRevisions)
	{
		UE_LOG(LogPakFile, Display, TEXT("checking deleted revision %s chunk %d vs %d   pak version %d vs %d"), *Itr.Key, Itr.Value.PakChunkIndex, CurrentPatchChunkIndex, Itr.Value.PakVersion, FPakInfo::PakFile_Version_DeleteRecords );

		//asset hasn't been deleted in the latest version and the latest known version is in a different chunk to us from a previous version of unrealpak
		if( Itr.Value.PakChunkIndex != CurrentPatchChunkIndex )
		{
			int NumDeleted = InDeleteRecords.RemoveAll( [&]( const FPakInputPair& InPair )
			{
				return InPair.Source == Itr.Key;
			});
			if( NumDeleted > 0 )
			{
				UE_LOG( LogPakFile, Display, TEXT("Ignoring delete record for %s - it was moved to %s before delete records were created"), *Itr.Key, *FPaths::GetCleanFilename(Itr.Value.PakFilename) );
			}
		}
	}

	//make sure files who's latest revision was in a different chunk to the one we're building are added to the pak
	//#TODO: I think this RequiredFileRevision code is not needed
	for (auto Itr : RequiredFileRevisions)
	{
		if (Itr.Value.PakVersion < FPakInfo::PakFile_Version_DeleteRecords && Itr.Value.PakChunkIndex != CurrentPatchChunkIndex )
		{
			if( InExistingPackagedFileHashes.Contains(Itr.Key) )
			{
				UE_LOG( LogPakFile, Display, TEXT("Ensuring %s is included in the pak file - it was moved to %s before delete records were created"), *Itr.Key, *FPaths::GetCleanFilename(Itr.Value.PakFilename) );
				InExistingPackagedFileHashes[Itr.Key].bForceInclude = true;
			}
		}
	}

	UE_LOG(LogPakFile, Display, TEXT("...took %.2fs to manage legacy patch pak files"), FPlatformTime::Seconds() - StartTime );
}


TArray<FPakInputPair> GetNewDeleteRecords( const TArray<FPakInputPair>& InFilesToPak, const TMap<FString, FFileInfo>& InExistingPackagedFileHashes)
{
	double StartTime = FPlatformTime::Seconds();
	TArray<FPakInputPair> DeleteRecords;

	//build lookup table of files to pack
	TSet<FString> FilesToPack;
	for (const FPakInputPair& PakEntry : InFilesToPak)
	{
		FString PakFilename = PakEntry.Dest.Replace(TEXT("../../../"), TEXT(""));
		FilesToPack.Add(PakFilename);
	}

	//check all assets in the previous patch packs
	for (const TTuple<FString, FFileInfo>& Pair : InExistingPackagedFileHashes)
	{
		//ignore this file if the most recent revision is deleted already
		if (Pair.Value.bIsDeleteRecord)
		{
			continue;
		}

		//see if the file exists in the files to package
		FString SourceFileName = Pair.Key;
		bool bFound = FilesToPack.Contains(SourceFileName);

		if (bFound == false)
		{
			//file cannot be found now, and was not deleted in the most recent pak patch
			FPakInputPair DeleteRecord;
			DeleteRecord.bIsDeleteRecord = true;
			DeleteRecord.Source = SourceFileName;
			DeleteRecord.Dest = TEXT("../../../") + SourceFileName;
			DeleteRecords.Add(DeleteRecord);
			UE_LOG(LogPakFile, Display, TEXT("Existing pak entry %s not found in new pak asset list, so a delete record will be created in the patch pak."), *SourceFileName);
		}
 	}


	UE_LOG(LogPakFile, Display, TEXT("Took %.2fS for delete records"), FPlatformTime::Seconds()-StartTime );
	return DeleteRecords;
}

FString GetPakPath(const TCHAR* SpecifiedPath, bool bIsForCreation)
{
	FString PakFilename(SpecifiedPath);
	FPaths::MakeStandardFilename(PakFilename);
	
	// if we are trying to open (not create) it, but BaseDir relative doesn't exist, look in LaunchDir
	if (!bIsForCreation && !FPaths::FileExists(PakFilename))
	{
		PakFilename = FPaths::LaunchDir() + SpecifiedPath;

		if (!FPaths::FileExists(PakFilename))
		{
			UE_LOG(LogPakFile, Fatal, TEXT("Existing pak file %s could not be found (checked against binary and launch directories)"), SpecifiedPath);
			return TEXT("");
		}
	}
	
	return PakFilename;
}

bool Repack(const FString& InputPakFile, const FString& OutputPakFile, const FPakCommandLineParameters& CmdLineParameters, const FKeyPair& SigningKey, const FNamedAESKey& InEncryptionKey, bool bIncludeDeleted, bool bSigned)
{
	bool bResult = false;

	// Extract the existing pak file
	TMap<FString, FFileInfo> Hashes;
	TArray<FPakInputPair> Entries;
	TArray<FPakInputPair> DeletedEntries;
	TMap<FString, uint64> OrderMap;
	FString TempDir = FPaths::EngineIntermediateDir() / TEXT("UnrealPak") / TEXT("Repack") / FPaths::GetBaseFilename(InputPakFile);
	if (ExtractFilesFromPak(*InputPakFile, Hashes, *TempDir, false, InEncryptionKey, bSigned, &Entries, &DeletedEntries, &OrderMap))
	{
		TArray<FPakInputPair> FilesToAdd;
		CollectFilesToAdd(FilesToAdd, Entries, OrderMap);

		if (bIncludeDeleted)
		{
			for( const FPakInputPair& Entry : DeletedEntries )
			{
				FilesToAdd.Add(Entry);
			}
		}
		else if (DeletedEntries.Num() > 0)
		{
			UE_LOG(LogPakFile, Display, TEXT("%s has %d delete records - these will not be included in the repackage. Specify -IncludeDeleted to include them"), *InputPakFile, DeletedEntries.Num() );
		}

		// Get a temporary output filename. We'll only create/replace the final output file once successful.
		FString TempOutputPakFile = FPaths::CreateTempFilename(*FPaths::GetPath(OutputPakFile), *FPaths::GetCleanFilename(OutputPakFile));

		// Create the new pak file
		UE_LOG(LogPakFile, Display, TEXT("Creating %s..."), *OutputPakFile);
		if (CreatePakFile(*TempOutputPakFile, FilesToAdd, CmdLineParameters, SigningKey, InEncryptionKey))
		{
			IFileManager::Get().Move(*OutputPakFile, *TempOutputPakFile);

			FString OutputSigFile = FPaths::ChangeExtension(OutputPakFile, TEXT(".sig"));
			if (IFileManager::Get().FileExists(*OutputSigFile))
			{
				IFileManager::Get().Delete(*OutputSigFile);
			}

			FString TempOutputSigFile = FPaths::ChangeExtension(TempOutputPakFile, TEXT(".sig"));
			if (IFileManager::Get().FileExists(*TempOutputSigFile))
			{
				IFileManager::Get().Move(*OutputSigFile, *TempOutputSigFile);
			}

			bResult = true;
		}
	}
	IFileManager::Get().DeleteDirectory(*TempDir, false, true);

	return bResult;
}

/**
 * Application entry point
 * Params:
 *   -Test test if the pak file is healthy
 *   -Extract extracts pak file contents (followed by a path, i.e.: -extract D:\ExtractedPak)
 *   -Create=filename response file to create a pak file with
 *   -Sign=filename use the key pair in filename to sign a pak file, or: -sign=key_hex_values_separated_with_+, i.e: -sign=0x123456789abcdef+0x1234567+0x12345abc
 *    where the first number is the private key exponend, the second one is modulus and the third one is the public key exponent.
 *   -Signed use with -extract and -test to let the code know this is a signed pak
 *   -GenerateKeys=filename generates encryption key pair for signing a pak file
 *   -P=prime will use a predefined prime number for generating encryption key file
 *   -Q=prime same as above, P != Q, GCD(P, Q) = 1 (which is always true if they're both prime)
 *   -GeneratePrimeTable=filename generates a prime table for faster prime number generation (.inl file)
 *   -TableMax=number maximum prime number in the generated table (default is 10000)
 *
 * @param	ArgC	Command-line argument count
 * @param	ArgV	Argument strings
 */
bool ExecuteUnrealPak(const TCHAR* CmdLine)
{
	// Parse all the non-option arguments from the command line
	TArray<FString> NonOptionArguments;
	for (const TCHAR* CmdLineEnd = CmdLine; *CmdLineEnd != 0;)
	{
		FString Argument = FParse::Token(CmdLineEnd, false);
		if (Argument.Len() > 0 && !Argument.StartsWith(TEXT("-")))
		{
			NonOptionArguments.Add(Argument);
		}
	}

	FKeyPair SigningKey;
	FNamedAESKey EncryptionKey;
	PrepareEncryptionAndSigningKeys(CmdLine, SigningKey, EncryptionKey);

	FString BatchFileName;
	if (FParse::Value(CmdLine, TEXT("-Batch="), BatchFileName))
	{
		TArray<FString> Commands;
		if (!FFileHelper::LoadFileToStringArray(Commands, *BatchFileName))
		{
			UE_LOG(LogPakFile, Error, TEXT("Unable to read '%s'"), *BatchFileName);
			return false;
		}

		TAtomic<bool> Result(true);
		ParallelFor(Commands.Num(), [&Commands, &Result](int32 Idx) { if (!ExecuteUnrealPak(*Commands[Idx])) { Result = false; } });
		return Result;
	}

	FString KeyFilename;
	if (FParse::Value(CmdLine, TEXT("GenerateKeys="), KeyFilename, false))
	{
		return GenerateKeys(*KeyFilename);
	}

	if (FParse::Value(CmdLine, TEXT("GeneratePrimeTable="), KeyFilename, false))
	{
		int64 MaxPrimeValue = 10000;
		FParse::Value(CmdLine, TEXT("TableMax="), MaxPrimeValue);
		GeneratePrimeNumberTable(MaxPrimeValue, *KeyFilename);
		return true;
	}

	if (FParse::Param(CmdLine, TEXT("TestEncryption")))
	{
		void TestEncryption();
		TestEncryption();
		return true;
	}

	if (FParse::Param(CmdLine, TEXT("Test")))
	{
		if (NonOptionArguments.Num() != 1)
		{
			UE_LOG(LogPakFile, Error, TEXT("Incorrect arguments. Expected: -Test <PakFile>"));
			return false;
		}

		FString PakFilename = GetPakPath(*NonOptionArguments[0], false);
		bool bSigned = FParse::Param(CmdLine, TEXT("signed"));
		return TestPakFile(*PakFilename, bSigned);
	}

	if (FParse::Param(CmdLine, TEXT("List")))
	{
		if (NonOptionArguments.Num() != 1)
		{
			UE_LOG(LogPakFile, Error, TEXT("Incorrect arguments. Expected: -List <PakFile> [-SizeFilter=N] [-Signed]"));
			return false;
		}

		int64 SizeFilter = 0;
		FParse::Value(CmdLine, TEXT("SizeFilter="), SizeFilter);

		bool bExcludeDeleted = FParse::Param( CmdLine, TEXT("ExcludeDeleted") );

		FString PakFilename = GetPakPath(*NonOptionArguments[0], false);
		bool bSigned = FParse::Param(CmdLine, TEXT("signed"));

		FString CSVFilename;
		FParse::Value(CmdLine, TEXT("csv="), CSVFilename);

		bool bExtractToMountPoint = FParse::Param(CmdLine, TEXT("ExtractToMountPoint"));

		return ListFilesInPak(*PakFilename, SizeFilter, !bExcludeDeleted, *CSVFilename, bExtractToMountPoint, bSigned);
	}

	if (FParse::Param(CmdLine, TEXT("Diff")))
	{
		if(NonOptionArguments.Num() != 2)
		{
			UE_LOG(LogPakFile,Error,TEXT("Incorrect arguments. Expected: -Diff <PakFile1> <PakFile2> [-NoUniques] [-NoUniquesFile1] [-NoUniquesFile2]"));
			return false;
		}

		FString PakFilename1 = GetPakPath(*NonOptionArguments[0], false);
		FString PakFilename2 = GetPakPath(*NonOptionArguments[1], false);

		// Allow the suppression of unique file logging for one or both files
		const bool bLogUniques = !FParse::Param(CmdLine, TEXT("nouniques"));
		const bool bLogUniques1 = bLogUniques && !FParse::Param(CmdLine, TEXT("nouniquesfile1"));
		const bool bLogUniques2 = bLogUniques && !FParse::Param(CmdLine, TEXT("nouniquesfile2"));

		const bool bSigned = FParse::Param(CmdLine, TEXT("signed"));

		return DiffFilesInPaks(PakFilename1, PakFilename2, bLogUniques1, bLogUniques2, EncryptionKey, bSigned);
	}

	if (FParse::Param(CmdLine, TEXT("Extract")))
	{
		if (NonOptionArguments.Num() != 2)
		{
			UE_LOG(LogPakFile, Error, TEXT("Incorrect arguments. Expected: -Extract <PakFile> <OutputPath>"));
			return false;
		}

		FString PakFilename = GetPakPath(*NonOptionArguments[0], false);
		bool bSigned = FParse::Param(CmdLine, TEXT("signed"));

		FString DestPath = NonOptionArguments[1];
		bool bExtractToMountPoint = FParse::Param(CmdLine, TEXT("ExtractToMountPoint"));
		TMap<FString, FFileInfo> EmptyMap;
		return ExtractFilesFromPak(*PakFilename, EmptyMap, *DestPath, bExtractToMountPoint, EncryptionKey, bSigned);
	}

	if (FParse::Param(CmdLine, TEXT("AuditFiles")))
	{
		if (NonOptionArguments.Num() != 1)
		{
			UE_LOG(LogPakFile, Error, TEXT("Incorrect arguments. Expected: -AuditFiles <PakFile> -CSV=<OutputPath> [-OnlyDeleted]"));
			return false;
		}
		
		FString PakFilename = GetPakPath(*NonOptionArguments[0], false);
		
		FString CSVFilename;
		FParse::Value( CmdLine, TEXT("CSV="), CSVFilename );

		bool bOnlyDeleted = FParse::Param( CmdLine, TEXT("OnlyDeleted") );
		bool bSigned = FParse::Param(CmdLine, TEXT("signed"));

		return AuditPakFiles(*PakFilename, bOnlyDeleted, CSVFilename, bSigned);
	}
	
	if (FParse::Param(CmdLine, TEXT("WhatsAtOffset")))
	{
		if (NonOptionArguments.Num() < 2)
		{
			UE_LOG(LogPakFile, Error, TEXT("Incorrect arguments. Expected: -WhatsAtOffset <PakFile> [Offset...]"));
			return false;
		}
		
		FString PakFilename = GetPakPath(*NonOptionArguments[0], false);

		TArray<int64> Offsets;
		for( int ArgI = 1; ArgI < NonOptionArguments.Num(); ArgI++ )
		{
			if( FCString::IsNumeric(*NonOptionArguments[ArgI]) )
			{
				Offsets.Add( FCString::Strtoi64( *NonOptionArguments[ArgI], nullptr, 10 ) );
			}
		}

		bool bSigned = FParse::Param(CmdLine, TEXT("signed"));

		return ListFilesAtOffset( *PakFilename, Offsets, bSigned );
	}
	
	if (FParse::Param(CmdLine, TEXT("Repack")))
	{
		if (NonOptionArguments.Num() != 1)
		{
			UE_LOG(LogPakFile, Error, TEXT("Incorrect arguments. Expected: -Repack <PakFile> [-Output=<PakFile>] [-Signed]"));
			return false;
		}

		TArray<FPakInputPair> Entries;
		FPakCommandLineParameters CmdLineParameters;
		PreProcessCommandline(CmdLine, CmdLineParameters);
		ProcessCommandLine(CmdLine, NonOptionArguments, Entries, CmdLineParameters);

		// Find all the input pak files
		FString InputDir = FPaths::GetPath(*NonOptionArguments[0]);

		TArray<FString> InputPakFiles;
		IFileManager::Get().FindFiles(InputPakFiles, *InputDir, *FPaths::GetCleanFilename(*NonOptionArguments[0]));

		for (int Idx = 0; Idx < InputPakFiles.Num(); Idx++)
		{
			InputPakFiles[Idx] = InputDir / InputPakFiles[Idx];
		}

		if (InputPakFiles.Num() == 0)
		{
			UE_LOG(LogPakFile, Error, TEXT("No files found matching '%s'"), *NonOptionArguments[0]);
			return false;
		}

		// Find all the output paths
		TArray<FString> OutputPakFiles;

		FString OutputPath;
		if (!FParse::Value(CmdLine, TEXT("Output="), OutputPath, false))
		{
			for (const FString& InputPakFile : InputPakFiles)
			{
				OutputPakFiles.Add(InputPakFile);
			}
		}
		else if (IFileManager::Get().DirectoryExists(*OutputPath))
		{
			for (const FString& InputPakFile : InputPakFiles)
			{
				OutputPakFiles.Add(FPaths::Combine(OutputPath, FPaths::GetCleanFilename(InputPakFile)));
			}
		}
		else
		{
			for (const FString& InputPakFile : InputPakFiles)
			{
				OutputPakFiles.Add(OutputPath);
			}
		}

		bool bExcludeDeleted = FParse::Param(CmdLine, TEXT("ExcludeDeleted"));

		// Repack them all
		bool bSigned = FParse::Param(CmdLine, TEXT("signed"));
		for (int Idx = 0; Idx < InputPakFiles.Num(); Idx++)
		{
			UE_LOG(LogPakFile, Display, TEXT("Repacking %s into %s"), *InputPakFiles[Idx], *OutputPakFiles[Idx]);
			if (!Repack(InputPakFiles[Idx], OutputPakFiles[Idx], CmdLineParameters, SigningKey, EncryptionKey, !bExcludeDeleted, bSigned))
			{
				return false;
			}
		}

		return true;
	}

	if(NonOptionArguments.Num() > 0)
	{
		// since this is for creation, we pass true to make it not look in LaunchDir
		FString PakFilename = GetPakPath(*NonOptionArguments[0], true);
		bool bSigned = FParse::Param(CmdLine, TEXT("signed"));

		// List of all items to add to pak file
		TArray<FPakInputPair> Entries;
		FPakCommandLineParameters CmdLineParameters;
		ProcessCommandLine(CmdLine, NonOptionArguments, Entries, CmdLineParameters);

		TMap<FString, uint64> OrderMap;
		FString ResponseFile;
		if (FParse::Value(CmdLine, TEXT("-order="), ResponseFile) && !ProcessOrderFile(*ResponseFile, OrderMap))
		{
			return false;
		}

		if (Entries.Num() == 0)
		{
			UE_LOG(LogPakFile, Error, TEXT("No files specified to add to pak file."));
			return false;
		}

		int32 LowestSourcePakVersion = 0;
		TMap<FString, FFileInfo> SourceFileHashes;

		if ( CmdLineParameters.GeneratePatch )
		{
			FString OutputPath;
			if (!FParse::Value(CmdLine, TEXT("TempFiles="), OutputPath))
			{
				OutputPath = FPaths::GetPath(PakFilename) / FString(TEXT("TempFiles"));
			}

			IFileManager::Get().DeleteDirectory(*OutputPath);

			// Check command line for the "patchcryptokeys" param, which will tell us where to look for the encryption keys that
			// we need to access the patch reference data
			FString PatchReferenceCryptoKeysFilename;
			FNamedAESKey PatchReferenceEncryptionKey = EncryptionKey;
			if (FParse::Value(CmdLine, TEXT("PatchCryptoKeys="), PatchReferenceCryptoKeysFilename))
			{
				FKeyPair UnusedSigningKey;
				TArray<FNamedAESKey> UnusedSecondaryEncryptionKeys;
				PrepareEncryptionAndSigningKeysFromCryptoKeyCache(PatchReferenceCryptoKeysFilename, UnusedSigningKey, PatchReferenceEncryptionKey, UnusedSecondaryEncryptionKeys);
			}

			UE_LOG(LogPakFile, Display, TEXT("Generating patch from %s."), *CmdLineParameters.SourcePatchPakFilename, true );

			if (!GenerateHashesFromPak(*CmdLineParameters.SourcePatchPakFilename, *PakFilename, SourceFileHashes, true, PatchReferenceEncryptionKey, /*Out*/LowestSourcePakVersion, bSigned))
			{
				if (ExtractFilesFromPak(*CmdLineParameters.SourcePatchPakFilename, SourceFileHashes, *OutputPath, true, PatchReferenceEncryptionKey, bSigned) == false)
				{
					UE_LOG(LogPakFile, Warning, TEXT("Unable to extract files from source pak file for patch"));
				}
				else
				{
					CmdLineParameters.SourcePatchDiffDirectory = OutputPath;
				}
			}
		}


		// Start collecting files
		TArray<FPakInputPair> FilesToAdd;
		CollectFilesToAdd(FilesToAdd, Entries, OrderMap);

		if ( CmdLineParameters.GeneratePatch )
		{
			// We need to get a list of files that were in the previous patch('s) Pak, but NOT in FilesToAdd
			TArray<FPakInputPair> DeleteRecords = GetNewDeleteRecords(FilesToAdd, SourceFileHashes);

			//if the patch is built using old source pak files, we need to handle the special case where a file has been moved between chunks but no delete record was created (this would cause a rogue delete record to be created in the latest pak), and also a case where the file was moved between chunks and back again without being changed (this would cause the file to not be included in this chunk because the file would be considered unchanged)
			if (LowestSourcePakVersion < FPakInfo::PakFile_Version_DeleteRecords)
			{
				int32 CurrentPatchChunkIndex = GetPakChunkIndexFromFilename(PakFilename);

				UE_LOG(LogPakFile, Display, TEXT("Some patch source paks were generated with an earlier version of UnrealPak that didn't support delete records. checking for historic assets that have moved between chunks to avoid creating invalid delete records"));
				FString SourcePakFolder = FPaths::GetPath(CmdLineParameters.SourcePatchPakFilename);

				//remove invalid items from DeleteRecords and set 'bForceInclude' on some SourceFileHashes
				ProcessLegacyFileMoves(DeleteRecords, SourceFileHashes, SourcePakFolder, FilesToAdd, CurrentPatchChunkIndex, bSigned);
			}
			FilesToAdd.Append(DeleteRecords);

			// if we are generating a patch here we remove files which are already shipped...
			RemoveIdenticalFiles(FilesToAdd, CmdLineParameters.SourcePatchDiffDirectory, SourceFileHashes);
		}


		bool bResult = CreatePakFile(*PakFilename, FilesToAdd, CmdLineParameters, SigningKey, EncryptionKey);

		if (CmdLineParameters.GeneratePatch)
		{
			FString OutputPath = FPaths::GetPath(PakFilename) / FString(TEXT("TempFiles"));
			// delete the temporary directory
			IFileManager::Get().DeleteDirectory(*OutputPath, false, true);
		}

		return bResult;
	}

	UE_LOG(LogPakFile, Error, TEXT("No pak file name specified. Usage:"));
	UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -Test"));
	UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -List [-ExcludeDeleted]"));
	UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> <GameUProjectName> <GameFolderName> -ExportDependencies=<OutputFileBase> -NoAssetRegistryCache -ForceDependsGathering"));
	UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -Extract <ExtractDir>"));
	UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -Create=<ResponseFile> [Options]"));
	UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -Dest=<MountPoint>"));
	UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -Repack [-Output=Path] [-ExcludeDeleted] [Options]"));
	UE_LOG(LogPakFile, Error, TEXT("  UnrealPak GenerateKeys=<KeyFilename>"));
	UE_LOG(LogPakFile, Error, TEXT("  UnrealPak GeneratePrimeTable=<KeyFilename> [-TableMax=<N>]"));
	UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename1> <PakFilename2> -diff"));
	UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFolder> -AuditFiles [-OnlyDeleted] [-CSV=<filename>]"));
	UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -WhatsAtOffset [offset1] [offset2] [offset3] [...]"));
	UE_LOG(LogPakFile, Error, TEXT("  UnrealPak -TestEncryption"));
	UE_LOG(LogPakFile, Error, TEXT("  Options:"));
	UE_LOG(LogPakFile, Error, TEXT("    -blocksize=<BlockSize>"));
	UE_LOG(LogPakFile, Error, TEXT("    -bitwindow=<BitWindow>"));
	UE_LOG(LogPakFile, Error, TEXT("    -compress"));
	UE_LOG(LogPakFile, Error, TEXT("    -encrypt"));
	UE_LOG(LogPakFile, Error, TEXT("    -order=<OrderingFile>"));
	UE_LOG(LogPakFile, Error, TEXT("    -diff (requires 2 filenames first)"));
	UE_LOG(LogPakFile, Error, TEXT("    -enginedir (specify engine dir for when using ini encryption configs)"));
	UE_LOG(LogPakFile, Error, TEXT("    -projectdir (specify project dir for when using ini encryption configs)"));
	UE_LOG(LogPakFile, Error, TEXT("    -encryptionini (specify ini base name to gather encryption settings from)"));
	UE_LOG(LogPakFile, Error, TEXT("    -extracttomountpoint (Extract to mount point path of pak file)"));
	UE_LOG(LogPakFile, Error, TEXT("    -encryptindex (encrypt the pak file index, making it unusable in unrealpak without supplying the key)"));
	UE_LOG(LogPakFile, Error, TEXT("    -compressor=<DllPath> (register a custom compressor)"))
	UE_LOG(LogPakFile, Error, TEXT("    -overrideplatformcompressor (override the native platform compressor)"))
	return false;
}
