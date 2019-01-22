// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AssetData.h"
#include "EdGraph/EdGraph.h"
#include "Misc/AssetRegistryInterface.h"
#include "EdGraph_ReferenceViewer.generated.h"

class FAssetThumbnailPool;
class UEdGraphNode_Reference;
class SReferenceViewer;

UCLASS()
class ASSETMANAGEREDITOR_API UEdGraph_ReferenceViewer : public UEdGraph
{
	GENERATED_UCLASS_BODY()

public:
	// UObject implementation
	virtual void BeginDestroy() override;
	// End UObject implementation

	/** Set reference viewer to focus on these assets */
	void SetGraphRoot(const TArray<FAssetIdentifier>& GraphRootIdentifiers, const FIntPoint& GraphRootOrigin = FIntPoint(ForceInitToZero));

	/** Returns list of currently focused assets */
	const TArray<FAssetIdentifier>& GetCurrentGraphRootIdentifiers() const;

	/** If you're extending the reference viewer via GetAllGraphEditorContextMenuExtender you can use this to get the list of selected assets to use in your menu extender */
	bool GetSelectedAssetsForMenuExtender(const class UEdGraphNode* Node, TArray<FAssetIdentifier>& SelectedAssets) const;

	/** Accessor for the thumbnail pool in this graph */
	const TSharedPtr<class FAssetThumbnailPool>& GetAssetThumbnailPool() const;

	/** Force the graph to rebuild */
	class UEdGraphNode_Reference* RebuildGraph();

	bool IsSearchDepthLimited() const;
	bool IsSearchBreadthLimited() const;
	bool IsShowSoftReferences() const;
	bool IsShowHardReferences() const;
	bool IsShowManagementReferences() const;
	bool IsShowSearchableNames() const;
	bool IsShowNativePackages() const;

	void SetSearchDepthLimitEnabled(bool newEnabled);
	void SetSearchBreadthLimitEnabled(bool newEnabled);
	void SetShowSoftReferencesEnabled(bool newEnabled);
	void SetShowHardReferencesEnabled(bool newEnabled);
	void SetShowManagementReferencesEnabled(bool newEnabled);
	void SetShowSearchableNames(bool newEnabled);
	void SetShowNativePackages(bool newEnabled);

	int32 GetSearchDepthLimit() const;
	int32 GetSearchBreadthLimit() const;

	void SetSearchDepthLimit(int32 NewDepthLimit);
	void SetSearchBreadthLimit(int32 NewBreadthLimit);

	FName GetCurrentCollectionFilter() const;
	void SetCurrentCollectionFilter(FName NewFilter);

	bool GetEnableCollectionFilter() const;
	void SetEnableCollectionFilter(bool bEnabled);

private:
	void SetReferenceViewer(TSharedPtr<SReferenceViewer> InViewer);
	UEdGraphNode_Reference* ConstructNodes(const TArray<FAssetIdentifier>& GraphRootIdentifiers, const FIntPoint& GraphRootOrigin);
	int32 RecursivelyGatherSizes(bool bReferencers, const TArray<FAssetIdentifier>& Identifiers, const TSet<FName>& AllowedPackageNames, int32 CurrentDepth, TSet<FAssetIdentifier>& VisitedNames, TMap<FAssetIdentifier, int32>& OutNodeSizes) const;
	void GatherAssetData(const TSet<FName>& AllPackageNames, TMap<FName, FAssetData>& OutPackageToAssetDataMap) const;
	class UEdGraphNode_Reference* RecursivelyConstructNodes(bool bReferencers, UEdGraphNode_Reference* RootNode, const TArray<FAssetIdentifier>& Identifiers, const FIntPoint& NodeLoc, const TMap<FAssetIdentifier, int32>& NodeSizes, const TMap<FName, FAssetData>& PackagesToAssetDataMap, const TSet<FName>& AllowedPackageNames, int32 CurrentDepth, TSet<FAssetIdentifier>& VisitedNames);

	bool ExceedsMaxSearchDepth(int32 Depth) const;
	bool ExceedsMaxSearchBreadth(int32 Breadth) const;
	EAssetRegistryDependencyType::Type GetReferenceSearchFlags(bool bHardOnly) const;

	UEdGraphNode_Reference* CreateReferenceNode();

	/** Removes all nodes from the graph */
	void RemoveAllNodes();

	/** Returns true if filtering is enabled and we have a valid collection */
	bool ShouldFilterByCollection() const;

private:
	/** Pool for maintaining and rendering thumbnails */
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;

	/** Editor for this pool */
	TWeakPtr<SReferenceViewer> ReferenceViewer;

	TArray<FAssetIdentifier> CurrentGraphRootIdentifiers;
	FIntPoint CurrentGraphRootOrigin;

	int32 MaxSearchDepth;
	int32 MaxSearchBreadth;

	/** Current collection filter. NAME_None for no filter */
	FName CurrentCollectionFilter;
	bool bEnableCollectionFilter;

	bool bLimitSearchDepth;
	bool bLimitSearchBreadth;
	bool bIsShowSoftReferences;
	bool bIsShowHardReferences;
	bool bIsShowManagementReferences;
	bool bIsShowSearchableNames;
	bool bIsShowNativePackages;

	friend SReferenceViewer;
};
