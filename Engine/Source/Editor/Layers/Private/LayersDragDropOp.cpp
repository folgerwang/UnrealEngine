// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LayersDragDropOp.h"
#include "Textures/SlateIcon.h"


void FLayersDragDropOp::Construct()
{
	const FSlateBrush* Icon = FEditorStyle::GetBrush(TEXT("Layer.Icon16x"));
	if (Layers.Num() == 1)
	{
		SetToolTip(FText::FromName(Layers[0]), Icon);
	}
	else
	{
		FText Text = FText::Format(NSLOCTEXT("FLayersDragDropOp", "MultipleFormat", "{0} Layers"), Layers.Num());
		SetToolTip(Text, Icon);
	}

	SetupDefaults();
	FDecoratedDragDropOp::Construct();
}
