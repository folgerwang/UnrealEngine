// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DetailTreeNode.h"
#include "DetailWidgetRow.h"

FNodeWidgets FDetailTreeNode::CreateNodeWidgets() const
{
	FDetailWidgetRow Row;
	GenerateStandaloneWidget(Row);

	FNodeWidgets Widgets;

	if(Row.HasAnyContent())
	{
		if (Row.HasColumns())
		{
			Widgets.NameWidget = Row.NameWidget.Widget;
			Widgets.NameWidgetLayoutData = FNodeWidgetLayoutData(
				Row.NameWidget.HorizontalAlignment, Row.NameWidget.VerticalAlignment, Row.NameWidget.MinWidth, Row.NameWidget.MaxWidth);
			Widgets.ValueWidget = Row.ValueWidget.Widget;
			Widgets.ValueWidgetLayoutData = FNodeWidgetLayoutData(
				Row.ValueWidget.HorizontalAlignment, Row.ValueWidget.VerticalAlignment, Row.ValueWidget.MinWidth, Row.ValueWidget.MaxWidth);
		}
		else
		{
			Widgets.WholeRowWidget = Row.WholeRowWidget.Widget;
			Widgets.WholeRowWidgetLayoutData = FNodeWidgetLayoutData(
				Row.WholeRowWidget.HorizontalAlignment, Row.WholeRowWidget.VerticalAlignment, Row.WholeRowWidget.MinWidth, Row.WholeRowWidget.MaxWidth);
		}
	}

	return Widgets;
}

void FDetailTreeNode::GetChildren(TArray<TSharedRef<IDetailTreeNode>>& OutChildren)
{
	FDetailNodeList Children;
	GetChildren(Children);

	OutChildren.Reset(Children.Num());

	OutChildren.Append(Children);
}
