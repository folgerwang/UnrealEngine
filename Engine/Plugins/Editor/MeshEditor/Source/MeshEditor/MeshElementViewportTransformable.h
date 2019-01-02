// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewportTransformable.h"
#include "MeshElement.h"

/**
 * A transformable mesh element
 */
class FMeshElementViewportTransformable : public FViewportTransformable
{
public:

	/** Sets up safe defaults */
	FMeshElementViewportTransformable( class FMeshEditorMode& InitMeshEditorMode )
		: MeshEditorMode( InitMeshEditorMode )
	{
	}

	// FViewportTransformable overrides
	virtual const FTransform GetTransform() const override;
	virtual bool IsUnorientedPoint() const override;
	virtual void ApplyTransform( const FTransform& NewTransform, const bool bSweep ) override;
	virtual FBox BuildBoundingBox( const FTransform& BoundingBoxToWorld ) const override;


	/** Mesh editor mode object */
	FMeshEditorMode& MeshEditorMode;

	/** The actual mesh element being transformed */
	FMeshElement MeshElement;

	/** Current transform for the element */
	FTransform CurrentTransform;
};

