// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/Map.h"
#include "Chaos/Particles.h"
#include "Chaos/SegmentMesh.h"

#include <unordered_set>

namespace Chaos
{
	template<class T>
	class CHAOS_API TTriangleMesh
	{
	public:
		TTriangleMesh(TArray<TVector<int32, 3>>&& Elements, const int32 StartIdx=0, const int32 EndIdx=-1);
		TTriangleMesh(const TTriangleMesh& Other) = delete;
		TTriangleMesh(TTriangleMesh&& Other);
		~TTriangleMesh()
		{}

		/**
		 * Returns the closed interval of the smallest vertex index used by 
		 * this class, to the largest.
		 *
		 * If this mesh is empty, the second index of the range will be negative.
		 */
		TPair<int32, int32> GetVertexRange() const
		{
			return TPair<int32, int32>(MStartIdx, MStartIdx + MNumIndices - 1);
		}

		/**
		 * Extends the vertex range.
		 *
		 * Since the vertex range is built from connectivity, it won't include any 
		 * free vertices that either precede the first vertex, or follow the last.
		 */
		void ExpandVertexRange(const int32 StartIdx, const int32 EndIdx)
		{
			const TPair<int32, int32> CurrRange = GetVertexRange();
			if (StartIdx <= CurrRange.Key && EndIdx >= CurrRange.Value)
			{
				MStartIdx = StartIdx;
				MNumIndices = EndIdx - StartIdx + 1;
			}
		}

		const TArray<TVector<int32, 3>>& GetSurfaceElements() const
		{
			return MElements;
		}

		int32 GetNumElements() const
		{
			return MElements.Num();
		}

		const TSet<uint32>& GetNeighbors(const int32 Element) const
		{
			return MPointToNeighborsMap[Element];
		}

		TSet<int32> GetNRing(const int32 Element, const int32 N) const
		{
			TSet<int32> Neighbors;
			TSet<uint32> LevelNeighbors, PrevLevelNeighbors;
			PrevLevelNeighbors = GetNeighbors(Element);
			for (auto SubElement : PrevLevelNeighbors)
			{
				check(SubElement != Element);
				Neighbors.Add(SubElement);
			}
			for (int32 i = 1; i < N; ++i)
			{
				for (auto SubElement : PrevLevelNeighbors)
				{
					const auto& SubNeighbors = GetNeighbors(SubElement);
					for (auto SubSubElement : SubNeighbors)
					{
						if (!Neighbors.Contains(SubSubElement) && SubSubElement != Element)
						{
							LevelNeighbors.Add(SubSubElement);
						}
					}
				}
				PrevLevelNeighbors = LevelNeighbors;
				LevelNeighbors.Reset();
				for (auto SubElement : PrevLevelNeighbors)
				{
					if (!Neighbors.Contains(SubElement))
					{
						check(SubElement != Element);
						Neighbors.Add(SubElement);
					}
				}
			}
			return Neighbors;
		}

		TArray<Chaos::TVector<int32, 2>> GetUniqueAdjacentPoints() const;
		TArray<Chaos::TVector<int32, 4>> GetUniqueAdjacentElements() const;

		TArray<TVector<T, 3>> GetFaceNormals(const TArrayView<const TVector<T, 3>>& points) const;
		/** Deprecated. Use TArrayView version. */
		TArray<TVector<T, 3>> GetFaceNormals(const TParticles<T, 3>& InParticles) const
		{
			const TArrayCollectionArray<TVector<T, 3>>& X = InParticles.X();
			const TArrayView<const TVector<T, 3>> XV(X.GetData(), X.Num());
			return GetFaceNormals(XV);
		}

		TArray<TVector<T, 3>> GetPointNormals(const TArrayView<const TVector<T, 3>>& points) const;
		/** Deprecated. Use TArrayView version. */
		TArray<TVector<T, 3>> GetPointNormals(const TParticles<T, 3>& InParticles) const
		{
			const TArrayCollectionArray<TVector<T, 3>>& X = InParticles.X();
			const TArrayView<const TVector<T, 3>> XV(X.GetData(), X.Num());
			return GetPointNormals(XV);
		}

		static TTriangleMesh<T> GetConvexHullFromParticles(const TArrayView<const TVector<T, 3>>& points);
		/** Deprecated. Use TArrayView version. */
		static TTriangleMesh<T> GetConvexHullFromParticles(const TParticles<T, 3>& InParticles)
		{
			const TArrayCollectionArray<TVector<T, 3>>& X = InParticles.X();
			const TArrayView<const TVector<T, 3>> XV(const_cast<TVector<T, 3>*>(X.GetData()), X.Num());
			return GetConvexHullFromParticles(XV);
		}

		/**
		 * @ret The connectivity of this mesh represented as a collection of unique segments.
		 */
		TSegmentMesh<T>& GetSegmentMesh();
		/** @ret A map from all face indices, to the indices of their associated edges. */
		const TArray<TVector<int32, 3>>& GetFaceToEdges();
		/** @ret A map from all edge indices, to the indices of their containing faces. */
		const TArray<TVector<int32, 2>>& GetEdgeToFaces();

		/**
		 * @ret Curvature between adjacent faces, specified on edges in radians.
		 * @param faceNormals - a normal per face.
		 * Curvature between adjacent faces is measured by the angle between face normals,
		 * where a curvature of 0 means they're coplanar.
		 */
		TArray<T> GetCurvatureOnEdges(const TArray<TVector<T, 3>>& faceNormals);
		/** @brief Helper that generates face normals on the fly. */
		TArray<T> GetCurvatureOnEdges(const TArrayView<const TVector<T, 3>>& points);

		/**
		 * @ret The maximum curvature at points from connected edges, specified in radians.
		 * @param edgeCurvatures - a curvature per edge.
		 * The greater the number, the sharper the crease. -FLT_MAX denotes free particles.
		 */
		TArray<T> GetCurvatureOnPoints(const TArray<T>& edgeCurvatures);
		/** @brief Helper that generates edge curvatures on the fly. */
		TArray<T> GetCurvatureOnPoints(const TArrayView<const TVector<T, 3>>& points);

		/**
		 * @ret An array of vertex indices ordered from most important to least.
		 * @param Points - point positions.
		 * @param PointCurvatures - a per-point measure of curvature.
		 * @param CoincidentVertices - indices of points that are coincident to another point.
		 * @param RestrictToLocalIndexRange - ignores points outside of the index range used by this mesh.
		 */
		TArray<int32> GetVertexImportanceOrdering(
		    const TArrayView<const TVector<T, 3>>& Points,
		    const TArray<T>& PointCurvatures,
		    TArray<int32>* CoincidentVertices = nullptr,
		    const bool RestrictToLocalIndexRange = false);
		/** @brief Helper that generates point curvatures on the fly. */
		TArray<int32> GetVertexImportanceOrdering(
		    const TArrayView<const TVector<T, 3>>& Points,
		    TArray<int32>* CoincidentVertices = nullptr,
		    const bool RestrictToLocalIndexRange = false);

		/** @brief Reorder vertices according to @param Order. */
		void RemapVertices(const TArray<int32>& Order);

	private:
		int32 GlobalToLocal(int32 GlobalIdx)
		{
			int32 LocalIdx = GlobalIdx - MStartIdx;
			check(LocalIdx >= 0 && LocalIdx < MNumIndices);
			return LocalIdx;
		}

		TArray<TVector<int32, 3>> MElements;

		TMap<int32, TArray<int32>> MPointToTriangleMap;
		TMap<int32, TSet<uint32>> MPointToNeighborsMap;

		TSegmentMesh<T> MSegmentMesh;
		TArray<TVector<int32, 3>> MFaceToEdges;
		TArray<TVector<int32, 2>> MEdgeToFaces;

		int32 MStartIdx;
		int32 MNumIndices;
	};
}
