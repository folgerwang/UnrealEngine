// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalResources.h: Metal resource RHI definitions..
=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"
#include "MetalShaderResources.h"
THIRD_PARTY_INCLUDES_START
#include "mtlpp.hpp"
THIRD_PARTY_INCLUDES_END

/** Parallel execution is available on Mac but not iOS for the moment - it needs to be tested because it isn't cost-free */
#define METAL_SUPPORTS_PARALLEL_RHI_EXECUTE 1

class FMetalContext;
@class FMetalShaderPipeline;

/** The MTLVertexDescriptor and a pre-calculated hash value used to simplify comparisons (as vendor MTLVertexDescriptor implementations aren't all comparable) */
struct FMetalHashedVertexDescriptor
{
	NSUInteger VertexDescHash;
	mtlpp::VertexDescriptor VertexDesc;
	
	FMetalHashedVertexDescriptor();
	FMetalHashedVertexDescriptor(mtlpp::VertexDescriptor Desc, uint32 Hash);
	FMetalHashedVertexDescriptor(FMetalHashedVertexDescriptor const& Other);
	~FMetalHashedVertexDescriptor();
	
	FMetalHashedVertexDescriptor& operator=(FMetalHashedVertexDescriptor const& Other);
	bool operator==(FMetalHashedVertexDescriptor const& Other) const;
	
	friend uint32 GetTypeHash(FMetalHashedVertexDescriptor const& Hash)
	{
		return Hash.VertexDescHash;
	}
};

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FMetalVertexDeclaration : public FRHIVertexDeclaration
{
public:

	/** Initialization constructor. */
	FMetalVertexDeclaration(const FVertexDeclarationElementList& InElements);
	~FMetalVertexDeclaration();
	
	/** Cached element info array (offset, stream index, etc) */
	FVertexDeclarationElementList Elements;

	/** This is the layout for the vertex elements */
	FMetalHashedVertexDescriptor Layout;
	
	/** Hash without considering strides which may be overriden */
	uint32 BaseHash;
	
	virtual bool GetInitializer(FVertexDeclarationElementList& Init)
	{
		Init = Elements;
		return true;
	}

protected:
	void GenerateLayout(const FVertexDeclarationElementList& Elements);

};

extern NSString* DecodeMetalSourceCode(uint32 CodeSize, TArray<uint8> const& CompressedSource);

enum EMetalIndexType
{
	EMetalIndexType_None   = 0,
	EMetalIndexType_UInt16 = 1,
	EMetalIndexType_UInt32 = 2,
	EMetalIndexType_Num	   = 3
};

enum EMetalBufferType
{
	EMetalBufferType_Dynamic = 0,
	EMetalBufferType_Static = 1,
	EMetalBufferType_Num = 2
};

/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
template<typename BaseResourceType, int32 ShaderType>
class TMetalBaseShader : public BaseResourceType, public IRefCountedObject
{
public:
	enum { StaticFrequency = ShaderType };

	/** Initialization constructor. */
	TMetalBaseShader()
	: SideTableBinding(-1)
	, SourceLen(0)
	, SourceCRC(0)
    , BufferTypeHash(0)
	, GlslCodeNSString(nil)
	, CodeSize(0)
	{
		Function[EMetalIndexType_None][EMetalBufferType_Dynamic] = Function[EMetalIndexType_UInt16][EMetalBufferType_Dynamic] = Function[EMetalIndexType_UInt32][EMetalBufferType_Dynamic] = nil;
		Function[EMetalIndexType_None][EMetalBufferType_Static] = Function[EMetalIndexType_UInt16][EMetalBufferType_Static] = Function[EMetalIndexType_UInt32][EMetalBufferType_Static] = nil;
	}
	
	void Init(const TArray<uint8>& InCode, FMetalCodeHeader& Header, mtlpp::Library InLibrary = nil);

	/** Destructor */
	virtual ~TMetalBaseShader();

	/** @returns The Metal source code as an NSString if available or nil if not. Will dynamically decompress from compressed data on first invocation. */
	inline NSString* GetSourceCode()
	{
		if (!GlslCodeNSString && CodeSize && CompressedSource.Num())
		{
			GlslCodeNSString = DecodeMetalSourceCode(CodeSize, CompressedSource);
		}
		if (!GlslCodeNSString)
		{
			GlslCodeNSString = [FString::Printf(TEXT("Hash: %s, Name: Main_%0.8x_%0.8x"), *BaseResourceType::GetHash().ToString(), SourceLen, SourceCRC).GetNSString() retain];
		}
		return GlslCodeNSString;
	}

	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}

	/** External bindings for this shader. */
	FMetalShaderBindings Bindings;

	// List of memory copies from RHIUniformBuffer to packed uniforms
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;
	
	/** The binding for the buffer side-table if present */
	int32 SideTableBinding;

	/** CRC & Len for name disambiguation */
	uint32 SourceLen;
	uint32 SourceCRC;
	
	/** Hash for the shader/material permutation constants */
	uint32 ConstantValueHash;

	/** Hash of the typed_buffer format types */
    uint32 BufferTypeHash;
    
protected:
	mtlpp::Function GetCompiledFunction(EMetalIndexType IndexType, EPixelFormat const* const BufferTypes, uint32 BufferTypeHash, bool const bAsync = false);
	uint32 GetBufferBindingHash(EPixelFormat const* const BufferTypes) const;

    // this is the compiler shader
	mtlpp::Function Function[EMetalIndexType_Num][EMetalBufferType_Num];
    
private:
	// This is the MTLLibrary for the shader so we can dynamically refine the MTLFunction
	mtlpp::Library Library;
	
	/** The debuggable text source */
	NSString* GlslCodeNSString;
	
	/** The compressed text source */
	TArray<uint8> CompressedSource;
	
	/** The uncompressed text source size */
	uint32 CodeSize;
    
    // Function constant states
    bool bHasFunctionConstants;
    bool bTessFunctionConstants;
    bool bDeviceFunctionConstants;
};

class FMetalVertexShader : public TMetalBaseShader<FRHIVertexShader, SF_Vertex>
{
public:
	FMetalVertexShader(const TArray<uint8>& InCode);
	FMetalVertexShader(const TArray<uint8>& InCode, mtlpp::Library InLibrary);
	
	uint32 GetBindingHash(EPixelFormat const* const BufferTypes) const;
	mtlpp::Function GetFunction(EMetalIndexType IndexType, EPixelFormat const* const BufferTypes, uint32 BufferTypeHash);
	
	// for VSHS
	FMetalTessellationOutputs TessellationOutputAttribs;
	float  TessellationMaxTessFactor;
	uint32 TessellationOutputControlPoints;
	uint32 TessellationDomain;
	uint32 TessellationInputControlPoints;
	uint32 TessellationPatchesPerThreadGroup;
	uint32 TessellationPatchCountBuffer;
	uint32 TessellationIndexBuffer;
	uint32 TessellationHSOutBuffer;
	uint32 TessellationHSTFOutBuffer;
	uint32 TessellationControlPointOutBuffer;
	uint32 TessellationControlPointIndexBuffer;
};

class FMetalPixelShader : public TMetalBaseShader<FRHIPixelShader, SF_Pixel>
{
public:
	FMetalPixelShader(const TArray<uint8>& InCode);
	FMetalPixelShader(const TArray<uint8>& InCode, mtlpp::Library InLibrary);
	
	uint32 GetBindingHash(EPixelFormat const* const BufferTypes) const;
	mtlpp::Function GetFunction(EMetalIndexType IndexType, EPixelFormat const* const BufferTypes, uint32 BufferTypeHash);
};

class FMetalHullShader : public TMetalBaseShader<FRHIHullShader, SF_Hull>
{
public:
	FMetalHullShader(const TArray<uint8>& InCode);
	FMetalHullShader(const TArray<uint8>& InCode, mtlpp::Library InLibrary);
	
	uint32 GetBindingHash(EPixelFormat const* const BufferTypes) const;
	mtlpp::Function GetFunction(EMetalIndexType IndexType, EPixelFormat const* const BufferTypes, uint32 BufferTypeHash);
};

class FMetalDomainShader : public TMetalBaseShader<FRHIDomainShader, SF_Domain>
{
public:
	FMetalDomainShader(const TArray<uint8>& InCode);
	FMetalDomainShader(const TArray<uint8>& InCode, mtlpp::Library InLibrary);
	
	uint32 GetBindingHash(EPixelFormat const* const BufferTypes) const;
	mtlpp::Function GetFunction(EMetalIndexType IndexType, EPixelFormat const* const BufferTypes, uint32 BufferTypeHash);
	
	mtlpp::Winding TessellationOutputWinding;
	mtlpp::TessellationPartitionMode TessellationPartitioning;
	uint32 TessellationHSOutBuffer;
	uint32 TessellationControlPointOutBuffer;
};

typedef TMetalBaseShader<FRHIGeometryShader, SF_Geometry> FMetalGeometryShader;

class FMetalComputeShader : public TMetalBaseShader<FRHIComputeShader, SF_Compute>
{
public:
	FMetalComputeShader(const TArray<uint8>& InCode, mtlpp::Library InLibrary = nil);
	virtual ~FMetalComputeShader();
	
	uint32 GetBindingHash(EPixelFormat const* const BufferTypes) const;
	FMetalShaderPipeline* GetPipeline(EPixelFormat const* const BufferTypes, uint32 BufferTypeHash);
	
	// thread group counts
	int32 NumThreadsX;
	int32 NumThreadsY;
	int32 NumThreadsZ;
    
private:
    // the state object for a compute shader
    FMetalShaderPipeline* Pipeline[EMetalBufferType_Num];
};

struct FMetalRenderPipelineHash
{
	friend uint32 GetTypeHash(FMetalRenderPipelineHash const& Hash)
	{
		return HashCombine(GetTypeHash(Hash.RasterBits), GetTypeHash(Hash.TargetBits));
	}
	
	friend bool operator==(FMetalRenderPipelineHash const& Left, FMetalRenderPipelineHash const& Right)
	{
		return Left.RasterBits == Right.RasterBits && Left.TargetBits == Right.TargetBits;
	}
	
	uint64 RasterBits;
	uint64 TargetBits;
};

class DEPRECATED(4.15, "Use GraphicsPipelineState Interface") FMetalBoundShaderState : public FRHIBoundShaderState
{
};

class FMetalGraphicsPipelineState : public FRHIGraphicsPipelineState
{
public:
	FMetalGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Init);
	virtual ~FMetalGraphicsPipelineState();

	FMetalShaderPipeline* GetPipeline(EMetalIndexType IndexType, uint32 VertexBufferHash, uint32 PixelBufferHash, uint32 DomainBufferHash, EPixelFormat const* const VertexBufferTypes = nullptr, EPixelFormat const* const PixelBufferTypes = nullptr, EPixelFormat const* const DomainBufferTypes = nullptr);
	
	/** Cached vertex structure */
	TRefCountPtr<FMetalVertexDeclaration> VertexDeclaration;
	
	/** Cached shaders */
	TRefCountPtr<FMetalVertexShader> VertexShader;
	TRefCountPtr<FMetalPixelShader> PixelShader;
	TRefCountPtr<FMetalHullShader> HullShader;
	TRefCountPtr<FMetalDomainShader> DomainShader;
	TRefCountPtr<FMetalGeometryShader> GeometryShader;
	
	/** Cached state objects */
	TRefCountPtr<FMetalDepthStencilState> DepthStencilState;
	TRefCountPtr<FMetalRasterizerState> RasterizerState;
	
	inline EPrimitiveType GetPrimitiveType() { return Initializer.PrimitiveType; }
	
private:
	// Needed to runtime refine shaders currently.
	FGraphicsPipelineStateInitializer Initializer;
	// Tessellation pipelines have three different variations for the indexing-style.
	FMetalShaderPipeline* PipelineStates[EMetalIndexType_Num][EMetalBufferType_Num][EMetalBufferType_Num][EMetalBufferType_Num];
};

class FMetalComputePipelineState : public FRHIComputePipelineState
{
public:
	FMetalComputePipelineState(FMetalComputeShader* InComputeShader)
	: ComputeShader(InComputeShader)
	{
		check(InComputeShader);
	}
	virtual ~FMetalComputePipelineState() {}

	FMetalComputeShader* GetComputeShader()
	{
		return ComputeShader;
	}

private:
	TRefCountPtr<FMetalComputeShader> ComputeShader;
};

class FMetalSubBufferHeap;
class FMetalSubBufferLinear;
class FMetalSubBufferMagazine;

class FMetalBuffer : public mtlpp::Buffer
{
public:
	FMetalBuffer(ns::Ownership retain = ns::Ownership::Retain) : mtlpp::Buffer(retain), Heap(nullptr), Linear(nullptr), Magazine(nullptr), bPooled(false) { }
	FMetalBuffer(ns::Protocol<id<MTLBuffer>>::type handle, ns::Ownership retain = ns::Ownership::Retain);
	
	FMetalBuffer(mtlpp::Buffer&& rhs, FMetalSubBufferHeap* heap);
	FMetalBuffer(mtlpp::Buffer&& rhs, FMetalSubBufferLinear* heap);
	FMetalBuffer(mtlpp::Buffer&& rhs, FMetalSubBufferMagazine* magazine);
	FMetalBuffer(mtlpp::Buffer&& rhs, bool bInPooled);
	
	FMetalBuffer(const FMetalBuffer& rhs);
	FMetalBuffer(FMetalBuffer&& rhs);
	virtual ~FMetalBuffer();
	
	FMetalBuffer& operator=(const FMetalBuffer& rhs);
	FMetalBuffer& operator=(FMetalBuffer&& rhs);
	
	inline bool operator==(FMetalBuffer const& rhs) const
	{
		return mtlpp::Buffer::operator==(rhs);
	}
	
	inline bool IsPooled() const { return bPooled; }
	inline bool IsSingleUse() const { return bSingleUse; }
	inline void MarkSingleUse() { bSingleUse = true; }
	void Release();
	
	friend uint32 GetTypeHash(FMetalBuffer const& Hash)
	{
		return HashCombine(GetTypeHash(Hash.GetPtr()), GetTypeHash((uint64)Hash.GetOffset()));
	}
	
private:
	FMetalSubBufferHeap* Heap;
	FMetalSubBufferLinear* Linear;
	FMetalSubBufferMagazine* Magazine;
	bool bPooled;
	bool bSingleUse;
};

class FMetalTexture : public mtlpp::Texture
{
public:
	FMetalTexture(ns::Ownership retain = ns::Ownership::Retain) : mtlpp::Texture(retain) { }
	FMetalTexture(ns::Protocol<id<MTLTexture>>::type handle, ns::Ownership retain = ns::Ownership::Retain)
	: mtlpp::Texture(handle, nullptr, retain) {}
	
	FMetalTexture(mtlpp::Texture&& rhs)
	: mtlpp::Texture((mtlpp::Texture&&)rhs)
	{
		
	}
	
	FMetalTexture(const FMetalTexture& rhs)
	: mtlpp::Texture(rhs)
	{
		
	}
	
	FMetalTexture(FMetalTexture&& rhs)
	: mtlpp::Texture((mtlpp::Texture&&)rhs)
	{
		
	}
	
	FMetalTexture& operator=(const FMetalTexture& rhs)
	{
		if (this != &rhs)
		{
			mtlpp::Texture::operator=(rhs);
		}
		return *this;
	}
	
	FMetalTexture& operator=(FMetalTexture&& rhs)
	{
		mtlpp::Texture::operator=((mtlpp::Texture&&)rhs);
		return *this;
	}
	
	inline bool operator==(FMetalTexture const& rhs) const
	{
		return mtlpp::Texture::operator==(rhs);
	}
	
	friend uint32 GetTypeHash(FMetalTexture const& Hash)
	{
		return GetTypeHash(Hash.GetPtr());
	}
};

/** Texture/RT wrapper. */
class FMetalSurface
{
public:

	/** 
	 * Constructor that will create Texture and Color/DepthBuffers as needed
	 */
	FMetalSurface(ERHIResourceType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumSamples, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData);

	FMetalSurface(FMetalSurface& Source, NSRange MipRange);
	
	FMetalSurface(FMetalSurface& Source, NSRange MipRange, EPixelFormat Format);
	
	/**
	 * Destructor
	 */
	~FMetalSurface();

	/** Prepare for texture-view support - need only call this once on the source texture which is to be viewed. */
	void PrepareTextureView();
	
	/** @returns A newly allocated buffer object large enough for the surface within the texture specified. */
	FMetalBuffer AllocSurface(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride);

	/** Apply the data in Buffer to the surface specified. */
	void UpdateSurface(FMetalBuffer& Buffer, uint32 MipIndex, uint32 ArrayIndex);
	
	/**
	 * Locks one of the texture's mip-maps.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 * @return A pointer to the specified texture data.
	 */
	void* Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride);

	/** Unlocks a previously locked mip-map.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 */
	void Unlock(uint32 MipIndex, uint32 ArrayIndex);
	
	/**
	 * Locks one of the texture's mip-maps.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 * @return A pointer to the specified texture data.
	 */
	void* AsyncLock(class FRHICommandListImmediate& RHICmdList, uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride, bool bNeedsDefaultRHIFlush);
	
	/** Unlocks a previously locked mip-map.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 */
	void AsyncUnlock(class FRHICommandListImmediate& RHICmdList, uint32 MipIndex, uint32 ArrayIndex);

	/**
	 * Returns how much memory a single mip uses, and optionally returns the stride
	 */
	uint32 GetMipSize(uint32 MipIndex, uint32* Stride, bool bSingleLayer);

	/**
	 * Returns how much memory is used by the surface
	 */
	uint32 GetMemorySize();

	/** Returns the number of faces for the texture */
	uint32 GetNumFaces();
	
	/** Gets the drawable texture if this is a back-buffer surface. */
	FMetalTexture GetDrawableTexture();

	FMetalTexture Reallocate(FMetalTexture Texture, mtlpp::TextureUsage UsageModifier);
	void ReplaceTexture(FMetalContext& Context, FMetalTexture OldTexture, FMetalTexture NewTexture);
	void MakeAliasable(void);
	void MakeUnAliasable(void);
	
	ERHIResourceType Type;
	EPixelFormat PixelFormat;
	uint8 FormatKey;
	//texture used for store actions and binding to shader params
	FMetalTexture Texture;
	//if surface is MSAA, texture used to bind for RT
	FMetalTexture MSAATexture;

	//texture used for a resolve target.  Same as texture on iOS.  
	//Dummy target on Mac where RHISupportsSeparateMSAAAndResolveTextures is true.	In this case we don't always want a resolve texture but we
	//have to have one until renderpasses are implemented at a high level.
	// Mac / RHISupportsSeparateMSAAAndResolveTextures == true
	// iOS A9+ where depth resolve is available
	// iOS < A9 where depth resolve is unavailable.
	FMetalTexture MSAAResolveTexture;
	FMetalTexture StencilTexture;
	uint32 SizeX, SizeY, SizeZ;
	bool bIsCubemap;
	int32 volatile Written;
	
	uint32 Flags;
	// one per mip
	FMetalBuffer LockedMemory[16];
	uint32 WriteLock;

	// how much memory is allocated for this texture
	uint64 TotalTextureSize;
	
	// For back-buffers, the owning viewport.
	class FMetalViewport* Viewport;
	
	TSet<class FMetalShaderResourceView*> SRVs;

private:
	void Init(FMetalSurface& Source, NSRange MipRange);
	
	void Init(FMetalSurface& Source, NSRange MipRange, EPixelFormat Format);
	
private:
	// The movie playback IOSurface/CVTexture wrapper to avoid page-off
	CFTypeRef ImageSurfaceRef;
	
	// Texture view surfaces don't own their resources, only reference
	bool bTextureView;
	
	// Count of outstanding async. texture uploads
	static volatile int64 ActiveUploads;
};

class FMetalTexture2D : public FRHITexture2D
{
public:
	/** The surface info */
	FMetalSurface Surface;

	// Constructor, just calls base and Surface constructor
	FMetalTexture2D(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITexture2D(SizeX, SizeY, NumMips, NumSamples, Format, Flags, InClearValue)
		, Surface(RRT_Texture2D, Format, SizeX, SizeY, 1, NumSamples, /*bArray=*/ false, 1, NumMips, Flags, BulkData)
	{
	}
	
	virtual ~FMetalTexture2D()
	{
	}
	
	virtual void* GetTextureBaseRHI() override final
	{
		return &Surface;
	}
	
	virtual void* GetNativeResource() const override final
	{
		return Surface.Texture;
	}
};

class FMetalTexture2DArray : public FRHITexture2DArray
{
public:
	/** The surface info */
	FMetalSurface Surface;

	// Constructor, just calls base and Surface constructor
	FMetalTexture2DArray(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITexture2DArray(SizeX, SizeY, ArraySize, NumMips, Format, Flags, InClearValue)
		, Surface(RRT_Texture2DArray, Format, SizeX, SizeY, 1, /*NumSamples=*/1, /*bArray=*/ true, ArraySize, NumMips, Flags, BulkData)
	{
	}
	
	virtual ~FMetalTexture2DArray()
	{
	}
	
	virtual void* GetTextureBaseRHI() override final
	{
		return &Surface;
	}
};

class FMetalTexture3D : public FRHITexture3D
{
public:
	/** The surface info */
	FMetalSurface Surface;

	// Constructor, just calls base and Surface constructor
	FMetalTexture3D(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITexture3D(SizeX, SizeY, SizeZ, NumMips, Format, Flags, InClearValue)
		, Surface(RRT_Texture3D, Format, SizeX, SizeY, SizeZ, /*NumSamples=*/1, /*bArray=*/ false, 1, NumMips, Flags, BulkData)
	{
	}
	
	virtual ~FMetalTexture3D()
	{
	}
	
	virtual void* GetTextureBaseRHI() override final
	{
		return &Surface;
	}
};

class FMetalTextureCube : public FRHITextureCube
{
public:
	/** The surface info */
	FMetalSurface Surface;

	// Constructor, just calls base and Surface constructor
	FMetalTextureCube(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITextureCube(Size, NumMips, Format, Flags, InClearValue)
		, Surface(RRT_TextureCube, Format, Size, Size, 6, /*NumSamples=*/1, bArray, ArraySize, NumMips, Flags, BulkData)
	{
	}
	
	virtual ~FMetalTextureCube()
	{
	}
	
	virtual void* GetTextureBaseRHI() override final
	{
		return &Surface;
	}

	virtual void* GetNativeResource() const override final
	{
		return Surface.Texture;
	}
};

struct FMetalCommandBufferFence
{
	bool Wait(uint64 Millis);
	
	mtlpp::CommandBufferFence CommandBufferFence;
};

struct FMetalQueryBuffer : public FRHIResource
{
	FMetalQueryBuffer(FMetalContext* InContext, FMetalBuffer InBuffer);
	
	virtual ~FMetalQueryBuffer();
	
	uint64 GetResult(uint32 Offset);
	
	TWeakPtr<struct FMetalQueryBufferPool, ESPMode::ThreadSafe> Pool;
	FMetalBuffer Buffer;
	uint32 WriteOffset;
};
typedef TRefCountPtr<FMetalQueryBuffer> FMetalQueryBufferRef;

struct FMetalQueryResult
{
	bool Wait(uint64 Millis);
	uint64 GetResult();
	
	FMetalQueryBufferRef SourceBuffer;
	TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe> CommandBufferFence;
	uint32 Offset;
	bool bCompleted;
	bool bBatchFence;
};

/** Metal occlusion query */
class FMetalRenderQuery : public FRHIRenderQuery
{
public:

	/** Initialization constructor. */
	FMetalRenderQuery(ERenderQueryType InQueryType);

	virtual ~FMetalRenderQuery();

	/**
	 * Kick off an occlusion test 
	 */
	void Begin(FMetalContext* Context, TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe> const& BatchFence);

	/**
	 * Finish up an occlusion test 
	 */
	void End(FMetalContext* Context);
	
	// The type of query
	ERenderQueryType Type;

	// Query buffer allocation details as the buffer is already set on the command-encoder
	FMetalQueryResult Buffer;
	
	// Query result.
	volatile uint64 Result;
	
	// Result availability - if not set the first call to acquire it will read the buffer & cache
	volatile bool bAvailable;
};

@interface FMetalBufferData : FApplePlatformObject<NSObject>
{
@public
	uint8* Data;
	uint32 Len;	
}
-(instancetype)initWithSize:(uint32)Size;
-(instancetype)initWithBytes:(void const*)Data length:(uint32)Size;
@end

enum EMetalBufferUsage
{
	EMetalBufferUsage_GPUOnly = 0x80000000,
	EMetalBufferUsage_LinearTex = 0x40000000,
};

class FMetalRHIBuffer
{
public:
	FMetalRHIBuffer(uint32 InSize, uint32 InUsage, ERHIResourceType InType);
	virtual ~FMetalRHIBuffer();
	
	/**
	 * Allocate the index buffer backing store.
	 */
	void Alloc(uint32 InSize, EResourceLockMode LockMode);
	
	/**
	 * Allocate a linear texture for given format.
	 */
	FMetalTexture AllocLinearTexture(EPixelFormat Format);
	
	/**
	 * Get a linear texture for given format.
	 */
	ns::AutoReleased<FMetalTexture> CreateLinearTexture(EPixelFormat Format);
	
	/**
	 * Get a linear texture for given format.
	 */
	ns::AutoReleased<FMetalTexture> GetLinearTexture(EPixelFormat Format);
	
	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void* Lock(EResourceLockMode LockMode, uint32 Offset, uint32 Size=0);
	
	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void Unlock();
	
	// balsa buffer memory
	FMetalBuffer Buffer;
	
	// A temporary shared/CPU accessible buffer for upload/download
	FMetalBuffer CPUBuffer;
	
	// The map of linear textures for this vertex buffer - may be more than one due to type conversion.
	TMap<EPixelFormat, FMetalTexture> LinearTextures;
	
	/** Buffer for small buffers < 4Kb to avoid heap fragmentation. */
	FMetalBufferData* Data;
	
	// Frame of last upload, if there was one.
	uint32 LastUpdate;
	
	// offset into the buffer (for lock usage)
	uint32 LockOffset;
	
	// Sizeof outstanding lock.
	uint32 LockSize;
	
	// Initial buffer size.
	uint32 Size;
	
	// Buffer usage.
	uint32 Usage;
	
	// Resource type
	ERHIResourceType Type;
};

/** Index buffer resource class that stores stride information. */
class FMetalIndexBuffer : public FRHIIndexBuffer, public FMetalRHIBuffer
{
public:
	
	/** Constructor */
	FMetalIndexBuffer(uint32 InStride, uint32 InSize, uint32 InUsage);
	virtual ~FMetalIndexBuffer();
	
	// 16- or 32-bit
	mtlpp::IndexType IndexType;
};

/** Vertex buffer resource class that stores usage type. */
class FMetalVertexBuffer : public FRHIVertexBuffer, public FMetalRHIBuffer
{
public:

	/** Constructor */
	FMetalVertexBuffer(uint32 InSize, uint32 InUsage);
	virtual ~FMetalVertexBuffer();
};

class FMetalUniformBuffer : public FRHIUniformBuffer, public FMetalRHIBuffer
{
public:

	// Constructor
	FMetalUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage);

	// Destructor 
	virtual ~FMetalUniformBuffer();
	
	void const* GetData();
	
	/** Resource table containing RHI references. */
	TArray<TRefCountPtr<FRHIResource> > ResourceTable;

};


class FMetalStructuredBuffer : public FRHIStructuredBuffer, public FMetalRHIBuffer
{
public:
	// Constructor
	FMetalStructuredBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 InUsage);

	// Destructor
	~FMetalStructuredBuffer();
};


class FMetalShaderResourceView : public FRHIShaderResourceView
{
public:

	// The vertex buffer this SRV comes from (can be null)
	TRefCountPtr<FMetalVertexBuffer> SourceVertexBuffer;
	
	// The index buffer this SRV comes from (can be null)
	TRefCountPtr<FMetalIndexBuffer> SourceIndexBuffer;

	// The texture that this SRV come from
	TRefCountPtr<FRHITexture> SourceTexture;
	
	// The source structured buffer (can be null)
	TRefCountPtr<FMetalStructuredBuffer> SourceStructuredBuffer;
	
	FMetalSurface* TextureView;
	uint8 MipLevel;
	uint8 NumMips;
	uint8 Format;
	uint8 Stride;

	FMetalShaderResourceView();
	~FMetalShaderResourceView();
	
	ns::AutoReleased<FMetalTexture> GetLinearTexture(bool const bUAV);
};



class FMetalUnorderedAccessView : public FRHIUnorderedAccessView
{
public:
	
	// the potential resources to refer to with the UAV object
	TRefCountPtr<FMetalShaderResourceView> SourceView;
};

class FMetalShaderParameterCache
{
public:
	/** Constructor. */
	FMetalShaderParameterCache();

	/** Destructor. */
	~FMetalShaderParameterCache();

	inline void PrepareGlobalUniforms(uint32 TypeIndex, uint32 UniformArraySize)
	{
		if (PackedGlobalUniformsSizes[TypeIndex] < UniformArraySize)
		{
			ResizeGlobalUniforms(TypeIndex, UniformArraySize);
		}
	}
	
	/**
	 * Invalidates all existing data.
	 */
	void Reset();

	/**
	 * Marks all uniform arrays as dirty.
	 */
	void MarkAllDirty();

	/**
	 * Sets values directly into the packed uniform array
	 */
	void Set(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValues);

	/**
	 * Commit shader parameters to the currently bound program.
	 */
	void CommitPackedGlobals(class FMetalStateCache* Cache, class FMetalCommandEncoder* Encoder, EShaderFrequency Frequency, const FMetalShaderBindings& Bindings);
	void CommitPackedUniformBuffers(class FMetalStateCache* Cache, TRefCountPtr<FMetalGraphicsPipelineState> BoundShaderState, FMetalComputeShader* ComputeShader, int32 Stage, const TRefCountPtr<FRHIUniformBuffer>* UniformBuffers, const TArray<CrossCompiler::FUniformBufferCopyInfo>& UniformBuffersCopyInfo);

private:
	/** CPU memory block for storing uniform values. */
	uint8* PackedGlobalUniforms[CrossCompiler::PACKED_TYPEINDEX_MAX];

	struct FRange
	{
		uint32	LowVector;
		uint32	HighVector;
	};
	/** Dirty ranges for each uniform array. */
	FRange	PackedGlobalUniformDirty[CrossCompiler::PACKED_TYPEINDEX_MAX];

	uint32 PackedGlobalUniformsSizes[CrossCompiler::PACKED_TYPEINDEX_MAX];

	void ResizeGlobalUniforms(uint32 TypeIndex, uint32 UniformArraySize);
};

class FMetalComputeFence : public FRHIComputeFence
{
public:
	
	FMetalComputeFence(FName InName)
	: FRHIComputeFence(InName)
	{}
	
	virtual ~FMetalComputeFence()
	{
	}
	
	virtual void Reset() final override
	{
		FRHIComputeFence::Reset();
		Fence = nil;
	}
	
	void Write(mtlpp::Fence InFence)
	{
		check(Fence.GetPtr() == nil);
		Fence = InFence;
		FRHIComputeFence::WriteFence();
	}
	
	void Wait(FMetalContext& Context);
	
private:
	mtlpp::Fence Fence;
};

class FMetalShaderLibrary final : public FRHIShaderLibrary
{	
public:
	FMetalShaderLibrary(EShaderPlatform Platform, FString const& Name, mtlpp::Library Library, FMetalShaderMap const& Map);
	virtual ~FMetalShaderLibrary();
	
	virtual bool IsNativeLibrary() const override final {return true;}
		
	class FMetalShaderLibraryIterator : public FRHIShaderLibrary::FShaderLibraryIterator
	{
	public:
		FMetalShaderLibraryIterator(FMetalShaderLibrary* MetalShaderLibrary) : FShaderLibraryIterator(MetalShaderLibrary), IteratorImpl(MetalShaderLibrary->Map.HashMap.CreateIterator()) {}
		
		virtual bool IsValid() const final override
		{
			return !!IteratorImpl;
		}
		
		virtual FShaderLibraryEntry operator*() const final override;
		virtual FShaderLibraryIterator& operator++() final override
		{
			++IteratorImpl;
			return *this;
		}
		
	private:		
		TMap<FSHAHash, TPair<uint8, TArray<uint8>>>::TIterator IteratorImpl;
	};
	
	virtual TRefCountPtr<FShaderLibraryIterator> CreateIterator(void) final override
	{
		return new FMetalShaderLibraryIterator(this);
	}
	
	virtual bool ContainsEntry(const FSHAHash& Hash) final override;
	virtual bool RequestEntry(const FSHAHash& Hash, FArchive* Ar) final override;
	
	virtual uint32 GetShaderCount(void) const final override { return Map.HashMap.Num(); }
	
private:

	friend class FMetalDynamicRHI;
	
	FPixelShaderRHIRef CreatePixelShader(const FSHAHash& Hash);
	FVertexShaderRHIRef CreateVertexShader(const FSHAHash& Hash);
	FHullShaderRHIRef CreateHullShader(const FSHAHash& Hash);
	FDomainShaderRHIRef CreateDomainShader(const FSHAHash& Hash);
	FGeometryShaderRHIRef CreateGeometryShader(const FSHAHash& Hash);
	FGeometryShaderRHIRef CreateGeometryShaderWithStreamOutput(const FSHAHash& Hash, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream);
	FComputeShaderRHIRef CreateComputeShader(const FSHAHash& Hash);
	
private:
	mtlpp::Library Library;
	FMetalShaderMap Map;
};

template<class T>
struct TMetalResourceTraits
{
};
template<>
struct TMetalResourceTraits<FRHIShaderLibrary>
{
	typedef FMetalShaderLibrary TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIVertexDeclaration>
{
	typedef FMetalVertexDeclaration TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIVertexShader>
{
	typedef FMetalVertexShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIGeometryShader>
{
	typedef FMetalGeometryShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIHullShader>
{
	typedef FMetalHullShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIDomainShader>
{
	typedef FMetalDomainShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIPixelShader>
{
	typedef FMetalPixelShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIComputeShader>
{
	typedef FMetalComputeShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHITexture3D>
{
	typedef FMetalTexture3D TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHITexture2D>
{
	typedef FMetalTexture2D TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHITexture2DArray>
{
	typedef FMetalTexture2DArray TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHITextureCube>
{
	typedef FMetalTextureCube TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIRenderQuery>
{
	typedef FMetalRenderQuery TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIUniformBuffer>
{
	typedef FMetalUniformBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIIndexBuffer>
{
	typedef FMetalIndexBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIStructuredBuffer>
{
	typedef FMetalStructuredBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIVertexBuffer>
{
	typedef FMetalVertexBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIShaderResourceView>
{
	typedef FMetalShaderResourceView TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIUnorderedAccessView>
{
	typedef FMetalUnorderedAccessView TConcreteType;
};

template<>
struct TMetalResourceTraits<FRHISamplerState>
{
	typedef FMetalSamplerState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIRasterizerState>
{
	typedef FMetalRasterizerState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIDepthStencilState>
{
	typedef FMetalDepthStencilState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIBlendState>
{
	typedef FMetalBlendState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIComputeFence>
{
	typedef FMetalComputeFence TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIGraphicsPipelineState>
{
	typedef FMetalGraphicsPipelineState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIComputePipelineState>
{
	typedef FMetalComputePipelineState TConcreteType;
};
