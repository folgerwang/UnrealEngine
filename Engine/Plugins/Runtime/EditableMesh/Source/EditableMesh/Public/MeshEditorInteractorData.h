// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshElement.h"

/**
* The types of interactor shapes we support
*/
enum class EInteractorShape
{
	/** Invalid shape (or none) */
	Invalid,

	/** Grabber sphere */
	GrabberSphere,

	/** Laser pointer shape */
	Laser,
};

/**
* Contains state for either a mouse cursor or a virtual hand (in VR), to be used to interact with a mesh
*/
struct FMeshEditorInteractorData
{
	/** The viewport interactor that is this data's counterpart */
	TWeakObjectPtr<const class UViewportInteractor> ViewportInteractor;

	/** True if we have a valid interaction grabber sphere right now */
	bool bGrabberSphereIsValid;

	/** The sphere for radial interactions */
	FSphere GrabberSphere;

	/** True if we have a valid interaction ray right now */
	bool bLaserIsValid;

	/** World space start location of the interaction ray the last time we were ticked */
	FVector LaserStart;

	/** World space end location of the interaction ray */
	FVector LaserEnd;

	/** What shape of interactor are we using to hover? */
	EInteractorShape HoverInteractorShape;

	/** Information about a mesh we're hovering over or editing */
	FMeshElement HoveredMeshElement;

	/** The element we were hovering over last frame */
	FMeshElement PreviouslyHoveredMeshElement;

	/** The hover point.  With a ray, this could be the impact point along the ray.  With grabber sphere interaction, this
	would be the point within the sphere radius where we've found a point on an object to interact with */
	FVector HoverLocation;


	/** Default constructor that initializes everything to safe values */
	FMeshEditorInteractorData()
		: ViewportInteractor(nullptr),
		bGrabberSphereIsValid(false),
		GrabberSphere(0),
		bLaserIsValid(false),
		LaserStart(FVector::ZeroVector),
		LaserEnd(FVector::ZeroVector),
		HoverInteractorShape(EInteractorShape::Invalid),
		HoveredMeshElement(),
		PreviouslyHoveredMeshElement(),
		HoverLocation(FVector::ZeroVector)
	{
	}
};