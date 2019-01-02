// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/Map.h"
#include "Chaos/Particles.h"
#include "Chaos/Vector.h"

namespace Chaos
{
	/**
	* Mesh structure of connected particles via edges.
	*/
	template<class T>
	class CHAOS_API TSegmentMesh
	{
	public:
		TSegmentMesh()
		{}
		TSegmentMesh(TArray<TVector<int32, 2>>&& Elements);
		TSegmentMesh(const TSegmentMesh& Other) = delete;
		TSegmentMesh(TSegmentMesh&& Other)
		    : MElements(MoveTemp(Other.MElements))
		    , MPointToEdgeMap(MoveTemp(Other.MPointToEdgeMap))
		    , MPointToNeighborsMap(MoveTemp(Other.MPointToNeighborsMap))
		{}
		~TSegmentMesh();

		void
		Init(const TArray<TVector<int32, 2>>& Elements);
		void
		Init(TArray<TVector<int32, 2>>&& Elements);

		int32
		GetNumElements() const
		{
			return MElements.Num();
		}

		const TArray<TVector<int32, 2>>&
		GetElements() const
		{
			return MElements;
		}

		/**
		 * @ret The set of neighbor nodes, or nullptr if \p index is not found.
		*/
		const TSet<int32>*
		GetNeighbors(const int32 index) const
		{
			return GetPointToNeighborsMap().Find(index);
		}

		/**
		* @ret A map of each point index to the list of connected points.
		*/
		const TMap<int32, TSet<int32>>&
		GetPointToNeighborsMap() const;

		/**
		* @ret A map of each point index to the list of connected edges.
		*/
		const TMap<int32, TArray<int32>>&
		GetPointToEdges() const;

		/**
		* @ret Lengths (or lengths squared) of all edges.
		* @param InParticles - The particle positions to use.  This routine assumes it's sized appropriately.
		* @param lengthSquared - If true, the squared length is returned, which is faster.
		*/
		TArray<T>
		GetEdgeLengths(
			const TParticles<T, 3>& InParticles, 
			const bool lengthSquared = false) const;

	private:
		void
		_ClearAuxStructures();

		void
		_UpdatePointToNeighborsMap() const;

		void
		_UpdatePointToEdgesMap() const;

	private:
		// We use TVector rather than FEdge to represent connectivity because
		// sizeof(TVector<int32,2>) < sizeof(FEdge).  FEdge has an extra int32
		// member called Count, which we don't currently have a use for.
		TArray<TVector<int32, 2>> MElements;

		// Members are mutable so they can be generated on demand by const API.
		mutable TMap<int32, TArray<int32>> MPointToEdgeMap;
		mutable TMap<int32, TSet<int32>> MPointToNeighborsMap;
	};
} // namespace Chaos
