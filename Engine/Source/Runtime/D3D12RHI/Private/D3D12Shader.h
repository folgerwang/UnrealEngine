// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Shader.h: D3D12 Shaders
=============================================================================*/

#pragma once

template <>
struct TTypeTraits<D3D12_INPUT_ELEMENT_DESC> : public TTypeTraitsBase < D3D12_INPUT_ELEMENT_DESC >
{
	enum { IsBytewiseComparable = true };
};

/** Convenience typedef: preallocated array of D3D12 input element descriptions. */
typedef TArray<D3D12_INPUT_ELEMENT_DESC, TFixedAllocator<MaxVertexElementCount> > FD3D12VertexElements;

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FD3D12VertexDeclaration : public FRHIVertexDeclaration
{
public:
	/** Elements of the vertex declaration. */
	FD3D12VertexElements VertexElements;

	uint16 StreamStrides[MaxVertexElementCount];

	/** Initialization constructor. */
	explicit FD3D12VertexDeclaration(const FD3D12VertexElements& InElements, const uint16* InStrides)
		: VertexElements(InElements)
	{
		FMemory::Memcpy(StreamStrides, InStrides, sizeof(StreamStrides));
	}

	virtual bool GetInitializer(FVertexDeclarationElementList& Init) final override;
};

/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
class FD3D12VertexShader : public FRHIVertexShader
{
public:
	enum { StaticFrequency = SF_Vertex };

	/** The shader's bytecode. */
	FD3D12ShaderBytecode ShaderBytecode;

	FD3D12ShaderResourceTable ShaderResourceTable;

	/** The vertex shader's bytecode, with custom data in the last byte. */
	TArray<uint8> Code;

	// TEMP remove with removal of bound shader state
	int32 Offset;

	FShaderCodePackedResourceCounts ResourceCounts;
};

class FD3D12GeometryShader : public FRHIGeometryShader
{
public:
	enum { StaticFrequency = SF_Geometry };

	/** The shader's bytecode. */
	FD3D12ShaderBytecode ShaderBytecode;

	FD3D12ShaderResourceTable ShaderResourceTable;

	/** The shader's bytecode, with custom data in the last byte. */
	TArray<uint8> Code;

	/** The shader's stream output description. */
	D3D12_STREAM_OUTPUT_DESC StreamOutput;
	D3D12_SO_DECLARATION_ENTRY* pStreamOutEntries;
	uint32* pStreamOutStrides;

	bool bShaderNeedsStreamOutput;

	FShaderCodePackedResourceCounts ResourceCounts;

	FD3D12GeometryShader()
		: pStreamOutEntries(nullptr)
		, pStreamOutStrides(nullptr)
		, bShaderNeedsStreamOutput(false)
	{
		FMemory::Memzero(&StreamOutput, sizeof(StreamOutput));
	}

	virtual ~FD3D12GeometryShader()
	{
		if (pStreamOutEntries)
		{
			delete[] pStreamOutEntries;
			pStreamOutEntries = nullptr;
		}

		if (pStreamOutStrides)
		{
			delete[] pStreamOutStrides;
			pStreamOutStrides = nullptr;
		}
	}
};

class FD3D12HullShader : public FRHIHullShader
{
public:
	enum { StaticFrequency = SF_Hull };

	/** The shader's bytecode. */
	FD3D12ShaderBytecode ShaderBytecode;

	FD3D12ShaderResourceTable ShaderResourceTable;

	/** The shader's bytecode, with custom data in the last byte. */
	TArray<uint8> Code;

	FShaderCodePackedResourceCounts ResourceCounts;
};

class FD3D12DomainShader : public FRHIDomainShader
{
public:
	enum { StaticFrequency = SF_Domain };

	/** The shader's bytecode. */
	FD3D12ShaderBytecode ShaderBytecode;

	FD3D12ShaderResourceTable ShaderResourceTable;

	/** The shader's bytecode, with custom data in the last byte. */
	TArray<uint8> Code;

	FShaderCodePackedResourceCounts ResourceCounts;
};

class FD3D12PixelShader : public FRHIPixelShader
{
public:
	enum { StaticFrequency = SF_Pixel };

	/** The shader's bytecode. */
	FD3D12ShaderBytecode ShaderBytecode;

	/** The shader's bytecode, with custom data in the last byte. */
	TArray<uint8> Code;

	FD3D12ShaderResourceTable ShaderResourceTable;

	FShaderCodePackedResourceCounts ResourceCounts;
};

class FD3D12ComputeShader : public FRHIComputeShader
{
public:
	enum { StaticFrequency = SF_Compute };

	/** The shader's bytecode. */
	FD3D12ShaderBytecode ShaderBytecode;

	/** The shader's bytecode, with custom data in the last byte. */
	TArray<uint8> Code;

	FD3D12ShaderResourceTable ShaderResourceTable;

	FShaderCodePackedResourceCounts ResourceCounts;
	const FD3D12RootSignature* pRootSignature;
};

/**
* Combined shader state and vertex definition for rendering geometry.
* Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
*/
class FD3D12BoundShaderState : public FRHIBoundShaderState
{
public:

#if D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE
	FCachedBoundShaderStateLink_Threadsafe CacheLink;
#else
	FCachedBoundShaderStateLink CacheLink;
#endif

	const FD3D12RootSignature* pRootSignature;

	/** Initialization constructor. */
	FD3D12BoundShaderState(
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI,
		FD3D12Device* InDevice
		);

	virtual ~FD3D12BoundShaderState();

	/**
	* Get the shader for the given frequency.
	*/
	FORCEINLINE FD3D12VertexDeclaration* GetVertexDeclaration() const { return (FD3D12VertexDeclaration*) CacheLink.GetVertexDeclaration(); }
	FORCEINLINE FD3D12VertexShader*      GetVertexShader()      const { return (FD3D12VertexShader*)      CacheLink.GetVertexShader();      }
	FORCEINLINE FD3D12PixelShader*       GetPixelShader()       const { return (FD3D12PixelShader*)       CacheLink.GetPixelShader();       }
	FORCEINLINE FD3D12HullShader*        GetHullShader()        const { return (FD3D12HullShader*)        CacheLink.GetHullShader();        }
	FORCEINLINE FD3D12DomainShader*      GetDomainShader()      const { return (FD3D12DomainShader*)      CacheLink.GetDomainShader();      }
	FORCEINLINE FD3D12GeometryShader*    GetGeometryShader()    const { return (FD3D12GeometryShader*)    CacheLink.GetGeometryShader();    }
};

#if D3D12_RHI_RAYTRACING

class FD3D12RayTracingShader : public FRHIRayTracingShader
{
public:
	/** The shader's bytecode. */
	FD3D12ShaderBytecode ShaderBytecode;

	FD3D12ShaderResourceTable ShaderResourceTable;

	/** The shader's bytecode, with custom data in the last byte. */
	TArray<uint8> Code;

	/** The shader's DXIL entrypoint & base export name for DXR (required for RTPSO creation) */
	FString EntryPoint; // Primary entry point for all ray tracing shaders. Assumed to be closest hit shader for SF_RayHitGroup.
	FString AnyHitEntryPoint; // Optional any-hit shader entry point for SF_RayHitGroup.
	FString IntersectionEntryPoint; // Optional intersection shader entry point for SF_RayHitGroup.

	FShaderCodePackedResourceCounts ResourceCounts;

	const FD3D12RootSignature* pRootSignature = nullptr;
};

#endif // D3D12_RHI_RAYTRACING

template<>
struct TD3D12ResourceTraits<FRHIVertexShader>
{
	typedef FD3D12VertexShader TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIGeometryShader>
{
	typedef FD3D12GeometryShader TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIHullShader>
{
	typedef FD3D12HullShader TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIDomainShader>
{
	typedef FD3D12DomainShader TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIPixelShader>
{
	typedef FD3D12PixelShader TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIComputeShader>
{
	typedef FD3D12ComputeShader TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIVertexDeclaration>
{
	typedef FD3D12VertexDeclaration TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIBoundShaderState>
{
	typedef FD3D12BoundShaderState TConcreteType;
};