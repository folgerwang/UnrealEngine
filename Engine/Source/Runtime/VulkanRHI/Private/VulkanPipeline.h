// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPipeline.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanResources.h"
#include "VulkanShaderResources.h"
#include "VulkanDescriptorSets.h"
#include "ShaderPipelineCache.h"

#if VULKAN_ENABLE_LRU_CACHE
#include "PsoLruCache.h"
extern TAutoConsoleVariable<int32> CVarLRUMaxPipelineSize;
extern TAutoConsoleVariable<int32> CVarEnableLRU;
#endif

class FVulkanDevice;
class FVulkanRHIGraphicsPipelineState;
class FVulkanGfxPipeline;

class FGfxPSIKey : public TDataKey<FGfxPSIKey> {};
class FGfxEntryKey : public TDataKey<FGfxEntryKey> {};

template <typename T>
static inline uint64 GetShaderKey(T* ShaderType)
{
	auto* VulkanShader = ResourceCast(ShaderType);
	return VulkanShader ? VulkanShader->GetShaderKey() : 0;
}

class FVulkanPipelineStateCacheManager
{
public:
	FVulkanRHIGraphicsPipelineState* FindInRuntimeCache(const FGraphicsPipelineStateInitializer& Initializer, FGfxPSIKey& OutKey);

	// Array of potential cache locations; first entries have highest priority. Only one cache file is loaded. If unsuccessful, tries next entry in the array.
	void InitAndLoad(const TArray<FString>& CacheFilenames);
	void Save(const FString& CacheFilename, bool bFromPSOFC = false);

	FVulkanPipelineStateCacheManager(FVulkanDevice* InParent);
	~FVulkanPipelineStateCacheManager();

	void RebuildCache();

	struct FDescriptorSetLayoutBinding
	{
		uint32 Binding;
		//uint16 DescriptorCount;
		uint8 DescriptorType;
		uint8 StageFlags;

		void ReadFrom(const VkDescriptorSetLayoutBinding& InState);
		void WriteInto(VkDescriptorSetLayoutBinding& OutState) const;

		bool operator==(const FDescriptorSetLayoutBinding& In) const
		{
			return  Binding == In.Binding &&
				//DescriptorCount == In.DescriptorCount &&
				DescriptorType == In.DescriptorType &&
				StageFlags == In.StageFlags;
		}
	};

	// Shader microcode is shared between pipeline entries so keep a cache around to prevent duplicated storage
	struct FShaderUCodeCache
	{
		using TDataMap = TMap<FSHAHash, TArray<uint32>>;
		TDataMap Data;

		TArray<uint32>* Add(const FSHAHash& Hash, const FVulkanShader* Shader)
		{
			check(Shader->Spirv.Num() != 0);

			TArray<uint32>& Code = Data.Add(Hash);
			Code = Shader->Spirv;

			return &Data[Hash];
		}

		TArray<uint32>* Get(const FSHAHash& Hash)
		{
			return Data.Find(Hash);
		}
	};

	struct FShaderHashes
	{
		uint32 Hash;
		FSHAHash Stages[ShaderStage::NumStages];

		FShaderHashes();
		FShaderHashes(const FGraphicsPipelineStateInitializer& PSOInitializer);

		friend inline uint32 GetTypeHash(const FShaderHashes& Hashes)
		{
			return Hashes.Hash;
		}

		inline void Finalize()
		{
			Hash = FCrc::MemCrc32(Stages, sizeof(Stages));
		}

		friend inline bool operator == (const FShaderHashes& A, const FShaderHashes& B)
		{
			for (int32 Index = 0; Index < ShaderStage::NumStages; ++Index)
			{
				if (A.Stages[Index] != B.Stages[Index])
				{
					return false;
				}
			}

			return true;
		}
	};

	// Actual information required to recreate a pipeline when saving/loading from disk
	struct FGfxPipelineEntry
	{
		FGfxEntryKey CreateKey() const;

		uint32 VertexInputKey;
		bool bLoaded;

		uint32 RasterizationSamples;
		uint32 Topology;
		struct FBlendAttachment
		{
			bool bBlend;
			uint8 ColorBlendOp;
			uint8 SrcColorBlendFactor;
			uint8 DstColorBlendFactor;
			uint8 AlphaBlendOp;
			uint8 SrcAlphaBlendFactor;
			uint8 DstAlphaBlendFactor;
			uint8 ColorWriteMask;

			void ReadFrom(const VkPipelineColorBlendAttachmentState& InState);
			void WriteInto(VkPipelineColorBlendAttachmentState& OutState) const;

			bool operator==(const FBlendAttachment& In) const
			{
				return bBlend == In.bBlend &&
					ColorBlendOp == In.ColorBlendOp &&
					SrcColorBlendFactor == In.SrcColorBlendFactor &&
					DstColorBlendFactor == In.DstColorBlendFactor &&
					AlphaBlendOp == In.AlphaBlendOp &&
					SrcAlphaBlendFactor == In.SrcAlphaBlendFactor &&
					DstAlphaBlendFactor == In.DstAlphaBlendFactor &&
					ColorWriteMask == In.ColorWriteMask;
			}
		};
		TArray<FBlendAttachment> ColorAttachmentStates;

		TArray<TArray<FDescriptorSetLayoutBinding>> DescriptorSetLayoutBindings;

		struct FVertexBinding
		{
			uint32 Stride;
			uint16 Binding;
			uint16 InputRate;

			void ReadFrom(const VkVertexInputBindingDescription& InState);
			void WriteInto(VkVertexInputBindingDescription& OutState) const;

			bool operator==(const FVertexBinding& In) const
			{
				return Stride == In.Stride &&
					Binding == In.Binding &&
					InputRate == In.InputRate;
			}
		};
		TArray<FVertexBinding> VertexBindings;
		struct FVertexAttribute
		{
			uint32 Location;
			uint32 Binding;
			uint32 Format;
			uint32 Offset;

			void ReadFrom(const VkVertexInputAttributeDescription& InState);
			void WriteInto(VkVertexInputAttributeDescription& OutState) const;

			bool operator==(const FVertexAttribute& In) const
			{
				return Location == In.Location &&
					Binding == In.Binding &&
					Format == In.Format &&
					Offset == In.Offset;
			}
		};
		TArray<FVertexAttribute> VertexAttributes;

		struct FRasterizer
		{
			uint8 PolygonMode;
			uint8 CullMode;
			float DepthBiasSlopeScale;
			float DepthBiasConstantFactor;

			void ReadFrom(const VkPipelineRasterizationStateCreateInfo& InState);
			void WriteInto(VkPipelineRasterizationStateCreateInfo& OutState) const;

			bool operator==(const FRasterizer& In) const
			{
				return PolygonMode == In.PolygonMode &&
					CullMode == In.CullMode &&
					DepthBiasSlopeScale == In.DepthBiasSlopeScale &&
					DepthBiasConstantFactor == In.DepthBiasConstantFactor;
			}
		};
		FRasterizer Rasterizer;

		struct FDepthStencil
		{
			uint8 DepthCompareOp;
			bool bDepthTestEnable;
			bool bDepthWriteEnable;
			bool bStencilTestEnable;
			bool bDepthBoundsTestEnable;
			uint8 FrontFailOp;
			uint8 FrontPassOp;
			uint8 FrontDepthFailOp;
			uint8 FrontCompareOp;
			uint32 FrontCompareMask;
			uint32 FrontWriteMask;
			uint32 FrontReference;
			uint8 BackFailOp;
			uint8 BackPassOp;
			uint8 BackDepthFailOp;
			uint8 BackCompareOp;
			uint32 BackCompareMask;
			uint32 BackWriteMask;
			uint32 BackReference;

			void ReadFrom(const VkPipelineDepthStencilStateCreateInfo& InState);
			void WriteInto(VkPipelineDepthStencilStateCreateInfo& OutState) const;

			bool operator==(const FDepthStencil& In) const
			{
				return DepthCompareOp == In.DepthCompareOp &&
					bDepthTestEnable == In.bDepthTestEnable &&
					bDepthWriteEnable == In.bDepthWriteEnable &&
					bDepthBoundsTestEnable == In.bDepthBoundsTestEnable &&
					bStencilTestEnable == In.bStencilTestEnable &&
					FrontFailOp == In.FrontFailOp &&
					FrontPassOp == In.FrontPassOp &&
					FrontDepthFailOp == In.FrontDepthFailOp &&
					FrontCompareOp == In.FrontCompareOp &&
					FrontCompareMask == In.FrontCompareMask &&
					FrontWriteMask == In.FrontWriteMask &&
					FrontReference == In.FrontReference &&
					BackFailOp == In.BackFailOp &&
					BackPassOp == In.BackPassOp &&
					BackDepthFailOp == In.BackDepthFailOp &&
					BackCompareOp == In.BackCompareOp &&
					BackCompareMask == In.BackCompareMask &&
					BackWriteMask == In.BackWriteMask &&
					BackReference == In.BackReference;
			}
		};
		FDepthStencil DepthStencil;

		FShaderHashes ShaderHashes;
#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
		SIZE_T ImmutableSamplers[MaxImmutableSamplers] = { 0 };
#endif

		struct FRenderTargets
		{
			struct FAttachmentRef
			{
				uint32 Attachment;
				uint64 Layout;

				void ReadFrom(const VkAttachmentReference& InState);
				void WriteInto(VkAttachmentReference& OutState) const;
				bool operator == (const FAttachmentRef& In) const
				{
					return Attachment == In.Attachment && Layout == In.Layout;
				}
			};
			TArray<FAttachmentRef> ColorAttachments;
			TArray<FAttachmentRef> ResolveAttachments;
			FAttachmentRef DepthStencil;

			struct FAttachmentDesc
			{
				uint32 Format;
				uint8 Flags;
				uint8 Samples;
				uint8 LoadOp;
				uint8 StoreOp;
				uint8 StencilLoadOp;
				uint8 StencilStoreOp;
				uint64 InitialLayout;
				uint64 FinalLayout;

				bool operator==(const FAttachmentDesc& In) const
				{
					return Format == In.Format &&
						Flags == In.Flags &&
						Samples == In.Samples &&
						LoadOp == In.LoadOp &&
						StoreOp == In.StoreOp &&
						StencilLoadOp == In.StencilLoadOp &&
						StencilStoreOp == In.StencilStoreOp &&
						InitialLayout == In.InitialLayout &&
						FinalLayout == In.FinalLayout;
				}

				void ReadFrom(const VkAttachmentDescription &InState);
				void WriteInto(VkAttachmentDescription& OutState) const;
			};
			TArray<FAttachmentDesc> Descriptions;

			uint8 NumAttachments;
			uint8 NumColorAttachments;
			uint8 bHasDepthStencil;
			uint8 bHasResolveAttachments;
			uint8 NumUsedClearValues;
			uint32 RenderPassCompatibleHash;
			FVector Extent3D;

			void ReadFrom(const FVulkanRenderTargetLayout &InState);
			void WriteInto(FVulkanRenderTargetLayout& OutState) const;

			bool operator==(const FRenderTargets& In) const
			{
				return ColorAttachments == In.ColorAttachments &&
					ResolveAttachments == In.ResolveAttachments &&
					DepthStencil == In.DepthStencil &&
					Descriptions == In.Descriptions &&
					NumAttachments == In.NumAttachments &&
					NumColorAttachments == In.NumColorAttachments &&
					bHasDepthStencil == In.bHasDepthStencil &&
					bHasResolveAttachments == In.bHasResolveAttachments &&
					NumUsedClearValues == In.NumUsedClearValues &&
					RenderPassCompatibleHash == In.RenderPassCompatibleHash &&
					Extent3D == In.Extent3D;
			}
		};
		FRenderTargets RenderTargets;

		FGfxPipelineEntry()
			: VertexInputKey(0)
			, bLoaded(false)
			, RasterizationSamples(0)
			, Topology(0)
			, RenderPass(nullptr)
			, Layout(nullptr)
		{
			FMemory::Memzero(Rasterizer);
			FMemory::Memzero(DepthStencil);
			FMemory::Memzero(ShaderModules);
		}

		~FGfxPipelineEntry();

		VkShaderModule ShaderModules[ShaderStage::NumStages];
		const FVulkanRenderPass* RenderPass;
		FVulkanGfxLayout* Layout;

		void GetOrCreateShaderModules(FVulkanShader*const* Shaders);
		void PurgeShaderModules(FVulkanShader*const* Shaders);
		void PurgeLoadedShaderModules(FVulkanDevice* InDevice);

		bool operator==(const FGfxPipelineEntry& In) const
		{
			if (VertexInputKey != In.VertexInputKey)
			{
				return false;
			}

			if( bLoaded != In.bLoaded)
			{
				return false;
			}

			if (RasterizationSamples != In.RasterizationSamples)
			{
				return false;
			}

			if (Topology != In.Topology)
			{
				return false;
			}

			if (ColorAttachmentStates != In.ColorAttachmentStates)
			{
				return false;
			}

			if (DescriptorSetLayoutBindings != In.DescriptorSetLayoutBindings)
			{
				return false;
			}

			if (!(Rasterizer == In.Rasterizer))
			{
				return false;
			}

			if (!(DepthStencil == In.DepthStencil))
			{
				return false;
			}

			if (!(ShaderHashes == In.ShaderHashes))
			{
				return false;
			}

#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
			for (uint32 Index = 0; Index < MaxImmutableSamplers; ++Index)
			{
				if (ImmutableSamplers[Index] != In.ImmutableSamplers[Index])
				{
					return false;
				}
			}
#endif

			if (!(RenderTargets == In.RenderTargets))
			{
				return false;
			}

			if (VertexBindings != In.VertexBindings)
			{
				return false;
			}

			if (VertexAttributes != In.VertexAttributes)
			{
				return false;
			}

			return true;
		}
	};

	using TGfxPipelineEntrySharedPtr = TSharedPtr<FGfxPipelineEntry, ESPMode::ThreadSafe>;

#if VULKAN_ENABLE_LRU_CACHE
	struct FPipelineSize
	{
		uint32 ShaderHash;
		uint32 PipelineSize;

		FPipelineSize()
			:ShaderHash(0), PipelineSize(0)
		{
		}
	};
#endif

	FVulkanComputePipeline* GetOrCreateComputePipeline(FVulkanComputeShader* ComputeShader);

#if VULKAN_ENABLE_LRU_CACHE

	bool ShouldEvictImmediately()
	{
		return bEvictImmediately && PipelineLRU.IsActive();
	}

	TMap<uint32, FPipelineSize*> PipelineSizeList;	// key: Shader hash (FShaderHash), value: pipeline size

	class FVKPipelineLRU
	{
		typedef TPsoLruCache<FVulkanGfxPipeline*, FVulkanGfxPipeline*> FVulkanPipelineLRUCache;

		const int LRUCapacity = 2048;
		int32 LRUUsedPipelineSize;

		enum
		{
			NUM_BUFFERS = 3,
		};

		static void DeleteVkPipeline(FVulkanGfxPipeline* GfxPipeline);

		void EnsureVkPipelineAndAddToLRU(FVulkanRHIGraphicsPipelineState* Pipeline);

		void AddToLRU(FVulkanGfxPipeline* GfxPipeline);

		// Use with external lock - LRUCS
		void EvictFromLRU();

	public:
		FVKPipelineLRU() : LRUUsedPipelineSize(0), LRU(LRUCapacity)
		{
			bUseLRU = (int32)CVarEnableLRU.GetValueOnAnyThread() == 1;
		}

		bool IsActive()
		{
			return bUseLRU;
		}

		void Add(FVulkanGfxPipeline* GfxPipeline);

		void Touch(FVulkanRHIGraphicsPipelineState* Pipeline);
	
	private:
		bool bUseLRU;
		FVulkanPipelineLRUCache LRU;
		FCriticalSection LRUCS; // For LRU, LRUUsedPipelineSize and GfxPipeline->LRUNode
	};

	FVKPipelineLRU PipelineLRU;

#endif
private:
	FVulkanDevice* Device;

	bool bEvictImmediately;

	// if true, we will link to the PSOFC, loading later, when we have that guid and only if the guid matches, saving only if there is no match, and only saving after the PSOFC is done.
	bool bLinkedToPSOFC;
	bool bLinkedToPSOFCSucessfulLoaded;
	FString LinkedToPSOFCCacheFolderPath;
	FString LinkedToPSOFCCacheFolderFilename;
	FDelegateHandle OnShaderPipelineCacheOpenedDelegate;
	FDelegateHandle OnShaderPipelineCachePrecompilationCompleteDelegate;

	/** Delegate handlers to track the ShaderPipelineCache precompile. */
	void OnShaderPipelineCacheOpened(FString const& Name, EShaderPlatform Platform, uint32 Count, const FGuid& VersionGuid, FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext);
	void OnShaderPipelineCachePrecompilationComplete(uint32 Count, double Seconds, const FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext);

	TMap<FGfxPSIKey, FVulkanRHIGraphicsPipelineState*> InitializerToPipelineMap;
	FCriticalSection InitializerToPipelineMapCS;

	FCriticalSection GfxPipelineEntriesCS;
	TMap<FGfxEntryKey, TGfxPipelineEntrySharedPtr> GfxPipelineEntries;

	FRWLock ComputePipelineLock;
	TMap<uint64, FVulkanComputePipeline*> ComputePipelineEntries;

	VkPipelineCache PipelineCache;

	FShaderUCodeCache ShaderCache;

	FVulkanRHIGraphicsPipelineState* CreateAndAdd(const FGraphicsPipelineStateInitializer& PSOInitializer, FGfxPSIKey PSIKey, TGfxPipelineEntrySharedPtr GfxEntry, FGfxEntryKey GfxEntryKey);
	void CreateGfxPipelineFromEntry(FGfxPipelineEntry* GfxEntry, const FBoundShaderStateInput* BSI, FVulkanGfxPipeline* Pipeline);

	FGfxPipelineEntry* CreateGfxEntry(const FGraphicsPipelineStateInitializer& PSOInitializer);

	bool Load(const TArray<FString>& CacheFilenames);
	void DestroyCache();

	void GetVulkanShaders(const FBoundShaderStateInput& BSI, FVulkanShader* OutShaders[ShaderStage::NumStages]);
	FVulkanGfxLayout* GetOrGenerateGfxLayout(const FGraphicsPipelineStateInitializer& PSOInitializer, FVulkanShader*const* Shaders, FVulkanVertexInputStateInfo& OutVertexInputState);

	TMap<FGfxEntryKey, FVulkanGfxPipeline*> EntryKeyToGfxPipelineMap;

	//@@
	TMap<FVulkanDescriptorSetsLayoutInfo, FVulkanLayout*> LayoutMap;
	FCriticalSection LayoutMapCS;

	FVulkanRHIGraphicsPipelineState* FindInLoadedLibrary(const FGraphicsPipelineStateInitializer& PSOInitializer, FGfxPSIKey& PSIKey, TGfxPipelineEntrySharedPtr& OutGfxEntry, FGfxEntryKey& OutGfxEntryKey);

	friend class FVulkanDynamicRHI;

	//@@
	FVulkanLayout* FindOrAddLayout(const FVulkanDescriptorSetsLayoutInfo& DescriptorSetLayoutInfo, bool bGfxLayout);

	FVulkanComputePipeline* CreateComputePipelineFromShader(FVulkanComputeShader* Shader);

#if VULKAN_ENABLE_LRU_CACHE
	struct FVulkanLRUCacheFile
	{
		enum
		{
			LRU_CACHE_VERSION = 1,
		};
		struct FFileHeader
		{
			int32 Version = -1;
			int32 SizeOfPipelineSizes = -1;
		} Header;

		TArray<FPipelineSize*> PipelineSizes;

		void Save(FArchive& Ar);
		bool Load(FArchive& Ar);
	};
#endif

	static bool BinaryCacheMatches(FVulkanDevice* InDevice, const TArray<uint8>& DeviceCache);
};

// Common pipeline class
class FVulkanPipeline
{
public:
	FVulkanPipeline(FVulkanDevice* InDevice);
	virtual ~FVulkanPipeline();

	inline VkPipeline GetHandle() const
	{
		return Pipeline;
	}

	inline const FVulkanLayout& GetLayout() const
	{
		return *Layout;
	}
#if VULKAN_ENABLE_LRU_CACHE
	inline void DeleteVkPipeline(bool ImmediateDestroy)
	{
		if (Pipeline != VK_NULL_HANDLE)
		{
			if (ImmediateDestroy)
			{
				VulkanRHI::vkDestroyPipeline(Device->GetInstanceHandle(), Pipeline, VULKAN_CPU_ALLOCATOR);
			}
			else
			{
				Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::Pipeline, Pipeline);
			}
			Pipeline = VK_NULL_HANDLE;
		}
	}
#endif
protected:
	FVulkanDevice* Device;
	VkPipeline Pipeline;
	FVulkanLayout* Layout; /*owned by FVulkanPipelineStateCacheManager, do not delete yourself !*/

	friend class FVulkanPipelineStateCacheManager;
	friend class FVulkanRHIGraphicsPipelineState;
	friend class FVulkanComputePipelineDescriptorState;
};

class FVulkanComputePipeline : public FVulkanPipeline, public FRHIComputePipelineState
{
public:
	FVulkanComputePipeline(FVulkanDevice* InDevice);
	virtual ~FVulkanComputePipeline();

	inline const FVulkanShaderHeader& GetShaderCodeHeader() const
	{
		return ComputeShader->GetCodeHeader();
	}

	inline const FVulkanComputeShader* GetShader() const
	{
		return ComputeShader;
	}

	inline void Bind(VkCommandBuffer CmdBuffer)
	{
		VulkanRHI::vkCmdBindPipeline(CmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
	}

	inline const FVulkanComputeLayout& GetComputeLayout() const
	{
		return *(FVulkanComputeLayout*)Layout;
	}

protected:
	FVulkanComputeShader*	ComputeShader;

	friend class FVulkanPipelineStateCacheManager;
};

using TGfxPipelineEntrySharedPtr = TSharedPtr<FVulkanPipelineStateCacheManager::FGfxPipelineEntry, ESPMode::ThreadSafe>;

class FVulkanGfxPipeline : public FVulkanPipeline, public FRHIResource
{
public:
#if VULKAN_ENABLE_LRU_CACHE
	FVulkanGfxPipeline(FVulkanDevice* InDevice, TGfxPipelineEntrySharedPtr InGfxPipelineEntry = {});
#else
	FVulkanGfxPipeline(FVulkanDevice* InDevice);
#endif

	inline void Bind(VkCommandBuffer CmdBuffer)
	{
#if VULKAN_ENABLE_LRU_CACHE
		RecentFrame = GFrameNumberRenderThread;
#endif
		VulkanRHI::vkCmdBindPipeline(CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);
	}

	inline bool IsRuntimeInitialized() const
	{
		return bRuntimeObjectsValid;
	}

	inline const FVulkanVertexInputStateInfo& GetVertexInputState() const
	{
		check(bRuntimeObjectsValid);
		return VertexInputState;
	}

	void CreateRuntimeObjects(const FGraphicsPipelineStateInitializer& InPSOInitializer);

	inline const FVulkanGfxLayout& GetGfxLayout() const
	{
		return *(FVulkanGfxLayout*)Layout;
	}

private:
#if VULKAN_ENABLE_LRU_CACHE
	TGfxPipelineEntrySharedPtr GfxPipelineEntry;
	uint32 PipelineCacheSize;
	uint32 RecentFrame;
	// ID to LRU (if used) allows quick access when updating LRU status.
	FSetElementId LRUNode;
	bool bTrackedByLRU;
	friend class FVulkanPipelineStateCacheManager;
#endif
	FVulkanVertexInputStateInfo VertexInputState;
	bool						bRuntimeObjectsValid;
};

class FVulkanRHIGraphicsPipelineState : public FRHIGraphicsPipelineState
{
public:
	FVulkanRHIGraphicsPipelineState(const FBoundShaderStateInput& InBSI, FVulkanGfxPipeline* InPipeline, EPrimitiveType InPrimitiveType)
		: Pipeline(InPipeline)
		, BSI(InBSI)
		, PrimitiveType(InPrimitiveType)
	{
		bHasInputAttachments = InPipeline->GetGfxLayout().GetDescriptorSetsLayout().HasInputAttachments();
	}

	~FVulkanRHIGraphicsPipelineState();

	inline void Bind(VkCommandBuffer CmdBuffer)
	{
		Pipeline->Bind(CmdBuffer);
	}

	inline const FVulkanShader* GetShader(EShaderFrequency Frequency) const
	{
		FVulkanShader* Shader = nullptr;

		switch (Frequency)
		{
		case SF_Vertex:   Shader = ResourceCast(BSI.VertexShaderRHI); break;
		case SF_Hull:     Shader = ResourceCast(BSI.HullShaderRHI); break;
		case SF_Domain:   Shader = ResourceCast(BSI.DomainShaderRHI); break;
		case SF_Pixel:    Shader = ResourceCast(BSI.PixelShaderRHI); break;
		case SF_Geometry: Shader = ResourceCast(BSI.GeometryShaderRHI); break;
		default:
			check(0);
			break;
		}

		return Shader;
	}

	TRefCountPtr<FVulkanGfxPipeline>	Pipeline;
	FBoundShaderStateInput				BSI;
	TEnumAsByte<EPrimitiveType>			PrimitiveType;
	bool								bHasInputAttachments;
};

template<>
struct TVulkanResourceTraits<class FRHIComputePipelineState>
{
	typedef class FVulkanComputePipeline TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHIGraphicsPipelineState>
{
	typedef FVulkanRHIGraphicsPipelineState TConcreteType;
};
