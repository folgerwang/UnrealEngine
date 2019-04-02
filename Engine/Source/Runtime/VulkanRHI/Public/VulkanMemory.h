// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanMemory.h: Vulkan Memory RHI definitions.
=============================================================================*/

#pragma once 

// Enable to store file & line of every mem & resource allocation
#define VULKAN_MEMORY_TRACK_FILE_LINE	0

// Enable to save the callstack for every mem and resource allocation
#define VULKAN_MEMORY_TRACK_CALLSTACK	0


class FVulkanQueue;
class FVulkanCmdBuffer;

enum class EDelayAcquireImageType
{
	None,			// acquire next image on frame start
	DelayAcquire,	// acquire next image just before presenting, rendering is done to intermediate image which is copied to real backbuffer
	PreAcquire,		// acquire next image immediately after presenting current
};

extern EDelayAcquireImageType GVulkanDelayAcquireImage;

namespace VulkanRHI
{
	class FFenceManager;

	extern int32 GVulkanUseBufferBinning;

	enum
	{
#if PLATFORM_ANDROID || PLATFORM_LUMIN || PLATFORM_LUMINGL4
		NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS = 3,
#else
		NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS = 10,
#endif
	};

	// Custom ref counting
	class FRefCount
	{
	public:
		FRefCount() {}
		virtual ~FRefCount()
		{
			check(NumRefs.GetValue() == 0);
		}

		inline uint32 AddRef()
		{
			int32 NewValue = NumRefs.Increment();
			check(NewValue > 0);
			return (uint32)NewValue;
		}

		inline uint32 Release()
		{
			int32 NewValue = NumRefs.Decrement();
			if (NewValue == 0)
			{
				delete this;
			}
			check(NewValue >= 0);
			return (uint32)NewValue;
		}

		inline uint32 GetRefCount() const
		{
			int32 Value = NumRefs.GetValue();
			check(Value >= 0);
			return (uint32)Value;
		}

	private:
		FThreadSafeCounter NumRefs;
	};

	class FDeviceChild
	{
	public:
		FDeviceChild(FVulkanDevice* InDevice = nullptr) :
			Device(InDevice)
		{
		}

		inline FVulkanDevice* GetParent() const
		{
			// Has to have one if we are asking for it...
			check(Device);
			return Device;
		}

		inline void SetParent(FVulkanDevice* InDevice)
		{
			check(!Device);
			Device = InDevice;
		}

	protected:
		FVulkanDevice* Device;
	};

	// An Allocation off a Device Heap. Lowest level of allocations and bounded by VkPhysicalDeviceLimits::maxMemoryAllocationCount.
	class FDeviceMemoryAllocation
	{
	public:
		FDeviceMemoryAllocation()
			: Size(0)
			, DeviceHandle(VK_NULL_HANDLE)
			, Handle(VK_NULL_HANDLE)
			, MappedPointer(nullptr)
			, MemoryTypeIndex(0)
			, bCanBeMapped(0)
			, bIsCoherent(0)
			, bIsCached(0)
			, bFreedBySystem(false)
#if VULKAN_MEMORY_TRACK_FILE_LINE
			, File(nullptr)
			, Line(0)
			, UID(0)
#endif
		{
		}

		void* Map(VkDeviceSize Size, VkDeviceSize Offset);
		void Unmap();

		inline bool CanBeMapped() const
		{
			return bCanBeMapped != 0;
		}

		inline bool IsMapped() const
		{
			return !!MappedPointer;
		}

		inline void* GetMappedPointer()
		{
			check(IsMapped());
			return MappedPointer;
		}

		inline bool IsCoherent() const
		{
			return bIsCoherent != 0;
		}

		void FlushMappedMemory(VkDeviceSize InOffset, VkDeviceSize InSize);
		void InvalidateMappedMemory(VkDeviceSize InOffset, VkDeviceSize InSize);

		inline VkDeviceMemory GetHandle() const
		{
			return Handle;
		}

		inline VkDeviceSize GetSize() const
		{
			return Size;
		}

		inline uint32 GetMemoryTypeIndex() const
		{
			return MemoryTypeIndex;
		}

	protected:
		VkDeviceSize Size;
		VkDevice DeviceHandle;
		VkDeviceMemory Handle;
		void* MappedPointer;
		uint32 MemoryTypeIndex : 8;
		uint32 bCanBeMapped : 1;
		uint32 bIsCoherent : 1;
		uint32 bIsCached : 1;
		uint32 bFreedBySystem : 1;
		uint32 : 0;

#if VULKAN_MEMORY_TRACK_FILE_LINE
		const char* File;
		uint32 Line;
		uint32 UID;
#endif
#if VULKAN_MEMORY_TRACK_CALLSTACK
		FString Callstack;
#endif
		// Only owner can delete!
		~FDeviceMemoryAllocation();

		friend class FDeviceMemoryManager;
	};

	// Manager of Device Heap Allocations. Calling Alloc/Free is expensive!
	class FDeviceMemoryManager
	{
	public:
		FDeviceMemoryManager();
		~FDeviceMemoryManager();

		void Init(FVulkanDevice* InDevice);

		void Deinit();

		inline bool HasUnifiedMemory() const
		{
			return bHasUnifiedMemory;
		}

		inline uint32 GetNumMemoryTypes() const
		{
			return MemoryProperties.memoryTypeCount;
		}

		bool SupportsMemoryType(VkMemoryPropertyFlags Properties) const;

		//#todo-rco: Might need to revisit based on https://gitlab.khronos.org/vulkan/vulkan/merge_requests/1165
		inline VkResult GetMemoryTypeFromProperties(uint32 TypeBits, VkMemoryPropertyFlags Properties, uint32* OutTypeIndex)
		{
			// Search memtypes to find first index with those properties
			for (uint32 i = 0; i < MemoryProperties.memoryTypeCount && TypeBits; i++)
			{
				if ((TypeBits & 1) == 1)
				{
					// Type is available, does it match user properties?
					if ((MemoryProperties.memoryTypes[i].propertyFlags & Properties) == Properties)
					{
						*OutTypeIndex = i;
						return VK_SUCCESS;
					}
				}
				TypeBits >>= 1;
			}

			// No memory types matched, return failure
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}

		inline VkResult GetMemoryTypeFromPropertiesExcluding(uint32 TypeBits, VkMemoryPropertyFlags Properties, uint32 ExcludeTypeIndex, uint32* OutTypeIndex)
		{
			// Search memtypes to find first index with those properties
			for (uint32 i = 0; i < MemoryProperties.memoryTypeCount && TypeBits; i++)
			{
				if ((TypeBits & 1) == 1)
				{
					// Type is available, does it match user properties?
					if ((MemoryProperties.memoryTypes[i].propertyFlags & Properties) == Properties && ExcludeTypeIndex != i)
					{
						*OutTypeIndex = i;
						return VK_SUCCESS;
					}
				}
				TypeBits >>= 1;
			}

			// No memory types matched, return failure
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}

		inline const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const
		{
			return MemoryProperties;
		}


		// bCanFail means an allocation failing is not a fatal error, just returns nullptr
		FDeviceMemoryAllocation* Alloc(bool bCanFail, VkDeviceSize AllocationSize, uint32 MemoryTypeIndex, void* DedicatedAllocateInfo, const char* File, uint32 Line);

		inline FDeviceMemoryAllocation* Alloc(bool bCanFail, VkDeviceSize AllocationSize, uint32 MemoryTypeBits, VkMemoryPropertyFlags MemoryPropertyFlags, void* DedicatedAllocateInfo, const char* File, uint32 Line)
		{
			uint32 MemoryTypeIndex = ~0;
			VERIFYVULKANRESULT(this->GetMemoryTypeFromProperties(MemoryTypeBits, MemoryPropertyFlags, &MemoryTypeIndex));
			return Alloc(bCanFail, AllocationSize, MemoryTypeIndex, DedicatedAllocateInfo, File, Line);
		}

		// Sets the Allocation to nullptr
		void Free(FDeviceMemoryAllocation*& Allocation);

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		void DumpMemory();
#endif

		uint64 GetTotalMemory(bool bGPU) const;

	protected:
		VkPhysicalDeviceMemoryProperties MemoryProperties;
		VkDevice DeviceHandle;
		bool bHasUnifiedMemory;
		FVulkanDevice* Device;
		uint32 NumAllocations;
		uint32 PeakNumAllocations;

		struct FHeapInfo
		{
			VkDeviceSize TotalSize;
			VkDeviceSize UsedSize;
			VkDeviceSize PeakSize;
			TArray<FDeviceMemoryAllocation*> Allocations;

			FHeapInfo() :
				TotalSize(0),
				UsedSize(0),
				PeakSize(0)
			{
			}
		};

		TArray<FHeapInfo> HeapInfos;
		void SetupAndPrintMemInfo();
	};

	class FOldResourceHeap;
	class FOldResourceHeapPage;
	class FResourceHeapManager;

	// A sub allocation for a specific memory type
	class FOldResourceAllocation : public FRefCount
	{
	public:
		FOldResourceAllocation(FOldResourceHeapPage* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation,
			uint32 InRequestedSize, uint32 InAlignedOffset,
			uint32 InAllocationSize, uint32 InAllocationOffset, const char* InFile, uint32 InLine);

		virtual ~FOldResourceAllocation();

		inline uint32 GetSize() const
		{
			return RequestedSize;
		}

		inline uint32 GetAllocationSize()
		{
			return AllocationSize;
		}

		inline uint32 GetOffset() const
		{
			return AlignedOffset;
		}

		inline VkDeviceMemory GetHandle() const
		{
			return DeviceMemoryAllocation->GetHandle();
		}

		inline void* GetMappedPointer()
		{
			check(DeviceMemoryAllocation->CanBeMapped());
			check(DeviceMemoryAllocation->IsMapped());
			return (uint8*)DeviceMemoryAllocation->GetMappedPointer() + AlignedOffset;
		}

		inline uint32 GetMemoryTypeIndex() const
		{
			return DeviceMemoryAllocation->GetMemoryTypeIndex();
		}

		inline void FlushMappedMemory()
		{
			DeviceMemoryAllocation->FlushMappedMemory(AllocationOffset, AllocationSize);
		}

		inline void InvalidateMappedMemory()
		{
			DeviceMemoryAllocation->InvalidateMappedMemory(AllocationOffset, AllocationSize);
		}

		void BindBuffer(FVulkanDevice* Device, VkBuffer Buffer);
		void BindImage(FVulkanDevice* Device, VkImage Image);

#if VULKAN_USE_LLM
		void SetLLMTrackerID(uint64 InTrackerID) { LLMTrackerID = InTrackerID; }
		uint64 GetLLMTrackerID() { return LLMTrackerID; }
#endif

	private:
		FOldResourceHeapPage* Owner;

		// Total size of allocation
		uint32 AllocationSize;

		// Original offset of allocation
		uint32 AllocationOffset;

		// Requested size
		uint32 RequestedSize;

		// Requested alignment offset
		uint32 AlignedOffset;

		FDeviceMemoryAllocation* DeviceMemoryAllocation;

#if VULKAN_MEMORY_TRACK_FILE_LINE
		const char* File;
		uint32 Line;
#endif
#if VULKAN_MEMORY_TRACK_CALLSTACK
		FString Callstack;
#endif

#if VULKAN_USE_LLM
		uint64 LLMTrackerID;
#endif

		friend class FOldResourceHeapPage;
	};

	struct FRange
	{
		uint32 Offset;
		uint32 Size;

		inline bool operator<(const FRange& In) const
		{
			return Offset < In.Offset;
		}

		static void JoinConsecutiveRanges(TArray<FRange>& Ranges);
	};

	// One device allocation that is shared amongst different resources
	class FOldResourceHeapPage
	{
	public:
		FOldResourceHeapPage(FOldResourceHeap* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation, uint32 InID);
		~FOldResourceHeapPage();

		FOldResourceAllocation* TryAllocate(uint32 Size, uint32 Alignment, const char* File, uint32 Line);

		FOldResourceAllocation* Allocate(uint32 Size, uint32 Alignment, const char* File, uint32 Line)
		{
			FOldResourceAllocation* ResourceAllocation = TryAllocate(Size, Alignment, File, Line);
			check(ResourceAllocation);
			return ResourceAllocation;
		}

		void ReleaseAllocation(FOldResourceAllocation* Allocation);

		inline FOldResourceHeap* GetOwner()
		{
			return Owner;
		}

		inline uint32 GetID() const
		{
			return ID;
		}

	protected:
		FOldResourceHeap* Owner;
		FDeviceMemoryAllocation* DeviceMemoryAllocation;
		TArray<FOldResourceAllocation*>  ResourceAllocations;
		uint32 MaxSize;
		uint32 UsedSize;
		int32 PeakNumAllocations;
		uint32 FrameFreed;
		uint32 ID;

		bool JoinFreeBlocks();

		TArray<FRange> FreeList;
		friend class FOldResourceHeap;
	};

	class FBufferAllocation;

	// This holds the information for a SubAllocation (a range); does NOT hold any information about what the object type is
	class FResourceSuballocation : public FRefCount
	{
	public:
		FResourceSuballocation(uint32 InRequestedSize, uint32 InAlignedOffset,
			uint32 InAllocationSize, uint32 InAllocationOffset)
			: RequestedSize(InRequestedSize)
			, AlignedOffset(InAlignedOffset)
			, AllocationSize(InAllocationSize)
			, AllocationOffset(InAllocationOffset)
		{
		}

		inline uint32 GetOffset() const
		{
			return AlignedOffset;
		}

		inline uint32 GetSize() const
		{
			return RequestedSize;
		}

#if VULKAN_USE_LLM
		void SetLLMTrackerID(uint64 InTrackerID) { LLMTrackerID = InTrackerID; }
		uint64 GetLLMTrackerID() { return LLMTrackerID; }
#endif
	protected:
		uint32 RequestedSize;
		uint32 AlignedOffset;
		uint32 AllocationSize;
		uint32 AllocationOffset;
#if VULKAN_MEMORY_TRACK_FILE_LINE
		const char* File;
		uint32 Line;
#endif
#if VULKAN_MEMORY_TRACK_CALLSTACK
		FString Callstack;
#endif
#if VULKAN_USE_LLM
		uint64 LLMTrackerID;
#endif
#if VULKAN_MEMORY_TRACK_CALLSTACK || VULKAN_MEMORY_TRACK_FILE_LINE
		friend class FSubresourceAllocator;
#endif
	};

	// Suballocation of a VkBuffer
	class FBufferSuballocation : public FResourceSuballocation
	{
	public:
		FBufferSuballocation(FBufferAllocation* InOwner, VkBuffer InHandle,
			uint32 InRequestedSize, uint32 InAlignedOffset,
			uint32 InAllocationSize, uint32 InAllocationOffset)
			: FResourceSuballocation(InRequestedSize, InAlignedOffset, InAllocationSize, InAllocationOffset)
			, Owner(InOwner)
			, Handle(InHandle)
		{
		}

		virtual ~FBufferSuballocation();

		inline VkBuffer GetHandle() const
		{
			return Handle;
		}

		inline FBufferAllocation* GetBufferAllocation()
		{
			return Owner;
		}

		// Returns the pointer to the mapped data for this SubAllocation, not the full buffer!
		void* GetMappedPointer();

	protected:
		friend class FBufferAllocation;
		FBufferAllocation* Owner;
		VkBuffer Handle;
	};

	// Generically mantains/manages sub-allocations; doesn't know what the object type is
	class FSubresourceAllocator
	{
	public:
		FSubresourceAllocator(FResourceHeapManager* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation,
			uint32 InMemoryTypeIndex, VkMemoryPropertyFlags InMemoryPropertyFlags,
			uint32 InAlignment)
			: Owner(InOwner)
			, MemoryTypeIndex(InMemoryTypeIndex)
			, MemoryPropertyFlags(InMemoryPropertyFlags)
			, MemoryAllocation(InDeviceMemoryAllocation)
			, Alignment(InAlignment)
			, FrameFreed(0)
			, UsedSize(0)
		{
			MaxSize = InDeviceMemoryAllocation->GetSize();
			FRange FullRange;
			FullRange.Offset = 0;
			FullRange.Size = MaxSize;
			FreeList.Add(FullRange);
		}

		virtual ~FSubresourceAllocator() {}

		virtual FResourceSuballocation* CreateSubAllocation(uint32 Size, uint32 AlignedOffset, uint32 AllocatedSize, uint32 AllocatedOffset) = 0;
		virtual void Destroy(FVulkanDevice* Device) = 0;

		FResourceSuballocation* TryAllocateNoLocking(uint32 InSize, uint32 InAlignment, const char* File, uint32 Line);

		inline FResourceSuballocation* TryAllocateLocking(uint32 InSize, uint32 InAlignment, const char* File, uint32 Line)
		{
			FScopeLock ScopeLock(&CS);
			return TryAllocateNoLocking(InSize, InAlignment, File, Line);
		}

		inline uint32 GetAlignment() const
		{
			return Alignment;
		}

		inline void* GetMappedPointer()
		{
			return MemoryAllocation->GetMappedPointer();
		}

	protected:
		FResourceHeapManager* Owner;
		uint32 MemoryTypeIndex;
		VkMemoryPropertyFlags MemoryPropertyFlags;
		FDeviceMemoryAllocation* MemoryAllocation;
		uint32 MaxSize;
		uint32 Alignment;
		uint32 FrameFreed;
		int64 UsedSize;
		static FCriticalSection CS;

		// List of free ranges
		TArray<FRange> FreeList;

		// Active sub-allocations
		TArray<FResourceSuballocation*> Suballocations;
		bool JoinFreeBlocks();
	};

	// Manages/maintains sub-allocations of a VkBuffer; assumes it was created elsewhere, but it does destroy it
	class FBufferAllocation : public FSubresourceAllocator
	{
	public:
		FBufferAllocation(FResourceHeapManager* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation,
			uint32 InMemoryTypeIndex, VkMemoryPropertyFlags InMemoryPropertyFlags,
			uint32 InAlignment, VkBuffer InBuffer, uint32 InBufferId, VkBufferUsageFlags InBufferUsageFlags, int32 InPoolSizeIndex)
			: FSubresourceAllocator(InOwner, InDeviceMemoryAllocation, InMemoryTypeIndex, InMemoryPropertyFlags, InAlignment)
			, BufferUsageFlags(InBufferUsageFlags)
			, Buffer(InBuffer)
			, BufferId(InBufferId)
			, PoolSizeIndex(InPoolSizeIndex)
		{
		}

		virtual ~FBufferAllocation()
		{
			check(Buffer == VK_NULL_HANDLE);
		}

		virtual FResourceSuballocation* CreateSubAllocation(uint32 Size, uint32 AlignedOffset, uint32 AllocatedSize, uint32 AllocatedOffset) override
		{
			return new FBufferSuballocation(this, Buffer, Size, AlignedOffset, AllocatedSize, AllocatedOffset);// , File, Line);
		}
		virtual void Destroy(FVulkanDevice* Device) override;

		void Release(FBufferSuballocation* Suballocation);

		inline VkBuffer GetHandle() const
		{
			return Buffer;
		}

		inline uint32 GetHandleId() const
		{
			return BufferId;
		}

	protected:
		VkBufferUsageFlags BufferUsageFlags;
		VkBuffer Buffer;
		uint32 BufferId;
		int32 PoolSizeIndex;
		friend class FResourceHeapManager;
	};

	// A set of Device Allocations (Heap Pages) for a specific memory type. This handles pooling allocations inside memory pages to avoid
	// doing allocations directly off the device's heaps
	class FOldResourceHeap
	{
	public:
		enum class EType
		{
			Image,
			Buffer,
		};
		FOldResourceHeap(FResourceHeapManager* InOwner, uint32 InMemoryTypeIndex, uint32 InPageSize);
		~FOldResourceHeap();

		void FreePage(FOldResourceHeapPage* InPage);

		void ReleaseFreedPages(bool bImmediately);

		inline FResourceHeapManager* GetOwner()
		{
			return Owner;
		}

		inline bool IsHostCachedSupported() const
		{
			return bIsHostCachedSupported;
		}

		inline bool IsLazilyAllocatedSupported() const
		{
			return bIsLazilyAllocatedSupported;
		}
		
		inline uint32 GetMemoryTypeIndex() const
		{
			return MemoryTypeIndex;
		}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		void DumpMemory();
#endif

	protected:
		FResourceHeapManager* Owner;
		uint32 MemoryTypeIndex;

		bool bIsHostCachedSupported;
		bool bIsLazilyAllocatedSupported;

#if VULKAN_FREEPAGE_FOR_TYPE
		uint32 DefaultPageSizeForImage;
		uint32 DefaultPageSizeForBuffer;
#endif
		uint32 DefaultPageSize;

		uint32 PeakPageSize;
		uint64 UsedMemory;
		uint32 PageIDCounter;

		TArray<FOldResourceHeapPage*> UsedBufferPages;
		TArray<FOldResourceHeapPage*> UsedImagePages;

#if VULKAN_FREEPAGE_FOR_TYPE
		TArray<FOldResourceHeapPage*> FreeBufferPages;
		TArray<FOldResourceHeapPage*> FreeImagePages;
#else
		TArray<FOldResourceHeapPage*> FreePages;
#endif

		FOldResourceAllocation* AllocateResource(EType Type, uint32 Size, uint32 Alignment, bool bMapAllocation, const char* File, uint32 Line);

#if VULKAN_SUPPORTS_DEDICATED_ALLOCATION
		TArray<FOldResourceHeapPage*> UsedDedicatedImagePages;
		TArray<FOldResourceHeapPage*> FreeDedicatedImagePages;

		FOldResourceAllocation* AllocateDedicatedImage(VkImage Image, uint32 Size, uint32 Alignment, const char* File, uint32 Line);
#endif


		friend class FResourceHeapManager;
	};

	// Manages heaps and their interactions
	class FResourceHeapManager : public FDeviceChild
	{
	public:
		FResourceHeapManager(FVulkanDevice* InDevice);
		~FResourceHeapManager();

		void Init();
		void Deinit();

		// Returns a sub-allocation, as there can be space inside a previously allocated VkBuffer to be reused; to release a sub allocation, just delete the pointer
		FBufferSuballocation* AllocateBuffer(uint32 Size, VkBufferUsageFlags BufferUsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line);

		// Release a whole allocation; this is only called from within a FBufferAllocation
		void ReleaseBuffer(FBufferAllocation* BufferAllocation);

#if VULKAN_SUPPORTS_DEDICATED_ALLOCATION
		FOldResourceAllocation* AllocateDedicatedImageMemory(VkImage Image, const VkMemoryRequirements& MemoryReqs, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line);
#endif

		inline FOldResourceAllocation* AllocateImageMemory(const VkMemoryRequirements& MemoryReqs, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line)
		{
			uint32 TypeIndex = 0;
			VERIFYVULKANRESULT(DeviceMemoryManager->GetMemoryTypeFromProperties(MemoryReqs.memoryTypeBits, MemoryPropertyFlags, &TypeIndex));
			bool bMapped = (MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			if (!ResourceTypeHeaps[TypeIndex])
			{
				UE_LOG(LogVulkanRHI, Fatal, TEXT("Missing memory type index %d, MemSize %d, MemPropTypeBits %u, MemPropertyFlags %u, %s(%d)"), TypeIndex, (uint32)MemoryReqs.size, (uint32)MemoryReqs.memoryTypeBits, (uint32)MemoryPropertyFlags, ANSI_TO_TCHAR(File), Line);
			}
			FOldResourceAllocation* Allocation = ResourceTypeHeaps[TypeIndex]->AllocateResource(FOldResourceHeap::EType::Image, MemoryReqs.size, MemoryReqs.alignment, bMapped, File, Line);
			if (!Allocation)
			{
				VERIFYVULKANRESULT(DeviceMemoryManager->GetMemoryTypeFromPropertiesExcluding(MemoryReqs.memoryTypeBits, MemoryPropertyFlags, TypeIndex, &TypeIndex));
				bMapped = (MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
				Allocation = ResourceTypeHeaps[TypeIndex]->AllocateResource(FOldResourceHeap::EType::Image, MemoryReqs.size, MemoryReqs.alignment, bMapped, File, Line);
			}
			return Allocation;
		}

		inline FOldResourceAllocation* AllocateBufferMemory(const VkMemoryRequirements& MemoryReqs, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line)
		{
			uint32 TypeIndex = 0;
			VERIFYVULKANRESULT(DeviceMemoryManager->GetMemoryTypeFromProperties(MemoryReqs.memoryTypeBits, MemoryPropertyFlags, &TypeIndex));
			bool bMapped = (MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			if (!ResourceTypeHeaps[TypeIndex])
			{
				if ((MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) == VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
				{
					// Try non-cached flag
					MemoryPropertyFlags = MemoryPropertyFlags & ~VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
				}

				if ((MemoryPropertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) == VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
				{
					// Try non-lazy flag
					MemoryPropertyFlags = MemoryPropertyFlags & ~VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
				}

				// Try another heap type
				uint32 OriginalTypeIndex = TypeIndex;
				if (DeviceMemoryManager->GetMemoryTypeFromPropertiesExcluding(MemoryReqs.memoryTypeBits, MemoryPropertyFlags, TypeIndex, &TypeIndex) != VK_SUCCESS)
				{
					UE_LOG(LogVulkanRHI, Fatal, TEXT("Unable to find alternate type for index %d, MemSize %d, MemPropTypeBits %u, MemPropertyFlags %u, %s(%d)"), OriginalTypeIndex, (uint32)MemoryReqs.size, (uint32)MemoryReqs.memoryTypeBits, (uint32)MemoryPropertyFlags, ANSI_TO_TCHAR(File), Line);
				}

				if (!ResourceTypeHeaps[TypeIndex])
				{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
					DumpMemory();
#endif
					UE_LOG(LogVulkanRHI, Fatal, TEXT("Missing memory type index %d (originally requested %d), MemSize %d, MemPropTypeBits %u, MemPropertyFlags %u, %s(%d)"), TypeIndex, OriginalTypeIndex, (uint32)MemoryReqs.size, (uint32)MemoryReqs.memoryTypeBits, (uint32)MemoryPropertyFlags, ANSI_TO_TCHAR(File), Line);
				}
			}

			FOldResourceAllocation* Allocation = ResourceTypeHeaps[TypeIndex]->AllocateResource(FOldResourceHeap::EType::Buffer, MemoryReqs.size, MemoryReqs.alignment, bMapped, File, Line); //-V595
			if (!Allocation)
			{
				// Try another memory type if the allocation failed
				VERIFYVULKANRESULT(DeviceMemoryManager->GetMemoryTypeFromPropertiesExcluding(MemoryReqs.memoryTypeBits, MemoryPropertyFlags, TypeIndex, &TypeIndex));
				bMapped = (MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
				if (!ResourceTypeHeaps[TypeIndex])
				{
					UE_LOG(LogVulkanRHI, Fatal, TEXT("Missing memory type index %d, MemSize %d, MemPropTypeBits %u, MemPropertyFlags %u, %s(%d)"), TypeIndex, (uint32)MemoryReqs.size, (uint32)MemoryReqs.memoryTypeBits, (uint32)MemoryPropertyFlags, ANSI_TO_TCHAR(File), Line);
				}
				Allocation = ResourceTypeHeaps[TypeIndex]->AllocateResource(FOldResourceHeap::EType::Buffer, MemoryReqs.size, MemoryReqs.alignment, bMapped, File, Line);
			}
			return Allocation;
		}

		void ReleaseFreedPages();

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		void DumpMemory();
#endif

		void* Hotfix;

	protected:
		FDeviceMemoryManager* DeviceMemoryManager;
		TArray<FOldResourceHeap*> ResourceTypeHeaps;

		enum
		{
			BufferAllocationSize = 1 * 1024 * 1024,
			UniformBufferAllocationSize = 2 * 1024 * 1024,
		};


		// pool sizes that we support
		enum class EPoolSizes : uint8
		{
// 			E32,
// 			E64,
			E128,
			E256,
			E512,
			E1k,
			E2k,
			E8k,
			E16k,
			SizesCount,
		};

		constexpr static uint32 PoolSizes[(int32)EPoolSizes::SizesCount] =
		{
// 			32,
// 			64,
			128,
			256,
			512,
			1024,
			2048,
			8192,
// 			16 * 1024,
		};

		constexpr static uint32 BufferSizes[(int32)EPoolSizes::SizesCount + 1] =
		{
// 			64 * 1024,
// 			64 * 1024,
			128 * 1024,
			128 * 1024,
			256 * 1024,
			256 * 1024,
			512 * 1024,
			512 * 1024,
			1024 * 1024,
			1 * 1024 * 1024,
		};

		EPoolSizes GetPoolTypeForAlloc(uint32 Size, uint32 Alignment)
		{
			EPoolSizes PoolSize = EPoolSizes::SizesCount;
			if (GVulkanUseBufferBinning != 0)
			{
				for (int32 i = 0; i < (int32)EPoolSizes::SizesCount; ++i)
				{
					if (PoolSizes[i] >= Size)
					{
						PoolSize = (EPoolSizes)i;
						break;
					}
				}
			}
			return PoolSize;
		}

		TArray<FBufferAllocation*> UsedBufferAllocations[(int32)EPoolSizes::SizesCount + 1];
		TArray<FBufferAllocation*> FreeBufferAllocations[(int32)EPoolSizes::SizesCount + 1];

		void ReleaseFreedResources(bool bImmediately);
		void DestroyResourceAllocations();
	};

	class FStagingBuffer : public FRefCount
	{
	public:
		FStagingBuffer()
			: ResourceAllocation(nullptr)
			, Buffer(VK_NULL_HANDLE)
			, bCPURead(false)
			, BufferSize(0)
		{
		}

		VkBuffer GetHandle() const
		{
			return Buffer;
		}

		inline void* GetMappedPointer()
		{
			return ResourceAllocation->GetMappedPointer();
		}

		inline uint32 GetSize() const
		{
			return BufferSize;
		}

		inline VkDeviceMemory GetDeviceMemoryHandle() const
		{
			return ResourceAllocation->GetHandle();
		}

		inline void FlushMappedMemory()
		{
			ResourceAllocation->FlushMappedMemory();
		}

		inline void InvalidateMappedMemory()
		{
			ResourceAllocation->InvalidateMappedMemory();
		}

	protected:
		TRefCountPtr<FOldResourceAllocation> ResourceAllocation;
		VkBuffer Buffer;
		bool bCPURead;
		uint32 BufferSize;

		// Owner maintains lifetime
		virtual ~FStagingBuffer();

		void Destroy(FVulkanDevice* Device);

		friend class FStagingManager;
	};

	class FStagingManager
	{
	public:
		FStagingManager() :
			PeakUsedMemory(0),
			UsedMemory(0),
			Device(nullptr)
		{
		}
		~FStagingManager();

		void Init(FVulkanDevice* InDevice)
		{
			Device = InDevice;
		}

		void Deinit();

		FStagingBuffer* AcquireBuffer(uint32 Size, VkBufferUsageFlags InUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, bool bCPURead = false);

		// Sets pointer to nullptr
		void ReleaseBuffer(FVulkanCmdBuffer* CmdBuffer, FStagingBuffer*& StagingBuffer);

		void ProcessPendingFree(bool bImmediately, bool bFreeToOS);

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		void DumpMemory();
#endif

	protected:
		struct FPendingItemsPerCmdBuffer
		{
			FVulkanCmdBuffer* CmdBuffer;
			struct FPendingItems
			{
				uint64 FenceCounter;
				TArray<FStagingBuffer*> Resources;
			};


			inline FPendingItems* FindOrAddItemsForFence(uint64 Fence);

			TArray<FPendingItems> PendingItems;
		};

		TArray<FStagingBuffer*> UsedStagingBuffers;
		TArray<FPendingItemsPerCmdBuffer> PendingFreeStagingBuffers;
		struct FFreeEntry
		{
			FStagingBuffer* StagingBuffer;
			uint32 FrameNumber;
		};
		TArray<FFreeEntry> FreeStagingBuffers;

		uint64 PeakUsedMemory;
		uint64 UsedMemory;

		FPendingItemsPerCmdBuffer* FindOrAdd(FVulkanCmdBuffer* CmdBuffer);

		void ProcessPendingFreeNoLock(bool bImmediately, bool bFreeToOS);

		FVulkanDevice* Device;
	};

	class FFence
	{
	public:
		FFence(FVulkanDevice* InDevice, FFenceManager* InOwner, bool bCreateSignaled);

		inline VkFence GetHandle() const
		{
			return Handle;
		}

		inline bool IsSignaled() const
		{
			return State == EState::Signaled;
		}

		FFenceManager* GetOwner()
		{
			return Owner;
		}

	protected:
		VkFence Handle;

		enum class EState
		{
			// Initial state
			NotReady,

			// After GPU processed it
			Signaled,
		};

		EState State;

		FFenceManager* Owner;

		// Only owner can delete!
		~FFence();
		friend class FFenceManager;
	};

	class FFenceManager
	{
	public:
		FFenceManager()
			: Device(nullptr)
		{
		}
		~FFenceManager();

		void Init(FVulkanDevice* InDevice);
		void Deinit();

		FFence* AllocateFence(bool bCreateSignaled = false);

		inline bool IsFenceSignaled(FFence* Fence)
		{
			if (Fence->IsSignaled())
			{
				return true;
			}

			return CheckFenceState(Fence);
		}

		// Returns false if it timed out
		bool WaitForFence(FFence* Fence, uint64 TimeInNanoseconds);

		void ResetFence(FFence* Fence);

		// Sets it to nullptr
		void ReleaseFence(FFence*& Fence);

		// Sets it to nullptr
		void WaitAndReleaseFence(FFence*& Fence, uint64 TimeInNanoseconds);

	protected:
		FVulkanDevice* Device;
		TArray<FFence*> FreeFences;
		TArray<FFence*> UsedFences;

		// Returns true if signaled
		bool CheckFenceState(FFence* Fence);

		void DestroyFence(FFence* Fence);
	};

	class FGPUEvent : public FDeviceChild, public FRefCount
	{
	public:
		FGPUEvent(FVulkanDevice* InDevice);
		virtual ~FGPUEvent();

		inline VkEvent GetHandle() const
		{
			return Handle;
		}

	protected:
		VkEvent Handle;
	};

	class FDeferredDeletionQueue : public FDeviceChild
	{
		//typedef TPair<FRefCountedObject*, uint64> FFencedObject;
		//FThreadsafeQueue<FFencedObject> DeferredReleaseQueue;

	public:
		FDeferredDeletionQueue(FVulkanDevice* InDevice);
		~FDeferredDeletionQueue();

		enum class EType
		{
			RenderPass,
			Buffer,
			BufferView,
			Image,
			ImageView,
			Pipeline,
			PipelineLayout,
			Framebuffer,
			DescriptorSetLayout,
			Sampler,
			Semaphore,
			ShaderModule,
			Event,
		};

		template <typename T>
		inline void EnqueueResource(EType Type, T Handle)
		{
			static_assert(sizeof(T) <= sizeof(uint64), "Vulkan resource handle type size too large.");
			EnqueueGenericResource(Type, (uint64)Handle);
		}

		void ReleaseResources(bool bDeleteImmediately = false);

		inline void Clear()
		{
			ReleaseResources(true);
		}

		void OnCmdBufferDeleted(FVulkanCmdBuffer* CmdBuffer);
/*
		class FVulkanAsyncDeletionWorker : public FVulkanDeviceChild, FNonAbandonableTask
		{
		public:
			FVulkanAsyncDeletionWorker(FVulkanDevice* InDevice, FThreadsafeQueue<FFencedObject>* DeletionQueue);

			void DoWork();

		private:
			TQueue<FFencedObject> Queue;
		};
*/
	private:
		//TQueue<FAsyncTask<FVulkanAsyncDeletionWorker>*> DeleteTasks;

		void EnqueueGenericResource(EType Type, uint64 Handle);

		struct FEntry
		{
			uint64 FenceCounter;
			uint64 Handle;
			FVulkanCmdBuffer* CmdBuffer;
			EType StructureType;
			uint32 FrameNumber;
		};
		FCriticalSection CS;
		TArray<FEntry> Entries;
	};

	// Simple tape allocation per frame for a VkBuffer, used for Volatile allocations
	class FTempFrameAllocationBuffer : public FDeviceChild
	{
		enum
		{
			ALLOCATION_SIZE = (4 * 1024 * 1024),
		};

	public:
		FTempFrameAllocationBuffer(FVulkanDevice* InDevice);
		virtual ~FTempFrameAllocationBuffer();
		void Destroy();

		struct FTempAllocInfo
		{
			void* Data;

			FBufferSuballocation* BufferSuballocation;

			// Offset into the locked area
			uint32 CurrentOffset;

			// Simple counter used for the SRVs to know a new one is required
			uint32 LockCounter;

			FTempAllocInfo()
				: Data(nullptr)
				, BufferSuballocation(nullptr)
				, CurrentOffset(0)
				, LockCounter(0)
			{
			}

			inline FBufferAllocation* GetBufferAllocation() const
			{
				return BufferSuballocation->GetBufferAllocation();
			}

			inline uint32 GetBindOffset() const
			{
				return BufferSuballocation->GetOffset() + CurrentOffset;
			}

			inline VkBuffer GetHandle() const
			{
				return BufferSuballocation->GetHandle();
			}
		};

		void Alloc(uint32 InSize, uint32 InAlignment, FTempAllocInfo& OutInfo);

		void Reset();

	protected:
		uint32 BufferIndex;

		enum
		{
			NUM_BUFFERS = 3,
		};

		struct FFrameEntry
		{
			TRefCountPtr<FBufferSuballocation> BufferSuballocation;
			TArray<TRefCountPtr<FBufferSuballocation>> PendingDeletionList;
			uint8* MappedData = nullptr;
			uint8* CurrentData = nullptr;
			uint32 Size = 0;
			uint32 PeakUsed = 0;

			void InitBuffer(FVulkanDevice* InDevice, uint32 InSize);
			void Reset();
			bool TryAlloc(uint32 InSize, uint32 InAlignment, FTempAllocInfo& OutInfo);
		};
		FFrameEntry Entries[NUM_BUFFERS];
		FCriticalSection CS;

		friend class FVulkanCommandListContext;
	};

	inline void* FBufferSuballocation::GetMappedPointer()
	{
		return (uint8*)Owner->GetMappedPointer() + AlignedOffset;
	}

	enum class EImageLayoutBarrier
	{
		Undefined,
		TransferDest,
		ColorAttachment,
		DepthStencilAttachment,
		TransferSource,
		Present,
		PixelShaderRead,
		PixelDepthStencilRead,
		ComputeGeneralRW,
		PixelGeneralRW,
#if VULKAN_SUPPORTS_MAINTENANCE_LAYER2
		DepthReadStencilAttachment,
#endif
	};

	inline EImageLayoutBarrier GetImageLayoutFromVulkanLayout(VkImageLayout Layout)
	{
		switch (Layout)
		{
		case VK_IMAGE_LAYOUT_UNDEFINED:
			return EImageLayoutBarrier::Undefined;

		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return EImageLayoutBarrier::TransferDest;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return EImageLayoutBarrier::ColorAttachment;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			return EImageLayoutBarrier::DepthStencilAttachment;

#if VULKAN_SUPPORTS_MAINTENANCE_LAYER2
		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR:
			return EImageLayoutBarrier::DepthReadStencilAttachment;
#endif
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return EImageLayoutBarrier::TransferSource;

		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			return EImageLayoutBarrier::Present;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			//#todo-rco: Not necessarily right...
			return EImageLayoutBarrier::PixelShaderRead;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			return EImageLayoutBarrier::PixelDepthStencilRead;

		case VK_IMAGE_LAYOUT_GENERAL:
			return EImageLayoutBarrier::PixelGeneralRW;

		default:
			checkf(0, TEXT("Unknown VkImageLayout %d"), (int32)Layout);
			break;
		}

		return EImageLayoutBarrier::Undefined;
	}

	inline VkPipelineStageFlags GetImageBarrierFlags(EImageLayoutBarrier Target, VkAccessFlags& AccessFlags, VkImageLayout& Layout)
	{
		VkPipelineStageFlags StageFlags = (VkPipelineStageFlags)0;
		switch (Target)
		{
		case EImageLayoutBarrier::Undefined:
			AccessFlags = 0;
			StageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			Layout = VK_IMAGE_LAYOUT_UNDEFINED;
			break;

		case EImageLayoutBarrier::TransferDest:
			AccessFlags = VK_ACCESS_TRANSFER_WRITE_BIT;
			StageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			break;

		case EImageLayoutBarrier::ColorAttachment:
			AccessFlags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			StageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			break;

		case EImageLayoutBarrier::DepthStencilAttachment:
			AccessFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			StageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			break;

#if VULKAN_SUPPORTS_MAINTENANCE_LAYER2
		case EImageLayoutBarrier::DepthReadStencilAttachment:
			AccessFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			StageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			Layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR;
			break;
#endif

		case EImageLayoutBarrier::TransferSource:
			AccessFlags = VK_ACCESS_TRANSFER_READ_BIT;
			StageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			break;

		case EImageLayoutBarrier::Present:
			AccessFlags = 0;
			StageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			break;

		case EImageLayoutBarrier::PixelShaderRead:
			AccessFlags = VK_ACCESS_SHADER_READ_BIT;
			StageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			break;

		case EImageLayoutBarrier::PixelDepthStencilRead:
			AccessFlags = VK_ACCESS_SHADER_READ_BIT;
			StageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			break;

		case EImageLayoutBarrier::ComputeGeneralRW:
			AccessFlags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			StageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			Layout = VK_IMAGE_LAYOUT_GENERAL;
			break;

		case EImageLayoutBarrier::PixelGeneralRW:
			AccessFlags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			StageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			Layout = VK_IMAGE_LAYOUT_GENERAL;
			break;

		default:
			checkf(0, TEXT("Unknown ImageLayoutBarrier %d"), (int32)Target);
			break;
		}

		return StageFlags;
	}

	inline VkImageLayout GetImageLayout(EImageLayoutBarrier Target)
	{
		VkAccessFlags Flags;
		VkImageLayout Layout;
		GetImageBarrierFlags(Target, Flags, Layout);
		return Layout;
	}

	inline void SetImageBarrierInfo(EImageLayoutBarrier Source, EImageLayoutBarrier Dest, VkImageMemoryBarrier& InOutBarrier, VkPipelineStageFlags& InOutSourceStage, VkPipelineStageFlags& InOutDestStage)
	{
		InOutSourceStage |= GetImageBarrierFlags(Source, InOutBarrier.srcAccessMask, InOutBarrier.oldLayout);
		InOutDestStage |= GetImageBarrierFlags(Dest, InOutBarrier.dstAccessMask, InOutBarrier.newLayout);
	}

	void ImagePipelineBarrier(VkCommandBuffer CmdBuffer, VkImage Image, EImageLayoutBarrier SourceTransition, EImageLayoutBarrier DestTransition, const VkImageSubresourceRange& SubresourceRange);

	inline VkImageSubresourceRange SetupImageSubresourceRange(VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_COLOR_BIT, uint32 StartMip = 0)
	{
		VkImageSubresourceRange Range;
		FMemory::Memzero(Range);
		Range.aspectMask = Aspect;
		Range.baseMipLevel = StartMip;
		Range.levelCount = 1;
		Range.baseArrayLayer = 0;
		Range.layerCount = 1;
		return Range;
	}

	inline VkImageMemoryBarrier SetupImageMemoryBarrier(VkImage Image, VkImageAspectFlags Aspect, uint32 NumMips = 1)
	{
		VkImageMemoryBarrier Barrier;
		ZeroVulkanStruct(Barrier, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
		Barrier.image = Image;
		Barrier.subresourceRange.aspectMask = Aspect;
		Barrier.subresourceRange.levelCount = NumMips;
		Barrier.subresourceRange.layerCount = 1;
		Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		return Barrier;
	}

	struct FPendingBarrier
	{
	protected:
		VkPipelineStageFlags SourceStage = 0;
		VkPipelineStageFlags DestStage = 0;
		TArray<VkImageMemoryBarrier> ImageBarriers;
		TArray<VkBufferMemoryBarrier> BufferBarriers;

		void InnerExecute(FVulkanCmdBuffer* CmdBuffer, bool bEnsure);

	public:
		inline int32 NumImageBarriers() const
		{
			return ImageBarriers.Num();
		}

		inline int32 NumBufferBarriers() const
		{
			return BufferBarriers.Num();
		}

		inline void ResetStages()
		{
			SourceStage = 0;
			DestStage = 0;
		}

		inline int32 AddImageBarrier(VkImage Image, VkImageAspectFlags Aspect, uint32 NumMips, uint32 NumLayers = 1)
		{
			int32 Index = ImageBarriers.AddZeroed();
			VkImageMemoryBarrier& Barrier = ImageBarriers[Index];
			Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			Barrier.image = Image;
			Barrier.subresourceRange.aspectMask = Aspect;
			Barrier.subresourceRange.levelCount = NumMips;
			Barrier.subresourceRange.layerCount = NumLayers;
			Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			return Index;
		}

		inline void SetTransition(int32 BarrierIndex, EImageLayoutBarrier Source, EImageLayoutBarrier Dest)
		{
			VkImageMemoryBarrier& Barrier = ImageBarriers[BarrierIndex];

			if (FVulkanPlatform::RequiresPresentLayoutFix() && GVulkanDelayAcquireImage != EDelayAcquireImageType::DelayAcquire)
			{
				VkPipelineStageFlags NewSourceStage = GetImageBarrierFlags(Source, Barrier.srcAccessMask, Barrier.oldLayout);;
				VkPipelineStageFlags NewDestStage = GetImageBarrierFlags(Dest, Barrier.dstAccessMask, Barrier.newLayout);

				// special handling for VK_IMAGE_LAYOUT_PRESENT_SRC_KHR (otherwise Mali devices flicker)
				if (Source == EImageLayoutBarrier::Present)
				{
					NewSourceStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
					NewDestStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				}
				else if (Dest == EImageLayoutBarrier::Present)
				{
					NewSourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
					NewDestStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
				}

				SourceStage |= NewSourceStage;
				DestStage |= NewDestStage;
			}
			else
			{
				SourceStage |= GetImageBarrierFlags(Source, Barrier.srcAccessMask, Barrier.oldLayout);
				DestStage |= GetImageBarrierFlags(Dest, Barrier.dstAccessMask, Barrier.newLayout);
			}
		}

		// This is only valid while no other ImageBarriers are added/removed
		inline VkImageSubresourceRange& GetSubresource(int32 BarrierIndex)
		{
			return ImageBarriers[BarrierIndex].subresourceRange;
		}

		inline VkImageLayout GetDestLayout(int32 BarrierIndex)
		{
			return ImageBarriers[BarrierIndex].newLayout;
		}

		// Actually Insert the cmd in cmdbuffer 
		void Execute(FVulkanCmdBuffer* CmdBuffer, bool bEnsure = true)
		{
			if (ImageBarriers.Num() > 0 || BufferBarriers.Num() > 0)
			{
				InnerExecute(CmdBuffer, bEnsure);
			}
		}
	};

	class FSemaphore : public FRefCount
	{
	public:
		FSemaphore(FVulkanDevice& InDevice);
		virtual ~FSemaphore();

		inline VkSemaphore GetHandle() const
		{
			return SemaphoreHandle;
		}

	private:
		FVulkanDevice& Device;
		VkSemaphore SemaphoreHandle;
	};
}


#if VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED
struct FVulkanCustomMemManager
{
	static VKAPI_ATTR void* Alloc(void* UserData, size_t Size, size_t Alignment, VkSystemAllocationScope AllocScope);
	static VKAPI_ATTR void Free(void* pUserData, void* pMem);
	static VKAPI_ATTR void* Realloc(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocScope);
	static VKAPI_ATTR void InternalAllocationNotification(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);
	static VKAPI_ATTR void InternalFreeNotification(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);

	FVulkanCustomMemManager();

	struct FType
	{
		size_t UsedMemory = 0;
		size_t MaxAllocSize = 0;
		TMap<void*, size_t> Allocs;
	};
	TStaticArray<FType, VK_SYSTEM_ALLOCATION_SCOPE_RANGE_SIZE> Types;

	static FType& GetType(void* pUserData, VkSystemAllocationScope AllocScope);
};
#endif
