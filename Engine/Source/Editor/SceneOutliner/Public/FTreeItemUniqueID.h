// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

//#include "CoreMinimal.h"

namespace SceneOutliner
{
	using FTreeItemUniqueID = uint32;

	class FTreeItemUniqueIDGenerator
	{
	public:
		FTreeItemUniqueIDGenerator() : LastID(0) { }

		void Reset() {
			LastID = 0;
			FreeList.Empty();
		}

		FTreeItemUniqueID GetNextID()
		{
			uint32 NextID = 0;
			if (FreeList.Num() > 0)
			{
				NextID = FreeList.Pop();
			}
			else
			{
				NextID = LastID++;
				check(NextID < UINT32_MAX);
			}

			return FTreeItemUniqueID(NextID);
		}

		void ReleaseID(FTreeItemUniqueID ID)
		{
			FreeList.Add(ID);
		}

		/** Return a single FCustomIDGenerator object */
		SCENEOUTLINER_API static FTreeItemUniqueIDGenerator& Get()
		{
			// return the singleton object
			static FTreeItemUniqueIDGenerator Singleton;
			return Singleton;
		}

	private:
		uint32 LastID;
		TArray<uint32> FreeList;
	};

} // namespace SceneOutliner
