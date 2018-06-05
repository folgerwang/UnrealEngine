// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalShaders.cpp: Metal shader RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "MetalShaderResources.h"
#include "MetalResources.h"
#include "ShaderCache.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "Serialization/MemoryReader.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/Compression.h"
#include "Misc/MessageDialog.h"

// The Metal standard library extensions we need for UE4.
#include "ue4_stdlib.h"

#define SHADERCOMPILERCOMMON_API
#	include "Developer/ShaderCompilerCommon/Public/ShaderCompilerCommon.h"
#undef SHADERCOMPILERCOMMON_API

/** Set to 1 to enable shader debugging (makes the driver save the shader source) */
#define DEBUG_METAL_SHADERS (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)

static FString METAL_LIB_EXTENSION(TEXT(".metallib"));
static FString METAL_MAP_EXTENSION(TEXT(".metalmap"));

struct FMetalCompiledShaderKey
{
	FMetalCompiledShaderKey(
		uint32 InCodeSize,
		uint32 InCodeCRC,
		uint32 InConstants,
		uint32 InBufferTypes
		)
		: CodeSize(InCodeSize)
		, CodeCRC(InCodeCRC)
		, Constants(InConstants)
		, BufferTypes(InBufferTypes)
	{}

	friend bool operator ==(const FMetalCompiledShaderKey& A, const FMetalCompiledShaderKey& B)
	{
		return A.CodeSize == B.CodeSize && A.CodeCRC == B.CodeCRC && A.Constants == B.Constants && A.BufferTypes == B.BufferTypes;
	}

	friend uint32 GetTypeHash(const FMetalCompiledShaderKey &Key)
	{
		return HashCombine(HashCombine(HashCombine(GetTypeHash(Key.CodeSize), GetTypeHash(Key.CodeCRC)), GetTypeHash(Key.Constants)), GetTypeHash(Key.BufferTypes));
	}

	uint32 CodeSize;
	uint32 CodeCRC;
	uint32 Constants;
	uint32 BufferTypes;
};

struct FMetalCompiledShaderCache
{
public:
	FMetalCompiledShaderCache()
	{
	}
	
	~FMetalCompiledShaderCache()
	{
	}
	
	mtlpp::Function FindRef(FMetalCompiledShaderKey const& Key)
	{
		FRWScopeLock ScopedLock(Lock, SLT_ReadOnly);
		mtlpp::Function Func = Cache.FindRef(Key);
		return Func;
	}
	
	mtlpp::Library FindLibrary(mtlpp::Function const& Function)
	{
		FRWScopeLock ScopedLock(Lock, SLT_ReadOnly);
		mtlpp::Library Lib = LibCache.FindRef(Function.GetPtr());
		return Lib;
	}
	
	void Add(FMetalCompiledShaderKey Key, mtlpp::Library const& Lib, mtlpp::Function const& Function)
	{
		FRWScopeLock ScopedLock(Lock, SLT_Write);
		if (Cache.FindRef(Key) == nil)
		{
			Cache.Add(Key, Function);
			LibCache.Add(Function.GetPtr(), Lib);
		}
	}
	
private:
	FRWLock Lock;
	TMap<FMetalCompiledShaderKey, mtlpp::Function> Cache;
	TMap<mtlpp::Function::Type, mtlpp::Library> LibCache;
};

static FMetalCompiledShaderCache& GetMetalCompiledShaderCache()
{
	static FMetalCompiledShaderCache CompiledShaderCache;
	return CompiledShaderCache;
}

NSString* DecodeMetalSourceCode(uint32 CodeSize, TArray<uint8> const& CompressedSource)
{
	NSString* GlslCodeNSString = nil;
	if (CodeSize && CompressedSource.Num())
	{
		TArray<ANSICHAR> UncompressedCode;
		UncompressedCode.AddZeroed(CodeSize+1);
		bool bSucceed = FCompression::UncompressMemory(ECompressionFlags::COMPRESS_ZLIB, UncompressedCode.GetData(), CodeSize, CompressedSource.GetData(), CompressedSource.Num());
		if (bSucceed)
		{
			GlslCodeNSString = [[NSString stringWithUTF8String:UncompressedCode.GetData()] retain];
		}
	}
	return GlslCodeNSString;
}

static mtlpp::LanguageVersion ValidateVersion(uint8 Version)
{
	static uint32 MetalMacOSVersions[][3] = {
		{10,11,6},
		{10,11,6},
		{10,12,6},
		{10,13,0},
	};
	static uint32 MetaliOSVersions[][3] = {
		{8,0,0},
		{9,0,0},
		{10,0,0},
		{11,0,0},
	};
	static TCHAR const* StandardNames[] =
	{
		TEXT("Metal 1.0"),
		TEXT("Metal 1.1"),
		TEXT("Metal 1.2"),
		TEXT("Metal 2.0"),
	};
	
	Version = FMath::Min(Version, (uint8)3);
	
	mtlpp::LanguageVersion Result = mtlpp::LanguageVersion::Version1_1;
	if (Version < 3)
	{
#if PLATFORM_MAC
		Result = Version == 0 ? mtlpp::LanguageVersion::Version1_1 : (mtlpp::LanguageVersion)((1 << 16) + FMath::Min(Version, (uint8)2u));
#else
		Result = (mtlpp::LanguageVersion)((1 << 16) + FMath::Min(Version, (uint8)2u));
#endif
	}
	else if (Version == 3)
	{
		Result = (mtlpp::LanguageVersion)(2 << 16);
	}
	
	if (!FApplePlatformMisc::IsOSAtLeastVersion(MetalMacOSVersions[Version], MetaliOSVersions[Version], MetaliOSVersions[Version]))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ShaderVersion"), FText::FromString(FString(StandardNames[Version])));
#if PLATFORM_MAC
		Args.Add(TEXT("RequiredOS"), FText::FromString(FString::Printf(TEXT("macOS %d.%d.%d"), MetalMacOSVersions[Version][0], MetalMacOSVersions[Version][1], MetalMacOSVersions[Version][2])));
#else
		Args.Add(TEXT("RequiredOS"), FText::FromString(FString::Printf(TEXT("macOS %d.%d.%d"), MetaliOSVersions[Version][0], MetaliOSVersions[Version][1], MetaliOSVersions[Version][2])));
#endif
		FText LocalizedMsg = FText::Format(NSLOCTEXT("MetalRHI", "ShaderVersionUnsupported", "The current OS version does not support {Version} required by the project. You must upgrade to {RequiredOS} to run this project."),Args);
		
		FText Title = NSLOCTEXT("MetalRHI", "ShaderVersionUnsupportedTitle", "Shader Version Unsupported");
		FMessageDialog::Open(EAppMsgType::Ok, LocalizedMsg, &Title);
		
		FPlatformMisc::RequestExit(true);
	}
	
	return Result;
}

/** Initialization constructor. */
template<typename BaseResourceType, int32 ShaderType>
void TMetalBaseShader<BaseResourceType, ShaderType>::Init(const TArray<uint8>& InShaderCode, FMetalCodeHeader& Header, mtlpp::Library InLibrary)
{
	FShaderCodeReader ShaderCode(InShaderCode);

	FMemoryReader Ar(InShaderCode, true);
	
	Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());

	// was the shader already compiled offline?
	uint8 OfflineCompiledFlag;
	Ar << OfflineCompiledFlag;
	check(OfflineCompiledFlag == 0 || OfflineCompiledFlag == 1);

	// get the header
	Header = { 0 };
	Ar << Header;
	
	ValidateVersion(Header.Version);
	
	// Validate that the compiler flags match the offline compiled flag - somehow they sometimes don't..
	UE_CLOG((Header.CompileFlags & (1 << CFLAG_Debug)) != 0 && (OfflineCompiledFlag == 0), LogMetal, Warning, TEXT("Metal shader was meant to be compiled as bytecode but stored as text: Header: 0x%x, Offline: 0x%x"), Header.CompileFlags, OfflineCompiledFlag);
	
    SourceLen = Header.SourceLen;
    SourceCRC = Header.SourceCRC;
	
	// If this triggers than a level above us has failed to provide valid shader data and the cook is probably bogus
	UE_CLOG(Header.SourceLen == 0 || Header.SourceCRC == 0, LogMetal, Fatal, TEXT("Invalid Shader Bytecode provided."));
	
    bTessFunctionConstants = Header.bTessFunctionConstants;
    bDeviceFunctionConstants = Header.bDeviceFunctionConstants;

	// remember where the header ended and code (precompiled or source) begins
	int32 CodeOffset = Ar.Tell();
	uint32 BufferSize = ShaderCode.GetActualShaderCodeSize() - CodeOffset;
	const ANSICHAR* SourceCode = (ANSICHAR*)InShaderCode.GetData() + CodeOffset;

	// Only archived shaders should be in here.
	UE_CLOG(InLibrary && !(Header.CompileFlags & (1 << CFLAG_Archive)), LogMetal, Warning, TEXT("Shader being loaded wasn't marked for archiving but a MTLLibrary was provided - this is unsupported."));

	if (!OfflineCompiledFlag)
	{
		UE_LOG(LogMetal, Display, TEXT("Loaded a text shader (will be slower to load)"));
	}
	
	bool bOfflineCompile = (OfflineCompiledFlag > 0);
	
	const ANSICHAR* ShaderSource = ShaderCode.FindOptionalData('c');
	bool bHasShaderSource = (ShaderSource && FCStringAnsi::Strlen(ShaderSource) > 0);
    
    static bool bForceTextShaders = FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUTrace);
    if (!bHasShaderSource)
    {
        int32 LZMASourceSize = 0;
        int32 SourceSize = 0;
        const uint8* LZMASource = ShaderCode.FindOptionalDataAndSize('z', LZMASourceSize);
        const uint8* UnSourceLen = ShaderCode.FindOptionalDataAndSize('u', SourceSize);
        if (LZMASource && LZMASourceSize > 0 && UnSourceLen && SourceSize == sizeof(uint32))
        {
            CompressedSource.Append(LZMASource, LZMASourceSize);
            memcpy(&CodeSize, UnSourceLen, sizeof(uint32));
			bHasShaderSource = false;
        }
        if (bForceTextShaders)
        {
            bHasShaderSource = (GetSourceCode() != nil);
        }
    }
    else if (bOfflineCompile && bHasShaderSource)
	{
		GlslCodeNSString = [NSString stringWithUTF8String:ShaderSource];
		check(GlslCodeNSString);
		[GlslCodeNSString retain];
	}
	
	bHasFunctionConstants = (Header.bTessFunctionConstants || Header.bDeviceFunctionConstants || Header.Bindings.TypedBuffers);

	ConstantValueHash = 0;
	for (uint8 Constant : Header.Bindings.TypedBufferFormats)
	{
		BufferTypeHash ^= Constant;
	}
	
	Library = InLibrary;
	
	bool bNeedsCompiling = false;
	uint32 const Count = Header.bTessFunctionConstants ? EMetalIndexType_Num : 1;
	for (uint32 i = 0; i < Count; i++)
	{
		// Find the existing compiled shader in the cache.
		uint32 FunctionConstantHash = i ^ ConstantValueHash;
		FMetalCompiledShaderKey Key(Header.SourceLen, Header.SourceCRC, FunctionConstantHash, 0);
		
		Function[i][EMetalBufferType_Dynamic] = GetMetalCompiledShaderCache().FindRef(Key);
		if (!Library && Function[i][EMetalBufferType_Dynamic])
		{
			Library = GetMetalCompiledShaderCache().FindLibrary(Function[i][EMetalBufferType_Dynamic]);
		}
		else
		{
			bNeedsCompiling = true;
		}
	}
	
    Bindings = Header.Bindings;
	if (bNeedsCompiling || !Library)
	{
		if (bOfflineCompile && bHasShaderSource)
		{
			// For debug/dev/test builds we can use the stored code for debugging - but shipping builds shouldn't have this as it is inappropriate.
	#if METAL_DEBUG_OPTIONS
			// For iOS/tvOS we must use runtime compilation to make the shaders debuggable, but
			bool bSavedSource = false;
			
	#if PLATFORM_MAC
			const ANSICHAR* ShaderPath = ShaderCode.FindOptionalData('p');
			bool const bHasShaderPath = (ShaderPath && FCStringAnsi::Strlen(ShaderPath) > 0);
			
			// on Mac if we have a path for the shader we can access the shader code
			if (bHasShaderPath && !bForceTextShaders && (GetSourceCode() != nil))
			{
				FString ShaderPathString(ShaderPath);
				
				if (IFileManager::Get().MakeDirectory(*FPaths::GetPath(ShaderPathString), true))
				{
					FString Source(GetSourceCode());
					bSavedSource = FFileHelper::SaveStringToFile(Source, *ShaderPathString);
				}
				
				static bool bAttemptedAuth = false;
				if (!bSavedSource && !bAttemptedAuth)
				{
					bAttemptedAuth = true;
					
					if (IFileManager::Get().MakeDirectory(*FPaths::GetPath(ShaderPathString), true))
					{
						bSavedSource = FFileHelper::SaveStringToFile(FString(GlslCodeNSString), *ShaderPathString);
					}
					
					if (!bSavedSource)
					{
						FPlatformMisc::MessageBoxExt(EAppMsgType::Ok,
													 *NSLOCTEXT("MetalRHI", "ShaderDebugAuthFail", "Could not access directory required for debugging optimised Metal shaders. Falling back to slower runtime compilation of shaders for debugging.").ToString(), TEXT("Error"));
					}
				}
			}
	#endif
			// Switch the compile mode so we get debuggable shaders even if we failed to save - if we didn't want
			// shader debugging we wouldn't have included the code...
			bOfflineCompile = bSavedSource || (bOfflineCompile && !bForceTextShaders);
	#endif
		}
		
		if (bOfflineCompile METAL_DEBUG_OPTION(&& !(bHasShaderSource && bForceTextShaders)))
		{
			if (InLibrary)
			{
				Library = InLibrary;
			}
			else
			{
				METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewLibraryBinary: %d_%d"), SourceLen, SourceCRC)));
				
				// Archived shaders should never get in here.
				check(!(Header.CompileFlags & (1 << CFLAG_Archive)) || BufferSize > 0);
				
				// allow GCD to copy the data into its own buffer
				//		dispatch_data_t GCDBuffer = dispatch_data_create(InShaderCode.GetTypedData() + CodeOffset, ShaderCode.GetActualShaderCodeSize() - CodeOffset, nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
				ns::AutoReleasedError AError;
				void* Buffer = FMemory::Malloc( BufferSize );
				FMemory::Memcpy( Buffer, InShaderCode.GetData() + CodeOffset, BufferSize );
				dispatch_data_t GCDBuffer = dispatch_data_create(Buffer, BufferSize, dispatch_get_main_queue(), ^(void) { FMemory::Free(Buffer); } );
				
				// load up the already compiled shader
				Library = GetMetalDeviceContext().GetDevice().NewLibrary(GCDBuffer, &AError);
				dispatch_release(GCDBuffer);
				
                if (Library == nil)
                {
					NSLog(@"Failed to create library: %@", ns::Error(AError).GetPtr());
                }
			}
		}
		else
		{
			METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewLibrarySource: %d_%d"), SourceLen, SourceCRC)));
			NSString* ShaderString = ((OfflineCompiledFlag == 0) ? [NSString stringWithUTF8String:SourceCode] : GlslCodeNSString);
			
			if(Header.ShaderName.Len())
			{
				ShaderString = [NSString stringWithFormat:@"// %@\n%@", Header.ShaderName.GetNSString(), ShaderString];
			}
			
			static NSString* UE4StdLibString = [[NSString alloc] initWithBytes:ue4_stdlib_metal length:ue4_stdlib_metal_len encoding:NSUTF8StringEncoding];
			
			NSString* NewShaderString = [ShaderString stringByReplacingOccurrencesOfString:@"#include \"ue4_stdlib.metal\"" withString:UE4StdLibString];
			NewShaderString = [NewShaderString stringByReplacingOccurrencesOfString:@"#pragma once" withString:@""];
			
			mtlpp::CompileOptions CompileOptions;
			
#if DEBUG_METAL_SHADERS
			static bool bForceFastMath = FParse::Param(FCommandLine::Get(), TEXT("metalfastmath"));
			static bool bForceNoFastMath = FParse::Param(FCommandLine::Get(), TEXT("metalnofastmath"));
			if (bForceNoFastMath)
			{
				CompileOptions.SetFastMathEnabled(NO);
			}
			else if (bForceFastMath)
			{
				CompileOptions.SetFastMathEnabled(YES);
			}
			else
#endif
			{
				CompileOptions.SetFastMathEnabled((BOOL)(!(Header.CompileFlags & (1 << CFLAG_NoFastMath))));
			}
			
#if !PLATFORM_MAC || DEBUG_METAL_SHADERS
			NSMutableDictionary *PreprocessorMacros = [NSMutableDictionary new];
#if !PLATFORM_MAC // Pretty sure that as_type-casts work on macOS, but they don't for half2<->uint on older versions of the iOS runtime compiler.
			[PreprocessorMacros addEntriesFromDictionary: @{ @"METAL_RUNTIME_COMPILER" : @(1)}];
#endif
#if DEBUG_METAL_SHADERS
			[PreprocessorMacros addEntriesFromDictionary: @{ @"MTLSL_ENABLE_DEBUG_INFO" : @(1)}];
#endif
			CompileOptions.SetPreprocessorMacros(PreprocessorMacros);
#endif
			if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesShaderVersions))
			{
				if (Header.Version < 3)
				{
	#if PLATFORM_MAC
					CompileOptions.SetLanguageVersion(Header.Version == 0 ? mtlpp::LanguageVersion::Version1_1 : (mtlpp::LanguageVersion)((1 << 16) + FMath::Min(Header.Version, (uint8)2u)));
	#else
					CompileOptions.SetLanguageVersion((mtlpp::LanguageVersion)((1 << 16) + FMath::Min(Header.Version, (uint8)2u)));
	#endif
				}
				else if (Header.Version == 3)
				{
					CompileOptions.SetLanguageVersion((mtlpp::LanguageVersion)(2 << 16));
				}
			}
			
			ns::AutoReleasedError Error;
			Library = GetMetalDeviceContext().GetDevice().NewLibrary(NewShaderString, CompileOptions, &Error);
			if (Library == nil)
			{
				UE_LOG(LogRHI, Error, TEXT("*********** Error\n%s"), *FString(NewShaderString));
				UE_LOG(LogRHI, Fatal, TEXT("Failed to create shader: %s"), *FString([Error.GetPtr() description]));
			}
			else if (Error != nil)
			{
				// Warning...
				UE_LOG(LogRHI, Warning, TEXT("*********** Warning\n%s"), *FString(NewShaderString));
				UE_LOG(LogRHI, Warning, TEXT("Created shader with warnings: %s"), *FString([Error.GetPtr() description]));
			}
			
			GlslCodeNSString = NewShaderString;
			[GlslCodeNSString retain];
		}
		
		// Make sure that the current device can actually run with function constants otherwise bad things will happen!
		UE_CLOG(bHasFunctionConstants && !FMetalCommandQueue::SupportsFeature(EMetalFeaturesFunctionConstants), LogMetal, Error, TEXT("Metal shader has function constants but current OS/device does not support them."));
		UE_CLOG(bHasFunctionConstants && !FMetalCommandQueue::SupportsFeature(EMetalFeaturesFunctionConstants), LogMetal, Fatal, TEXT("%s"), *FString(GetSourceCode()));
		
        for (uint32 i = 0; i < Count; i++)
        {
            GetCompiledFunction((EMetalIndexType)i, nullptr, 0, true);
        }
	}
	UniformBuffersCopyInfo = Header.UniformBuffersCopyInfo;
	SideTableBinding = Header.SideTable;
}

/** Destructor */
template<typename BaseResourceType, int32 ShaderType>
TMetalBaseShader<BaseResourceType, ShaderType>::~TMetalBaseShader()
{
	[GlslCodeNSString release];
}

template<typename BaseResourceType, int32 ShaderType>
uint32 TMetalBaseShader<BaseResourceType, ShaderType>::GetBufferBindingHash(EPixelFormat const* const BufferTypes) const
{
	check(BufferTypes);
	uint32 VHash = 0;
	uint BoundBuffers = Bindings.TypedBuffers;
    while(BoundBuffers)
    {
        uint32 Index = __builtin_ctz(BoundBuffers);
        BoundBuffers &= ~(1 << Index);
        
        if (Index < ML_MaxBuffers)
        {
        	VHash ^= GMetalBufferFormats[BufferTypes[Index]].DataFormat;
        }
    }
    
    return VHash;
}

template<typename BaseResourceType, int32 ShaderType>
mtlpp::Function TMetalBaseShader<BaseResourceType, ShaderType>::GetCompiledFunction(EMetalIndexType IndexType, EPixelFormat const* const BufferTypes, uint32 InBufferTypeHash, bool const bAsync)
{
	EMetalBufferType BT = (InBufferTypeHash == BufferTypeHash && BufferTypeHash) ? EMetalBufferType_Static : EMetalBufferType_Dynamic;
	
    mtlpp::Function Func = Function[IndexType][BT];
    
	if (!Func)
	{
		// Find the existing compiled shader in the cache.
		uint32 FunctionConstantHash = IndexType ^ ConstantValueHash;
		FMetalCompiledShaderKey Key(SourceLen, SourceCRC, FunctionConstantHash, (InBufferTypeHash == BufferTypeHash) ? BufferTypeHash : 0);
		Func = Function[IndexType][BT] = GetMetalCompiledShaderCache().FindRef(Key);
		
		if (!Func)
		{
			// Get the function from the library - the function name is "Main" followed by the CRC32 of the source MTLSL as 0-padded hex.
			// This ensures that even if we move to a unified library that the function names will be unique - duplicates will only have one entry in the library.
			NSString* Name = [NSString stringWithFormat:@"Main_%0.8x_%0.8x", SourceLen, SourceCRC];
			mtlpp::FunctionConstantValues ConstantValues(nil);
            if (bHasFunctionConstants)
            {
                ConstantValues = mtlpp::FunctionConstantValues();
				
				uint32 BoundBuffers = Bindings.TypedBuffers;
				while(BoundBuffers)
				{
					uint32 Index = __builtin_ctz(BoundBuffers);
					BoundBuffers &= ~(1 << Index);
					
					if (Index < ML_MaxBuffers)
					{
						if(BT == EMetalBufferType_Static)
						{
							// It all matches what is in the shader, so bind the MAX value to force it to elide all the switch-case stuff and just load as directly as possible
							uint32 v = EMetalBufferFormat::Max;
							ConstantValues.SetConstantValues(&v, mtlpp::DataType::UInt, ns::Range(Index, 1));
						}
						else
						{
							// It doesn't match and we don't know what it will be, so load the type dynamically from the buffer meta-table - adds a lot of instructions and hurts performance
							uint32 v = EMetalBufferFormat::Unknown;
							ConstantValues.SetConstantValues(&v, mtlpp::DataType::UInt, ns::Range(Index, 1));
						}
					}
				}
				
                if(bTessFunctionConstants)
                {
                    // Index 32 is the tessellation index-buffer presence constant
					ConstantValues.SetConstantValues(&IndexType, mtlpp::DataType::UInt, ns::Range(32, 1));
                }
                if (bDeviceFunctionConstants)
                {
                    // Index 33 is the device vendor id constant
					ConstantValues.SetConstantValue(&GRHIVendorId, mtlpp::DataType::UInt, @"GMetalDeviceManufacturer");
                }
            }
            
            if (!bHasFunctionConstants || !bAsync)
            {
				METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewFunction: %s"), *FString(Name))));
                if (!bHasFunctionConstants)
                {
                    Function[IndexType][BT] = Library.NewFunction(Name);
                }
                else
                {
					ns::AutoReleasedError AError;
					Function[IndexType][BT] = Library.NewFunction(Name, ConstantValues, &AError);
					ns::Error Error = AError;
					UE_CLOG(Function[IndexType][BT] == nil, LogMetal, Error, TEXT("Failed to create function: %s"), *FString(Error.GetPtr().description));
                    UE_CLOG(Function[IndexType][BT] == nil, LogMetal, Fatal, TEXT("*********** Error\n%s"), *FString(GetSourceCode()));
                }
                
                check(Function[IndexType][BT]);
                GetMetalCompiledShaderCache().Add(Key, Library, Function[IndexType][BT]);
                
                Func = Function[IndexType][BT];
            }
            else
            {
				METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewFunctionAsync: %s"), *FString(Name))));
				METAL_GPUPROFILE(uint64 CPUStart = CPUStat.Stats ? CPUStat.Stats->CPUStartTime : 0);
#if ENABLE_METAL_GPUPROFILE
                ns::String nsName(Name);
				Library.NewFunction(Name, ConstantValues, [Key, this, CPUStart, nsName](mtlpp::Function const& NewFunction, ns::Error const& Error){
#else
				Library.NewFunction(Name, ConstantValues, [Key, this](mtlpp::Function const& NewFunction, ns::Error const& Error){
#endif
					METAL_GPUPROFILE(FScopedMetalCPUStats CompletionStat(FString::Printf(TEXT("NewFunctionCompletion: %s"), *FString(nsName.GetPtr()))));
					UE_CLOG(NewFunction == nil, LogMetal, Error, TEXT("Failed to create function: %s"), *FString(Error.GetPtr().description));
					UE_CLOG(NewFunction == nil, LogMetal, Fatal, TEXT("*********** Error\n%s"), *FString(GetSourceCode()));
					
					GetMetalCompiledShaderCache().Add(Key, Library, NewFunction);
#if ENABLE_METAL_GPUPROFILE
					if (CompletionStat.Stats)
					{
						CompletionStat.Stats->CPUStartTime = CPUStart;
					}
#endif
				});

                return nil;
            }
		}
	}	
	
    check(Func);
	return Func;
}

FMetalComputeShader::FMetalComputeShader(const TArray<uint8>& InCode, mtlpp::Library InLibrary)
: NumThreadsX(0)
, NumThreadsY(0)
, NumThreadsZ(0)
{
    Pipeline[0] = Pipeline[1] = nil;
	FMetalCodeHeader Header = {0};
	Init(InCode, Header, InLibrary);
	
	NumThreadsX = FMath::Max((int32)Header.NumThreadsX, 1);
	NumThreadsY = FMath::Max((int32)Header.NumThreadsY, 1);
	NumThreadsZ = FMath::Max((int32)Header.NumThreadsZ, 1);
}

FMetalComputeShader::~FMetalComputeShader()
{
    for (uint32 i = 0; i < EMetalBufferType_Num; i++)
    {
        [Pipeline[i] release];
        Pipeline[i] = nil;
    }
}
	
uint32 FMetalComputeShader::GetBindingHash(EPixelFormat const* const BufferTypes) const
{
	if (BufferTypes)
	{
		return GetBufferBindingHash(BufferTypes);
	}
	return 0;
}

FMetalShaderPipeline* FMetalComputeShader::GetPipeline(EPixelFormat const* const BufferTypes, uint32 InBufferTypeHash)
{
	EMetalBufferType BT = (InBufferTypeHash == BufferTypeHash && BufferTypeHash) ? EMetalBufferType_Static : EMetalBufferType_Dynamic;
	
	if (!Pipeline[BT])
	{
		mtlpp::Function Func = GetCompiledFunction(EMetalIndexType_None, BufferTypes, (InBufferTypeHash == BufferTypeHash) ? BufferTypeHash : 0);
		check(Func);
        
		ns::Error Error;
		mtlpp::ComputePipelineState Kernel;
        mtlpp::ComputePipelineReflection Reflection;
		
		METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewComputePipeline: %d_%d"), SourceLen, SourceCRC)));
    #if METAL_DEBUG_OPTIONS
        if (GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation METAL_STATISTIC(|| GetMetalDeviceContext().GetCommandQueue().GetStatistics()))
        {
			ns::AutoReleasedError ComputeError;
            mtlpp::AutoReleasedComputePipelineReflection ComputeReflection;
			
			NSUInteger ComputeOption = mtlpp::PipelineOption::ArgumentInfo|mtlpp::PipelineOption::BufferTypeInfo METAL_STATISTIC(|NSUInteger(EMTLPipelineStats));
			Kernel = GetMetalDeviceContext().GetDevice().NewComputePipelineState(Func, mtlpp::PipelineOption(ComputeOption), ComputeReflection, &ComputeError);
			Error = ComputeError;
			Reflection = ComputeReflection;
        }
        else
    #endif
        {
			ns::AutoReleasedError ComputeError;
			Kernel = GetMetalDeviceContext().GetDevice().NewComputePipelineState(Func, &ComputeError);
			Error = ComputeError;
        }
        
        if (Kernel == nil)
        {
            UE_LOG(LogRHI, Error, TEXT("*********** Error\n%s"), *FString(GetSourceCode()));
            UE_LOG(LogRHI, Fatal, TEXT("Failed to create compute kernel: %s"), *FString([Error description]));
        }
        
        Pipeline[BT] = [FMetalShaderPipeline new];
        Pipeline[BT]->ComputePipelineState = Kernel;
#if METAL_DEBUG_OPTIONS
        Pipeline[BT]->ComputePipelineReflection = Reflection;
        Pipeline[BT]->ComputeSource = GetSourceCode();
		if (Reflection)
		{
			Pipeline[BT]->ComputeDesc = mtlpp::ComputePipelineDescriptor();
			Pipeline[BT]->ComputeDesc.SetLabel(Func.GetName());
			Pipeline[BT]->ComputeDesc.SetComputeFunction(Func);
		}
#endif
        METAL_DEBUG_OPTION(FMemory::Memzero(Pipeline[BT]->ResourceMask, sizeof(Pipeline[BT]->ResourceMask)));
	}
	check(Pipeline[BT]);

	return Pipeline[BT];
}

FMetalVertexShader::FMetalVertexShader(const TArray<uint8>& InCode)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header);
	
	TessellationOutputAttribs = Header.TessellationOutputAttribs;
	TessellationPatchCountBuffer = Header.TessellationPatchCountBuffer;
	TessellationIndexBuffer = Header.TessellationIndexBuffer;
	TessellationHSOutBuffer = Header.TessellationHSOutBuffer;
	TessellationHSTFOutBuffer = Header.TessellationHSTFOutBuffer;
	TessellationControlPointOutBuffer = Header.TessellationControlPointOutBuffer;
	TessellationControlPointIndexBuffer = Header.TessellationControlPointIndexBuffer;
	TessellationOutputControlPoints = Header.TessellationOutputControlPoints;
	TessellationDomain = Header.TessellationDomain;
	TessellationInputControlPoints = Header.TessellationInputControlPoints;
	TessellationMaxTessFactor = Header.TessellationMaxTessFactor;
	TessellationPatchesPerThreadGroup = Header.TessellationPatchesPerThreadGroup;
}

FMetalVertexShader::FMetalVertexShader(const TArray<uint8>& InCode, mtlpp::Library InLibrary)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header, InLibrary);
	
	TessellationOutputAttribs = Header.TessellationOutputAttribs;
	TessellationPatchCountBuffer = Header.TessellationPatchCountBuffer;
	TessellationIndexBuffer = Header.TessellationIndexBuffer;
	TessellationHSOutBuffer = Header.TessellationHSOutBuffer;
	TessellationHSTFOutBuffer = Header.TessellationHSTFOutBuffer;
	TessellationControlPointOutBuffer = Header.TessellationControlPointOutBuffer;
	TessellationControlPointIndexBuffer = Header.TessellationControlPointIndexBuffer;
	TessellationOutputControlPoints = Header.TessellationOutputControlPoints;
	TessellationDomain = Header.TessellationDomain;
	TessellationInputControlPoints = Header.TessellationInputControlPoints;
	TessellationMaxTessFactor = Header.TessellationMaxTessFactor;
	TessellationPatchesPerThreadGroup = Header.TessellationPatchesPerThreadGroup;
}
	
uint32 FMetalVertexShader::GetBindingHash(EPixelFormat const* const BufferTypes) const
{
	if (BufferTypes)
	{
		return GetBufferBindingHash(BufferTypes);
	}
	return 0;
}

mtlpp::Function FMetalVertexShader::GetFunction(EMetalIndexType IndexType, EPixelFormat const* const BufferTypes, uint32 BufferTypeHash)
{
	return GetCompiledFunction(IndexType, BufferTypes, BufferTypeHash);
}

FMetalPixelShader::FMetalPixelShader(const TArray<uint8>& InCode)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header);
}

FMetalPixelShader::FMetalPixelShader(const TArray<uint8>& InCode, mtlpp::Library InLibrary)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header, InLibrary);
}
	
uint32 FMetalPixelShader::GetBindingHash(EPixelFormat const* const BufferTypes) const
{
	if (BufferTypes)
	{
		return GetBufferBindingHash(BufferTypes);
	}
	return 0;
}

mtlpp::Function FMetalPixelShader::GetFunction(EMetalIndexType IndexType, EPixelFormat const* const BufferTypes, uint32 BufferTypeHash)
{
	return GetCompiledFunction(IndexType, BufferTypes, BufferTypeHash);
}

FMetalHullShader::FMetalHullShader(const TArray<uint8>& InCode)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header);
}

FMetalHullShader::FMetalHullShader(const TArray<uint8>& InCode, mtlpp::Library InLibrary)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header, InLibrary);
}
	
uint32 FMetalHullShader::GetBindingHash(EPixelFormat const* const BufferTypes) const
{
	if (BufferTypes)
	{
		return GetBufferBindingHash(BufferTypes);
	}
	return 0;
}

mtlpp::Function FMetalHullShader::GetFunction(EMetalIndexType IndexType, EPixelFormat const* const BufferTypes, uint32 BufferTypeHash)
{
	return GetCompiledFunction(IndexType, BufferTypes, BufferTypeHash);
}

FMetalDomainShader::FMetalDomainShader(const TArray<uint8>& InCode)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header);
	
	// for VSHS
	TessellationHSOutBuffer = Header.TessellationHSOutBuffer;
	TessellationControlPointOutBuffer = Header.TessellationControlPointOutBuffer;
	
	switch (Header.TessellationOutputWinding)
	{
		// NOTE: cw and ccw are flipped
		case EMetalOutputWindingMode::Clockwise:		TessellationOutputWinding = mtlpp::Winding::CounterClockwise; break;
		case EMetalOutputWindingMode::CounterClockwise:	TessellationOutputWinding = mtlpp::Winding::Clockwise; break;
		default: check(0);
	}
	
	switch (Header.TessellationPartitioning)
	{
		case EMetalPartitionMode::Pow2:				TessellationPartitioning = mtlpp::TessellationPartitionMode::ModePow2; break;
		case EMetalPartitionMode::Integer:			TessellationPartitioning = mtlpp::TessellationPartitionMode::ModeInteger; break;
		case EMetalPartitionMode::FractionalOdd:	TessellationPartitioning = mtlpp::TessellationPartitionMode::ModeFractionalOdd; break;
		case EMetalPartitionMode::FractionalEven:	TessellationPartitioning = mtlpp::TessellationPartitionMode::ModeFractionalEven; break;
		default: check(0);
	}
}

FMetalDomainShader::FMetalDomainShader(const TArray<uint8>& InCode, mtlpp::Library InLibrary)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header, InLibrary);
	
	// for VSHS
	TessellationHSOutBuffer = Header.TessellationHSOutBuffer;
	TessellationControlPointOutBuffer = Header.TessellationControlPointOutBuffer;
	
	switch (Header.TessellationOutputWinding)
	{
		// NOTE: cw and ccw are flipped
		case EMetalOutputWindingMode::Clockwise:		TessellationOutputWinding = mtlpp::Winding::CounterClockwise; break;
		case EMetalOutputWindingMode::CounterClockwise:	TessellationOutputWinding = mtlpp::Winding::Clockwise; break;
		default: check(0);
	}
	
	switch (Header.TessellationPartitioning)
	{
		case EMetalPartitionMode::Pow2:				TessellationPartitioning = mtlpp::TessellationPartitionMode::ModePow2; break;
		case EMetalPartitionMode::Integer:			TessellationPartitioning = mtlpp::TessellationPartitionMode::ModeInteger; break;
		case EMetalPartitionMode::FractionalOdd:	TessellationPartitioning = mtlpp::TessellationPartitionMode::ModeFractionalOdd; break;
		case EMetalPartitionMode::FractionalEven:	TessellationPartitioning = mtlpp::TessellationPartitionMode::ModeFractionalEven; break;
		default: check(0);
	}
}
	
uint32 FMetalDomainShader::GetBindingHash(EPixelFormat const* const BufferTypes) const
{
	if (BufferTypes)
	{
		return GetBufferBindingHash(BufferTypes);
	}
	return 0;
}

mtlpp::Function FMetalDomainShader::GetFunction(EMetalIndexType IndexType, EPixelFormat const* const BufferTypes, uint32 BufferTypeHash)
{
	return GetCompiledFunction(IndexType, BufferTypes, BufferTypeHash);
}

FVertexShaderRHIRef FMetalDynamicRHI::RHICreateVertexShader(const TArray<uint8>& Code)
{
    @autoreleasepool {
	FMetalVertexShader* Shader = new FMetalVertexShader(Code);
	return Shader;
	}
}

FVertexShaderRHIRef FMetalDynamicRHI::RHICreateVertexShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	@autoreleasepool {

	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);
	
	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	return MetalLibrary->CreateVertexShader(Hash);
	
	}
}

FPixelShaderRHIRef FMetalDynamicRHI::RHICreatePixelShader(const TArray<uint8>& Code)
{
	@autoreleasepool {
	FMetalPixelShader* Shader = new FMetalPixelShader(Code);
	return Shader;
	}
}

FPixelShaderRHIRef FMetalDynamicRHI::RHICreatePixelShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	@autoreleasepool {
	
	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);
	
	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	return MetalLibrary->CreatePixelShader(Hash);
	
	}
}

FHullShaderRHIRef FMetalDynamicRHI::RHICreateHullShader(const TArray<uint8>& Code) 
{
	@autoreleasepool {
	FMetalHullShader* Shader = new FMetalHullShader(Code);
	return Shader;
	}
}

FHullShaderRHIRef FMetalDynamicRHI::RHICreateHullShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	@autoreleasepool {
	
	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);
	
	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	return MetalLibrary->CreateHullShader(Hash);
	
	}
}

FDomainShaderRHIRef FMetalDynamicRHI::RHICreateDomainShader(const TArray<uint8>& Code) 
{
	@autoreleasepool {
	FMetalDomainShader* Shader = new FMetalDomainShader(Code);
	return Shader;
	}
}

FDomainShaderRHIRef FMetalDynamicRHI::RHICreateDomainShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	@autoreleasepool {
	
	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);
	
	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	return MetalLibrary->CreateDomainShader(Hash);
	
	}
}

FGeometryShaderRHIRef FMetalDynamicRHI::RHICreateGeometryShader(const TArray<uint8>& Code) 
{
	@autoreleasepool {
	FMetalGeometryShader* Shader = new FMetalGeometryShader;
	FMetalCodeHeader Header = {0};
	Shader->Init(Code, Header);
	return Shader;
	}
}

FGeometryShaderRHIRef FMetalDynamicRHI::RHICreateGeometryShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	@autoreleasepool {
	
	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);

	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	return MetalLibrary->CreateGeometryShader(Hash);
	
	}
}

FGeometryShaderRHIRef FMetalDynamicRHI::RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList,
	uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	checkf(0, TEXT("Not supported yet"));
	return NULL;
}

FGeometryShaderRHIRef FMetalDynamicRHI::RHICreateGeometryShaderWithStreamOutput(const FStreamOutElementList& ElementList,
	uint32 NumStrides, const uint32* Strides, int32 RasterizedStream, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	@autoreleasepool {
	
	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);
	
	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	return MetalLibrary->CreateGeometryShaderWithStreamOutput(Hash, ElementList, NumStrides, Strides, RasterizedStream);
	
	}
}

FComputeShaderRHIRef FMetalDynamicRHI::RHICreateComputeShader(const TArray<uint8>& Code) 
{
	@autoreleasepool {
	FMetalComputeShader* Shader = new FMetalComputeShader(Code);
	
	// @todo WARNING: We have to hash here because of the way we immediately link and don't afford the cache a chance to set the OutputHash from ShaderCore.
	if (FShaderCache::GetShaderCache())
	{
		FSHAHash Hash;
		FSHA1::HashBuffer(Code.GetData(), Code.Num(), Hash.Hash);
		Shader->SetHash(Hash);
	}
	
	return Shader;
	}
}

FComputeShaderRHIRef FMetalDynamicRHI::RHICreateComputeShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash) 
{ 
	@autoreleasepool {
	
	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);

	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	FComputeShaderRHIRef Shader = MetalLibrary->CreateComputeShader(Hash);
	
	if(Shader.IsValid() && FShaderCache::GetShaderCache())
	{
		// @todo WARNING: We have to hash here because of the way we immediately link and don't afford the cache a chance to set the OutputHash from ShaderCore.
		Shader->SetHash(Hash);
	}
	
	return Shader;
	
	}
}

FVertexShaderRHIRef FMetalDynamicRHI::CreateVertexShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code)
{
	return RHICreateVertexShader(Code);
}
FVertexShaderRHIRef FMetalDynamicRHI::CreateVertexShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return RHICreateVertexShader(Library, Hash);
}
FPixelShaderRHIRef FMetalDynamicRHI::CreatePixelShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code)
{
	return RHICreatePixelShader(Code);
}
FPixelShaderRHIRef FMetalDynamicRHI::CreatePixelShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return RHICreatePixelShader(Library, Hash);
}
FGeometryShaderRHIRef FMetalDynamicRHI::CreateGeometryShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code)
{
	return RHICreateGeometryShader(Code);
}
FGeometryShaderRHIRef FMetalDynamicRHI::CreateGeometryShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return RHICreateGeometryShader(Library, Hash);
}
FGeometryShaderRHIRef FMetalDynamicRHI::CreateGeometryShaderWithStreamOutput_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	return RHICreateGeometryShaderWithStreamOutput(Code, ElementList, NumStrides, Strides, RasterizedStream);
}
FGeometryShaderRHIRef FMetalDynamicRHI::CreateGeometryShaderWithStreamOutput_RenderThread(class FRHICommandListImmediate& RHICmdList, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return RHICreateGeometryShaderWithStreamOutput(ElementList, NumStrides, Strides, RasterizedStream, Library, Hash);
}
FComputeShaderRHIRef FMetalDynamicRHI::CreateComputeShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code)
{
	return RHICreateComputeShader(Code);
}
FComputeShaderRHIRef FMetalDynamicRHI::CreateComputeShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return RHICreateComputeShader(Library, Hash);
}
FHullShaderRHIRef FMetalDynamicRHI::CreateHullShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code)
{
	return RHICreateHullShader(Code);
}
FHullShaderRHIRef FMetalDynamicRHI::CreateHullShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return RHICreateHullShader(Library, Hash);
}
FDomainShaderRHIRef FMetalDynamicRHI::CreateDomainShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code)
{
	return RHICreateDomainShader(Code);
}
FDomainShaderRHIRef FMetalDynamicRHI::CreateDomainShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return RHICreateDomainShader(Library, Hash);
}

FMetalShaderLibrary::FMetalShaderLibrary(EShaderPlatform InPlatform, FString const& Name, mtlpp::Library InLibrary, FMetalShaderMap const& InMap)
: FRHIShaderLibrary(InPlatform, Name)
, Library(InLibrary)
, Map(InMap)
{
	
}

FMetalShaderLibrary::~FMetalShaderLibrary()
{
	
}

bool FMetalShaderLibrary::ContainsEntry(const FSHAHash& Hash)
{
	TPair<uint8, TArray<uint8>>* Code = Map.HashMap.Find(Hash);
	return (Code != nullptr);
}

bool FMetalShaderLibrary::RequestEntry(const FSHAHash& Hash, FArchive* Ar)
{
	TPair<uint8, TArray<uint8>>* Code = Map.HashMap.Find(Hash);
	return (Code != nullptr);
}

FPixelShaderRHIRef FMetalShaderLibrary::CreatePixelShader(const FSHAHash& Hash)
{
	TPair<uint8, TArray<uint8>>* Code = Map.HashMap.Find(Hash);
	if (Code)
	{
		FMetalPixelShader* Shader = new FMetalPixelShader(Code->Value, Library);
		if (Shader->GetFunction(EMetalIndexType_None, nullptr, 0))
		{
			return Shader;
		}
		else
		{
			delete Shader;
		}
	}
	
	UE_LOG(LogMetal, Error, TEXT("Failed to find Pixel Shader with SHA: %s"), *Hash.ToString());
	return FPixelShaderRHIRef();
}

FVertexShaderRHIRef FMetalShaderLibrary::CreateVertexShader(const FSHAHash& Hash)
{
	TPair<uint8, TArray<uint8>>* Code = Map.HashMap.Find(Hash);
	if (Code)
	{
		FMetalVertexShader* Shader = new FMetalVertexShader(Code->Value, Library);
		if (Shader->GetFunction(EMetalIndexType_None, nullptr, 0))
		{
			return Shader;
		}
		else
		{
			delete Shader;
		}
	}
	UE_LOG(LogMetal, Error, TEXT("Failed to find Vertex Shader with SHA: %s"), *Hash.ToString());
	return FVertexShaderRHIRef();
}

FHullShaderRHIRef FMetalShaderLibrary::CreateHullShader(const FSHAHash& Hash)
{
	TPair<uint8, TArray<uint8>>* Code = Map.HashMap.Find(Hash);
	if (Code)
	{
		FMetalHullShader* Shader = new FMetalHullShader(Code->Value, Library);
		if(Shader->GetFunction(EMetalIndexType_None, nullptr, 0))
		{
			return Shader;
		}
		else
		{
			delete Shader;
		}
	}
	UE_LOG(LogMetal, Error, TEXT("Failed to find Hull Shader with SHA: %s"), *Hash.ToString());
	return FHullShaderRHIRef();
}

FDomainShaderRHIRef FMetalShaderLibrary::CreateDomainShader(const FSHAHash& Hash)
{
	TPair<uint8, TArray<uint8>>* Code = Map.HashMap.Find(Hash);
	if (Code)
	{
		FMetalDomainShader* Shader = new FMetalDomainShader(Code->Value, Library);
		if (Shader->GetFunction(EMetalIndexType_None, nullptr, 0))
		{
			return Shader;
		}
		else
		{
			delete Shader;
		}
	}
	UE_LOG(LogMetal, Error, TEXT("Failed to find Domain Shader with SHA: %s"), *Hash.ToString());
	return FDomainShaderRHIRef();
}

FGeometryShaderRHIRef FMetalShaderLibrary::CreateGeometryShader(const FSHAHash& Hash)
{
	checkf(0, TEXT("Not supported yet"));
	return NULL;
}

FGeometryShaderRHIRef FMetalShaderLibrary::CreateGeometryShaderWithStreamOutput(const FSHAHash& Hash, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	checkf(0, TEXT("Not supported yet"));
	return NULL;
}

FComputeShaderRHIRef FMetalShaderLibrary::CreateComputeShader(const FSHAHash& Hash)
{
	TPair<uint8, TArray<uint8>>* Code = Map.HashMap.Find(Hash);
	if (Code)
	{
		FMetalComputeShader* Shader = new FMetalComputeShader(Code->Value, Library);
		if (Shader->GetPipeline(nullptr, 0))
		{
			return Shader;
		}
		else
		{
			delete Shader;
		}
	}
	UE_LOG(LogMetal, Error, TEXT("Failed to find Compute Shader with SHA: %s"), *Hash.ToString());
	return FComputeShaderRHIRef();
}

//
//Library Iterator
//
FRHIShaderLibrary::FShaderLibraryEntry FMetalShaderLibrary::FMetalShaderLibraryIterator::operator*() const
{
	FShaderLibraryEntry Entry;
	
	Entry.Hash = IteratorImpl->Key;
	Entry.Frequency = (EShaderFrequency)IteratorImpl->Value.Key;
	Entry.Platform = GetLibrary()->GetPlatform();
	
	return Entry;
}

FRHIShaderLibraryRef FMetalDynamicRHI::RHICreateShaderLibrary_RenderThread(class FRHICommandListImmediate& RHICmdList, EShaderPlatform Platform, FString FilePath, FString Name)
{
	return RHICreateShaderLibrary(Platform, FilePath, Name);
}

FRHIShaderLibraryRef FMetalDynamicRHI::RHICreateShaderLibrary(EShaderPlatform Platform, FString const& FilePath, FString const& Name)
{
	@autoreleasepool {
	FRHIShaderLibraryRef Result = nullptr;
	
	FName PlatformName = LegacyShaderPlatformToShaderFormat(Platform);
	FString LibName = FString::Printf(TEXT("%s_%s"), *Name, *PlatformName.GetPlainNameString());
	LibName.ToLowerInline();
	
	FMetalShaderMap Map;
	FString BinaryShaderFile = FilePath / LibName + METAL_MAP_EXTENSION;

	if ( IFileManager::Get().FileExists(*BinaryShaderFile) == false )
	{
		// the metal map files are stored in UFS file system
		// for pak files this means they might be stored in a different location as the pak files will mount them to the project content directory
		// the metal libraries are stores non UFS and could be anywhere on the file system.
		// if we don't find the metalmap file straight away try the pak file path
		BinaryShaderFile = FPaths::ProjectContentDir() / LibName + METAL_MAP_EXTENSION;
	}

	FArchive* BinaryShaderAr = IFileManager::Get().CreateFileReader(*BinaryShaderFile);

	if( BinaryShaderAr != NULL )
	{
		*BinaryShaderAr << Map;
		BinaryShaderAr->Flush();
		delete BinaryShaderAr;
		
		// Would be good to check the language version of the library with the archive format here.
		if (Map.Format == PlatformName.GetPlainNameString())
		{
			FString MetalLibraryFilePath = FilePath / LibName + METAL_LIB_EXTENSION;
			MetalLibraryFilePath = FPaths::ConvertRelativePathToFull(MetalLibraryFilePath);
#if !PLATFORM_MAC
			MetalLibraryFilePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*MetalLibraryFilePath);
#endif
			
			METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewLibraryFile: %s"), *MetalLibraryFilePath)));
			NSError* Error;
			mtlpp::Library Library = [GetMetalDeviceContext().GetDevice() newLibraryWithFile:MetalLibraryFilePath.GetNSString() error:&Error];
			if (Library != nil)
			{
				Result = new FMetalShaderLibrary(Platform, Name, Library, Map);
			}
			else
			{
				UE_LOG(LogMetal, Display, TEXT("Failed to create library: %s"), *FString(Error.description));
			}
		}
		else
		{
			UE_LOG(LogMetal, Display, TEXT("Wrong shader platform wanted: %s, got: %s"), *LibName, *Map.Format);
		}
	}
	else
	{
		UE_LOG(LogMetal, Display, TEXT("No .metalmap file found for %s!"), *LibName);
	}
	
	return Result;
	}
}

FBoundShaderStateRHIRef FMetalDynamicRHI::RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FHullShaderRHIParamRef HullShaderRHI, 
	FDomainShaderRHIParamRef DomainShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI,
	FGeometryShaderRHIParamRef GeometryShaderRHI
	)
{
	NOT_SUPPORTED("RHICreateBoundShaderState");
	return nullptr;
}

FMetalShaderParameterCache::FMetalShaderParameterCache()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniforms[ArrayIndex] = nullptr;
		PackedGlobalUniformsSizes[ArrayIndex] = 0;
		PackedGlobalUniformDirty[ArrayIndex].LowVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].HighVector = 0;
	}
}

void FMetalShaderParameterCache::ResizeGlobalUniforms(uint32 TypeIndex, uint32 UniformArraySize)
{
	PackedGlobalUniforms[TypeIndex] = (uint8*)FMemory::Realloc(PackedGlobalUniforms[TypeIndex], UniformArraySize);
	PackedGlobalUniformsSizes[TypeIndex] = UniformArraySize;
	PackedGlobalUniformDirty[TypeIndex].LowVector = 0;
	PackedGlobalUniformDirty[TypeIndex].HighVector = 0;
}

/** Destructor. */
FMetalShaderParameterCache::~FMetalShaderParameterCache()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		FMemory::Free(PackedGlobalUniforms[ArrayIndex]);
	}
}

/**
 * Invalidates all existing data.
 */
void FMetalShaderParameterCache::Reset()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].LowVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].HighVector = 0;
	}
}

const int SizeOfFloat4 = 4 * sizeof(float);

/**
 * Marks all uniform arrays as dirty.
 */
void FMetalShaderParameterCache::MarkAllDirty()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].LowVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].HighVector = PackedGlobalUniformsSizes[ArrayIndex] / SizeOfFloat4;
	}
}

/**
 * Set parameter values.
 */
void FMetalShaderParameterCache::Set(uint32 BufferIndexName, uint32 ByteOffset, uint32 NumBytes, const void* NewValues)
{
	uint32 BufferIndex = CrossCompiler::PackedTypeNameToTypeIndex(BufferIndexName);
	check(BufferIndex < CrossCompiler::PACKED_TYPEINDEX_MAX);
	check(PackedGlobalUniforms[BufferIndex]);
	check(ByteOffset + NumBytes <= PackedGlobalUniformsSizes[BufferIndex]);
	PackedGlobalUniformDirty[BufferIndex].LowVector = FMath::Min(PackedGlobalUniformDirty[BufferIndex].LowVector, ByteOffset / SizeOfFloat4);
	PackedGlobalUniformDirty[BufferIndex].HighVector = FMath::Max(PackedGlobalUniformDirty[BufferIndex].HighVector, (ByteOffset + NumBytes + SizeOfFloat4 - 1) / SizeOfFloat4);
	FMemory::Memcpy(PackedGlobalUniforms[BufferIndex] + ByteOffset, NewValues, NumBytes);
}

void FMetalShaderParameterCache::CommitPackedGlobals(FMetalStateCache* Cache, FMetalCommandEncoder* Encoder, EShaderFrequency Frequency, const FMetalShaderBindings& Bindings)
{
	// copy the current uniform buffer into the ring buffer to submit
	for (int32 Index = 0; Index < Bindings.PackedGlobalArrays.Num(); ++Index)
	{
		int32 UniformBufferIndex = Bindings.PackedGlobalArrays[Index].TypeIndex;
 
		// is there any data that needs to be copied?
		if (PackedGlobalUniformDirty[UniformBufferIndex].HighVector > 0)
		{
			uint32 TotalSize = Bindings.PackedGlobalArrays[Index].Size;
			uint32 SizeToUpload = PackedGlobalUniformDirty[UniformBufferIndex].HighVector * SizeOfFloat4;
			
			//@todo-rco: Temp workaround
			SizeToUpload = TotalSize;
			
			//@todo-rco: Temp workaround
			uint8 const* Bytes = PackedGlobalUniforms[UniformBufferIndex];
			uint32 Size = FMath::Min(TotalSize, SizeToUpload);
			
			FMetalBuffer Buffer = Encoder->GetRingBuffer().NewBuffer(Size, 0);
			
			FMemory::Memcpy((uint8*)Buffer.GetContents(), Bytes, Size);
			
			Cache->SetShaderBuffer(Frequency, Buffer, nil, 0, Size, UniformBufferIndex);

			// mark as clean
			PackedGlobalUniformDirty[UniformBufferIndex].HighVector = 0;
		}
	}
}

void FMetalShaderParameterCache::CommitPackedUniformBuffers(FMetalStateCache* Cache, TRefCountPtr<FMetalGraphicsPipelineState> BoundShaderState, FMetalComputeShader* ComputeShader, int32 Stage, const TRefCountPtr<FRHIUniformBuffer>* RHIUniformBuffers, const TArray<CrossCompiler::FUniformBufferCopyInfo>& UniformBuffersCopyInfo)
{
//	SCOPE_CYCLE_COUNTER(STAT_MetalConstantBufferUpdateTime);
	// Uniform Buffers are split into precision/type; the list of RHI UBs is traversed and if a new one was set, its
	// contents are copied per precision/type into corresponding scratch buffers which are then uploaded to the program
	if (Stage == CrossCompiler::SHADER_STAGE_PIXEL && !IsValidRef(BoundShaderState->PixelShader))
	{
		return;
	}

	auto& Bindings = [this, &Stage, &BoundShaderState, ComputeShader]() -> FMetalShaderBindings& {
		switch(Stage) {
			default: check(0);
			case CrossCompiler::SHADER_STAGE_VERTEX: return BoundShaderState->VertexShader->Bindings;
			case CrossCompiler::SHADER_STAGE_PIXEL: return BoundShaderState->PixelShader->Bindings;
			case CrossCompiler::SHADER_STAGE_COMPUTE: return ComputeShader->Bindings;
			case CrossCompiler::SHADER_STAGE_HULL: return BoundShaderState->HullShader->Bindings;
			case CrossCompiler::SHADER_STAGE_DOMAIN: return BoundShaderState->DomainShader->Bindings;
		}
	}();

	if (!Bindings.bHasRegularUniformBuffers && !FShaderCache::IsPredrawCall(Cache->GetShaderCacheStateObject()))
	{
		check(Bindings.NumUniformBuffers <= ML_MaxBuffers);
		int32 LastInfoIndex = 0;
		for (int32 BufferIndex = 0; BufferIndex < Bindings.NumUniformBuffers; ++BufferIndex)
		{
			const FRHIUniformBuffer* RHIUniformBuffer = RHIUniformBuffers[BufferIndex];
			check(RHIUniformBuffer);
			FMetalUniformBuffer* EmulatedUniformBuffer = (FMetalUniformBuffer*)RHIUniformBuffer;
			const uint32* RESTRICT SourceData = (uint32 const*)((uint8 const*)EmulatedUniformBuffer->GetData());
			for (int32 InfoIndex = LastInfoIndex; InfoIndex < UniformBuffersCopyInfo.Num(); ++InfoIndex)
			{
				const CrossCompiler::FUniformBufferCopyInfo& Info = UniformBuffersCopyInfo[InfoIndex];
				if (Info.SourceUBIndex == BufferIndex)
				{
					float* RESTRICT ScratchMem = (float*)PackedGlobalUniforms[Info.DestUBTypeIndex];
					ScratchMem += Info.DestOffsetInFloats;
					FMemory::Memcpy(ScratchMem, SourceData + Info.SourceOffsetInFloats, Info.SizeInFloats * sizeof(float));
					PackedGlobalUniformDirty[Info.DestUBTypeIndex].LowVector = FMath::Min(PackedGlobalUniformDirty[Info.DestUBTypeIndex].LowVector, uint32(Info.DestOffsetInFloats / SizeOfFloat4));
					PackedGlobalUniformDirty[Info.DestUBTypeIndex].HighVector = FMath::Max(PackedGlobalUniformDirty[Info.DestUBTypeIndex].HighVector, uint32(((Info.DestOffsetInFloats + Info.SizeInFloats) * sizeof(float) + SizeOfFloat4 - 1) / SizeOfFloat4));
				}
				else
				{
					LastInfoIndex = InfoIndex;
					break;
				}
			}
		}
	}
}
