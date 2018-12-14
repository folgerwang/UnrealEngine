// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"

class IPropertyHandle;
class SWidget;

enum class EDetailNodeType
{
	/** Node represents a category */
	Category,
	/** Node represents an item such as a property or widget */
	Item,
	/** Node represents an advanced dropdown */
	Advanced,
	/** Represents a top level object node if a view supports multiple root objects */
	Object,
};

/** Layout data for node's content widgets. */
struct FNodeWidgetLayoutData
{
	FNodeWidgetLayoutData()
	{
	}

	FNodeWidgetLayoutData(EHorizontalAlignment InHorizontalAlignment, EVerticalAlignment InVerticalAlignment, TOptional<float> InMinWidth, TOptional<float> InMaxWidth)
		: HorizontalAlignment(InHorizontalAlignment)
		, VerticalAlignment(InVerticalAlignment)
		, MinWidth(InMinWidth)
		, MaxWidth(InMaxWidth)
	{
	}

	/** The horizontal alignment requested by the widget. */
	EHorizontalAlignment HorizontalAlignment;

	/** The vertical alignment requested by the widget. */
	EVerticalAlignment VerticalAlignment;

	/** An optional minimum width requested by the widget. */
	TOptional<float> MinWidth;

	/** An optional maximum width requested by the widget. */
	TOptional<float> MaxWidth;
};

/** The widget contents of the node.  Any of these can be null depending on how the row was generated */
struct FNodeWidgets
{
	/** Widget for the name column */
	TSharedPtr<SWidget> NameWidget;

	/** Layout data for the widget in the name column. */
	FNodeWidgetLayoutData NameWidgetLayoutData;

	/** Widget for the value column*/
	TSharedPtr<SWidget> ValueWidget;

	/** Layout data for the widget in the value column. */
	FNodeWidgetLayoutData ValueWidgetLayoutData;

	/** Widget that spans the entire row.  Mutually exclusive with name/value widget */
	TSharedPtr<SWidget> WholeRowWidget;

	/** Layout data for the whole row widget. */
	FNodeWidgetLayoutData WholeRowWidgetLayoutData;
};

class IDetailTreeNode
{
public:
	virtual ~IDetailTreeNode() {}

	/**
	 * @return The type of this node.  Should be used to determine any external styling to apply to the generated r ow
	 */
	virtual EDetailNodeType GetNodeType() const = 0;

	/** 
	 * Creates a handle to the property on this row if the row represents a property. Only compatible with item node types that are properties
	 * 
	 * @return The property handle for the row or null if the node doesn't have a property 
	 */
	virtual TSharedPtr<IPropertyHandle> CreatePropertyHandle() const = 0;

	/**
	 * Creates the slate widgets for this row. 
	 *
	 * @return the node widget structure with either name/value pair or a whole row widget
	 */
	virtual FNodeWidgets CreateNodeWidgets() const = 0;

	/**
	 * Gets the children of this tree node    
	 * Note: Customizations can determine the visibility of children.  This will only return visible children
	 *
	 * @param OutChildren	The generated children
	 */
	virtual void GetChildren(TArray<TSharedRef<IDetailTreeNode>>& OutChildren) = 0;

	/**
	 * Gets an identifier name for this node.  This is not a name formatted for display purposes, but can be useful for storing
	 * UI state like if this row is expanded.
	 */
	virtual FName GetNodeName() const = 0;

	virtual TSharedPtr<class IDetailPropertyRow> GetRow() const = 0;

	/**
	 * Gets the filter strings for this node in the tree.
	 */
	virtual void GetFilterStrings(TArray<FString>& OutFilterStrings) const = 0;
};