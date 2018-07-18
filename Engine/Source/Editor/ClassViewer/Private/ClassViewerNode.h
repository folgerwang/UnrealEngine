// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class IPropertyHandle;
class IUnloadedBlueprintData;
class UBlueprint;

class FClassViewerNode
{
public:
	/**
	 * Creates a node for the widget's tree.
	 *
	 * @param	InClassName						The name of the class this node represents.
	 * @param	InClassDisplayName				The display name of the class this node represents
	 * @param	bInIsPlaceable					true if the class is a placeable class.
	 */
	FClassViewerNode( const FString& InClassName, const FString& InClassDisplayName );

	FClassViewerNode( const FClassViewerNode& InCopyObject);

	/**
	 * Adds the specified child to the node.
	 *
	 * @param	Child							The child to be added to this node for the tree.
	 */
	void AddChild( TSharedPtr<FClassViewerNode> Child );

	/**
	 * Adds the specified child to the node. If a child with the same class already exists the function add the child storing more info.
	 * The function does not persist child order.
	 *
	 * @param	NewChild	The child to be added to this node for the tree.
	 */
	void AddUniqueChild(TSharedPtr<FClassViewerNode> NewChild);

	/** 
	 * Retrieves the class name this node is associated with. This is not the literal UClass name as it is missing the _C for blueprints
	 * @param	bUseDisplayName	Whether to use the display name or class name
	 */
	TSharedPtr<FString> GetClassName(bool bUseDisplayName = false) const
	{
		return bUseDisplayName ? ClassDisplayName : ClassName;
	}

	/** Retrieves the children list. */
	TArray<TSharedPtr<FClassViewerNode>>& GetChildrenList()
	{
		return ChildrenList;
	}

	/** Checks if the class is placeable. */
	bool IsClassPlaceable() const;

	/** Checks if this is a blueprint */
	bool IsBlueprintClass() const;

	/** Rather this class is not allowed for the specific context */
	bool IsRestricted() const;

private:
	/** The nontranslated internal name for this class. This is not necessarily the UClass's name, as that may have _C for blueprints */
	TSharedPtr<FString> ClassName;

	/** The translated display name for this class */
	TSharedPtr<FString> ClassDisplayName;

	/** List of children. */
	TArray<TSharedPtr<FClassViewerNode>> ChildrenList;

public:
	/** The class this node is associated with. */
	TWeakObjectPtr<UClass> Class;

	/** The blueprint this node is associated with. */
	TWeakObjectPtr<UBlueprint> Blueprint;

	/** Full object path to the class including _C, set for both blueprint and native */
	FName ClassPath;

	/** Full object path to the parent class, may be blueprint or native */
	FName ParentClassPath;

	/** Full path to the Blueprint that this class is loaded from, none for native classes*/
	FName BlueprintAssetPath;

	/** true if the class passed the filter. */
	bool bPassesFilter;

	/** true if the class is a "normal type", this is used to identify unloaded blueprints as blueprint bases. */
	bool bIsBPNormalType;

	/** Pointer to the parent to this object. */
	TWeakPtr< FClassViewerNode > ParentNode;

	/** Data for unloaded blueprints, only valid if the class is unloaded. */
	TSharedPtr< class IUnloadedBlueprintData > UnloadedBlueprintData;

	/** The property this node will be working on. */
	TSharedPtr<class IPropertyHandle> PropertyHandle;
};
