// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlueprintAssetHandler.h"
#include "Engine/World.h"
#include "Engine/LevelScriptBlueprint.h"
#include "AssetData.h"

class FLevelBlueprintAssetHandler : public IBlueprintAssetHandler
{
	virtual UBlueprint* RetrieveBlueprint(UObject* InObject) const override
	{
		UWorld* World = CastChecked<UWorld>(InObject);

		const bool bCreateLevelScript = false;
		return World->PersistentLevel ? World->PersistentLevel->GetLevelScriptBlueprint(bCreateLevelScript) : nullptr;
	}

	virtual bool AssetContainsBlueprint(const FAssetData& InAssetData) const
	{
		static FName FiB("FiB");
		// Worlds are only considered to contain a blueprint if they have FiB data
		return InAssetData.TagsAndValues.Find(FiB) || InAssetData.TagsAndValues.Find(FBlueprintTags::FindInBlueprintsData);
	}
};


class FBlueprintAssetTypeHandler : public IBlueprintAssetHandler
{
	virtual UBlueprint* RetrieveBlueprint(UObject* InObject) const override
	{
		// The object is the blueprint for UBlueprint (and derived) assets
		return CastChecked<UBlueprint>(InObject);
	}

	virtual bool AssetContainsBlueprint(const FAssetData& InAssetData) const
	{
		return true;
	}
};

FBlueprintAssetHandler::FBlueprintAssetHandler()
{
	// Register default handlers
	RegisterHandler<FLevelBlueprintAssetHandler>(UWorld::StaticClass()->GetFName());
	RegisterHandler<FBlueprintAssetTypeHandler>(UBlueprint::StaticClass()->GetFName());
}

FBlueprintAssetHandler& FBlueprintAssetHandler::Get()
{
	static FBlueprintAssetHandler Singleton;
	return Singleton;
}

void FBlueprintAssetHandler::RegisterHandler(FName EligibleClass, TUniquePtr<IBlueprintAssetHandler>&& InHandler)
{
	ClassNames.Add(EligibleClass);
	Handlers.Add(MoveTemp(InHandler));
}

const IBlueprintAssetHandler* FBlueprintAssetHandler::FindHandler(const UClass* InClass) const
{
	UClass* StopAtClass = UObject::StaticClass();
	while (InClass && InClass != StopAtClass)
	{
		int32 Index = ClassNames.IndexOfByKey(InClass->GetFName());
		if (Index != INDEX_NONE)
		{
			return Handlers[Index].Get();
		}

		InClass = InClass->GetSuperClass();
	}

	return nullptr;
}