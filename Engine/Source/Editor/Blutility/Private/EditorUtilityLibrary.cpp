// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditorUtilityLibrary.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "ContentBrowserModule.h"
#include "Modules/ModuleManager.h"
#include "IContentBrowserSingleton.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"


#define LOCTEXT_NAMESPACE "BlutilityLevelEditorExtensions"

UEditorUtilityLibrary::UEditorUtilityLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<AActor*> UEditorUtilityLibrary::GetSelectionSet()
{
	TArray<AActor*> Result;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			Result.Add(Actor);
		}
	}

	return Result;
}

void UEditorUtilityLibrary::GetSelectionBounds(FVector& Origin, FVector& BoxExtent, float& SphereRadius)
{
	bool bFirstItem = true;

	FBoxSphereBounds Extents;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			if (bFirstItem)
			{
				Extents = Actor->GetRootComponent()->Bounds;
			}
			else
			{
				Extents = Extents + Actor->GetRootComponent()->Bounds;
			}

			bFirstItem = false;
		}
	}

	Origin = Extents.Origin;
	BoxExtent = Extents.BoxExtent;
	SphereRadius = Extents.SphereRadius;
}

TArray<UObject*> UEditorUtilityLibrary::GetSelectedAssets()
{
	//@TODO: Blocking load, no slow dialog
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	TArray<UObject*> Result;
	for (FAssetData& AssetData : SelectedAssets)
	{
		Result.Add(AssetData.GetAsset());
	}

	return Result;
}

void UEditorUtilityLibrary::RenameAsset(UObject* Asset, const FString& NewName)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	TArray<FAssetRenameData> AssetsAndNames;
	const FString PackagePath = FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());
	new (AssetsAndNames) FAssetRenameData(Asset, PackagePath, NewName);

	AssetToolsModule.Get().RenameAssetsWithDialog(AssetsAndNames);
}

AActor* UEditorUtilityLibrary::GetActorReference(FString PathToActor)
{
#if WITH_EDITOR
	return Cast<AActor>(StaticFindObject(AActor::StaticClass(), GEditor->GetEditorWorldContext().World(), *PathToActor, false));
#else
	return nullptr;
#endif //WITH_EDITOR
}

#undef LOCTEXT_NAMESPACE
