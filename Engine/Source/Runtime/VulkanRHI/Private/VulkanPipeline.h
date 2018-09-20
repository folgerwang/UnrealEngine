// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPipeline.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanResources.h"
#include "VulkanShaderResources.h"
#include "VulkanDescriptorSets.h"

#if VULKAN_ENABLE_LRU_CACHE
#include "PsoLruCache.h"
extern TAutoConsoleVariable<int32> CVarLRUMaxPipelineSize;
extern TAutoConsoleVariable<int32> CVarEnableLRU;
#endif

class FVulkanDevice;

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
	inline void DeleteVkPipeline()
	{
		Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::Pipeline, Pipeline);
		Pipeline = VK_NULL_HANDLE;
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

class FVulkanGfxPipeline : public FVulkanPipeline, public FRHIResource
{
public:
#if VULKAN_ENABLE_LRU_CACHE
	FVulkanGfxPipeline(FVulkanDevice* InDevice, uint32 InGfxEntryHash, uint32 InShaderHash);
#else
	FVulkanGfxPipeline(FVulkanDevice* InDevice);
#endif

	inline void Bind(VkCommandBuffer CmdBuffer)
	{
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

#if VULKAN_ENABLE_LRU_CACHE
	uint32 GfxEntryHash;
	uint32 PipelineCacheSize;
	uint32 ShaderHash;
	// ID to LRU (if used) allows quick access when updating LRU status.
	FSetElementId LRUNode;
#endif
private:
	FVulkanVertexInputStateInfo VertexInputState;
	bool						bRuntimeObjectsValid;
};

class FVulkanRHIGraphicsPipelineState : public FRHIGraphicsPipelineState
{
public:
	FVulkanRHIGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer, FVulkanGfxPipeline* InPipeline)
		: Pipeline(InPipeline)
		, PipelineStateInitializer(Initializer)
	{
		bHasInputAttachments = InPipeline->GetGfxLayout().GetDescriptorSetsLayout().HasInputAttachments();
	}

	~FVulkanRHIGraphicsPipelineState();

	inline void Bind(VkCommandBuffer CmdBuffer)
	{
		Pipeline->Bind(CmdBuffer);
	}

	TRefCountPtr<FVulkanGfxPipeline>	Pipeline;
	bool								bHasInputAttachments;

	FGraphicsPipelineStateInitializer	PipelineStateInitializer;
};

template <typename T>
static inline VkShaderModule GetDefaultShaderModule(T* ShaderType)
{
	auto* VulkanShader = ResourceCast(ShaderType);
	return VulkanShader ? VulkanShader->GetDefaultShaderModule() : VK_NULL_HANDLE;
}

class FVulkanPipelineStateCacheManager
{
public:
	struct FPSOHashable
	{
		FVulkanVertexDeclaration*		VertexDeclaration;
		VkShaderModule					Shaders[ShaderStage::NumStages];
#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
		VkSampler						ImmutableSamplers[MaxImmutableSamplers];
#endif
		FBlendStateRHIParamRef			BlendState;
		FRasterizerStateRHIParamRef		RasterizerState;
		FDepthStencilStateRHIParamRef	DepthStencilState;
		FExclusiveDepthStencil			DepthStencilAccess;
		EPixelFormat					DepthStencilTargetFormat;
		uint8							bDepthBounds;
		TEnumAsByte<EPrimitiveType>		PrimitiveType;
		uint8							RenderTargetsEnabled;
		uint8							NumSamples;
	};

	FVulkanRHIGraphicsPipelineState* FindInRuntimeCache(const FGraphicsPipelineStateInitializer& Initializer, uint32& OutHash);

	void DestroyPipeline(FVulkanGfxPipeline* Pipeline);

	// Array of potential cache locations; first entries have highest priority. Only one cache file is loaded. If unsuccessful, tries next entry in the array.
	void InitAndLoad(const TArray<FString>& CacheFilenames);
	void Save(const FString& CacheFilename);

	FVulkanPipelineStateCacheManager(FVulkanDevice* InParent);
	~FVulkanPipelineStateCacheManager();

	void RebuildCache();

#if VULKAN_ENABLE_GENERIC_PIPELINE_CACHE_FILE
	enum
	{
		// Bump every time serialization changes
		VERSION = 23,
	};
#endif

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

	// Actual information required to recreate a pipeline when saving/loading from disk
	struct FGfxPipelineEntry
	{
		uint32 GetEntryHash(uint32 Crc = 0);
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

		TArray<uint32>* ShaderMicrocodes[ShaderStage::NumStages];
		FSHAHash ShaderHashes[ShaderStage::NumStages];
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
			FMemory::Memzero(ShaderMicrocodes);
		}

		~FGfxPipelineEntry();

		// Vulkan Runtime Data/Objects
		VkShaderModule ShaderModules[ShaderStage::NumStages];
		const FVulkanRenderPass* RenderPass;
		FVulkanGfxLayout* Layout;

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

			for (int32 Index = 0; Index < ARRAY_COUNT(In.ShaderHashes); ++Index)
			{
				if (ShaderHashes[Index] != In.ShaderHashes[Index])
				{
					return false;
				}
			}

			for (int32 Index = 0; Index < ARRAY_COUNT(In.ShaderMicrocodes); ++Index)
			{
				if (ShaderMicrocodes[Index] != In.ShaderMicrocodes[Index])
				{
					return false;
				}
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

	struct FComputePipelineEntry
	{
		uint32 EntryHash;
		void CalculateEntryHash();

		bool bLoaded;

		TArray<uint32>* ShaderMicrocode;
		FSHAHash ShaderHash;
		TArray<TArray<FDescriptorSetLayoutBinding>> DescriptorSetLayoutBindings;

		// Runtime objects
		VkShaderModule ShaderModule;
		FVulkanComputeLayout* Layout;

		FComputePipelineEntry()
			: EntryHash(0)
			, bLoaded(false)
			, ShaderMicrocode(nullptr)
			, ShaderModule(VK_NULL_HANDLE)
			, Layout(nullptr)
		{}

		~FComputePipelineEntry();
	};
	FVulkanComputePipeline* GetOrCreateComputePipeline(FVulkanComputeShader* ComputeShader);

#if VULKAN_ENABLE_LRU_CACHE
	void CreateGfxPipelineFromEntry(const FGfxPipelineEntry* GfxEntry, FVulkanGfxPipeline* Pipeline);
	TMap<uint32, FGfxPipelineEntry*> GfxPipelineEntries;

	TMap<uint32, FPipelineSize*> PipelineSizeList;	// key: Shader hash (FShaderHash), value: pipeline size

	class FVKPipelineLRU
	{
		class FEvictedVkPipeline
		{
			FVulkanGfxPipeline* GfxPipeline;
			FVulkanDevice* Device;
			FGfxPipelineEntry* GfxEntry;

		public:
			FEvictedVkPipeline(FVulkanDevice* InDevice, FVulkanGfxPipeline* InPipeline, FGfxPipelineEntry* InGfxEntry)
				: Device(InDevice), GfxPipeline(InPipeline), GfxEntry(InGfxEntry)
			{
				check(InGfxEntry);

				GfxPipeline->DeleteVkPipeline();
			}

			void RestoreVkPipeline();

			FVulkanGfxPipeline* GetGfxPipeline()
			{
				return GfxPipeline;
			}
		};
		typedef TMap<uint32, FEvictedVkPipeline> FVkEvictedPipelineMap;	//key: GfxEntry hash
		typedef TPsoLruCache<uint32, FVulkanGfxPipeline*> FVulkanPipelineLRUCache;	//key: GfxEntryHash

		const int LRUCapacity = 2048;
		int32 LRUUsedPipelineSize;

		FVulkanGfxPipeline* FindEvictedAndUpdateLRU(FVulkanDevice* InDevice, uint32 EntryHash, const TMap<uint32, FGfxPipelineEntry*>& InGfxEntriesMap)
		{
			FScopeLock Lock(&EvictCS);

			FEvictedVkPipeline* FoundEvicted = EvictedPipelines.Find(EntryHash);
			if (FoundEvicted)
			{
				FoundEvicted->RestoreVkPipeline();
				FVulkanGfxPipeline* GfxPipeline = FoundEvicted->GetGfxPipeline();

				Add(InDevice, GfxPipeline, InGfxEntriesMap);

				EvictedPipelines.Remove(EntryHash);

				return GfxPipeline;
			}

			return nullptr;
		}

		void EvictFromLRU(FVulkanDevice* InDevice, uint32 InEntryHash, const TMap<uint32, FGfxPipelineEntry*>& InGfxEntriesMap)
		{
			FVulkanGfxPipeline* LeastRecentPipeline = nullptr;
			{
				FScopeLock Lock(&LRUCS);
				LeastRecentPipeline = LRU.RemoveLeastRecent();
				check(LeastRecentPipeline);
				LeastRecentPipeline->LRUNode = FSetElementId();

				LRUUsedPipelineSize -= LeastRecentPipeline->PipelineCacheSize;

				uint32 LeastGfxEntryHash = LeastRecentPipeline->GfxEntryHash;
				check(!(LeastGfxEntryHash == InEntryHash));
				check(!EvictedPipelines.Contains(LeastGfxEntryHash));
				FGfxPipelineEntry*const* Found = InGfxEntriesMap.Find(LeastGfxEntryHash);
				check(Found);
				FEvictedVkPipeline& test = EvictedPipelines.Emplace(LeastGfxEntryHash, FEvictedVkPipeline(InDevice, LeastRecentPipeline, *Found));
			}
		}

	public:
		FVKPipelineLRU() : LRUUsedPipelineSize(0), LRU(LRUCapacity)
		{
			bUseLRU = (int32)CVarEnableLRU.GetValueOnAnyThread() == 1;
		}

		void Add(FVulkanDevice* InDevice, FVulkanGfxPipeline* GfxPipeline, const TMap<uint32, FGfxPipelineEntry*>& InGfxEntriesMap)
		{
			if (!bUseLRU)
			{
				return;
			}

			uint32 EntryHash = GfxPipeline->GfxEntryHash;
			check(!LRU.Contains(EntryHash));

			while (LRUUsedPipelineSize + GfxPipeline->PipelineCacheSize > (uint32)CVarLRUMaxPipelineSize.GetValueOnAnyThread() || LRU.Num() == LRU.Max())
			{
				EvictFromLRU(InDevice, GfxPipeline->GfxEntryHash, InGfxEntriesMap);
			}


			{
				FScopeLock Lock(&LRUCS);
				GfxPipeline->LRUNode = LRU.Add(EntryHash, GfxPipeline);
			}
			LRUUsedPipelineSize += GfxPipeline->PipelineCacheSize;
		}

		FVulkanGfxPipeline* Find(FVulkanDevice* InDevice, uint32 EntryHash, const TMap<uint32, FGfxPipelineEntry*>& InGfxEntriesMap)
		{
			check(bUseLRU);

			FVulkanGfxPipeline*const* Found = LRU.FindAndTouch(EntryHash);
			if (Found)
			{
				check((*Found)->LRUNode.IsValidId());
				return *Found;
			}

			return FindEvictedAndUpdateLRU(InDevice, EntryHash, InGfxEntriesMap);
		}

		FORCEINLINE_DEBUGGABLE void Touch(FVulkanDevice* InDevice, FVulkanGfxPipeline* GfxPipeline, const TMap<uint32, FGfxPipelineEntry*>& InGfxEntriesMap)
		{
			if (!bUseLRU)
			{
				return;
			}

			if (GfxPipeline->LRUNode.IsValidId())
			{
				FScopeLock Lock(&LRUCS);
				LRU.MarkAsRecent(GfxPipeline->LRUNode);
			}
			else
			{
				FindEvictedAndUpdateLRU(InDevice, GfxPipeline->GfxEntryHash, InGfxEntriesMap);
			}
		}

		void Empty()
		{
			EvictedPipelines.Empty();
			LRU.Empty(LRUCapacity);
		}

	private:
		bool bUseLRU;
		FVulkanPipelineLRUCache LRU;
		FVkEvictedPipelineMap EvictedPipelines;
		FCriticalSection LRUCS;
		FCriticalSection EvictCS;
	};

	FVKPipelineLRU PipelineLRU;

#endif
private:
	FVulkanDevice* Device;

	// Key is a hash of the PSO, which is based off shader pointers
	TMap<uint32, FVulkanRHIGraphicsPipelineState*> InitializerToPipelineMap;
	FCriticalSection InitializerToPipelineMapCS;

	TMap<FVulkanComputeShader*, FVulkanComputePipeline*> ComputeShaderToPipelineMap;
	TMap<uint32, FVulkanComputePipeline*> ComputeEntryHashToPipelineMap;

	FCriticalSection GfxPipelineEntriesCS;
#if !VULKAN_ENABLE_LRU_CACHE
	TMap<uint32, FGfxPipelineEntry*> GfxPipelineEntries;
#endif
	FCriticalSection CreateComputePipelineCS;
	TMap<uint32, FComputePipelineEntry*> ComputePipelineEntries;

	VkPipelineCache PipelineCache;

	FShaderUCodeCache ShaderCache;

#if VULKAN_ENABLE_LRU_CACHE
	FVulkanRHIGraphicsPipelineState* CreateAndAdd(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 PSOInitializerHash, FGfxPipelineEntry* GfxEntry, uint32 ShaderHash);
#else
	FVulkanRHIGraphicsPipelineState* CreateAndAdd(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 PSOInitializerHash, FGfxPipelineEntry* GfxEntry);
	void CreateGfxPipelineFromEntry(const FGfxPipelineEntry* GfxEntry, FVulkanGfxPipeline* Pipeline);
#endif

#if VULKAN_ENABLE_LRU_CACHE
	FGfxPipelineEntry* CreateGfxEntry(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 PSOHash);
#else
	FGfxPipelineEntry* CreateGfxEntry(const FGraphicsPipelineStateInitializer& PSOInitializer);
#endif
	void CreatGfxEntryRuntimeObjects(FGfxPipelineEntry* GfxEntry);
	void Load(const TArray<FString>& CacheFilenames);
	void DestroyCache();

#if VULKAN_ENABLE_LRU_CACHE
	FVulkanGfxLayout* GetOrGenerateGfxLayout(const FGraphicsPipelineStateInitializer& PSOInitializer, FVulkanShader** OutShaders, FVulkanVertexInputStateInfo& OutVertexInputState, uint32 PSOHash);
#else
	FVulkanGfxLayout* GetOrGenerateGfxLayout(const FGraphicsPipelineStateInitializer& PSOInitializer, FVulkanShader** OutShaders, FVulkanVertexInputStateInfo& OutVertexInputState);
#endif


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

	typedef TMap<uint32, FVulkanGfxPipeline*> FHashToGfxPipelinesMap;
	TMap<FShaderHashes, FHashToGfxPipelinesMap> ShaderHashToGfxPipelineMap;

	//@@
	TMap<uint32, FVulkanLayout*> LayoutMapGfx;
	TMap<FVulkanDescriptorSetsLayoutInfo, FVulkanLayout*> LayoutMap;
	FCriticalSection LayoutMapCS;

	FVulkanRHIGraphicsPipelineState* FindInLoadedLibrary(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 PSOInitializerHash, const FShaderHashes& ShaderHashes, FGfxPipelineEntry*& OutGfxEntry, FVulkanPipelineStateCacheManager::FHashToGfxPipelinesMap*& OutHashToGfxPipelinesMap);

	friend class FVulkanDynamicRHI;

	//@@
	FVulkanLayout* FindOrAddLayoutForGfx(uint32 PSOHash, const FVulkanDescriptorSetsLayoutInfo& DescriptorSetLayoutInfo, bool bGfxLayout);
	FVulkanLayout* FindOrAddLayout(const FVulkanDescriptorSetsLayoutInfo& DescriptorSetLayoutInfo, bool bGfxLayout);

	FComputePipelineEntry* CreateComputeEntry(FVulkanComputeShader* ComputeShader);
	FVulkanComputePipeline* CreateComputePipelineFromEntry(const FComputePipelineEntry* ComputeEntry);
	void CreateComputeEntryRuntimeObjects(FComputePipelineEntry* GfxEntry);

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

#if VULKAN_ENABLE_GENERIC_PIPELINE_CACHE_FILE
	struct FVulkanPipelineStateCacheFile
	{
		struct FFileHeader
		{
			int32 Version = -1;
			int32 SizeOfGfxEntry = -1;
			int32 SizeOfComputeEntry = -1;
			int32 UncompressedSize = 0; // 0 == file is uncompressed
		} Header;

		FShaderUCodeCache::TDataMap* ShaderCache = nullptr;

		TArray<FGfxPipelineEntry*> GfxPipelineEntries;
		TArray<FComputePipelineEntry*> ComputePipelineEntries;

		void Save(FArchive& Ar);
		bool Load(FArchive& Ar, const TCHAR* Filename);

		static const ECompressionFlags CompressionFlags = (ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasSpeed);
	};
#endif

	static bool BinaryCacheMatches(FVulkanDevice* InDevice, const TArray<uint8>& DeviceCache);
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