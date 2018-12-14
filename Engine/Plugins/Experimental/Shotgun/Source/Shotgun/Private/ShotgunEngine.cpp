// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ShotgunEngine.h"

#include "AssetRegistryModule.h"
#include "IAssetRegistry.h"

#include "GameFramework/Actor.h"
#include "IPythonScriptPlugin.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Templates/Casts.h"
#include "UObject/UObjectHash.h"

UShotgunEngine* UShotgunEngine::GetInstance()
{
	// The Python Shotgun Engine instance must come from a Python class derived from UShotgunEngine
	// In Python, there should be only one derivation, but hot-reloading will create new derived classes, so use the last one
	TArray<UClass*> ShotgunEngineClasses;
	GetDerivedClasses(UShotgunEngine::StaticClass(), ShotgunEngineClasses);
	int32 NumClasses = ShotgunEngineClasses.Num();
	if (NumClasses > 0)
	{
		return Cast<UShotgunEngine>(ShotgunEngineClasses[NumClasses - 1]->GetDefaultObject());
	}
	return nullptr;
}

static void OnEditorExit()
{
	if (UShotgunEngine* Engine = UShotgunEngine::GetInstance())
	{
		Engine->Shutdown();
	}
}

void UShotgunEngine::OnEngineInitialized() const
{
	IPythonScriptPlugin::Get()->OnPythonShutdown().AddStatic(OnEditorExit);
}

void UShotgunEngine::SetSelection(const TArray<FAssetData>* InSelectedAssets, const TArray<AActor*>* InSelectedActors)
{
	SelectedAssets.Reset();
	SelectedActors.Reset();

	if (InSelectedAssets)
	{
		SelectedAssets = *InSelectedAssets;
	}
	if (InSelectedActors)
	{
		SelectedActors = *InSelectedActors;

		// Also set the assets referenced by the selected actors as selected assets
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> AllReferencedAssets;
		for (const AActor* Actor : SelectedActors)
		{
			TArray<UObject*> ActorAssets = GetReferencedAssets(Actor);
			for (UObject* Asset : ActorAssets)
			{
				if (Asset && Asset->IsAsset() && !Asset->IsPendingKill())
				{
					FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FName(*Asset->GetPathName()));
					AllReferencedAssets.AddUnique(MoveTemp(AssetData));
				}
			}
		}
		SelectedAssets = MoveTemp(AllReferencedAssets);
	}
}

TArray<UObject*> UShotgunEngine::GetReferencedAssets(const AActor* Actor) const
{
	TArray<UObject*> ReferencedAssets;

	if (Actor)
	{
		Actor->GetReferencedContentObjects(ReferencedAssets);
	}

	return ReferencedAssets;
}

FString UShotgunEngine::GetShotgunWorkDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
}
