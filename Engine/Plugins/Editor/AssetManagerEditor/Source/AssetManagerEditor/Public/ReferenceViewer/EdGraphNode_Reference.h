// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNode.h"
#include "AssetData.h"
#include "EdGraph_ReferenceViewer.h"
#include "EdGraphNode_Reference.generated.h"

class UEdGraphPin;

UCLASS()
class ASSETMANAGEREDITOR_API UEdGraphNode_Reference : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

	/** Returns first asset identifier */
	FAssetIdentifier GetIdentifier() const;
	
	/** Returns all identifiers on this node including virtual things */
	void GetAllIdentifiers(TArray<FAssetIdentifier>& OutIdentifiers) const;

	/** Returns only the packages in this node, skips searchable names */
	void GetAllPackageNames(TArray<FName>& OutPackageNames) const;

	/** Returns our owning graph */
	UEdGraph_ReferenceViewer* GetReferenceViewerGraph() const;

	// UEdGraphNode implementation
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual void AllocateDefaultPins() override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	// End UEdGraphNode implementation

	bool UsesThumbnail() const;
	bool IsPackage() const;
	bool IsCollapsed() const;
	FAssetData GetAssetData() const;

	UEdGraphPin* GetDependencyPin();
	UEdGraphPin* GetReferencerPin();

private:
	void CacheAssetData(const FAssetData& AssetData);
	void SetupReferenceNode(const FIntPoint& NodeLoc, const TArray<FAssetIdentifier>& NewIdentifiers, const FAssetData& InAssetData);
	void SetReferenceNodeCollapsed(const FIntPoint& NodeLoc, int32 InNumReferencesExceedingMax);
	void AddReferencer(class UEdGraphNode_Reference* ReferencerNode);

	TArray<FAssetIdentifier> Identifiers;
	FText NodeTitle;

	bool bUsesThumbnail;
	bool bIsPackage;
	bool bIsPrimaryAsset;
	bool bIsCollapsed;
	FAssetData CachedAssetData;

	UEdGraphPin* DependencyPin;
	UEdGraphPin* ReferencerPin;

	friend UEdGraph_ReferenceViewer;
};


