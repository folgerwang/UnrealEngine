// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Chaos/SegmentMesh.h"

using namespace Chaos;

template<class T>
TSegmentMesh<T>::TSegmentMesh(TArray<TVector<int32, 2>>&& Elements)
    : MElements(MoveTemp(Elements))
{
	// Check for degenerate edges.
	for (const TVector<int32, 2>& edge : MElements)
	{
		check(edge[0] != edge[1]);
	}
}

template<class T>
TSegmentMesh<T>::~TSegmentMesh()
{}

template<class T>
void 
TSegmentMesh<T>::_ClearAuxStructures()
{
	MPointToEdgeMap.Empty();
	MPointToNeighborsMap.Empty();
}

template<class T>
void 
TSegmentMesh<T>::Init(const TArray<TVector<int32, 2>>& Elements)
{
	_ClearAuxStructures();
	MElements = Elements;
	// Check for degenerate edges.
	for (const TVector<int32, 2>& edge : MElements)
	{
		check(edge[0] != edge[1]);
	}
}

template<class T>
void
TSegmentMesh<T>::Init(TArray<TVector<int32, 2>>&& Elements)
{
	_ClearAuxStructures();
	MElements = MoveTempIfPossible(Elements);
	// Check for degenerate edges.
	for (const TVector<int32, 2>& edge : MElements)
	{
		check(edge[0] != edge[1]);
	}
}

template<class T>
const TMap<int32, TSet<int32>>&
TSegmentMesh<T>::GetPointToNeighborsMap() const
{
	if (!MPointToNeighborsMap.Num())
		_UpdatePointToNeighborsMap();
	return MPointToNeighborsMap;
}

template<class T>
void 
TSegmentMesh<T>::_UpdatePointToNeighborsMap() const
{
	MPointToNeighborsMap.Reset();
	MPointToNeighborsMap.Reserve(MElements.Num() * 2);
	for (const TVector<int32, 2>& edge : MElements)
	{
		MPointToNeighborsMap.FindOrAdd(edge[0]).Add(edge[1]);
		MPointToNeighborsMap.FindOrAdd(edge[1]).Add(edge[0]);
		// Paranoia:
		check(MPointToNeighborsMap.Find(edge[0]) != nullptr);
		check(MPointToNeighborsMap.Find(edge[1]) != nullptr);
		check(MPointToNeighborsMap.Find(edge[0])->Find(edge[1]) != nullptr);
		check(MPointToNeighborsMap.Find(edge[1])->Find(edge[0]) != nullptr);
	}
}

template<class T>
const TMap<int32, TArray<int32>>&
TSegmentMesh<T>::GetPointToEdges() const
{
	if (!MPointToEdgeMap.Num())
		_UpdatePointToEdgesMap();
	return MPointToEdgeMap;
}

template<class T>
void 
TSegmentMesh<T>::_UpdatePointToEdgesMap() const
{
	MPointToEdgeMap.Reset();
	MPointToEdgeMap.Reserve(MElements.Num() * 2);
	for (int32 i = 0; i < MElements.Num(); i++)
	{
		const TVector<int32, 2>& edge = MElements[i];
		MPointToEdgeMap.FindOrAdd(edge[0]).Add(i);
		MPointToEdgeMap.FindOrAdd(edge[1]).Add(i);
	}
}

template<class T>
TArray<T>
TSegmentMesh<T>::GetEdgeLengths(const TParticles<T, 3>& InParticles, const bool lengthSquared) const
{
	TArray<T> lengths;
	lengths.AddUninitialized(MElements.Num());
	if (lengthSquared)
	{
		for (int32 i = 0; i < MElements.Num(); i++)
		{
			const TVector<int32, 2>& edge = MElements[i];
			lengths[i] = (InParticles.X(edge[0]) - InParticles.X(edge[1])).SizeSquared();
		}
	}
	else
	{
		for (int32 i = 0; i < MElements.Num(); i++)
		{
			const TVector<int32, 2>& edge = MElements[i];
			lengths[i] = (InParticles.X(edge[0]) - InParticles.X(edge[1])).Size();
		}
	}
	return lengths;
}

template class Chaos::TSegmentMesh<float>;