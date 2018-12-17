// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DragAndDrop/DecoratedDragDropOp.h"

class FDragDropEvent;

/** Drag/drop operation for dragging layers in the editor */
class LAYERS_API FLayersDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FLayersDragDropOp, FDragDropOperation)

	/** The names of the layers being dragged */
	TArray<FName> Layers;

	virtual void Construct() override;
};
