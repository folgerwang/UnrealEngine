// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUSkinVertexFactory.h: GPU skinning vertex factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "RHI.h"
#include "RenderResource.h"
#include "BoneIndices.h"
#include "GPUSkinPublicDefs.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include "ResourcePool.h"
#include "Matrix3x4.h"

template <class T> class TConsoleVariableData;

// Uniform buffer for APEX cloth
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FAPEXClothUniformShaderParameters,)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

enum
{
	MAX_GPU_BONE_MATRICES_UNIFORMBUFFER = 75,
};

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FBoneMatricesUniformShaderParameters,)
	SHADER_PARAMETER_ARRAY(FMatrix3x4, BoneMatrices, [MAX_GPU_BONE_MATRICES_UNIFORMBUFFER])
END_GLOBAL_SHADER_PARAMETER_STRUCT()

#define SET_BONE_DATA(B, X) B.SetMatrixTranspose(X)

/** Shared data & implementation for the different types of pool */
class FSharedPoolPolicyData
{
public:
	/** Buffers are created with a simple byte size */
	typedef uint32 CreationArguments;
	enum
	{
		NumSafeFrames = 3, /** Number of frames to leaves buffers before reclaiming/reusing */
		NumPoolBucketSizes = 17, /** Number of pool buckets */
		NumToDrainPerFrame = 10, /** Max. number of resources to cull in a single frame */
		CullAfterFramesNum = 30 /** Resources are culled if unused for more frames than this */
	};
	
	/** Get the pool bucket index from the size
	 * @param Size the number of bytes for the resource 
	 * @returns The bucket index.
	 */
	uint32 GetPoolBucketIndex(uint32 Size);
	
	/** Get the pool bucket size from the index
	 * @param Bucket the bucket index
	 * @returns The bucket size.
	 */
	uint32 GetPoolBucketSize(uint32 Bucket);
	
private:
	/** The bucket sizes */
	static uint32 BucketSizes[NumPoolBucketSizes];
};

/** Struct to pool the vertex buffer & SRV together */
struct FVertexBufferAndSRV
{
	void SafeRelease()
	{
		VertexBufferRHI.SafeRelease();
		VertexBufferSRV.SafeRelease();
	}

	FVertexBufferRHIRef VertexBufferRHI;
	FShaderResourceViewRHIRef VertexBufferSRV;
};

/**
 * Helper function to test whether the buffer is valid.
 * @param Buffer Buffer to test
 * @returns True if the buffer is valid otherwise false
 */
inline bool IsValidRef(const FVertexBufferAndSRV& Buffer)
{
	return IsValidRef(Buffer.VertexBufferRHI) && IsValidRef(Buffer.VertexBufferSRV);
}

/** The policy for pooling bone vertex buffers */
class FBoneBufferPoolPolicy : public FSharedPoolPolicyData
{
public:
	enum
	{
		NumSafeFrames = FSharedPoolPolicyData::NumSafeFrames,
		NumPoolBuckets = FSharedPoolPolicyData::NumPoolBucketSizes,
		NumToDrainPerFrame = FSharedPoolPolicyData::NumToDrainPerFrame,
		CullAfterFramesNum = FSharedPoolPolicyData::CullAfterFramesNum
	};
	/** Creates the resource 
	 * @param Args The buffer size in bytes.
	 */
	FVertexBufferAndSRV CreateResource(FSharedPoolPolicyData::CreationArguments Args);
	
	/** Gets the arguments used to create resource
	 * @param Resource The buffer to get data for.
	 * @returns The arguments used to create the buffer.
	 */
	FSharedPoolPolicyData::CreationArguments GetCreationArguments(const FVertexBufferAndSRV& Resource);
	
	/** Frees the resource
	 * @param Resource The buffer to prepare for release from the pool permanently.
	 */
	void FreeResource(FVertexBufferAndSRV Resource);
};

/** A pool for vertex buffers with consistent usage, bucketed for efficiency. */
class FBoneBufferPool : public TRenderResourcePool<FVertexBufferAndSRV, FBoneBufferPoolPolicy, FSharedPoolPolicyData::CreationArguments>
{
public:
	/** Destructor */
	virtual ~FBoneBufferPool();

public: // From FTickableObjectRenderThread
	virtual TStatId GetStatId() const override;
};

/** The policy for pooling bone vertex buffers */
class FClothBufferPoolPolicy : public FBoneBufferPoolPolicy
{
public:
	/** Creates the resource 
	 * @param Args The buffer size in bytes.
	 */
	FVertexBufferAndSRV CreateResource(FSharedPoolPolicyData::CreationArguments Args);
};

/** A pool for vertex buffers with consistent usage, bucketed for efficiency. */
class FClothBufferPool : public TRenderResourcePool<FVertexBufferAndSRV, FClothBufferPoolPolicy, FSharedPoolPolicyData::CreationArguments>
{
public:
	/** Destructor */
	virtual ~FClothBufferPool();
	
public: // From FTickableObjectRenderThread
	virtual TStatId GetStatId() const override;
};


/** Vertex factory with vertex stream components for GPU skinned vertices */
class FGPUBaseSkinVertexFactory : public FVertexFactory
{
public:
	struct FShaderDataType
	{
		FShaderDataType()
			: CurrentBuffer(0)
			, PreviousRevisionNumber(0)
			, CurrentRevisionNumber(0)
		{
			// BoneDataOffset and BoneTextureSize are not set as they are only valid if IsValidRef(BoneTexture)
			MaxGPUSkinBones = GetMaxGPUSkinBones();
			check(MaxGPUSkinBones <= GHardwareMaxGPUSkinBones);
		}

		// @param FrameTime from GFrameTime
		bool UpdateBoneData(FRHICommandListImmediate& RHICmdList, const TArray<FMatrix>& ReferenceToLocalMatrices,
			const TArray<FBoneIndexType>& BoneMap, uint32 RevisionNumber, bool bPrevious, ERHIFeatureLevel::Type FeatureLevel, bool bUseSkinCache);

		void ReleaseBoneData()
		{
			ensure(IsInRenderingThread());

			UniformBuffer.SafeRelease();

			for(uint32 i = 0; i < 2; ++i)
			{
				if (IsValidRef(BoneBuffer[i]))
				{
					BoneBufferPool.ReleasePooledResource(BoneBuffer[i]);
				}
				BoneBuffer[i].SafeRelease();
			}
		}
		
		// if FeatureLevel <= ERHIFeatureLevel::ES3_1
		FUniformBufferRHIParamRef GetUniformBuffer() const
		{
			return UniformBuffer;
		}
		
		// @param bPrevious true:previous, false:current
		const FVertexBufferAndSRV& GetBoneBufferForReading(bool bPrevious) const
		{
			const FVertexBufferAndSRV* RetPtr = &GetBoneBufferInternal(bPrevious);

			if(!RetPtr->VertexBufferRHI.IsValid())
			{
				// this only should happen if we request the old data
				check(bPrevious);

				// if we don't have any old data we use the current one
				RetPtr = &GetBoneBufferInternal(false);

				// at least the current one needs to be valid when reading
				check(RetPtr->VertexBufferRHI.IsValid());
			}

			return *RetPtr;
		}

		// @param bPrevious true:previous, false:current
		// @return IsValid() can fail, then you have to create the buffers first (or if the size changes)
		FVertexBufferAndSRV& GetBoneBufferForWriting(bool bPrevious)
		{
			const FShaderDataType* This = (const FShaderDataType*)this;
			// non const version maps to const version
			return (FVertexBufferAndSRV&)This->GetBoneBufferInternal(bPrevious);
		}

		// @param bPrevious true:previous, false:current
		// @return returns revision number 
		uint32 GetRevisionNumber(bool bPrevious)
		{
			return (bPrevious) ? PreviousRevisionNumber : CurrentRevisionNumber;
		}

	private:
		// double buffered bone positions+orientations to support normal rendering and velocity (new-old position) rendering
		FVertexBufferAndSRV BoneBuffer[2];
		// 0 / 1 to index into BoneBuffer
		uint32 CurrentBuffer;
		// RevisionNumber Tracker
		uint32 PreviousRevisionNumber;
		uint32 CurrentRevisionNumber;
		// if FeatureLevel <= ERHIFeatureLevel::ES3_1
		FUniformBufferRHIRef UniformBuffer;
		
		static TConsoleVariableData<int32>* MaxBonesVar;
		static uint32 MaxGPUSkinBones;
		
		// @param RevisionNumber - updated last revision number
		// This flips revision number to previous if this is new
		// otherwise, it keeps current version
		void SetCurrentRevisionNumber(uint32 RevisionNumber)
		{
			if (CurrentRevisionNumber != RevisionNumber)
			{
				PreviousRevisionNumber = CurrentRevisionNumber;
				CurrentRevisionNumber = RevisionNumber;
				CurrentBuffer = 1 - CurrentBuffer;
			}
		}
		// to support GetBoneBufferForWriting() and GetBoneBufferForReading()
		// @param bPrevious true:previous, false:current
		// @return might not pass the IsValid() 
		const FVertexBufferAndSRV& GetBoneBufferInternal(bool bPrevious) const
		{
			check(IsInParallelRenderingThread());

			if ((CurrentRevisionNumber - PreviousRevisionNumber) > 1)
			{
				bPrevious = false;
			}

			uint32 BufferIndex = CurrentBuffer ^ (uint32)bPrevious;

			const FVertexBufferAndSRV& Ret = BoneBuffer[BufferIndex];
			return Ret;
		}
	};

	FGPUBaseSkinVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, uint32 InNumVertices)
		: FVertexFactory(InFeatureLevel)
		, NumVertices(InNumVertices)
	{
	}

	virtual ~FGPUBaseSkinVertexFactory() {}

	/** accessor */
	FORCEINLINE FShaderDataType& GetShaderData()
	{
		return ShaderData;
	}

	FORCEINLINE const FShaderDataType& GetShaderData() const
	{
		return ShaderData;
	}

	virtual bool UsesExtraBoneInfluences() const { return false; }

	static bool SupportsTessellationShaders() { return true; }

	uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	ENGINE_API static int32 GetMaxGPUSkinBones();

	static const uint32 GHardwareMaxGPUSkinBones = 256;	
	
	virtual const FShaderResourceViewRHIRef GetPositionsSRV() const = 0;
	virtual const FShaderResourceViewRHIRef GetTangentsSRV() const = 0;
	virtual const FShaderResourceViewRHIRef GetTextureCoordinatesSRV() const = 0;
	virtual const FShaderResourceViewRHIRef GetColorComponentsSRV() const = 0;
	virtual uint32 GetNumTexCoords() const = 0;
	virtual const uint32 GetColorIndexMask() const = 0;

	inline const FVertexStreamComponent& GetTangentStreamComponent(int Index)
	{
		check(TangentStreamComponents[Index].VertexBuffer != nullptr);
		return TangentStreamComponents[Index];
	}

protected:
	/** dynamic data need for setting the shader */ 
	FShaderDataType ShaderData;
	/** Pool of buffers for bone matrices. */
	static TGlobalResource<FBoneBufferPool> BoneBufferPool;

	FVertexStreamComponent TangentStreamComponents[2];

private:
	uint32 NumVertices;
};

/** Vertex factory with vertex stream components for GPU skinned vertices */
template<bool bExtraBoneInfluencesT>
class ENGINE_API TGPUSkinVertexFactory : public FGPUBaseSkinVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(TGPUSkinVertexFactory<bExtraBoneInfluencesT>);

public:

	enum
	{
		HasExtraBoneInfluences = bExtraBoneInfluencesT,
	};

	struct FDataType : public FStaticMeshDataType
	{
		/** The stream to read the bone indices from */
		FVertexStreamComponent BoneIndices;

		/** The stream to read the extra bone indices from */
		FVertexStreamComponent ExtraBoneIndices;

		/** The stream to read the bone weights from */
		FVertexStreamComponent BoneWeights;

		/** The stream to read the extra bone weights from */
		FVertexStreamComponent ExtraBoneWeights;
	};

	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	TGPUSkinVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, uint32 InNumVertices)
		: FGPUBaseSkinVertexFactory(InFeatureLevel, InNumVertices)
	{}

	virtual bool UsesExtraBoneInfluences() const override
	{
		return bExtraBoneInfluencesT;
	}

	static void ModifyCompilationEnvironment(const FVertexFactoryType* Type, EShaderPlatform Platform, const class ::FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const class ::FMaterial* Material, const FShaderType* ShaderType);
	
	/**
	* An implementation of the interface used by TSynchronizedResource to 
	* update the resource with new data from the game thread.
	* @param	InData - new stream component data
	*/
	void SetData(const FDataType& InData)
	{
		Data = InData;
		this->TangentStreamComponents[0] = InData.TangentBasisComponents[0];
		this->TangentStreamComponents[1] = InData.TangentBasisComponents[1];
		FGPUBaseSkinVertexFactory::UpdateRHI();
	}

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	void CopyDataTypeForPassthroughFactory(class FGPUSkinPassthroughVertexFactory* PassthroughVertexFactory);

	const FShaderResourceViewRHIRef GetPositionsSRV() const override
	{
		return Data.PositionComponentSRV;
	}

	const FShaderResourceViewRHIRef GetTangentsSRV() const override
	{
		return Data.TangentsSRV;
	}

	const FShaderResourceViewRHIRef GetTextureCoordinatesSRV() const override
	{
		return Data.TextureCoordinatesSRV;
	}

	uint32 GetNumTexCoords() const override
	{
		return Data.NumTexCoords;
	}

	const FShaderResourceViewRHIRef GetColorComponentsSRV() const override
	{
		return Data.ColorComponentsSRV;
	}

	const uint32 GetColorIndexMask() const override
	{
		return Data.ColorIndexMask;
	}

protected:
	/**
	* Add the decl elements for the streams
	* @param InData - type with stream components
	* @param OutElements - vertex decl list to modify
	*/
	void AddVertexElements(FDataType& InData, FVertexDeclarationElementList& OutElements);

	inline const FDataType& GetData() const { return Data; }

private:
	/** stream component data bound to this vertex factory */
	FDataType Data;  
};

/** 
 * Vertex factory with vertex stream components for GPU-skinned streams, enabled for passthrough mode when vertices have been pre-skinned 
 */
class FGPUSkinPassthroughVertexFactory : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FGPUSkinPassthroughVertexFactory);

	typedef FLocalVertexFactory Super;

public:
	FGPUSkinPassthroughVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FLocalVertexFactory(InFeatureLevel, "FGPUSkinPassthroughVertexFactory"), PositionStreamIndex(-1), TangentStreamIndex(-1)
	{
		bSupportsManualVertexFetch = true;
	}

	static void ModifyCompilationEnvironment(const FVertexFactoryType* Type, EShaderPlatform Platform, const class ::FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const class ::FMaterial* Material, const FShaderType* ShaderType);

	inline void UpdateVertexDeclaration(FGPUBaseSkinVertexFactory* SourceVertexFactory, struct FRWBuffer* PositionRWBuffer, struct FRWBuffer* TangentRWBuffer)
	{
		if (PositionStreamIndex == -1)
		{
			InternalUpdateVertexDeclaration(SourceVertexFactory, PositionRWBuffer, TangentRWBuffer);
		}
	}

	inline int32 GetPositionStreamIndex() const
	{
		check(PositionStreamIndex > -1);
		return PositionStreamIndex;
	}

	inline int32 GetTangentStreamIndex() const
	{
		return TangentStreamIndex;
	}

	void SetData(const FDataType& InData);

	// FRenderResource interface.
	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	//TODO should be supported
	bool SupportsPositionOnlyStream() const override { return false; }

	inline void InvalidateStreams()
	{
		PositionStreamIndex = -1;
		TangentStreamIndex = -1;
	}

	virtual void ReleaseRHI() override
	{
		FLocalVertexFactory::ReleaseRHI();

		//when adding anything else to this function be aware of the bypassing code in InternalUpdateVertexDeclaration
		PositionVBAlias.ReleaseRHI();
		TangentVBAlias.ReleaseRHI();
	}

protected:
	// Vertex buffer required for creating the Vertex Declaration
	FVertexBuffer PositionVBAlias;
	FVertexBuffer TangentVBAlias;
	int32 PositionStreamIndex;
	int32 TangentStreamIndex;

	void InternalUpdateVertexDeclaration(FGPUBaseSkinVertexFactory* SourceVertexFactory, struct FRWBuffer* PositionRWBuffer, struct FRWBuffer* TangentRWBuffer);
};

/** Vertex factory with vertex stream components for GPU-skinned and morph target streams */
template<bool bExtraBoneInfluencesT>
class TGPUSkinMorphVertexFactory : public TGPUSkinVertexFactory<bExtraBoneInfluencesT>
{
	DECLARE_VERTEX_FACTORY_TYPE(TGPUSkinMorphVertexFactory<bExtraBoneInfluencesT>);

	typedef TGPUSkinVertexFactory<bExtraBoneInfluencesT> Super;
public:

	struct FDataType : TGPUSkinVertexFactory<bExtraBoneInfluencesT>::FDataType
	{
		/** stream which has the position deltas to add to the vertex position */
		FVertexStreamComponent DeltaPositionComponent;
		/** stream which has the TangentZ deltas to add to the vertex normals */
		FVertexStreamComponent DeltaTangentZComponent;
	};

	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	TGPUSkinMorphVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, uint32 InNumVertices)
	: TGPUSkinVertexFactory<bExtraBoneInfluencesT>(InFeatureLevel, InNumVertices)
	{}

	static void ModifyCompilationEnvironment(const FVertexFactoryType* Type, EShaderPlatform Platform, const class ::FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const class ::FMaterial* Material, const FShaderType* ShaderType);
	
	/**
	* An implementation of the interface used by TSynchronizedResource to 
	* update the resource with new data from the game thread.
	* @param	InData - new stream component data
	*/
	void SetData(const FDataType& InData)
	{
		MorphData = InData;
		this->TangentStreamComponents[0] = InData.TangentBasisComponents[0];
		this->TangentStreamComponents[1] = InData.TangentBasisComponents[1];
		FGPUBaseSkinVertexFactory::UpdateRHI();
	}

	// FRenderResource interface.

	/**
	* Creates declarations for each of the vertex stream components and
	* initializes the device resource
	*/
	virtual void InitRHI() override;

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	const FShaderResourceViewRHIRef GetPositionsSRV() const override
	{
		return MorphData.PositionComponentSRV;
	}

	const FShaderResourceViewRHIRef GetTangentsSRV() const override
	{
		return MorphData.TangentsSRV;
	}

	const FShaderResourceViewRHIRef GetTextureCoordinatesSRV() const override
	{
		return MorphData.TextureCoordinatesSRV;
	}

	uint32 GetNumTexCoords() const override
	{
		return MorphData.NumTexCoords;
	}

	const FShaderResourceViewRHIRef GetColorComponentsSRV() const override
	{
		return MorphData.ColorComponentsSRV;
	}

	const uint32 GetColorIndexMask() const override
	{
		return MorphData.ColorIndexMask;
	}

protected:
	/**
	* Add the decl elements for the streams
	* @param InData - type with stream components
	* @param OutElements - vertex decl list to modify
	*/
	void AddVertexElements(FDataType& InData, FVertexDeclarationElementList& OutElements);

private:
	/** stream component data bound to this vertex factory */
	FDataType MorphData;

};

/** Vertex factory with vertex stream components for GPU-skinned and morph target streams */
class FGPUBaseSkinAPEXClothVertexFactory
{
public:
	struct ClothShaderType
	{
		ClothShaderType()
			: ClothBlendWeight(1.0f)
		{
			Reset();
		}

		bool UpdateClothSimulData(FRHICommandListImmediate& RHICmdList, const TArray<FVector>& InSimulPositions, const TArray<FVector>& InSimulNormals, uint32 FrameNumber, ERHIFeatureLevel::Type FeatureLevel);

		void ReleaseClothSimulData()
		{
			APEXClothUniformBuffer.SafeRelease();

			for(uint32 i = 0; i < 2; ++i)
			{
				if (IsValidRef(ClothSimulPositionNormalBuffer[i]))
				{
					ClothSimulDataBufferPool.ReleasePooledResource(ClothSimulPositionNormalBuffer[i]);
					ClothSimulPositionNormalBuffer[i].SafeRelease();
				}
			}
			Reset();
		}

		TUniformBufferRef<FAPEXClothUniformShaderParameters> GetClothUniformBuffer() const
		{
			return APEXClothUniformBuffer;
		}
		
		// @param FrameNumber usually from View.Family->FrameNumber
		// @return IsValid() can fail, then you have to create the buffers first (or if the size changes)
		FVertexBufferAndSRV& GetClothBufferForWriting(uint32 FrameNumber)
		{
			uint32 Index = GetOldestIndex(FrameNumber);

			// we don't write -1 as that is used to invalidate the entry
			if(FrameNumber == -1)
			{
				// this could cause a 1 frame glitch on wraparound
				FrameNumber = 0;
			}

			BufferFrameNumber[Index] = FrameNumber;

			return ClothSimulPositionNormalBuffer[Index];
		}

		// @param bPrevious true:previous, false:current
		// @param FrameNumber usually from View.Family->FrameNumber
		const FVertexBufferAndSRV& GetClothBufferForReading(bool bPrevious, uint32 FrameNumber) const
		{
			int32 Index = GetMostRecentIndex(FrameNumber);

			if(bPrevious && DoWeHavePreviousData())
			{
				Index = 1 - Index;
			}

			checkf(ClothSimulPositionNormalBuffer[Index].VertexBufferRHI.IsValid(), TEXT("Index: %i Buffer0: %s Frame0: %i Buffer1: %s Frame1: %i"), Index,  ClothSimulPositionNormalBuffer[0].VertexBufferRHI.IsValid() ? TEXT("true") : TEXT("false"), BufferFrameNumber[0], ClothSimulPositionNormalBuffer[1].VertexBufferRHI.IsValid() ? TEXT("true") : TEXT("false"), BufferFrameNumber[1]);
			return ClothSimulPositionNormalBuffer[Index];
		}
		
		FMatrix& GetClothLocalToWorldForWriting(uint32 FrameNumber)
		{
			uint32 Index = GetOldestIndex(FrameNumber);

			return ClothLocalToWorld[Index];
		}

		const FMatrix& GetClothLocalToWorldForReading(bool bPrevious, uint32 FrameNumber) const
		{
			int32 Index = GetMostRecentIndex(FrameNumber);

			if(bPrevious && DoWeHavePreviousData())
			{
				Index = 1 - Index;
			}

			return ClothLocalToWorld[Index];
		}

		/**
		 * weight to blend between simulated positions and key-framed poses
		 * if ClothBlendWeight is 1.0, it shows only simulated positions and if it is 0.0, it shows only key-framed animation
		 */
		float ClothBlendWeight;

	private:
		// fallback for ClothSimulPositionNormalBuffer if the shadermodel doesn't allow it
		TUniformBufferRef<FAPEXClothUniformShaderParameters> APEXClothUniformBuffer;
		// 
		FVertexBufferAndSRV ClothSimulPositionNormalBuffer[2];
		// from GFrameNumber, to detect pause and old data when an object was not rendered for some time
		uint32 BufferFrameNumber[2];

		/**
		 * Matrix to apply to positions/normals
		 */
		FMatrix ClothLocalToWorld[2];

		// @return 0 / 1, index into ClothSimulPositionNormalBuffer[]
		uint32 GetMostRecentIndex(uint32 FrameNumber) const
		{
			if(BufferFrameNumber[0] == -1)
			{
				//ensure(BufferFrameNumber[1] != -1);

				return 1;
			}
			else if(BufferFrameNumber[1] == -1)
			{
				//ensure(BufferFrameNumber[0] != -1);
				return 0;
			}

			// should handle warp around correctly, did some basic testing
			uint32 Age0 = FrameNumber - BufferFrameNumber[0];
			uint32 Age1 = FrameNumber - BufferFrameNumber[1];

			return (Age0 > Age1) ? 1 : 0;
		}

		// @return 0/1, index into ClothSimulPositionNormalBuffer[]
		uint32 GetOldestIndex(uint32 FrameNumber) const
		{
			if(BufferFrameNumber[0] == -1)
			{
				return 0;
			}
			else if(BufferFrameNumber[1] == -1)
			{
				return 1;
			}

			// should handle warp around correctly (todo: test)
			uint32 Age0 = FrameNumber - BufferFrameNumber[0];
			uint32 Age1 = FrameNumber - BufferFrameNumber[1];

			return (Age0 > Age1) ? 0 : 1;
		}

		bool DoWeHavePreviousData() const
		{
			if(BufferFrameNumber[0] == -1 || BufferFrameNumber[1] == -1)
			{
				return false;
			}
			
			int32 Diff = BufferFrameNumber[0] - BufferFrameNumber[1];

			uint32 DiffAbs = FMath::Abs(Diff);

			// threshold is >1 because there could be in between frames e.g. HitProxyRendering
			// We should switch to TickNumber to solve this
			return DiffAbs <= 2;
		}

		void Reset()
		{
			// both are not valid
			BufferFrameNumber[0] = -1;
			BufferFrameNumber[1] = -1;

			ClothLocalToWorld[0] = FMatrix::Identity;
			ClothLocalToWorld[1] = FMatrix::Identity;
		}
	};

	virtual ~FGPUBaseSkinAPEXClothVertexFactory() {}

	/** accessor */
	FORCEINLINE ClothShaderType& GetClothShaderData()
	{
		return ClothShaderData;
	}

	FORCEINLINE const ClothShaderType& GetClothShaderData() const
	{
		return ClothShaderData;
	}

	virtual FGPUBaseSkinVertexFactory* GetVertexFactory() = 0;
	virtual const FGPUBaseSkinVertexFactory* GetVertexFactory() const = 0;

protected:
	ClothShaderType ClothShaderData;

	/** Pool of buffers for clothing simulation data */
	static TGlobalResource<FClothBufferPool> ClothSimulDataBufferPool;
};

/** Vertex factory with vertex stream components for GPU-skinned and morph target streams */
template<bool bExtraBoneInfluencesT>
class TGPUSkinAPEXClothVertexFactory : public FGPUBaseSkinAPEXClothVertexFactory, public TGPUSkinVertexFactory<bExtraBoneInfluencesT>
{
	DECLARE_VERTEX_FACTORY_TYPE(TGPUSkinAPEXClothVertexFactory<bExtraBoneInfluencesT>);

	typedef TGPUSkinVertexFactory<bExtraBoneInfluencesT> Super;

public:

	struct FDataType : TGPUSkinVertexFactory<bExtraBoneInfluencesT>::FDataType
	{
		/** stream which has the physical mesh position + height offset */
		FVertexStreamComponent CoordPositionComponent;
		/** stream which has the physical mesh coordinate for normal + offset */
		FVertexStreamComponent CoordNormalComponent;
		/** stream which has the physical mesh coordinate for tangent + offset */
		FVertexStreamComponent CoordTangentComponent;
		/** stream which has the physical mesh vertex indices */
		FVertexStreamComponent SimulIndicesComponent;

		FShaderResourceViewRHIRef ClothBuffer;
		// Packed Map: u32 Key, u32 Value
		TArray<uint64> ClothIndexMapping;
	};

	inline FShaderResourceViewRHIRef GetClothBuffer()
	{
		return MeshMappingData.ClothBuffer;
	}

	inline const FShaderResourceViewRHIRef GetClothBuffer() const
	{
		return MeshMappingData.ClothBuffer;
	}

	inline uint32 GetClothIndexOffset(uint64 VertexIndex) const
	{
		for (uint64 Mapping : MeshMappingData.ClothIndexMapping)
		{
			uint64 CurrentVertexIndex = Mapping >> (uint64)32;
			if ((CurrentVertexIndex & (uint64)0xffffffff) == VertexIndex)
			{
				return (uint32)(Mapping & (uint64)0xffffffff);
			}
		}

		checkf(0, TEXT("Cloth Index Mapping not found for Vertex Index %u"), VertexIndex);
		return 0;
	}

	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	TGPUSkinAPEXClothVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, uint32 InNumVertices)
		: TGPUSkinVertexFactory<bExtraBoneInfluencesT>(InFeatureLevel, InNumVertices)
	{}

	static void ModifyCompilationEnvironment(const FVertexFactoryType* Type, EShaderPlatform Platform, const class ::FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const class ::FMaterial* Material, const FShaderType* ShaderType);

	/**
	* An implementation of the interface used by TSynchronizedResource to 
	* update the resource with new data from the game thread.
	* @param	InData - new stream component data
	*/
	void SetData(const FDataType& InData)
	{
        Super::SetData(InData);
		MeshMappingData = InData;
		this->TangentStreamComponents[0] = InData.TangentBasisComponents[0];
		this->TangentStreamComponents[1] = InData.TangentBasisComponents[1];
		FGPUBaseSkinVertexFactory::UpdateRHI();
	}

	virtual FGPUBaseSkinVertexFactory* GetVertexFactory() override
	{
		return this;
	}

	virtual const FGPUBaseSkinVertexFactory* GetVertexFactory() const override
	{
		return this;
	}

	const FShaderResourceViewRHIRef GetPositionsSRV() const override
	{
		return MeshMappingData.PositionComponentSRV;
	}

	const FShaderResourceViewRHIRef GetTangentsSRV() const override
	{
		return MeshMappingData.TangentsSRV;
	}

	const FShaderResourceViewRHIRef GetTextureCoordinatesSRV() const override
	{
		return MeshMappingData.TextureCoordinatesSRV;
	}

	uint32 GetNumTexCoords() const override
	{
		return MeshMappingData.NumTexCoords;
	}

	const FShaderResourceViewRHIRef GetColorComponentsSRV() const override
	{
		return MeshMappingData.ColorComponentsSRV;
	}

	const uint32 GetColorIndexMask() const override
	{
		return MeshMappingData.ColorIndexMask;
	}

	// FRenderResource interface.

	/**
	* Creates declarations for each of the vertex stream components and
	* initializes the device resource
	*/
	virtual void InitRHI() override;
	virtual void ReleaseDynamicRHI() override;

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

protected:
	/**
	* Add the decl elements for the streams
	* @param InData - type with stream components
	* @param OutElements - vertex decl list to modify
	*/
	void AddVertexElements(FDataType& InData, FVertexDeclarationElementList& OutElements);

private:
	/** stream component data bound to this vertex factory */
	FDataType MeshMappingData; 
};

