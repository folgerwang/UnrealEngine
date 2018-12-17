// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Engine/LevelScriptBlueprint.h"
#include "Misc/PackageName.h"
#include "Engine/LevelScriptActor.h"
#include "UObject/Package.h"

//////////////////////////////////////////////////////////////////////////
// ULevelScriptBlueprint

ULevelScriptBlueprint::ULevelScriptBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

void ULevelScriptBlueprint::SetObjectBeingDebugged(UObject* NewObject)
{
	// Only allowed because UWorld::TransferBlueprintDebugReferences needs to get us looking at our duplicated self
	UBlueprint::SetObjectBeingDebugged(NewObject);
}

UObject* ULevelScriptBlueprint::GetObjectBeingDebugged()
{ 
	UObject* CachedResult = UBlueprint::GetObjectBeingDebugged();

	if (CachedResult != NULL)
	{
		return CachedResult;
	}
	else
	{
		if (ULevel* Level = GetLevel()) 
		{ 
			CachedResult = Level->GetLevelScriptActor(); 
		}

		SetObjectBeingDebugged(CachedResult);
	}

	return CachedResult; 
} 

FString ULevelScriptBlueprint::GetFriendlyName() const
{
#if WITH_EDITORONLY_DATA
	return FriendlyName;
#endif
	return UBlueprint::GetFriendlyName();
}

FString ULevelScriptBlueprint::CreateLevelScriptNameFromLevel(const ULevel* Level)
{
	// Since all maps are named "PersistentLevel" the level script name is based on the LevelPackage
	check(Level);
	UObject* LevelPackage = Level->GetOutermost();
	return FPackageName::GetShortName(LevelPackage->GetFName().GetPlainNameString());

}

#endif	//#if WITH_EDITOR

