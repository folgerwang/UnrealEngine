// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/Box.h"
#include "Apeiron/Defines.h"
#include "Apeiron/GeometryParticles.h"
#include "Apeiron/ImplicitObject.h"
#include "Apeiron/PBDRigidParticles.h"
#include "Apeiron/Particles.h"
#include "Apeiron/Sphere.h"
#include "Apeiron/Transform.h"

#include "Async/ParallelFor.h"

namespace Apeiron
{
template<class OBJECT_ARRAY>
bool HasBoundingBox(const OBJECT_ARRAY& Objects, const int32 i)
{
	return Objects[i]->HasBoundingBox();
}

template<class T, int d>
bool HasBoundingBox(const TParticles<T, d>& Objects, const int32 i)
{
	return true;
}

template<class T, int d>
bool HasBoundingBox(const TGeometryParticles<T, d>& Objects, const int32 i)
{
	return Objects.Geometry(i)->HasBoundingBox();
}

template<class T, int d>
bool HasBoundingBox(const TPBDRigidParticles<T, d>& Objects, const int32 i)
{
	return HasBoundingBox(static_cast<const TGeometryParticles<T, d>&>(Objects), i);
}

template<class OBJECT_ARRAY, class T, int d>
const TBox<T, d>& GetWorldSpaceBoundingBox(const OBJECT_ARRAY& Objects, const int32 i, const TArray<TBox<T, d>>& WorldSpaceBoxes)
{
	return Objects[i]->BoundingBox();
}

template<class T, int d>
const TBox<T, d>& GetWorldSpaceBoundingBox(const TParticles<T, d>& Objects, const int32 i, const TArray<TBox<T, d>>& WorldSpaceBoxes)
{
	return WorldSpaceBoxes[i];
}

template<class T, int d>
const TBox<T, d>& GetWorldSpaceBoundingBox(const TGeometryParticles<T, d>& Objects, const int32 i, const TArray<TBox<T, d>>& WorldSpaceBoxes)
{
	return GetWorldSpaceBoundingBox(static_cast<const TParticles<T, d>&>(Objects), i, WorldSpaceBoxes);
}

template<class T, int d>
const TBox<T, d>& GetWorldSpaceBoundingBox(const TPBDRigidParticles<T, d>& Objects, const int32 i, const TArray<TBox<T, d>>& WorldSpaceBoxes)
{
	return GetWorldSpaceBoundingBox(static_cast<const TParticles<T, d>&>(Objects), i, WorldSpaceBoxes);
}

template<class T, int d>
TBox<T, d> ComputeWorldSpaceBoundingBox(const TParticles<T, d>& Objects, const int32 i)
{
	return TBox<T, d>(Objects.X(i), Objects.X(i));
}

template<class T, int d>
TBox<T, d> ComputeWorldSpaceBoundingBox(const TGeometryParticles<T, d>& Objects, const int32 i)
{
	TRigidTransform<T, d> LocalToWorld(Objects.X(i), Objects.R(i));
	const auto& LocalBoundingBox = Objects.Geometry(i)->BoundingBox();
	return LocalBoundingBox.TransformedBox(LocalToWorld);
}

template<class T, int d>
TBox<T, d> ComputeWorldSpaceBoundingBox(const TPBDRigidParticles<T, d>& Objects, const int32 i)
{
	TRigidTransform<T, d> LocalToWorld(Objects.P(i), Objects.Q(i));
	const auto& LocalBoundingBox = Objects.Geometry(i)->BoundingBox();
	return LocalBoundingBox.TransformedBox(LocalToWorld);
}

template<class OBJECT_ARRAY, class T, int d>
void ComputeAllWorldSpaceBoundingBoxes(const OBJECT_ARRAY& Objects, const TArray<int32>& AllObjects, TArray<TBox<T, d>>& WorldSpaceBoxes)
{
}

template<class T, int d>
void ComputeAllWorldSpaceBoundingBoxes(const TParticles<T, d>& Objects, const TArray<int32>& AllObjects, TArray<TBox<T, d>>& WorldSpaceBoxes)
{
	WorldSpaceBoxes.SetNum(Objects.Size());
	ParallelFor(AllObjects.Num(), [&](int32 i) {
		WorldSpaceBoxes[AllObjects[i]] = ComputeWorldSpaceBoundingBox(Objects, AllObjects[i]);
	});
}

template<class T, int d>
void ComputeAllWorldSpaceBoundingBoxes(const TGeometryParticles<T, d>& Objects, const TArray<int32>& AllObjects, TArray<TBox<T, d>>& WorldSpaceBoxes)
{
	WorldSpaceBoxes.SetNum(Objects.Size());
	ParallelFor(AllObjects.Num(), [&](int32 i) {
		WorldSpaceBoxes[AllObjects[i]] = ComputeWorldSpaceBoundingBox(Objects, AllObjects[i]);
	});
}

template<class T, int d>
void ComputeAllWorldSpaceBoundingBoxes(const TPBDRigidParticles<T, d>& Objects, const TArray<int32>& AllObjects, TArray<TBox<T, d>>& WorldSpaceBoxes)
{
	WorldSpaceBoxes.SetNum(Objects.Size());
	ParallelFor(AllObjects.Num(), [&](int32 i) {
		WorldSpaceBoxes[AllObjects[i]] = ComputeWorldSpaceBoundingBox(Objects, AllObjects[i]);
	});
}

template<class OBJECT_ARRAY>
int32 GetObjectCount(const OBJECT_ARRAY& Objects)
{
	return Objects.Num();
}

template<class T, int d>
int32 GetObjectCount(const TParticles<T, d>& Objects)
{
	return Objects.Size();
}

template<class T, int d>
int32 GetObjectCount(const TGeometryParticles<T, d>& Objects)
{
	return GetObjectCount(static_cast<const TParticles<T, d>&>(Objects));
}

template<class T, int d>
int32 GetObjectCount(const TPBDRigidParticles<T, d>& Objects)
{
	return GetObjectCount(static_cast<const TParticles<T, d>&>(Objects));
}

template<class OBJECT_ARRAY, class T, int d>
bool IsDisabled(const OBJECT_ARRAY& Objects, const uint32 Index)
{
	return false;
}

template<class T, int d>
bool IsDisabled(const TGeometryParticles<T, d>& Objects, const uint32 Index)
{
	return false;
}

template<class T, int d>
bool IsDisabled(const TPBDRigidParticles<T, d>& Objects, const uint32 Index)
{
	return Objects.Disabled(Index);
}
}
