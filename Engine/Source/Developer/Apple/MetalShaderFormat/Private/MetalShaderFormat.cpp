// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MetalShaderFormat.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "ShaderCore.h"
#include "Interfaces/IShaderFormatArchive.h"
#include "hlslcc.h"
#include "MetalShaderResources.h"
#include "HAL/FileManager.h"
#include "Serialization/Archive.h"
#include "Misc/ConfigCacheIni.h"
#include "MetalBackend.h"

extern uint16 GetXcodeVersion(uint64& BuildVersion);
extern bool StripShader_Metal(TArray<uint8>& Code, class FString const& DebugPath, bool const bNative);
extern uint64 AppendShader_Metal(class FName const& Format, class FString const& ArchivePath, const FSHAHash& Hash, TArray<uint8>& Code);
extern bool FinalizeLibrary_Metal(class FName const& Format, class FString const& ArchivePath, class FString const& LibraryPath, TSet<uint64> const& Shaders, class FString const& DebugOutputDir);

static FName NAME_SF_METAL(TEXT("SF_METAL"));
static FName NAME_SF_METAL_MRT(TEXT("SF_METAL_MRT"));
static FName NAME_SF_METAL_SM5_NOTESS(TEXT("SF_METAL_SM5_NOTESS"));
static FName NAME_SF_METAL_SM5(TEXT("SF_METAL_SM5"));
static FName NAME_SF_METAL_MACES3_1(TEXT("SF_METAL_MACES3_1"));
static FName NAME_SF_METAL_MACES2(TEXT("SF_METAL_MACES2"));
static FName NAME_SF_METAL_MRT_MAC(TEXT("SF_METAL_MRT_MAC"));
static FString METAL_LIB_EXTENSION(TEXT(".metallib"));
static FString METAL_MAP_EXTENSION(TEXT(".metalmap"));

class FMetalShaderFormatArchive : public IShaderFormatArchive
{
public:
	FMetalShaderFormatArchive(FString const& InLibraryName, FName InFormat, FString const& WorkingDirectory)
	: LibraryName(InLibraryName)
	, Format(InFormat)
	, WorkingDir(WorkingDirectory)
	{
		check(LibraryName.Len() > 0);
		check(Format == NAME_SF_METAL || Format == NAME_SF_METAL_MRT || Format == NAME_SF_METAL_SM5_NOTESS || Format == NAME_SF_METAL_SM5 || Format == NAME_SF_METAL_MACES3_1 || Format == NAME_SF_METAL_MACES2 || Format == NAME_SF_METAL_MRT_MAC);
		ArchivePath = (WorkingDir / Format.GetPlainNameString());
		IFileManager::Get().DeleteDirectory(*ArchivePath, false, true);
		IFileManager::Get().MakeDirectory(*ArchivePath);
		Map.Format = Format.GetPlainNameString();
	}
	
	virtual FName GetFormat( void ) const
	{
		return Format;
	}
	
	virtual bool AddShader( uint8 Frequency, const FSHAHash& Hash, TArray<uint8>& Code )
	{
		uint64 ShaderId = AppendShader_Metal(Format, ArchivePath, Hash, Code);
		if (ShaderId)
		{
			uint32 Index = 0;

			// Add Id to our list of shaders processed successfully
			uint32* IndexPtr = Shaders.Find(ShaderId);
			if (IndexPtr)
			{
				Index = *IndexPtr;
			}
			else
			{
				Index = (Shaders.Num() / 10000);
				Shaders.Add(ShaderId, Index);
				if (SubLibraries.Num() <= (int32)Index)
				{
					SubLibraries.Add(TSet<uint64>());
				}
				SubLibraries[Index].Add(ShaderId);
			}
			
			// Note code copy in the map is uncompressed
			Map.HashMap.Add(Hash, FMetalShadeEntry(Code, Index, Frequency));
		}
		return (ShaderId > 0);
	}
	
	virtual bool Finalize( FString OutputDir, FString DebugOutputDir, TArray<FString>* OutputFiles )
	{
		bool bOK = false;

		FString LibraryPlatformName = FString::Printf(TEXT("%s_%s"), *LibraryName, *Format.GetPlainNameString());
		volatile int32 CompiledLibraries = 0;
		TArray<FGraphEventRef> Tasks;

		for (uint32 Index = 0; Index < (uint32)SubLibraries.Num(); Index++)
		{
			TSet<uint64>& PartialShaders = SubLibraries[Index];

			FString LibraryPath = (OutputDir / LibraryPlatformName) + FString::Printf(TEXT(".%d"), Index) + METAL_LIB_EXTENSION;
			if (OutputFiles)
			{
				OutputFiles->Add(LibraryPath);
			}

			// Enqueue the library compilation as a task so we can go wide
			FGraphEventRef CompletionFence = FFunctionGraphTask::CreateAndDispatchWhenReady([this, LibraryPath, PartialShaders, DebugOutputDir, &CompiledLibraries]()
			{
				if (FinalizeLibrary_Metal(Format, ArchivePath, LibraryPath, PartialShaders, DebugOutputDir))
				{
					FPlatformAtomics::InterlockedIncrement(&CompiledLibraries);
				}
			}, TStatId(), NULL, ENamedThreads::AnyThread);

			Tasks.Add(CompletionFence);
		}

		// Wait for tasks
		for (auto& Task : Tasks)
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);
		}
				
		if (CompiledLibraries == SubLibraries.Num())
		{
			FString BinaryShaderFile = (OutputDir / LibraryPlatformName) + METAL_MAP_EXTENSION;
			FArchive* BinaryShaderAr = IFileManager::Get().CreateFileWriter(*BinaryShaderFile);
			if( BinaryShaderAr != NULL )
			{
				Map.Count = SubLibraries.Num();
				*BinaryShaderAr << Map;
				BinaryShaderAr->Flush();
				delete BinaryShaderAr;
				
				if (OutputFiles)
				{
					OutputFiles->Add(BinaryShaderFile);
				}
				
				bOK = true;
			}

#if PLATFORM_MAC
			if(bOK)
			{
				//TODO add a check in here - this will only work if we have shader archiving with debug info set.
				
				//We want to archive all the metal shader source files so that they can be unarchived into a debug location
				//This allows the debugging of optimised metal shaders within the xcode tool set
				//Currently using the 'tar' system tool to create a compressed tape archive
				
				//Place the archive in the same position as the .metallib file
				FString CompressedPath = (OutputDir / LibraryPlatformName) + TEXT(".tgz");
				
				FString ArchiveCommand = TEXT("/usr/bin/tar");
				
				// Iterative support for pre-stripped shaders - unpack existing tgz archive without file overwrite - if it exists in cooked dir we're in iterative mode
				if(FPaths::FileExists(CompressedPath))
				{
					int32 ReturnCode = -1;
					FString Result;
					FString Errors;
					
					FString ExtractCommandParams = FString::Printf(TEXT("xopfk \"%s\" -C \"%s\""), *CompressedPath, *DebugOutputDir);
					FPlatformProcess::ExecProcess( *ArchiveCommand, *ExtractCommandParams, &ReturnCode, &Result, &Errors );
				}
				
				//Due to the limitations of the 'tar' command and running through NSTask,
				//the most reliable way is to feed it a list of local file name (-T) with a working path set (-C)
				//if we built the list with absolute paths without -C then we'd get the full folder structure in the archive
				//I don't think we want this here
				
				//Build a file list that 'tar' can access
				const FString FileListPath = DebugOutputDir / TEXT("ArchiveInput.txt");
				IFileManager::Get().Delete( *FileListPath );
				
				{
					//Find the metal source files
					TArray<FString> FilesToArchive;
					IFileManager::Get().FindFilesRecursive( FilesToArchive, *DebugOutputDir, TEXT("*.metal"), true, false, false );
					
					//Write the local file names into the target file
					FArchive* FileListHandle = IFileManager::Get().CreateFileWriter( *FileListPath );
					if(FileListHandle)
					{
						const FString NewLine = TEXT("\n");
						
						const FString DebugDir = DebugOutputDir / *Format.GetPlainNameString();
						
						for(FString FileName : FilesToArchive)
						{
							FPaths::MakePathRelativeTo(FileName, *DebugDir);
							
							FString TextLine = FileName + NewLine;
							
							//We don't want the string to archive through the << operator otherwise we'd be creating a binary file - we need text
							auto AnsiFullPath = StringCast<ANSICHAR>( *TextLine );
							FileListHandle->Serialize( (ANSICHAR*)AnsiFullPath.Get(), AnsiFullPath.Length() );
						}
						
						//Clean up
						FileListHandle->Close();
						delete FileListHandle;
					}
				}
				
				int32 ReturnCode = -1;
				FString Result;
				FString Errors;
				
				//Setup the NSTask command and parameter list, Archive (-c) and Compress (-z) to target file (-f) the metal file list (-T) using a local dir in archive (-C).
				FString ArchiveCommandParams = FString::Printf( TEXT("czf \"%s\" -C \"%s\" -T \"%s\""), *CompressedPath, *DebugOutputDir, *FileListPath );
				
				//Execute command, this should end up with a .tgz file in the same location at the .metallib file
				if(!FPlatformProcess::ExecProcess( *ArchiveCommand, *ArchiveCommandParams, &ReturnCode, &Result, &Errors ) || ReturnCode != 0)
				{
					UE_LOG(LogShaders, Error, TEXT("Archive Shader Source failed %d: %s"), ReturnCode, *Errors);
				}
			}
#endif
		}

		return bOK;
	}
	
public:
	virtual ~FMetalShaderFormatArchive() { }
	
private:
	FString LibraryName;
	FName Format;
	FString WorkingDir;
	FString ArchivePath;
	TMap<uint64, uint32> Shaders;
	TArray<TSet<uint64>> SubLibraries;
	TSet<FString> SourceFiles;
	FMetalShaderMap Map;
};

class FMetalShaderFormat : public IShaderFormat
{
public:
	enum
	{
		HEADER_VERSION = 58,
	};
	
	struct FVersion
	{
		uint16 XcodeVersion;
		uint16 HLSLCCMinor		: 8;
		uint16 Format			: 8;
	};
	
	virtual uint32 GetVersion(FName Format) const override final
	{
		return GetMetalFormatVersion(Format);
	}
	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override final
	{
		OutFormats.Add(NAME_SF_METAL);
		OutFormats.Add(NAME_SF_METAL_MRT);
		OutFormats.Add(NAME_SF_METAL_SM5_NOTESS);
		OutFormats.Add(NAME_SF_METAL_SM5);
		OutFormats.Add(NAME_SF_METAL_MACES3_1);
		OutFormats.Add(NAME_SF_METAL_MACES2);
		OutFormats.Add(NAME_SF_METAL_MRT_MAC);
	}
	virtual void CompileShader(FName Format, const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output,const FString& WorkingDirectory) const override final
	{
		check(Format == NAME_SF_METAL || Format == NAME_SF_METAL_MRT || Format == NAME_SF_METAL_SM5_NOTESS || Format == NAME_SF_METAL_SM5 || Format == NAME_SF_METAL_MACES3_1 || Format == NAME_SF_METAL_MACES2 || Format == NAME_SF_METAL_MRT_MAC);
		CompileShader_Metal(Input, Output, WorkingDirectory);
	}
	virtual bool CanStripShaderCode(bool const bNativeFormat) const override final
	{
		return CanCompileBinaryShaders() && bNativeFormat;
	}
	virtual bool StripShaderCode( TArray<uint8>& Code, FString const& DebugOutputDir, bool const bNative ) const override final
	{
		return StripShader_Metal(Code, DebugOutputDir, bNative);
    }
	virtual bool SupportsShaderArchives() const override 
	{ 
		return CanCompileBinaryShaders();
	}
    virtual class IShaderFormatArchive* CreateShaderArchive( FString const& LibraryName, FName Format, const FString& WorkingDirectory ) const override final
    {
		return new FMetalShaderFormatArchive(LibraryName, Format, WorkingDirectory);
    }
	virtual bool CanCompileBinaryShaders() const override final
	{
#if PLATFORM_MAC
		return FPlatformMisc::IsSupportedXcodeVersionInstalled();
#else
		return IsRemoteBuildingConfigured();
#endif
	}
	virtual const TCHAR* GetPlatformIncludeDirectory() const
	{
		return TEXT("Metal");
	}
};

uint32 GetMetalFormatVersion(FName Format)
{
	static_assert(sizeof(FMetalShaderFormat::FVersion) == sizeof(uint32), "Out of bits!");
	union
	{
		FMetalShaderFormat::FVersion Version;
		uint32 Raw;
	} Version;
	
	// Include the Xcode version when the .ini settings instruct us to do so.
	uint16 AppVersion = 0;
	bool bAddXcodeVersionInShaderVersion = false;
	if(Format == NAME_SF_METAL || Format == NAME_SF_METAL_MRT)
	{
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("XcodeVersionInShaderVersion"), bAddXcodeVersionInShaderVersion, GEngineIni);
	}
	else
	{
		GConfig->GetBool(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("XcodeVersionInShaderVersion"), bAddXcodeVersionInShaderVersion, GEngineIni);
	}

	// We want to include the Xcode App and build version to avoid
	// weird mismatches where some shaders are built with one version
	// of the metal frontend and others with a different version.
	uint64 BuildVersion = 0;
	
	// GetXcodeVersion returns:
	// Major  << 8 | Minor << 4 | Patch
	AppVersion = GetXcodeVersion(BuildVersion);
	
	if (!FApp::IsEngineInstalled() && bAddXcodeVersionInShaderVersion)
	{
		// For local development we'll mix in the xcode version
		// and build version.
		AppVersion ^= (BuildVersion & 0xff);
		AppVersion ^= ((BuildVersion >> 16) & 0xff);
		AppVersion ^= ((BuildVersion >> 32) & 0xff);
		AppVersion ^= ((BuildVersion >> 48) & 0xff);
	}
	else
	{
		// In the other case (ie, shipping editor binary distributions)
		// We will only mix in the Major version of Xcode used to create
		// the shader binaries.
		AppVersion = (AppVersion >> 8) & 0xff;
	}

	Version.Version.XcodeVersion = AppVersion;
	Version.Version.Format = FMetalShaderFormat::HEADER_VERSION;
	Version.Version.HLSLCCMinor = HLSLCC_VersionMinor;
	
	// Check that we didn't overwrite any bits
	check(Version.Version.XcodeVersion == AppVersion);
	check(Version.Version.Format == FMetalShaderFormat::HEADER_VERSION);
	check(Version.Version.HLSLCCMinor == HLSLCC_VersionMinor);
	
	return Version.Raw;
}

/**
 * Module for OpenGL shaders
 */

static IShaderFormat* Singleton = NULL;

class FMetalShaderFormatModule : public IShaderFormatModule
{
public:
	virtual ~FMetalShaderFormatModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual IShaderFormat* GetShaderFormat()
	{
		if (!Singleton)
		{
			Singleton = new FMetalShaderFormat();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FMetalShaderFormatModule, MetalShaderFormat);
