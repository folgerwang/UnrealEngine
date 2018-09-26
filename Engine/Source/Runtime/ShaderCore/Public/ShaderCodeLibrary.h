// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCodeLibrary.h: 
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

DECLARE_LOG_CATEGORY_EXTERN(LogShaderLibrary, Log, All);

class FShaderPipeline;

struct SHADERCORE_API FShaderCodeLibraryPipeline
{
	FSHAHash VertexShader;
	FSHAHash PixelShader;
	FSHAHash GeometryShader;
	FSHAHash HullShader;
	FSHAHash DomainShader;
	mutable uint32 Hash;
	
	FShaderCodeLibraryPipeline() : Hash(0) {}
	
	friend bool operator ==(const FShaderCodeLibraryPipeline& A,const FShaderCodeLibraryPipeline& B)
	{
		return A.VertexShader == B.VertexShader && A.PixelShader == B.PixelShader && A.GeometryShader == B.GeometryShader && A.HullShader == B.HullShader && A.DomainShader == B.DomainShader;
	}
	
	friend uint32 GetTypeHash(const FShaderCodeLibraryPipeline &Key)
	{
		if(!Key.Hash)
		{
			Key.Hash = FCrc::MemCrc32(Key.VertexShader.Hash, sizeof(Key.VertexShader.Hash));
			Key.Hash = FCrc::MemCrc32(Key.PixelShader.Hash, sizeof(Key.PixelShader.Hash), Key.Hash);
			Key.Hash = FCrc::MemCrc32(Key.GeometryShader.Hash, sizeof(Key.GeometryShader.Hash), Key.Hash);
			Key.Hash = FCrc::MemCrc32(Key.HullShader.Hash, sizeof(Key.HullShader.Hash), Key.Hash);
			Key.Hash = FCrc::MemCrc32(Key.DomainShader.Hash, sizeof(Key.DomainShader.Hash), Key.Hash);
		}
		return Key.Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderCodeLibraryPipeline& Info )
	{
		return Ar << Info.VertexShader << Info.PixelShader << Info.GeometryShader << Info.HullShader << Info.DomainShader << Info.Hash;
	}
};

struct SHADERCORE_API FCompactFullName
{
	TArray<FName> ObjectClassAndPath;

	bool operator==(const FCompactFullName& Other) const
	{
		return ObjectClassAndPath == Other.ObjectClassAndPath;
	}

	FString ToString() const;
	void ParseFromString(const FString& Src);
	friend SHADERCORE_API uint32 GetTypeHash(const FCompactFullName& A);
};


struct SHADERCORE_API FStableShaderKeyAndValue
{
	FCompactFullName ClassNameAndObjectPath;
	FName ShaderType;
	FName ShaderClass;
	FName MaterialDomain;
	FName FeatureLevel;
	FName QualityLevel;
	FName TargetFrequency;
	FName TargetPlatform;
	FName VFType;
	FName PermutationId;

	uint32 KeyHash;

	FSHAHash OutputHash;

	FStableShaderKeyAndValue()
		: KeyHash(0)
	{
	}

	void ComputeKeyHash();
	void ParseFromString(const FString& Src);
	FString ToString() const;
	static FString HeaderLine();

	friend bool operator ==(const FStableShaderKeyAndValue& A, const FStableShaderKeyAndValue& B)
	{
		return
			A.ClassNameAndObjectPath == B.ClassNameAndObjectPath &&
			A.ShaderType == B.ShaderType &&
			A.ShaderClass == B.ShaderClass &&
			A.MaterialDomain == B.MaterialDomain &&
			A.FeatureLevel == B.FeatureLevel &&
			A.QualityLevel == B.QualityLevel &&
			A.TargetFrequency == B.TargetFrequency &&
			A.TargetPlatform == B.TargetPlatform &&
			A.VFType == B.VFType &&
			A.PermutationId == B.PermutationId;
	}

	friend uint32 GetTypeHash(const FStableShaderKeyAndValue &Key)
	{
		return Key.KeyHash;
	}

};

class FShaderFactoryInterface : public FRHIShaderLibrary
{
public:
	FShaderFactoryInterface(EShaderPlatform InPlatform, FString const& Name) : FRHIShaderLibrary(InPlatform, Name) {}
	
	virtual bool IsNativeLibrary() const override final {return false;}
	
	virtual FPixelShaderRHIRef CreatePixelShader(const FSHAHash& Hash) = 0; 
	virtual FVertexShaderRHIRef CreateVertexShader(const FSHAHash& Hash) = 0;
	virtual FHullShaderRHIRef CreateHullShader(const FSHAHash& Hash) = 0;
	virtual FDomainShaderRHIRef CreateDomainShader(const FSHAHash& Hash) = 0;
	virtual FGeometryShaderRHIRef CreateGeometryShader(const FSHAHash& Hash) = 0;
	virtual FGeometryShaderRHIRef CreateGeometryShaderWithStreamOutput(const FSHAHash& Hash, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream) = 0;
	virtual FComputeShaderRHIRef CreateComputeShader(const FSHAHash& Hash) = 0;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FSharedShaderCodeRequest, const FSHAHash&, FArchive*);
DECLARE_MULTICAST_DELEGATE_OneParam(FSharedShaderCodeRelease, const FSHAHash&);

// Collection of unique shader code
// Populated at cook time
struct SHADERCORE_API FShaderCodeLibrary
{
	static void InitForRuntime(EShaderPlatform ShaderPlatform);
	static void Shutdown();
	
	static bool IsEnabled();
	
	// Open a named library.
	// For cooking this will place all added shaders & pipelines into the library file with this name.
	// At runtime this will open the shader library with this name.
	static void OpenLibrary(FString const& Name, FString const& Directory);
    
	// Close a named library.
	// For cooking, after this point any AddShaderCode/AddShaderPipeline calls will be invalid until OpenLibrary is called again.
	// At runtime this will release the library data and further requests for shaders from this library will fail.
	static void CloseLibrary(FString const& Name);
	
	/** Instantiate or retrieve a vertex shader from the cache for the provided code & hash. */
	static FVertexShaderRHIRef CreateVertexShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a pixel shader from the cache for the provided code & hash. */
	static FPixelShaderRHIRef CreatePixelShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a geometry shader from the cache for the provided code & hash. */
	static FGeometryShaderRHIRef CreateGeometryShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a geometry shader from the cache for the provided code & hash. */
	static FGeometryShaderRHIRef CreateGeometryShaderWithStreamOutput(EShaderPlatform Platform, FSHAHash Hash, const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream);
	/** Instantiate or retrieve a hull shader from the cache for the provided code & hash. */
	static FHullShaderRHIRef CreateHullShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a domain shader from the cache for the provided code & hash. */
	static FDomainShaderRHIRef CreateDomainShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a compute shader from the cache for the provided code & hash. */
	static FComputeShaderRHIRef CreateComputeShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);

    static bool ContainsShaderCode(const FSHAHash& Hash);
    
	// Place a request to preload shader code
	// Blocking call if no Archive is provided or Archive is not a type of FLinkerLoad
	// Shader code preload will be finished before owning UObject PostLoad call
	static bool RequestShaderCode(const FSHAHash& Hash, FArchive* Ar);

	// Note that we skipped preloading shader code.
	// All this does is call the delegate so that other folks are aware that it was lazy
	static bool LazyRequestShaderCode(const FSHAHash& Hash, FArchive* Ar);

	// Get the raw payload synchronously
	// This does NOT require a ReleaseShaderCode; because it is synchronous
	// This also does not fire any delegates...which are used to load binary programs (we are calling this because we failed to find a binary program)
	static bool RequestShaderCode(const FSHAHash& Hash, TArray<uint8>& OutRaw);

	// Request to release shader code
	// Must match RequestShaderCode call
	// Invalid to call before owning UObject PostLoad call
	static void ReleaseShaderCode(const FSHAHash& Hash);

	// Request to release shader code that we lazy loaded
	// Must match LazyRequestShaderCode call
	// All this does is call the delegate so that other folks are aware that it was lazy
	static void LazyReleaseShaderCode(const FSHAHash& Hash);

	// Create an iterator over all the shaders in the library
	static TRefCountPtr<FRHIShaderLibrary::FShaderLibraryIterator> CreateIterator(void);
	
	// Total number of shader entries in the library
	static uint32 GetShaderCount(void);
	
	// The shader platform that the library manages - at runtime this will only be one
	static EShaderPlatform GetRuntimeShaderPlatform(void);
	
	// Get the shader pipelines in the library - only ever valid for OpenGL which can link without full PSO state
	static TSet<FShaderCodeLibraryPipeline> const* GetShaderPipelines(EShaderPlatform Platform);

#if WITH_EDITOR
	// Initialize the library cooker
	static void InitForCooking(bool bNativeFormat);
	
	// Clean the cook directories
	static void CleanDirectories(TArray<FName> const& ShaderFormats);
    
    // Specify the shader formats to cook
    static void CookShaderFormats(TArray<FName> const& ShaderFormats);
	
	// At cook time, add shader code to collection
	static bool AddShaderCode(EShaderPlatform ShaderPlatform, EShaderFrequency Frequency, const FSHAHash& Hash, const TArray<uint8>& InCode, uint32 const UncompressedSize);

	// We check this early in the callstack to avoid creating a bunch of FName and keys and things we will never save anyway
	static bool NeedsShaderStableKeys();

	// At cook time, add the human readable key value information
	static void AddShaderStableKeyValue(EShaderPlatform ShaderPlatform, FStableShaderKeyAndValue& StableKeyValue);

	// At cook time, add shader pipeline to collection
	static bool AddShaderPipeline(FShaderPipeline* Pipeline);
	
	// Save collected shader code to a file for each specified shader platform, collating all child cooker results.
	static bool SaveShaderCodeMaster(const FString& OutputDir, const FString& MetaOutputDir, const TArray<FName>& ShaderFormats, TArray<FString>& OutSCLCSVPath);
	
	// Save collected shader code to a file for each specified shader platform, handles only this instances intermediate results.
	static bool SaveShaderCodeChild(const FString& OutputDir, const FString& MetaOutputDir, const TArray<FName>& ShaderFormats);
	
	// Package the separate shader bytecode files into a single native shader library. Must be called by the master process.
	static bool PackageNativeShaderLibrary(const FString& ShaderCodeDir, const TArray<FName>& ShaderFormats);
	
	// Dump collected stats for each shader platform
	static void DumpShaderCodeStats();
#endif
	
	// Safely assign the hash to a shader object
	static void SafeAssignHash(FRHIShader* InShader, const FSHAHash& Hash);

	// Delegate called whenever shader code is requested.
	static FDelegateHandle RegisterSharedShaderCodeRequestDelegate_Handle(const FSharedShaderCodeRequest::FDelegate& Delegate);
	static void UnregisterSharedShaderCodeRequestDelegate_Handle(FDelegateHandle Handle);

	// Delegate called whenever shader code is released.
	static FDelegateHandle RegisterSharedShaderCodeReleaseDelegate_Handle(const FSharedShaderCodeRelease::FDelegate& Delegate);
	static void UnregisterSharedShaderCodeReleaseDelegate_Handle(FDelegateHandle Handle);
};
