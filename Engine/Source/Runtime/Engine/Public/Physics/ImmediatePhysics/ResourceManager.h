// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Containers/SparseArray.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsMaterial.h"

namespace ImmediatePhysics
{
	struct FMaterial;
}

namespace ImmediatePhysics
{
	/* List of available resource types managed here */
	enum class EResourceType : uint8
	{
		Material = 0,
		NumResources,
		Invalid
	};

	/* Handle to a specific shared resource */
	struct FResourceHandle
	{
		FResourceHandle()
			: Type(EResourceType::Invalid)
			, ResourceIndex(INDEX_NONE)
			, ResourceId(INDEX_NONE)
		{}

		FResourceHandle(EResourceType InType, int32 InIndex, int32 InId)
			: Type(InType)
			, ResourceIndex(InIndex)
			, ResourceId(InId)
		{}

		/** Get the type of the resource, see the list of supported types above */
		EResourceType GetType() const { return Type; }

		/** Get the storage index of this resource, note this is not enough to identify a resource, the comparison id is also required */
		int32 GetIndex() const { return ResourceIndex; }

		/** ID for this resource, used to tell whether a resource is what this handle describes, or just reuses memory */
		int32 GetId() const { return ResourceId; }

		/** Invalidate the handle */
		void Invalidate()
		{
			Type = EResourceType::Invalid;
			ResourceIndex = INDEX_NONE;
			ResourceId = INDEX_NONE;
		}

	private:

		EResourceType Type;
		int32 ResourceIndex;
		int32 ResourceId;
	};


	/** Simple pair for holding resources and IDs, with a per-type ID counter for comparisons */
	template<typename ResourceType>
	class TResourceWithId
	{
		static int32 ResourceIdCounter;

	public:

		TResourceWithId()
		{
			Id = ResourceIdCounter++;
		}

		ResourceType Resource;
		int32 Id;
	};

	template<typename ResourceType>
	int32 TResourceWithId<ResourceType>::ResourceIdCounter = 0;

	/** Responsible for holding and providing shared resources for the physics system under immediate mode.
	 *  Any resource that cannot be owned entirely by another physics object should be managed here by this
	 *  shared resource manager.
	 */
	class FSharedResourceManager
	{
	public:

		friend struct FScopedSharedResourceReadLock;

		static FSharedResourceManager& Get()
		{
			return Instance;
		}

		/** Material methods */
		FResourceHandle CreateMaterial();
		void ReleaseMaterial(int32 InIndex);
		int32 GetMaterialId(int32 InIndex);
		FMaterial* GetMaterial(int32 InIndex);

		/** Returns the locking primitive used by this manager, usually the scoped resource lock should suffice */
		FRWLock& GetLockObject();

	private:

		FSharedResourceManager() {}
		~FSharedResourceManager() {}
		FSharedResourceManager(const FSharedResourceManager& Other) = delete;
		FSharedResourceManager(FSharedResourceManager&& Other) = delete;
		FSharedResourceManager& operator=(const FSharedResourceManager&& Other) = delete;
		FSharedResourceManager& operator=(FSharedResourceManager&& Other) = delete;

		/** Storage for managed resources */
		TSparseArray<TResourceWithId<ImmediatePhysics::FMaterial>> Materials;

		/** Locking primitive. Should be used in the desired mode whenever manipulated. Simulations will read lock this
		 *  so user code cannot write to shared resources while a simulation is in flight
		 */
		FRWLock ResourceLock;

		/** Singleton instance of the manager */
		static FSharedResourceManager Instance;
	};

	/** Modes for scoped resource locking */
	enum class LockMode : uint8
	{
		Read,
		Write
	};

	/** Scoped locking object for physics resources, to be used whenever manipulating shared objects */
	template<LockMode LockType>
	struct FScopedSharedResourceLock
	{
		FScopedSharedResourceLock()
		{
			DoLock();
		}

		~FScopedSharedResourceLock()
		{
			DoUnlock();
		}

		void DoLock();
		void DoUnlock();

		FScopedSharedResourceLock(const FScopedSharedResourceLock& Other) = delete;
		FScopedSharedResourceLock(FScopedSharedResourceLock&& Other) = delete;
		FScopedSharedResourceLock& operator=(const FScopedSharedResourceLock& Other) = delete;
		FScopedSharedResourceLock& operator=(FScopedSharedResourceLock&& Other) = delete;
	};

	template<>
	inline void FScopedSharedResourceLock<LockMode::Read>::DoLock()
	{
		FSharedResourceManager& Manager = FSharedResourceManager::Get();
		Manager.GetLockObject().ReadLock();
	}

	template<>
	inline void FScopedSharedResourceLock<LockMode::Read>::DoUnlock()
	{
		FSharedResourceManager& Manager = FSharedResourceManager::Get();
		Manager.GetLockObject().ReadUnlock();
	}

	template<>
	inline void FScopedSharedResourceLock<LockMode::Write>::DoLock()
	{
		FSharedResourceManager& Manager = FSharedResourceManager::Get();
		Manager.GetLockObject().WriteLock();
	}

	template<>
	inline void FScopedSharedResourceLock<LockMode::Write>::DoUnlock()
	{
		FSharedResourceManager& Manager = FSharedResourceManager::Get();
		Manager.GetLockObject().WriteUnlock();
	}

}
