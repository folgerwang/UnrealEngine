// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SkeletalSimplifierMeshManager.h"



/**
*  Slightly modified version of the quadric simplifier found in MeshSimplifier\Private\MeshSimplify.h
*  that code caries the copyright--
*/
// Copyright (C) 2009 Nine Realms, Inc

#define INVALID_EDGE_ID UINT32_MAX
 
SkeletalSimplifier::FSimplifierMeshManager::FSimplifierMeshManager(const MeshVertType* InSrcVerts, const uint32 InNumSrcVerts,
	const uint32* InSrcIndexes, const uint32 InNumSrcIndexes)
	:
	NumSrcVerts(InNumSrcVerts),
	NumSrcTris(InNumSrcIndexes / 3),
	ReducedNumVerts(InNumSrcVerts),
	ReducedNumTris(InNumSrcIndexes / 3),
	EdgeVertIdHashMap(1 << FMath::Min(16u, FMath::FloorLog2(InNumSrcVerts)))
{
	// Allocate verts and tris

	VertArray = new SimpVertType[NumSrcVerts];
	TriArray  = new SimpTriType[NumSrcTris];


	// Deep copy the verts

	for (uint32 i = 0; i < InNumSrcVerts; ++i)
	{
		VertArray[ i ].vert = InSrcVerts[ i ];
	}

	// Register the verts with the tris

	for (uint32 i = 0; i < (uint32)NumSrcTris; ++i)
	{
		uint32 Offset = 3 * i;
		for (uint32 j = 0; j < 3; ++j)
		{
			uint32 IndexIdx = Offset + j;
			uint32 VertIdx = InSrcIndexes[ IndexIdx ];

			checkSlow(IndexIdx < InNumSrcIndexes);
			checkSlow(VertIdx < InNumSrcVerts);

			TriArray[ i ].verts[ j ] = &VertArray[ VertIdx ];
		}
	}


	// Register each tri with the vert.

	for (int32 i = 0; i < NumSrcTris; ++i)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			SimpTriType* TriPtr = &TriArray[ i ];
			SimpVertType* VertPtr = TriPtr->verts[j];

			VertPtr->adjTris.Add(TriPtr);
		}
	}


	// Group the verts that share the same location.

	GroupVerts(VertArray, NumSrcVerts);

	// Populate EdgeArray

	MakeEdges(VertArray, NumSrcVerts, NumSrcTris, EdgeArray);

	// Links all the edges together
	GroupEdges(EdgeArray);

	{
		EdgeVertIdHashMap.Resize(EdgeArray.Num());
		{
			TArray<int32> HashValues;
			ResizeArray(HashValues, EdgeArray.Num());

			const auto* Edges = EdgeArray.GetData();

			for (int32 i = 0, I = EdgeArray.Num(); i < I; ++i)
			{
				HashValues[i] = HashEdge(Edges[i].v0, Edges[i].v1);
			}
			

			for (int32 i = 0, I = EdgeArray.Num(); i < I; i++)
			{
				EdgeVertIdHashMap.Add(HashValues[i], i);
			}
		}
	}

}

void SkeletalSimplifier::FSimplifierMeshManager::GroupVerts(SimpVertType* Verts, const int32 NumVerts)
{
	// group verts that share a point
	FHashTable HashTable(1 << FMath::Min(16u, FMath::FloorLog2(NumVerts / 2)), NumVerts);

	TArray<uint32> HashValues;
	ResizeArray(HashValues, NumVerts);
	{
		// Compute the hash values

		for (int32 i = 0; i < NumVerts; ++i)
		{
			HashValues[i] = HashPoint(Verts[i].GetPos());
		}

		// insert the hash values.
		for (int i = 0; i < NumVerts; i++)
		{
			HashTable.Add(HashValues[i], i);
		}
	}

	for (int i = 0; i < NumVerts; i++)
	{
		SimpVertType* v1 = &Verts[i];

		// already grouped
		if (v1->next != v1)
		{
			continue;
		}

		// find any matching verts
		const uint32 hash = HashValues[i];


		for (int j = HashTable.First(hash); HashTable.IsValid(j); j = HashTable.Next(j))
		{

			SimpVertType* v2 = &Verts[j];

			if (v1 == v2)
				continue;

			// link
			if (v1->GetPos() == v2->GetPos())
			{
				checkSlow(v2->next == v2);
				checkSlow(v2->prev == v2);

				// insert v2 after v1
				v2->next = v1->next;
				v2->next->prev = v2;

				v2->prev = v1;
				v1->next = v2;


			}
		}
	}
}


void SkeletalSimplifier::FSimplifierMeshManager::MakeEdges(const SimpVertType* Verts, const int32 NumVerts, const int32 NumTris, TArray<SimpEdgeType>& Edges)
{

	// Populate the TArray of edges.

	int32 maxEdgeSize = FMath::Min(3 * NumTris, 3 * NumVerts - 6);
	Edges.Empty(maxEdgeSize);
	for (int i = 0; i < NumVerts; i++)
	{
		AppendConnectedEdges(&Verts[i], Edges);
	}

	// Guessed wrong on num edges. Array was resized so fix up pointers.

	if (Edges.Num() > maxEdgeSize)
	{
	
		for (int32 i = 0; i < Edges.Num(); ++i)
		{
			SimpEdgeType& edge = Edges[i];
			edge.next = &edge;
			edge.prev = &edge;
		}
	}

}

void SkeletalSimplifier::FSimplifierMeshManager::AppendConnectedEdges(const SimpVertType* Vert, TArray< SimpEdgeType >& Edges)
{


	// Need to cast the vert - but the method we are calling on it really should be const..
	SimpVertType* V = const_cast<SimpVertType*>(Vert);

	checkSlow(V->adjTris.Num() > 0);

	TArray< SimpVertType*, TInlineAllocator<64> > adjVerts;
	V->FindAdjacentVerts(adjVerts);

	SimpVertType* v0 = V;
	for (SimpVertType* v1 : adjVerts)
	{
		if (v0 < v1)
		{
			checkSlow(v0->GetMaterialIndex() == v1->GetMaterialIndex());

			// add edge
			Edges.AddDefaulted();
			SimpEdgeType& edge = Edges.Last();
			edge.v0 = v0;
			edge.v1 = v1;
		}
	}
}


void SkeletalSimplifier::FSimplifierMeshManager::GroupEdges(TArray< SimpEdgeType >& Edges)
{
	FHashTable HashTable(1 << FMath::Min(16u, FMath::FloorLog2(Edges.Num() / 2)), Edges.Num());

	TArray<uint32> HashValues;
	ResizeArray(HashValues, Edges.Num());

	
	{
		for (int32 i = 0, I = Edges.Num(); i < I; ++i)
		{
			uint32 Hash0 = HashPoint(Edges[i].v0->GetPos());
			uint32 Hash1 = HashPoint(Edges[i].v1->GetPos());
			HashValues[i] = Murmur32({ FMath::Min(Hash0, Hash1), FMath::Max(Hash0, Hash1) });
		}

	}

	for (int32 i = 0, IMax = Edges.Num(); i < IMax; ++i)
	{
		HashTable.Add(HashValues[i], i);
	}

	for (int32 i = 0, IMax = Edges.Num(); i < IMax; ++i)
	{
		// already grouped
		if (Edges[i].next != &Edges[i])
		{
			continue;
		}

		// find any matching edges
		uint32 Hash = HashValues[i];
		for (uint32 j = HashTable.First(Hash); HashTable.IsValid(j); j = HashTable.Next(j))
		{
			SimpEdgeType* e1 = &Edges[i];
			SimpEdgeType* e2 = &Edges[j];

			if (e1 == e2)
				continue;

			bool m1 =
				(e1->v0 == e2->v0 || e1->v0->GetPos() == e2->v0->GetPos()) &&
				(e1->v1 == e2->v1 || e1->v1->GetPos() == e2->v1->GetPos());

			bool m2 =
				(e1->v0 == e2->v1 || e1->v0->GetPos() == e2->v1->GetPos()) &&
				(e1->v1 == e2->v0 || e1->v1->GetPos() == e2->v0->GetPos());


			// backwards
			if (m2)
			{
				Swap(e2->v0, e2->v1);
			}

			

			// link
			if (m1 || m2)
			{
		
				checkSlow(e2->next == e2);
				checkSlow(e2->prev == e2);

				e2->next = e1->next;
				e2->prev = e1;
				e2->next->prev = e2;
				e2->prev->next = e2;
			}
		}
	}
}

void SkeletalSimplifier::FSimplifierMeshManager::GetCoincidentVertGroups(VertPtrArray& CoincidentVertGroups)
{
	for (int32 vId = 0; vId < NumSrcVerts; ++vId)
	{
		SimpVertType* Vert = &VertArray[vId];

		// Removed vert
		if (Vert->TestFlags(SIMP_REMOVED))
		{
			continue;
		}

		// Single vert
		if (Vert->next == Vert && Vert->prev == Vert)
		{
			continue;
		}

		// Find and add the max vert in this group.
		{
			SimpVertType* tmp = Vert;
			SimpVertType* maxVert = Vert;
			while (tmp->next != Vert)
			{
				tmp = tmp->next;
				checkSlow(!tmp->TestFlags(SIMP_REMOVED));
				if (tmp > maxVert)
				{
					maxVert = tmp;
				}
			}

			CoincidentVertGroups.AddUnique(maxVert);
		}
	}
}

// @todo, this shares a lot of code with GroupEdges - they should be unified..
void SkeletalSimplifier::FSimplifierMeshManager::RebuildEdgeLinkLists(EdgePtrArray& CandidateEdgePtrArray)
{
	const uint32 NumEdges = CandidateEdgePtrArray.Num();
	// Fix edge groups - when one edge of a triangle collapses the opposing edges end up merging.. this accounts for that i think.
	{
		FHashTable HashTable(128, NumEdges);

		// ungroup edges
		for (uint32 i = 0; i < NumEdges; ++i)
		{
			SimpEdgeType* edge = CandidateEdgePtrArray[i];

			if (edge->TestFlags(SIMP_REMOVED))
				continue;

			edge->next = edge;
			edge->prev = edge;
		}

		// Hash Edges.
		for (uint32 i = 0; i < NumEdges; ++i)
		{
			SimpEdgeType& edge = *CandidateEdgePtrArray[i];

			if (edge.TestFlags(SIMP_REMOVED))
				continue;

			HashTable.Add(HashEdgePosition(edge), i);
		}


		// regroup edges
		for (uint32 i = 0; i < NumEdges; ++i)
		{
			SimpEdgeType* edge = CandidateEdgePtrArray[i];

			if (edge->TestFlags(SIMP_REMOVED))
				continue;

			// already grouped
			if (edge->next != edge)
				continue;

			// find any matching edges
			uint32 hash = HashEdgePosition(*edge);
			SimpEdgeType* e1 = edge;
			for (uint32 j = HashTable.First(hash); HashTable.IsValid(j); j = HashTable.Next(j))
			{

				SimpEdgeType* e2 = CandidateEdgePtrArray[j];

				if (e1 == e2)
					continue;

				bool m1 =
					(e1->v0 == e2->v0 &&
						e1->v1 == e2->v1)
					||
					(e1->v0->GetPos() == e2->v0->GetPos() &&
						e1->v1->GetPos() == e2->v1->GetPos());

				bool m2 =
					(e1->v0 == e2->v1 &&
						e1->v1 == e2->v0)
					||
                    (e1->v0->GetPos() == e2->v1->GetPos() &&
	                e1->v1->GetPos() == e2->v0->GetPos());

				// backwards
				if (m2)
				Swap(e2->v0, e2->v1);

				// link
				if (m1 || m2)
				{
					checkSlow(e2->next == e2);
					checkSlow(e2->prev == e2);

					e2->next = e1->next;
					e2->prev = e1;
					e2->next->prev = e2;
					e2->prev->next = e2;
				}
			}
		}
	}
}


void SkeletalSimplifier::FSimplifierMeshManager::FlagBoundary(const ESimpElementFlags Flag)
{
	check(Flag == ESimpElementFlags::SIMP_LOCKED);

	TArray< SimpVertType*, TInlineAllocator<64> > adjVerts;

	for (int i = 0; i < NumSrcVerts; i++)
	{

		SimpVertType* v0 = &VertArray[i];
		checkSlow(v0 != NULL);
		check(v0->adjTris.Num() > 0);

		adjVerts.Reset();
		v0->FindAdjacentVertsGroup(adjVerts);

		for (SimpVertType* v1 : adjVerts)
		{
			if (v0 < v1)
			{

				// set if this edge is boundary
				// find faces that share v0 and v1
				v0->EnableAdjTriFlagsGroup(SIMP_MARK1);
				v1->DisableAdjTriFlagsGroup(SIMP_MARK1);

				int faceCount = 0;
				SimpVertType* vert = v0;
				do
				{
					for (TriIterator j = vert->adjTris.Begin(); j != vert->adjTris.End(); ++j)
					{
						SimpTriType* tri = *j;
						faceCount += tri->TestFlags(SIMP_MARK1) ? 0 : 1;
					}
					vert = vert->next;
				} while (vert != v0);

				v0->DisableAdjTriFlagsGroup(SIMP_MARK1);

				if (faceCount == 1)
				{
					// only one face on this edge
					v0->EnableFlagsGroup(Flag);
					v1->EnableFlagsGroup(Flag);
				}
			}
		}
	}
}

void SkeletalSimplifier::FSimplifierMeshManager::FlagBoxCorners(const ESimpElementFlags Flag)
{


	uint32* VisitedMask = new uint32[NumSrcVerts](); // initializes to zeros.

	for (int32 i = 0; i < NumSrcVerts; i++)
	{

		if (VisitedMask[i])
		{
			continue;
		}

		// Collect all the face normals associated with this vertgroup

		TArray<FVector, TInlineAllocator<6>> FaceNormals;

		SimpVertType* Vert = GetVertPtr(i);
		SimpVertType* seedVert = Vert;
		do {

			for (TriIterator triIter = Vert->adjTris.Begin(); triIter != Vert->adjTris.End(); ++triIter)
			{
				bool bIsDuplicate = false;

				SimpTriType* tri = *triIter;

				FVector Nrml = tri->GetNormal();
			
				for (int32 fnIdx = 0; fnIdx < FaceNormals.Num(); ++fnIdx)
				{
					FVector ExistingNormal = FaceNormals[fnIdx];
					ExistingNormal.Normalize();
					float DotValue = FVector::DotProduct(ExistingNormal, Nrml);

					if (1.f - DotValue < 0.133975f) // 30 degrees.
					{
						// we already have this vector.
						// could be a corner
						bIsDuplicate = true;
						FaceNormals[fnIdx] += Nrml;
						continue;

					}

				}

				if (!bIsDuplicate)
				{
					FaceNormals.Add(Nrml);
				}
			}

			// mark as visited.

			uint32 Idx = GetVertIndex(Vert);
			VisitedMask[Idx] = 1;

			Vert = Vert->next;
		} while (Vert != seedVert);



		int32 FaceCount = FaceNormals.Num();

		if ( FaceNormals.Num() == 3 )
		{
			FVector& A = FaceNormals[0];
			FVector& B = FaceNormals[1];
			FVector& C = FaceNormals[2];

			A.Normalize();
			B.Normalize();
			C.Normalize();

			float AdotB = FVector::DotProduct(A, B);
			float BdotC = FVector::DotProduct(B, C);
			float AdotC = FVector::DotProduct(A, C);

			if (FMath::Abs(AdotB) < 0.259f && FMath::Abs(BdotC) < 0.259f && FMath::Abs(AdotC) < 0.259f) // 15-degrees off normal
			{
				Vert->EnableFlagsGroup(Flag);
			}
			
		}

	
	}

	if (VisitedMask) delete[] VisitedMask;
}

void SkeletalSimplifier::FSimplifierMeshManager::GetAdjacentTopology(const SimpVertType* VertPtr,
	TriPtrArray& DirtyTris, VertPtrArray& DirtyVerts, EdgePtrArray& DirtyEdges)
{
	// need this cast because the const version is missing on the vert..

	SimpVertType* v = const_cast<SimpVertType*>(VertPtr);

	// Gather pointers to all the triangles that share this vert.

	// Update all tris touching collapse edge.
	for (TriIterator triIter = v->adjTris.Begin(); triIter != v->adjTris.End(); ++triIter)
	{
		DirtyTris.AddUnique(*triIter);
	}

	// Gather all verts that are adjacent to this one.

	TArray< SimpVertType*, TInlineAllocator<64> > adjVerts;
	v->FindAdjacentVerts(adjVerts);


	// Gather verts that are adjacent to VertPtr
	for (int i = 0, Num = adjVerts.Num(); i < Num; i++)
	{
		DirtyVerts.AddUnique(adjVerts[i]);
	}



	// Gather verts that are adjacent to VertPtr
	for (int i = 0, Num = adjVerts.Num(); i < Num; i++)
	{
		adjVerts[i]->EnableFlags(SIMP_MARK2);
	}

	// update the costs of all edges connected to any face adjacent to v
	for (int i = 0, iMax = adjVerts.Num(); i < iMax; ++i)
	{
		SimpVertType* AdjVert = adjVerts[i];
		AdjVert->EnableAdjVertFlags(SIMP_MARK1);

		for (TriIterator triIter = AdjVert->adjTris.Begin(); triIter != AdjVert->adjTris.End(); ++triIter)
		{
			SimpTriType* tri = *triIter;
			for (int k = 0; k < 3; k++)
			{
				SimpVertType* vert = tri->verts[k];
				if (vert->TestFlags(SIMP_MARK1) && !vert->TestFlags(SIMP_MARK2) && vert != AdjVert)
				{
					SimpEdgeType* edge = FindEdge(AdjVert, vert);
					DirtyEdges.AddUnique(edge);
				}
				vert->DisableFlags(SIMP_MARK1);
			}
		}
		AdjVert->DisableFlags(SIMP_MARK2);
	}


}

void SkeletalSimplifier::FSimplifierMeshManager::GetAdjacentTopology(const SimpEdgeType& GroupedEdge,
	TriPtrArray& DirtyTris, VertPtrArray& DirtyVerts, EdgePtrArray& DirtyEdges)
{
	// Find the parts of the mesh that will be 'dirty' after the 
	// edge collapse. 

	const SimpVertType* v = GroupedEdge.v0;
	do {
		GetAdjacentTopology(v, DirtyTris, DirtyVerts, DirtyEdges);
		v = v->next;
	} while (v != GroupedEdge.v0);

	v = GroupedEdge.v1;
	do {
		GetAdjacentTopology(v, DirtyTris, DirtyVerts, DirtyEdges);
		v = v->next;
	} while (v != GroupedEdge.v1);

}

int32 SkeletalSimplifier::FSimplifierMeshManager::RemoveEdgeIfInvalid(EdgePtrArray& CandidateEdges, IdxArray& RemovedEdgeIdxArray)
{

	const uint32 NumCandidateEdges = CandidateEdges.Num();
	for (uint32 i = 0; i < NumCandidateEdges; ++i)
	{
		SimpEdgeType* EdgePtr = CandidateEdges[i];
		
		// DJH - added 6/29/18
		if (!EdgePtr) continue;
		
		if (IsInvalid(EdgePtr))
		{
			const uint32 Idx = RemoveEdge(*EdgePtr);

			// Record the index of the edge we remove.
			if (Idx < INVALID_EDGE_ID)
			{
				RemovedEdgeIdxArray.AddUnique(Idx); // djh changed from Add()
			}
			CandidateEdges[i] = NULL;
		}
		else
		{
			checkSlow(!EdgePtr->TestFlags(SIMP_REMOVED));
		}
	}

	return RemovedEdgeIdxArray.Num();
}

uint32 SkeletalSimplifier::FSimplifierMeshManager::RemoveEdge(const SimpVertType* VertAPtr, const SimpVertType* VertBPtr)
{
	auto HashAndIdx = GetEdgeHashPair(VertAPtr, VertBPtr);

	uint32 Idx = HashAndIdx.Key;
	// Early out if this edge doesn't exist.
	if (Idx == INVALID_EDGE_ID)
	{
		return Idx;
	}

	SimpEdgeType& Edge = EdgeArray[Idx];
	if (Edge.TestFlags(SIMP_REMOVED))
	{
		Idx = INVALID_EDGE_ID;
	}
	else
	{
		// mark as removed
		Edge.EnableFlags(SIMP_REMOVED);
		EdgeVertIdHashMap.Remove(HashAndIdx.Value, Idx);

	}

	// remove this edge from its edge group
	Edge.prev->next = Edge.next;
	Edge.next->prev = Edge.prev;

	Edge.next = &Edge;
	Edge.prev = &Edge;

	// return the Idx
	return Idx;
}

uint32 SkeletalSimplifier::FSimplifierMeshManager::RemoveEdge(SimpEdgeType& Edge)
{
	// remove this edge from its edge group
	Edge.prev->next = Edge.next;
	Edge.next->prev = Edge.prev;

	Edge.next = &Edge;
	Edge.prev = &Edge;

	uint32 Idx = GetEdgeIndex(&Edge);

	if (Edge.TestFlags(SIMP_REMOVED))
	{
		Idx = INVALID_EDGE_ID;
	}
	else
	{

		// mark as removed
		Edge.EnableFlags(SIMP_REMOVED);

		uint32 Hash = HashEdge(Edge.v0, Edge.v1);

		EdgeVertIdHashMap.Remove(Hash, Idx);
	}
	// return the Idx
	return Idx;
}

uint32 SkeletalSimplifier::FSimplifierMeshManager::ReplaceVertInEdge(const SimpVertType* VertAPtr, const SimpVertType* VertBPtr, SimpVertType* VertAprimePtr)
{

	TPair<uint32, uint32> HashAndIdx = GetEdgeHashPair(VertAPtr, VertBPtr);
	const uint32 Idx = HashAndIdx.Key;
	const uint32 HashValue = HashAndIdx.Value;

	checkSlow(Idx != INVALID_EDGE_ID);
	SimpEdgeType* edge = &EdgeArray[Idx];

	EdgeVertIdHashMap.Remove(HashValue, Idx);

	EdgeVertIdHashMap.Add(HashEdge(VertAprimePtr, VertBPtr), Idx);

	if (edge->v0 == VertAPtr)
		edge->v0 = VertAprimePtr;
	else
		edge->v1 = VertAprimePtr;

	return Idx;
}

int32 SkeletalSimplifier::FSimplifierMeshManager::RemoveIfDegenerate(TriPtrArray& CandidateTrisPtrArray)
{
	int32 NumRemoved = 0;
	// remove degenerate triangles
	// not sure why this happens
	for (SimpTriType* CandidateTriPtr : CandidateTrisPtrArray)
	{
		if (CandidateTriPtr->TestFlags(SIMP_REMOVED))
			continue;


		const FVector& p0 = CandidateTriPtr->verts[0]->GetPos();
		const FVector& p1 = CandidateTriPtr->verts[1]->GetPos();
		const FVector& p2 = CandidateTriPtr->verts[2]->GetPos();
		const FVector n = (p2 - p0) ^ (p1 - p0);

		if (n.SizeSquared() == 0.0f)
		{
			NumRemoved++;
			CandidateTriPtr->EnableFlags(SIMP_REMOVED);

			// remove references to tri
			for (int j = 0; j < 3; j++)
			{
				SimpVertType* vert = CandidateTriPtr->verts[j];
				vert->adjTris.Remove(CandidateTriPtr);
				// orphaned verts are removed below
			}
		}
	}

	ReducedNumTris -= NumRemoved;
	return NumRemoved;
}



int32 SkeletalSimplifier::FSimplifierMeshManager::RemoveDegenerateTris()
{

	TriPtrArray TriPtrs;
	ResizeArray(TriPtrs, NumSrcTris);

	{
		for (int32 i = 0, IMax = NumSrcTris; i < IMax; ++i)
		{
			TriPtrs[i] = &TriArray[i];
		}
	}

	return RemoveIfDegenerate(TriPtrs);
}

int32 SkeletalSimplifier::FSimplifierMeshManager::RemoveIfDegenerate(VertPtrArray& CandidateVertPtrArray)
{
	int32 NumRemoved = 0;
	// remove orphaned verts
	for (SimpVertType* VertPtr : CandidateVertPtrArray)
	{
		if (VertPtr->TestFlags(SIMP_REMOVED))
			continue;

		if (VertPtr->adjTris.Num() == 0)
		{
			NumRemoved++;
			VertPtr->EnableFlags(SIMP_REMOVED);

			// ungroup
			VertPtr->prev->next = VertPtr->next;
			VertPtr->next->prev = VertPtr->prev;
			VertPtr->next = VertPtr;
			VertPtr->prev = VertPtr;
		}
	}

	ReducedNumVerts -= NumRemoved;
	return NumRemoved;
}

int32 SkeletalSimplifier::FSimplifierMeshManager::RemoveDegenerateVerts()
{

	VertPtrArray VertPtrs;
	ResizeArray(VertPtrs, NumSrcVerts);

	{

		for (int32 i = 0, IMax = NumSrcVerts; i < IMax; ++i)
		{
			VertPtrs[i] = &VertArray[i];
		}
	}
	return RemoveIfDegenerate(VertPtrs);
}

int32 SkeletalSimplifier::FSimplifierMeshManager::RemoveIfDegenerate(EdgePtrArray& CandidateEdges, IdxArray& RemovedEdgeIdxArray)
{
	const uint32 NumCandidateEdges = CandidateEdges.Num();

	// add all grouped edges
	for (uint32 i = 0; i < NumCandidateEdges; i++)
	{
		SimpEdgeType* edge = CandidateEdges[i];

		if (edge->TestFlags(SIMP_REMOVED))
			continue;

		SimpEdgeType* e = edge;
		do {
			CandidateEdges.AddUnique(e);
			e = e->next;
		} while (e != edge);
	}

	// remove dead edges from our edge hash.
	for (uint32 i = 0, Num = CandidateEdges.Num(); i < Num; i++)
	{
		SimpEdgeType* edge = CandidateEdges[i];

		if (edge->TestFlags(SIMP_REMOVED))
			continue;

		if (edge->v0 == edge->v1)
		{
			edge->EnableFlags(SIMP_REMOVED); // djh 8/3/18.  not sure why this happens
											 
			uint32 Idx = RemoveEdge(*edge);
			if (Idx < INVALID_EDGE_ID)
			{
				RemovedEdgeIdxArray.Add(Idx);
			}

		}

		if (edge->v0->TestFlags(SIMP_REMOVED) ||
			edge->v1->TestFlags(SIMP_REMOVED))
		{
			//RemoveEdgeFromLocationMapAndHeap(edge);
			uint32 Idx = RemoveEdge(*edge);
			if (Idx < INVALID_EDGE_ID)
			{
				RemovedEdgeIdxArray.Add(Idx);
			}
		}
	}

	return RemovedEdgeIdxArray.Num();
}



void SkeletalSimplifier::FSimplifierMeshManager::CollapseEdge(SimpEdgeType * EdgePtr, IdxArray& RemovedEdgeIdxArray)
{
	SimpVertType* v0 = EdgePtr->v0;
	SimpVertType* v1 = EdgePtr->v1;

	// Collapse the edge uv by moving vertex v0 onto v1
	checkSlow(v0 && v1);
	checkSlow(EdgePtr == FindEdge(v0, v1));

	checkSlow(v0->adjTris.Num() > 0);
	checkSlow(v1->adjTris.Num() > 0);
	checkSlow(v0->GetMaterialIndex() == v1->GetMaterialIndex());

	// Because another edge in the same edge group may share a vertex with this edge
	// and it might have already been collapsed, we can't do this check
	//checkSlow(! (v0->TestFlags(SIMP_LOCKED) && v1->TestFlags(SIMP_LOCKED)) );


    // Verify that this is truly an edge of a triangle

	v0->EnableAdjVertFlags(SIMP_MARK1);
	v1->DisableAdjVertFlags(SIMP_MARK1);

	if (v0->TestFlags(SIMP_MARK1))
	{
		// Invalid edge results from collapsing a bridge tri
		// There are no actual triangles connecting these verts
		v0->DisableAdjVertFlags(SIMP_MARK1);
		return;
	}

	// update edges from v0 to v1

	// Note, the position and other attributes have already been corrected
	// to have the same values.  Here we are just propagating any locked state.
	if (v0->TestFlags(SIMP_LOCKED))
		v1->EnableFlags(SIMP_LOCKED);


	// Update 'other'-u edges to 'other'-v edges ( where other != v) 

	for (TriIterator triIter = v0->adjTris.Begin(); triIter != v0->adjTris.End(); ++triIter)
	{
		SimpTriType* TriPtr = *triIter;
		for (int j = 0; j < 3; j++)
		{
			SimpVertType* VertPtr = TriPtr->verts[j];
			if (VertPtr->TestFlags(SIMP_MARK1))
			{

				// replace v0-vert with v1-vert
				// first remove v1-vert if it already exists ( it shouldn't..)
				{
					uint32 Idx = RemoveEdge(VertPtr, v1);
					if (Idx < INVALID_EDGE_ID)
					{
						RemovedEdgeIdxArray.AddUnique(Idx);
					}

					ReplaceVertInEdge(v0, VertPtr, v1);
				}
				VertPtr->DisableFlags(SIMP_MARK1);
			}
		}
	}

	// For faces with verts: v0, v1, other
	// remove the v0-other edges.
	v0->EnableAdjVertFlags(SIMP_MARK1);
	v0->DisableFlags(SIMP_MARK1);
	v1->DisableFlags(SIMP_MARK1);

	for (TriIterator triIter = v1->adjTris.Begin(); triIter != v1->adjTris.End(); ++triIter)
	{
		SimpTriType* TriPtr = *triIter;
		for (int j = 0; j < 3; j++)
		{
			SimpVertType* VertPtr = TriPtr->verts[j];
			if (VertPtr->TestFlags(SIMP_MARK1))
			{
				const uint32 Idx = RemoveEdge(v0, VertPtr);
				if (Idx < INVALID_EDGE_ID)
				{
					RemovedEdgeIdxArray.AddUnique(Idx);
				}
				//
				VertPtr->DisableFlags(SIMP_MARK1);
			}
		}
	}

	v1->DisableAdjVertFlags(SIMP_MARK1);

	// Remove collapsed triangles, and fix-up the others that now use v instead of u triangles

	TriPtrArray v0AdjTris;
	{
		uint32 i = 0;
		ResizeArray(v0AdjTris, v0->adjTris.Num());


		for (TriIterator triIter = v0->adjTris.Begin(); triIter != v0->adjTris.End(); ++triIter)
		{
			v0AdjTris[i] = *triIter;
			i++;
		}
	}

	for (int32 i = 0, iMax = v0AdjTris.Num(); i < iMax; ++i)
	{
		SimpTriType* TriPtr = v0AdjTris[i];

		checkSlow(!TriPtr->TestFlags(SIMP_REMOVED));
		checkSlow(TriPtr->HasVertex(v0));

		if (TriPtr->HasVertex(v1))  // tri shared by v0 and v1.. 
		{
			// delete triangles on edge uv
			ReducedNumTris--;
			RemoveTri(*TriPtr);
		}
		else
		{
			// update triangles to have v1 instead of v0
			ReplaceTriVertex(*TriPtr, *v0, *v1);
		}
	}


	// remove modified verts and tris from cache
	v1->EnableAdjVertFlags(SIMP_MARK1);
	for (TriIterator triIter = v1->adjTris.Begin(); triIter != v1->adjTris.End(); ++triIter)
	{
		SimpTriType* TriPtr = *triIter;

		for (int i = 0; i < 3; i++)
		{
			SimpVertType* VertPtr = TriPtr->verts[i];
			if (VertPtr->TestFlags(SIMP_MARK1))
			{
				VertPtr->DisableFlags(SIMP_MARK1);
			}
		}
	}

	// mark v0 as dead.

	v0->adjTris.Clear();	// u has been removed
	v0->EnableFlags(SIMP_REMOVED);

	// Remove the actual edge.
	const uint32 Idx = RemoveEdge(*EdgePtr);
	if (Idx < INVALID_EDGE_ID)
	{
		RemovedEdgeIdxArray.AddUnique(Idx);
	}

	// record the reduced number of verts

	ReducedNumVerts--;
}

void SkeletalSimplifier::FSimplifierMeshManager::OutputMesh(MeshVertType* verts, uint32* indexes, TArray<int32>* LockedVerts)
{



	int32 NumValidVerts = 0;
	for (int32 i = 0; i < NumSrcVerts; i++)
		NumValidVerts += VertArray[i].TestFlags(SIMP_REMOVED) ? 0 : 1;

	check(NumValidVerts <= ReducedNumVerts);


	FHashTable HashTable(4096, NumValidVerts);
	int32 numV = 0;
	int32 numI = 0;

	for (int32 i = 0; i < NumSrcTris; i++)
	{
		if (TriArray[i].TestFlags(SIMP_REMOVED))
			continue;

		// TODO this is sloppy. There should be no duped verts. Alias by index.
		for (int j = 0; j < 3; j++)
		{
			SimpVertType* vert = TriArray[i].verts[j];
			checkSlow(!vert->TestFlags(SIMP_REMOVED));
			checkSlow(vert->adjTris.Num() != 0);

			const FVector& p = vert->GetPos();
			uint32 hash = HashPoint(p);
			uint32 f;
			for (f = HashTable.First(hash); HashTable.IsValid(f); f = HashTable.Next(f))
			{
				if (vert->vert == verts[f])
					break;
			}
			if (!HashTable.IsValid(f))
			{
				// export the id of the locked vert.
				if (LockedVerts != NULL && vert->TestFlags(SIMP_LOCKED))
				{
					LockedVerts->Push(numV);
				}

				HashTable.Add(hash, numV);
				verts[numV] = vert->vert;
				indexes[numI++] = numV;
				numV++;
			}
			else
			{
				indexes[numI++] = f;
			}
		}
	}

#if 0
	check(numV <= NumValidVerts);
	check(numI <= numTris * 3);

	numVerts = numV;
	numTris = numI / 3;
#endif 
}


int32 SkeletalSimplifier::FSimplifierMeshManager::CountDegeneratesTris() const
{
	int32 DegenerateCount = 0;
	// remove degenerate triangles
	// not sure why this happens
	for (int i = 0; i < NumSrcTris; i++)
	{
		SimpTriType* tri = &TriArray[i];

		if (tri->TestFlags(SIMP_REMOVED))
			continue;

		const FVector& p0 = tri->verts[0]->GetPos();
		const FVector& p1 = tri->verts[1]->GetPos();
		const FVector& p2 = tri->verts[2]->GetPos();
		const FVector n = (p2 - p0) ^ (p1 - p0);

		if (n.SizeSquared() == 0.0f)
		{
			DegenerateCount++;
		}
	}

	return DegenerateCount;
}

int32 SkeletalSimplifier::FSimplifierMeshManager::CountDegenerateEdges() const
{

	int32 DegenerateCount = 0;
	const SkeletalSimplifier::SimpEdgeType*  Edges = EdgeArray.GetData();
	int32 NumEdges = EdgeArray.Num();

	for (int32 i = 0; i < NumEdges; ++i)
	{
		const SkeletalSimplifier::SimpEdgeType& Edge = Edges[i];

		if (Edge.TestFlags(SIMP_REMOVED))
			continue;

		if (Edge.v0 == Edge.v1)
		{
			DegenerateCount++;
		}

	}

	return DegenerateCount;
}