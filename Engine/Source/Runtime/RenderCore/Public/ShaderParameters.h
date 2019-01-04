// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameters.h: Shader parameter definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

class FShaderParameterMap;
class FShaderParametersMetadata;
struct FShaderCompilerEnvironment;

RENDERCORE_API void CacheUniformBufferIncludes(TMap<const TCHAR*, struct FCachedUniformBufferDeclaration>& Cache, EShaderPlatform Platform);


enum EShaderParameterFlags
{
	// no shader error if the parameter is not used
	SPF_Optional,
	// shader error if the parameter is not used
	SPF_Mandatory
};

/** A shader parameter's register binding. e.g. float1/2/3/4, can be an array, UAV */
class FShaderParameter
{
public:
	FShaderParameter()
	:	BufferIndex(0)
	,	BaseIndex(0)
	,	NumBytes(0)
#if UE_BUILD_DEBUG
	,	bInitialized(false)
#endif
	{}

	RENDERCORE_API void Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName, EShaderParameterFlags Flags = SPF_Optional);
	friend RENDERCORE_API FArchive& operator<<(FArchive& Ar,FShaderParameter& P);
	bool IsBound() const { return NumBytes > 0; }

	inline bool IsInitialized() const 
	{ 
#if UE_BUILD_DEBUG
		return bInitialized; 
#else 
		return true;
#endif
	}

	uint32 GetBufferIndex() const { return BufferIndex; }
	uint32 GetBaseIndex() const { return BaseIndex; }
	uint32 GetNumBytes() const { return NumBytes; }

private:
	uint16 BufferIndex;
	uint16 BaseIndex;
	// 0 if the parameter wasn't bound
	uint16 NumBytes;

#if UE_BUILD_DEBUG
	bool bInitialized;
#endif
};

/** A shader resource binding (textures or samplerstates). */
class FShaderResourceParameter
{
public:
	FShaderResourceParameter()
	:	BaseIndex(0)
	,	NumResources(0) 
#if UE_BUILD_DEBUG
	,	bInitialized(false)
#endif
	{}

	RENDERCORE_API void Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,EShaderParameterFlags Flags = SPF_Optional);
	friend RENDERCORE_API FArchive& operator<<(FArchive& Ar,FShaderResourceParameter& P);
	bool IsBound() const { return NumResources > 0; }

	inline bool IsInitialized() const 
	{ 
#if UE_BUILD_DEBUG
		return bInitialized; 
#else 
		return true;
#endif
	}

	uint32 GetBaseIndex() const { return BaseIndex; }
	uint32 GetNumResources() const { return NumResources; }
private:
	uint16 BaseIndex;
	uint16 NumResources;

#if UE_BUILD_DEBUG
	bool bInitialized;
#endif
};

/** A class that binds either a UAV or SRV of a resource. */
class FRWShaderParameter
{
public:

	void Bind(const FShaderParameterMap& ParameterMap,const TCHAR* BaseName)
	{
		SRVParameter.Bind(ParameterMap,BaseName);

		// If the shader wants to bind the parameter as a UAV, the parameter name must start with "RW"
		FString UAVName = FString(TEXT("RW")) + BaseName;
		UAVParameter.Bind(ParameterMap,*UAVName);

		// Verify that only one of the UAV or SRV parameters is accessed by the shader.
		checkf(!(SRVParameter.GetNumResources() && UAVParameter.GetNumResources()),TEXT("Shader binds SRV and UAV of the same resource: %s"),BaseName);
	}

	bool IsBound() const
	{
		return SRVParameter.IsBound() || UAVParameter.IsBound();
	}

	bool IsUAVBound() const
	{
		return UAVParameter.IsBound();
	}

	uint32 GetUAVIndex() const
	{
		return UAVParameter.GetBaseIndex();
	}

	friend FArchive& operator<<(FArchive& Ar,FRWShaderParameter& Parameter)
	{
		return Ar << Parameter.SRVParameter << Parameter.UAVParameter;
	}

	template<typename TShaderRHIRef, typename TRHICmdList>
	inline void SetBuffer(TRHICmdList& RHICmdList, TShaderRHIRef Shader, const FRWBuffer& RWBuffer) const;

	template<typename TShaderRHIRef, typename TRHICmdList>
	inline void SetBuffer(TRHICmdList& RHICmdList, TShaderRHIRef Shader, const FRWBufferStructured& RWBuffer) const;

	template<typename TShaderRHIRef, typename TRHICmdList>
	inline void SetTexture(TRHICmdList& RHICmdList, TShaderRHIRef Shader, const FTextureRHIParamRef Texture, FUnorderedAccessViewRHIParamRef UAV) const;

	template<typename TRHICmdList>
	inline void UnsetUAV(TRHICmdList& RHICmdList, FComputeShaderRHIParamRef ComputeShader) const;

private:

	FShaderResourceParameter SRVParameter;
	FShaderResourceParameter UAVParameter;
};

/** Creates a shader code declaration of this struct for the given shader platform. */
extern RENDERCORE_API void CreateUniformBufferShaderDeclaration(const TCHAR* Name,const FShaderParametersMetadata& UniformBufferStruct, FString& OutDeclaration);

class FShaderUniformBufferParameter
{
public:
	FShaderUniformBufferParameter()
	:	BaseIndex(0)
	,	bIsBound(false) 
#if UE_BUILD_DEBUG
	,	bInitialized(false)
#endif
	{}

	static RENDERCORE_API void ModifyCompilationEnvironment(const TCHAR* ParameterName,const FShaderParametersMetadata& Struct,EShaderPlatform Platform,FShaderCompilerEnvironment& OutEnvironment);

	RENDERCORE_API void Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,EShaderParameterFlags Flags = SPF_Optional);

	friend FArchive& operator<<(FArchive& Ar,FShaderUniformBufferParameter& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

	bool IsBound() const { return bIsBound; }

	void Serialize(FArchive& Ar)
	{
#if UE_BUILD_DEBUG
		if (Ar.IsLoading())
		{
			bInitialized = true;
		}
#endif
		Ar << BaseIndex << bIsBound;
	}

	inline bool IsInitialized() const 
	{ 
#if UE_BUILD_DEBUG
		return bInitialized; 
#else 
		return true;
#endif
	}

	inline void SetInitialized() 
	{ 
#if UE_BUILD_DEBUG
		bInitialized = true; 
#endif
	}

	uint32 GetBaseIndex() const { return BaseIndex; }

private:
	uint16 BaseIndex;
	bool bIsBound;

#if UE_BUILD_DEBUG
	bool bInitialized;
#endif
};

/** A shader uniform buffer binding with a specific structure. */
template<typename TBufferStruct>
class TShaderUniformBufferParameter : public FShaderUniformBufferParameter
{
public:
	static void ModifyCompilationEnvironment(const TCHAR* ParameterName,EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShaderUniformBufferParameter::ModifyCompilationEnvironment(ParameterName,TBufferStruct::StaticStruct,Platform,OutEnvironment);
	}

	friend FArchive& operator<<(FArchive& Ar,TShaderUniformBufferParameter& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

#if RHI_RAYTRACING
struct FRayTracingShaderBindingsWriter : FRayTracingShaderBindings
{
	void Set(const FShaderResourceParameter& Param, FTextureRHIParamRef Value)
	{
		if (Param.IsBound())
		{
			checkf(Param.GetNumResources() == 1, TEXT("Resource array binding is not implemented"));
			Textures[Param.GetBaseIndex()] = Value;
		}
	}

	void Set(const FShaderResourceParameter& Param, FShaderResourceViewRHIParamRef Value)
	{
		if (Param.IsBound())
		{
			checkf(Param.GetNumResources() == 1, TEXT("Resource array binding is not implemented"));
			SRVs[Param.GetBaseIndex()] = Value;
		}
	}

	void Set(const FShaderUniformBufferParameter& Param, FUniformBufferRHIParamRef Value)
	{
		if (Param.IsBound())
		{
			UniformBuffers[Param.GetBaseIndex()] = Value;
		}
	}

	void Set(const FShaderResourceParameter& Param, FUnorderedAccessViewRHIParamRef Value)
	{
		if (Param.IsBound())
		{
			checkf(Param.GetNumResources() == 1, TEXT("Resource array binding is not implemented"));
			UAVs[Param.GetBaseIndex()] = Value;
		}
	}

	void Set(const FShaderResourceParameter& Param, FSamplerStateRHIParamRef Value)
	{
		if (Param.IsBound())
		{
			checkf(Param.GetNumResources() == 1, TEXT("Resource array binding is not implemented"));
			Samplers[Param.GetBaseIndex()] = Value;
		}
	}

	void SetTexture(uint16 BaseIndex, FTextureRHIParamRef Value)
	{
		checkSlow(BaseIndex < ARRAY_COUNT(Textures));
		Textures[BaseIndex] = Value;
	}

	void SetSRV(uint16 BaseIndex, FShaderResourceViewRHIParamRef Value)
	{
		checkSlow(BaseIndex < ARRAY_COUNT(SRVs));
		SRVs[BaseIndex] = Value;
	}

	void SetSampler(uint16 BaseIndex, FSamplerStateRHIParamRef Value)
	{
		checkSlow(BaseIndex < ARRAY_COUNT(Samplers));
		Samplers[BaseIndex] = Value;
	}

	void SetUAV(uint16 BaseIndex, FUnorderedAccessViewRHIParamRef Value)
	{
		checkSlow(BaseIndex < ARRAY_COUNT(UAVs));
		UAVs[BaseIndex] = Value;
	}

	void SetUniformBuffer(uint16 BaseIndex, FUniformBufferRHIParamRef Value)
	{
		checkSlow(BaseIndex < ARRAY_COUNT(UniformBuffers));
		UniformBuffers[BaseIndex] = Value;
	}
};
#endif //RHI_RAYTRACING
