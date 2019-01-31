// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LODUtilities.h"
#include "Misc/MessageDialog.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/MorphTarget.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "GenericQuadTree.h"
#include "Engine/SkeletalMesh.h"
#include "EditorFramework/AssetImportData.h"
#include "MeshUtilities.h"

#if WITH_APEX_CLOTHING
	#include "ApexClothingUtils.h"
#endif // #if WITH_APEX_CLOTHING

#include "ComponentReregisterContext.h"
#include "IMeshReductionManagerModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogLODUtilities, Log, All);

bool FLODUtilities::RegenerateLOD(USkeletalMesh* SkeletalMesh, int32 NewLODCount /*= 0*/, bool bRegenerateEvenIfImported /*= false*/, bool bGenerateBaseLOD /*= false*/)
{
	if (SkeletalMesh)
	{
		int32 LODCount = SkeletalMesh->GetLODNum();

		if (NewLODCount > 0)
		{
			LODCount = NewLODCount;
		}

		SkeletalMesh->Modify();

		FSkeletalMeshUpdateContext UpdateContext;
		UpdateContext.SkeletalMesh = SkeletalMesh;

		// remove LODs
		int32 CurrentNumLODs = SkeletalMesh->GetLODNum();
		if (LODCount < CurrentNumLODs)
		{
			for (int32 LODIdx = CurrentNumLODs - 1; LODIdx >= LODCount; LODIdx--)
			{
				FLODUtilities::RemoveLOD(UpdateContext, LODIdx);
			}
		}
		// we need to add more
		else if (LODCount > CurrentNumLODs)
		{
			// Only create new skeletal mesh LOD level entries
			for (int32 LODIdx = CurrentNumLODs; LODIdx < LODCount; LODIdx++)
			{
				// if no previous setting found, it will use default setting. 
				FLODUtilities::SimplifySkeletalMeshLOD(UpdateContext, LODIdx);
			}
		}
		else
		{
			for (int32 LODIdx = 0; LODIdx < LODCount; LODIdx++)
			{
				FSkeletalMeshLODInfo& CurrentLODInfo = *(SkeletalMesh->GetLODInfo(LODIdx));
				if ((bRegenerateEvenIfImported && LODIdx > 0) || (bGenerateBaseLOD && LODIdx == 0) || CurrentLODInfo.bHasBeenSimplified )
				{
					FLODUtilities::SimplifySkeletalMeshLOD(UpdateContext, LODIdx);
				}
			}
		}

		SkeletalMesh->PostEditChange();

		return true;
	}

	return false;
}

void FLODUtilities::RemoveLOD(FSkeletalMeshUpdateContext& UpdateContext, int32 DesiredLOD )
{
	USkeletalMesh* SkeletalMesh = UpdateContext.SkeletalMesh;
	FSkeletalMeshModel* SkelMeshModel = SkeletalMesh->GetImportedModel();

	if(SkelMeshModel->LODModels.Num() == 1 )
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "NoLODToRemove", "No LODs to remove!") );
		return;
	}

	// Now display combo to choose which LOD to remove.
	TArray<FString> LODStrings;
	LODStrings.AddZeroed(SkelMeshModel->LODModels.Num()-1 );
	for(int32 i=0; i<SkelMeshModel->LODModels.Num()-1; i++)
	{
		LODStrings[i] = FString::Printf( TEXT("%d"), i+1 );
	}

	check( SkeletalMesh->GetLODNum() == SkelMeshModel->LODModels.Num() );

	// If its a valid LOD, kill it.
	if( DesiredLOD > 0 && DesiredLOD < SkelMeshModel->LODModels.Num() )
	{
		//We'll be modifying the skel mesh data so reregister

		//TODO - do we need to reregister something else instead?
		FMultiComponentReregisterContext ReregisterContext(UpdateContext.AssociatedComponents);

		// Release rendering resources before deleting LOD
		SkeletalMesh->ReleaseResources();

		// Block until this is done
		FlushRenderingCommands();

		SkelMeshModel->LODModels.RemoveAt(DesiredLOD);
		SkeletalMesh->RemoveLODInfo(DesiredLOD);
		SkeletalMesh->InitResources();

		RefreshLODChange(SkeletalMesh);

		// Set the forced LOD to Auto.
		for(auto Iter = UpdateContext.AssociatedComponents.CreateIterator(); Iter; ++Iter)
		{
			USkinnedMeshComponent* SkinnedComponent = Cast<USkinnedMeshComponent>(*Iter);
			if(SkinnedComponent)
			{
				SkinnedComponent->ForcedLodModel = 0;
			}
		}

		//remove all Morph target data for this LOD
		for (UMorphTarget* MorphTarget : SkeletalMesh->MorphTargets)
		{
			if (MorphTarget->HasDataForLOD(DesiredLOD))
			{
				MorphTarget->MorphLODModels.RemoveAt(DesiredLOD);
			}
		}

		// This will recache derived render data, and re-init resources
		SkeletalMesh->PostEditChange();

		//Notify calling system of change
		UpdateContext.OnLODChanged.ExecuteIfBound();

		// Mark things for saving.
		SkeletalMesh->MarkPackageDirty();
	}
}

/** Given three direction vectors, indicates if A and B are on the same 'side' of Vec. */
bool VectorsOnSameSide(const FVector2D& Vec, const FVector2D& A, const FVector2D& B)
{
	return !FMath::IsNegativeFloat(((B.Y - A.Y)*(Vec.X - A.X)) + ((A.X - B.X)*(Vec.Y - A.Y)));
}

float PointToSegmentDistanceSquare(const FVector2D& A, const FVector2D& B, const FVector2D& P)
{
	return FVector2D::DistSquared(P, FMath::ClosestPointOnSegment2D(P, A, B));
}

/** Return true if P is within triangle created by A, B and C. */
bool PointInTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& P)
{
	//If the point is on a triangle point we consider the point inside the triangle
	
	if (P.Equals(A) || P.Equals(B) || P.Equals(C))
	{
		return true;
	}
	// If its on the same side as the remaining vert for all edges, then its inside.	
	if (VectorsOnSameSide(A, B, P) &&
		VectorsOnSameSide(B, C, P) &&
		VectorsOnSameSide(C, A, P))
	{
		return true;
	}

	//Make sure point on the edge are count inside the triangle
	if (PointToSegmentDistanceSquare(A, B, P) <= KINDA_SMALL_NUMBER)
	{
		return true;
	}
	if (PointToSegmentDistanceSquare(B, C, P) <= KINDA_SMALL_NUMBER)
	{
		return true;
	}
	if (PointToSegmentDistanceSquare(C, A, P) <= KINDA_SMALL_NUMBER)
	{
		return true;
	}
	return false;
}

FVector GetBaryCentric(const FVector& Point, const FVector& A, const FVector& B, const FVector& C)
{
	// Compute the normal of the triangle
	const FVector TriNorm = (B - A) ^ (C - A);

	//check collinearity of A,B,C
	if (TriNorm.SizeSquared() <= SMALL_NUMBER)
	{
		//Degenerate polygon return a neutral barycentric
		return FVector(0.33f, 0.33f, 0.33f);
	}
	return FMath::ComputeBaryCentric2D(Point, A, B, C);
}

struct FTriangleElement
{
	FBox2D UVsBound;
	TArray<FSoftSkinVertex> Vertices;
	TArray<uint32> Indexes;
	uint32 TriangleIndex;
};

bool FindTriangleUVMatch(const FVector2D& TargetUV, const TArray<FTriangleElement>& Triangles, const TArray<uint32>& QuadTreeTriangleResults, TArray<uint32>& MatchTriangleIndexes)
{
	for (uint32 TriangleIndex : QuadTreeTriangleResults)
	{
		const FTriangleElement& TriangleElement = Triangles[TriangleIndex];
		if (PointInTriangle(TriangleElement.Vertices[0].UVs[0], TriangleElement.Vertices[1].UVs[0], TriangleElement.Vertices[2].UVs[0], TargetUV))
		{
			MatchTriangleIndexes.Add(TriangleIndex);
		}
		TriangleIndex++;
	}
	return MatchTriangleIndexes.Num() == 0 ? false : true;
}

struct FTargetMatch
{
	float BarycentricWeight[3]; //The weight we use to interpolate the TARGET data
	uint32 Indices[3]; //BASE Index of the triangle vertice
};

void ProjectTargetOnBase(const TArray<FSoftSkinVertex>& BaseVertices, const TArray<TArray<uint32>>& PerSectionBaseTriangleIndices,
						 TArray<FTargetMatch>& TargetMatchData, const TArray<FSkelMeshSection>& TargetSections, const TArray<int32>& TargetSectionMatchBaseIndex, const TCHAR* DebugContext)
{
	bool bNoMatchMsgDone = false;
	TArray<FTriangleElement> Triangles;
	//Project section target vertices on match base section using the UVs coordinates
	for (int32 SectionIndex = 0; SectionIndex < TargetSections.Num(); ++SectionIndex)
	{
		//Use the remap base index in case some sections disappear during the reduce phase
		int32 BaseSectionIndex = TargetSectionMatchBaseIndex[SectionIndex];
		if (BaseSectionIndex == INDEX_NONE || !PerSectionBaseTriangleIndices.IsValidIndex(BaseSectionIndex))
		{
			continue;
		}
		//Target vertices for the Section
		const TArray<FSoftSkinVertex>& TargetVertices = TargetSections[SectionIndex].SoftVertices;
		//Base Triangle indices for the matched base section
		const TArray<uint32>& BaseTriangleIndices = PerSectionBaseTriangleIndices[BaseSectionIndex];
		FBox2D BaseMeshUVBound(EForceInit::ForceInit);
		FBox BaseMeshPositionBound(EForceInit::ForceInit);
		//Fill the triangle element to speed up the triangle research
		Triangles.Reset(BaseTriangleIndices.Num() / 3);
		for (uint32 TriangleIndex = 0; TriangleIndex < (uint32)BaseTriangleIndices.Num(); TriangleIndex += 3)
		{
			FTriangleElement TriangleElement;
			TriangleElement.UVsBound.Init();
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				uint32 CornerIndice = BaseTriangleIndices[TriangleIndex + Corner];
				check(BaseVertices.IsValidIndex(CornerIndice));
				const FSoftSkinVertex& BaseVertex = BaseVertices[CornerIndice];
				TriangleElement.Indexes.Add(CornerIndice);
				TriangleElement.Vertices.Add(BaseVertex);
				TriangleElement.UVsBound += BaseVertex.UVs[0];
				BaseMeshPositionBound += BaseVertex.Position;
			}
			BaseMeshUVBound += TriangleElement.UVsBound;
			TriangleElement.TriangleIndex = Triangles.Num();
			Triangles.Add(TriangleElement);
		}
		//Setup the Quad tree
		float UVsQuadTreeMinSize = 0.001f;
		TQuadTree<uint32, 100> QuadTree(BaseMeshUVBound, UVsQuadTreeMinSize);
		for (FTriangleElement& TriangleElement : Triangles)
		{
			QuadTree.Insert(TriangleElement.TriangleIndex, TriangleElement.UVsBound, DebugContext);
		}
		//Retrieve all triangle that are close to our point, let get 5% of UV extend
		float DistanceThreshold = BaseMeshUVBound.GetExtent().Size()*0.05f;
		//Find a match triangle for every target vertices
		TArray<uint32> QuadTreeTriangleResults;
		QuadTreeTriangleResults.Reserve(Triangles.Num() / 10); //Reserve 10% to speed up the query
		for (uint32 TargetVertexIndex = 0; TargetVertexIndex < (uint32)TargetVertices.Num(); ++TargetVertexIndex)
		{
			FVector2D TargetUV = TargetVertices[TargetVertexIndex].UVs[0];
			//Reset the last data without flushing the memmery allocation
			QuadTreeTriangleResults.Reset();
			const uint32 FullTargetIndex = TargetSections[SectionIndex].BaseVertexIndex + TargetVertexIndex;
			//Make sure the array is allocate properly
			if (!TargetMatchData.IsValidIndex(FullTargetIndex))
			{
				continue;
			}
			//Set default data for the target match, in case we cannot found a match
			FTargetMatch& TargetMatch = TargetMatchData[FullTargetIndex];
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				TargetMatch.Indices[Corner] = INDEX_NONE;
				TargetMatch.BarycentricWeight[Corner] = 0.3333f; //The weight will be use to found the proper delta
			}

			FVector2D Extent(DistanceThreshold, DistanceThreshold);
			FBox2D CurBox(TargetUV - Extent, TargetUV + Extent);
			while (QuadTreeTriangleResults.Num() <= 0)
			{
				QuadTree.GetElements(CurBox, QuadTreeTriangleResults);
				Extent *= 2;
				CurBox = FBox2D(TargetUV - Extent, TargetUV + Extent);
			}

			auto GetDistancePointToBaseTriangle = [&Triangles, &TargetVertices, &TargetVertexIndex](const uint32 BaseTriangleIndex)->float
			{
				FTriangleElement& CandidateTriangle = Triangles[BaseTriangleIndex];
				return FVector::DistSquared(FMath::ClosestPointOnTriangleToPoint(TargetVertices[TargetVertexIndex].Position, CandidateTriangle.Vertices[0].Position, CandidateTriangle.Vertices[1].Position, CandidateTriangle.Vertices[2].Position), TargetVertices[TargetVertexIndex].Position);
			};

			auto FailSafeUnmatchVertex = [&GetDistancePointToBaseTriangle, &QuadTreeTriangleResults](uint32 &OutIndexMatch)->bool
			{
				bool bFoundMatch = false;
				float ClosestTriangleDistSquared = MAX_flt;
				for (uint32 MatchTriangleIndex : QuadTreeTriangleResults)
				{
					float TriangleDistSquared = GetDistancePointToBaseTriangle(MatchTriangleIndex);
					if (TriangleDistSquared < ClosestTriangleDistSquared)
					{
						ClosestTriangleDistSquared = TriangleDistSquared;
						OutIndexMatch = MatchTriangleIndex;
						bFoundMatch = true;
					}
				}
				return bFoundMatch;
			};

			//Find all Triangles that contain the Target UV
			if (QuadTreeTriangleResults.Num() > 0)
			{
				TArray<uint32> MatchTriangleIndexes;
				uint32 FoundIndexMatch = INDEX_NONE;
				if(!FindTriangleUVMatch(TargetUV, Triangles, QuadTreeTriangleResults, MatchTriangleIndexes))
				{
					if (!FailSafeUnmatchVertex(FoundIndexMatch))
					{
						//We should always have a match
						if (!bNoMatchMsgDone)
						{
							UE_LOG(LogLODUtilities, Warning, TEXT("Reduce LOD, remap morph target: Cannot find a triangle from the base LOD that contain a vertex UV in the target LOD. Remap morph target quality will be lower."));
							bNoMatchMsgDone = true;
						}
						continue;
					}
				}
				float ClosestTriangleDistSquared = MAX_flt;
				if (MatchTriangleIndexes.Num() == 1)
				{
					//One match, this mean no mirror UVs simply take the single match
					FoundIndexMatch = MatchTriangleIndexes[0];
					ClosestTriangleDistSquared = GetDistancePointToBaseTriangle(FoundIndexMatch);
				}
				else
				{
					//Geometry can use mirror so the UVs are not unique. Use the closest match triangle to the point to find the best match
					for (uint32 MatchTriangleIndex : MatchTriangleIndexes)
					{
						float TriangleDistSquared = GetDistancePointToBaseTriangle(MatchTriangleIndex);
						if (TriangleDistSquared < ClosestTriangleDistSquared)
						{
							ClosestTriangleDistSquared = TriangleDistSquared;
							FoundIndexMatch = MatchTriangleIndex;
						}
					}
				}

				//FAIL SAFE, make sure we have a match that make sense
				//Use the mesh section geometry bound extent (10% of it) to validate we are close enough.
				if (ClosestTriangleDistSquared > BaseMeshPositionBound.GetExtent().SizeSquared()*0.1f)
				{
					//Executing fail safe, if the UVs are too much off because of the reduction, use the closest distance to polygons to find the match
					//This path is not optimize and should not happen often.
					FailSafeUnmatchVertex(FoundIndexMatch);
				}

				//We should always have a valid match at this point
				check(FoundIndexMatch != INDEX_NONE);
				FTriangleElement& BestTriangle = Triangles[FoundIndexMatch];
				//Found the surface area of the 3 barycentric triangles from the UVs
				FVector BarycentricWeight;
				BarycentricWeight = GetBaryCentric(FVector(TargetUV, 0.0f), FVector(BestTriangle.Vertices[0].UVs[0], 0.0f), FVector(BestTriangle.Vertices[1].UVs[0], 0.0f), FVector(BestTriangle.Vertices[2].UVs[0], 0.0f));
				//Fill the target match
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					TargetMatch.Indices[Corner] = BestTriangle.Indexes[Corner];
					TargetMatch.BarycentricWeight[Corner] = BarycentricWeight[Corner]; //The weight will be use to found the proper delta
				}
			}
			else
			{
				if (!bNoMatchMsgDone)
				{
					UE_LOG(LogLODUtilities, Warning, TEXT("Reduce LOD, remap morph target: Cannot find a triangle from the base LOD that contain a vertex UV in the target LOD. Remap morph target quality will be lower."));
					bNoMatchMsgDone = true;
				}
				continue;
			}
		}
	}
}

void CreateLODMorphTarget(USkeletalMesh* SkeletalMesh, FReductionBaseSkeletalMeshBulkData* ReductionBaseSkeletalMeshBulkData, int32 SourceLOD, int32 DestinationLOD, const TMap<UMorphTarget *, TMap<uint32, uint32>>& PerMorphTargetBaseIndexToMorphTargetDelta, const TMap<uint32, TArray<uint32>>& BaseMorphIndexToTargetIndexList, const TArray<FSoftSkinVertex>& TargetVertices, const TArray<FTargetMatch>& TargetMatchData)
{
	TMap<FString, TArray<FMorphTargetDelta>> BaseLODMorphTargetData;
	if (ReductionBaseSkeletalMeshBulkData != nullptr)
	{
		FSkeletalMeshLODModel TempBaseLODModel;
		ReductionBaseSkeletalMeshBulkData->LoadReductionData(TempBaseLODModel, BaseLODMorphTargetData);
	}

	FSkeletalMeshModel* SkeletalMeshModel = SkeletalMesh->GetImportedModel();
	const FSkeletalMeshLODModel& TargetLODModel = SkeletalMeshModel->LODModels[DestinationLOD];

	bool bInitializeMorphData = false;

	for (UMorphTarget *MorphTarget : SkeletalMesh->MorphTargets)
	{
		if (!MorphTarget->HasDataForLOD(SourceLOD))
		{
			continue;
		}
		bool bUseBaseMorphDelta = SourceLOD == DestinationLOD && BaseLODMorphTargetData.Contains(MorphTarget->GetFullName());

		const TArray<FMorphTargetDelta> *BaseMorphDeltas = bUseBaseMorphDelta ? BaseLODMorphTargetData.Find(MorphTarget->GetFullName()) : nullptr;
		if (BaseMorphDeltas == nullptr || BaseMorphDeltas->Num() <= 0)
		{
			bUseBaseMorphDelta = false;
		}

		const TMap<uint32, uint32>& BaseIndexToMorphTargetDelta = PerMorphTargetBaseIndexToMorphTargetDelta[MorphTarget];
		TArray<FMorphTargetDelta> NewMorphTargetDeltas;
		TSet<uint32> CreatedTargetIndex;
		TMap<FVector, TArray<uint32>> MorphTargetPerPosition;
		const FMorphTargetLODModel& BaseMorphModel = MorphTarget->MorphLODModels[SourceLOD];
		//Iterate each original morph target source index to fill the NewMorphTargetDeltas array with the TargetMatchData.
		const TArray<FMorphTargetDelta>& Vertices = bUseBaseMorphDelta ? *BaseMorphDeltas : BaseMorphModel.Vertices;
		for (uint32 MorphDeltaIndex = 0; MorphDeltaIndex < (uint32)(Vertices.Num()); ++MorphDeltaIndex)
		{
			const FMorphTargetDelta& MorphDelta = Vertices[MorphDeltaIndex];
			const TArray<uint32>* TargetIndexesPtr = BaseMorphIndexToTargetIndexList.Find(MorphDelta.SourceIdx);
			if (TargetIndexesPtr == nullptr)
			{
				continue;
			}
			const TArray<uint32>& TargetIndexes = *TargetIndexesPtr;
			for (int32 MorphTargetIndex = 0; MorphTargetIndex < TargetIndexes.Num(); ++MorphTargetIndex)
			{
				uint32 TargetIndex = TargetIndexes[MorphTargetIndex];
				if (CreatedTargetIndex.Contains(TargetIndex))
				{
					continue;
				}
				CreatedTargetIndex.Add(TargetIndex);
				const FVector& SearchPosition = TargetVertices[TargetIndex].Position;
				FMorphTargetDelta MatchMorphDelta;
				MatchMorphDelta.SourceIdx = TargetIndex;

				const FTargetMatch& TargetMatch = TargetMatchData[TargetIndex];

				//Find the Position/tangent delta for the MatchMorphDelta using the barycentric weight
				MatchMorphDelta.PositionDelta = FVector(0.0f);
				MatchMorphDelta.TangentZDelta = FVector(0.0f);
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					const uint32* BaseMorphTargetIndexPtr = BaseIndexToMorphTargetDelta.Find(TargetMatch.Indices[Corner]);
					if (BaseMorphTargetIndexPtr != nullptr && Vertices.IsValidIndex(*BaseMorphTargetIndexPtr))
					{
						const FMorphTargetDelta& BaseMorphTargetDelta = Vertices[*BaseMorphTargetIndexPtr];
						FVector BasePositionDelta = !BaseMorphTargetDelta.PositionDelta.ContainsNaN() ? BaseMorphTargetDelta.PositionDelta : FVector(0.0f);
						FVector BaseTangentZDelta = !BaseMorphTargetDelta.TangentZDelta.ContainsNaN() ? BaseMorphTargetDelta.TangentZDelta : FVector(0.0f);
						MatchMorphDelta.PositionDelta += BasePositionDelta * TargetMatch.BarycentricWeight[Corner];
						MatchMorphDelta.TangentZDelta += BaseTangentZDelta * TargetMatch.BarycentricWeight[Corner];
					}
					ensure(!MatchMorphDelta.PositionDelta.ContainsNaN());
					ensure(!MatchMorphDelta.TangentZDelta.ContainsNaN());
				}

				//Make sure all morph delta that are at the same position use the same delta to avoid hole in the geometry
				TArray<uint32> *MorphTargetsIndexUsingPosition = nullptr;
				MorphTargetsIndexUsingPosition = MorphTargetPerPosition.Find(SearchPosition);
				if (MorphTargetsIndexUsingPosition != nullptr)
				{
					//Get the maximum position/tangent delta for the existing matched morph delta
					FVector PositionDelta = MatchMorphDelta.PositionDelta;
					FVector TangentZDelta = MatchMorphDelta.TangentZDelta;
					for (uint32 ExistingMorphTargetIndex : *MorphTargetsIndexUsingPosition)
					{
						const FMorphTargetDelta& ExistingMorphDelta = NewMorphTargetDeltas[ExistingMorphTargetIndex];
						PositionDelta = PositionDelta.SizeSquared() > ExistingMorphDelta.PositionDelta.SizeSquared() ? PositionDelta : ExistingMorphDelta.PositionDelta;
						TangentZDelta = TangentZDelta.SizeSquared() > ExistingMorphDelta.TangentZDelta.SizeSquared() ? TangentZDelta : ExistingMorphDelta.TangentZDelta;
					}
					//Update all MorphTarget that share the same position.
					for (uint32 ExistingMorphTargetIndex : *MorphTargetsIndexUsingPosition)
					{
						FMorphTargetDelta& ExistingMorphDelta = NewMorphTargetDeltas[ExistingMorphTargetIndex];
						ExistingMorphDelta.PositionDelta = PositionDelta;
						ExistingMorphDelta.TangentZDelta = TangentZDelta;
					}
					MatchMorphDelta.PositionDelta = PositionDelta;
					MatchMorphDelta.TangentZDelta = TangentZDelta;
					MorphTargetsIndexUsingPosition->Add(NewMorphTargetDeltas.Num());
				}
				else
				{
					MorphTargetPerPosition.Add(TargetVertices[TargetIndex].Position).Add(NewMorphTargetDeltas.Num());
				}
				NewMorphTargetDeltas.Add(MatchMorphDelta);
			}
		}
		
		//Register the new morph target on the target LOD
		MorphTarget->PopulateDeltas(NewMorphTargetDeltas, DestinationLOD, TargetLODModel.Sections, false, true);
		if (MorphTarget->HasValidData())
		{
			bInitializeMorphData |= SkeletalMesh->RegisterMorphTarget(MorphTarget, false);
		}
	}

	if (bInitializeMorphData)
	{
		SkeletalMesh->InitMorphTargetsAndRebuildRenderData();
	}
}

void FLODUtilities::ClearGeneratedMorphTarget(USkeletalMesh* SkeletalMesh, int32 TargetLOD)
{
	check(SkeletalMesh);
	FSkeletalMeshModel* SkeletalMeshResource = SkeletalMesh->GetImportedModel();
	if (!SkeletalMeshResource ||
		!SkeletalMeshResource->LODModels.IsValidIndex(TargetLOD))
	{
		//Abort clearing 
		return;
	}

	const FSkeletalMeshLODModel& TargetLODModel = SkeletalMeshResource->LODModels[TargetLOD];
	//Make sure we have some morph for this LOD
	for (UMorphTarget *MorphTarget : SkeletalMesh->MorphTargets)
	{
		if (!MorphTarget->HasDataForLOD(TargetLOD))
		{
			continue;
		}

		//if (MorphTarget->MorphLODModels[TargetLOD].bGeneratedByEngine)
		{
			MorphTarget->MorphLODModels[TargetLOD].Reset();

			// if this is the last one, we can remove empty ones
			if (TargetLOD == MorphTarget->MorphLODModels.Num() - 1)
			{
				MorphTarget->RemoveEmptyMorphTargets();
			}
		}
	}
}

void FLODUtilities::ApplyMorphTargetsToLOD(USkeletalMesh* SkeletalMesh, int32 SourceLOD, int32 DestinationLOD)
{
	check(SkeletalMesh);
	FSkeletalMeshModel* SkeletalMeshResource = SkeletalMesh->GetImportedModel();
	if (!SkeletalMeshResource ||
		!SkeletalMeshResource->LODModels.IsValidIndex(SourceLOD) ||
		!SkeletalMeshResource->LODModels.IsValidIndex(DestinationLOD) ||
		SourceLOD > DestinationLOD)
	{
		//Cannot reduce if the source model is missing or we reduce from a higher index LOD
		return;
	}

	FSkeletalMeshLODModel& SourceLODModel = SkeletalMeshResource->LODModels[SourceLOD];
	FReductionBaseSkeletalMeshBulkData* ReductionBaseSkeletalMeshBulkData = nullptr;
	bool bReduceBaseLOD = DestinationLOD == SourceLOD && SkeletalMeshResource->OriginalReductionSourceMeshData.IsValidIndex(SourceLOD) && !SkeletalMeshResource->OriginalReductionSourceMeshData[SourceLOD]->IsEmpty();
	if (!bReduceBaseLOD && SourceLOD == DestinationLOD)
	{
		//Abort remapping of morph target since the data is missing
		return;
	}
	if (bReduceBaseLOD)
	{
		ReductionBaseSkeletalMeshBulkData = SkeletalMeshResource->OriginalReductionSourceMeshData[SourceLOD];
	}

	FSkeletalMeshLODModel TempBaseLODModel;
	TMap<FString, TArray<FMorphTargetDelta>> TempBaseLODMorphTargetData;
	if (bReduceBaseLOD)
	{
		ReductionBaseSkeletalMeshBulkData->LoadReductionData(TempBaseLODModel, TempBaseLODMorphTargetData);
	}

	const FSkeletalMeshLODModel& BaseLODModel = bReduceBaseLOD ? TempBaseLODModel : SkeletalMeshResource->LODModels[SourceLOD];
	const FSkeletalMeshLODModel& TargetLODModel = SkeletalMeshResource->LODModels[DestinationLOD];
	//Make sure we have some morph for this LOD
	bool bContainsMorphTargets = false;
	for (UMorphTarget *MorphTarget : SkeletalMesh->MorphTargets)
	{
		if (MorphTarget->HasDataForLOD(SourceLOD))
		{
			bContainsMorphTargets = true;
		}
	}
	if (!bContainsMorphTargets)
	{
		//No morph target to remap
		return;
	}

	//We have to match target sections index with the correct base section index. Reduced LODs can contain a different number of sections than the base LOD
	TArray<int32> TargetSectionMatchBaseIndex;
	//Initialize the array to INDEX_NONE
	TargetSectionMatchBaseIndex.AddUninitialized(TargetLODModel.Sections.Num());
	for (int32 TargetSectionIndex = 0; TargetSectionIndex < TargetLODModel.Sections.Num(); ++TargetSectionIndex)
	{
		TargetSectionMatchBaseIndex[TargetSectionIndex] = INDEX_NONE;
	}
	//Find corresponding section indices from Source LOD for Target LOD
	for (int32 BaseSectionIndex = 0; BaseSectionIndex < BaseLODModel.Sections.Num(); ++BaseSectionIndex)
	{
		int32 TargetSectionIndexMatch = INDEX_NONE;
		for (int32 TargetSectionIndex = 0; TargetSectionIndex < TargetLODModel.Sections.Num(); ++TargetSectionIndex)
		{
			if (TargetLODModel.Sections[TargetSectionIndex].MaterialIndex == BaseLODModel.Sections[BaseSectionIndex].MaterialIndex && TargetSectionMatchBaseIndex[TargetSectionIndex] == INDEX_NONE)
			{
				TargetSectionIndexMatch = TargetSectionIndex;
				break;
			}
		}
		//We can set the data only once. There should be no clash
		if (TargetSectionMatchBaseIndex.IsValidIndex(TargetSectionIndexMatch) && TargetSectionMatchBaseIndex[TargetSectionIndexMatch] == INDEX_NONE)
		{
			TargetSectionMatchBaseIndex[TargetSectionIndexMatch] = BaseSectionIndex;
		}
	}
	//We should have match all the target sections
	check(!TargetSectionMatchBaseIndex.Contains(INDEX_NONE));
	TArray<FSoftSkinVertex> BaseVertices;
	TArray<FSoftSkinVertex> TargetVertices;
	BaseLODModel.GetNonClothVertices(BaseVertices);
	TargetLODModel.GetNonClothVertices(TargetVertices);
	//Create the base triangle indices per section
	TArray<TArray<uint32>> BaseTriangleIndices;
	int32 SectionCount = BaseLODModel.NumNonClothingSections();
	BaseTriangleIndices.AddDefaulted(SectionCount);
	for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		const FSkelMeshSection& Section = BaseLODModel.Sections[SectionIndex];
		uint32 TriangleCount = Section.NumTriangles;
		for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			for (uint32 PointIndex = 0; PointIndex < 3; PointIndex++)
			{
				BaseTriangleIndices[SectionIndex].Add(BaseLODModel.IndexBuffer[Section.BaseIndex + ((TriangleIndex * 3) + PointIndex)]);
			}
		}
	}
	//Every target vertices match a Base LOD triangle, we also want the barycentric weight of the triangle match. All this done using the UVs
	TArray<FTargetMatch> TargetMatchData;
	TargetMatchData.AddUninitialized(TargetVertices.Num());
	//Match all target vertices to a Base triangle Using UVs.
	ProjectTargetOnBase(BaseVertices, BaseTriangleIndices, TargetMatchData, TargetLODModel.Sections, TargetSectionMatchBaseIndex, *SkeletalMesh->GetName());
	//Helper to retrieve the FMorphTargetDelta from the BaseIndex
	TMap<UMorphTarget *, TMap<uint32, uint32>> PerMorphTargetBaseIndexToMorphTargetDelta;
	//Create a map from BaseIndex to a list of match target index for all base morph target point
	TMap<uint32, TArray<uint32>> BaseMorphIndexToTargetIndexList;
	for (UMorphTarget *MorphTarget : SkeletalMesh->MorphTargets)
	{
		if (!MorphTarget->HasDataForLOD(SourceLOD))
		{
			continue;
		}

		bool bUseTempMorphDelta = SourceLOD == DestinationLOD && bReduceBaseLOD && TempBaseLODMorphTargetData.Contains(MorphTarget->GetFullName());
		const TArray<FMorphTargetDelta> *TempMorphDeltas = bUseTempMorphDelta ? TempBaseLODMorphTargetData.Find(MorphTarget->GetFullName()) : nullptr;
		if (TempMorphDeltas == nullptr || TempMorphDeltas->Num() <= 0)
		{
			bUseTempMorphDelta = false;
		}

		TMap<uint32, uint32>& BaseIndexToMorphTargetDelta = PerMorphTargetBaseIndexToMorphTargetDelta.FindOrAdd(MorphTarget);
		const FMorphTargetLODModel& BaseMorphModel = MorphTarget->MorphLODModels[SourceLOD];
		const TArray<FMorphTargetDelta>& Vertices = bUseTempMorphDelta ? *TempMorphDeltas : BaseMorphModel.Vertices;
		for (uint32 MorphDeltaIndex = 0; MorphDeltaIndex < (uint32)(Vertices.Num()); ++MorphDeltaIndex)
		{
			const FMorphTargetDelta& MorphDelta = Vertices[MorphDeltaIndex];
			BaseIndexToMorphTargetDelta.Add(MorphDelta.SourceIdx, MorphDeltaIndex);
			//Iterate the targetmatch data so we can store which target indexes is impacted by this morph delta.
			for (int32 TargetIndex = 0; TargetIndex < TargetMatchData.Num(); ++TargetIndex)
			{
				const FTargetMatch& TargetMatch = TargetMatchData[TargetIndex];
				if (TargetMatch.Indices[0] == INDEX_NONE)
				{
					//In case this vertex did not found a triangle match
					continue;
				}
				if (TargetMatch.Indices[0] == MorphDelta.SourceIdx || TargetMatch.Indices[1] == MorphDelta.SourceIdx || TargetMatch.Indices[2] == MorphDelta.SourceIdx)
				{
					TArray<uint32>& TargetIndexes = BaseMorphIndexToTargetIndexList.FindOrAdd(MorphDelta.SourceIdx);
					TargetIndexes.AddUnique(TargetIndex);
				}
			}
		}
	}
	//Create the target morph target
	CreateLODMorphTarget(SkeletalMesh, ReductionBaseSkeletalMeshBulkData, SourceLOD, DestinationLOD, PerMorphTargetBaseIndexToMorphTargetDelta, BaseMorphIndexToTargetIndexList, TargetVertices, TargetMatchData);
}

void FLODUtilities::SimplifySkeletalMeshLOD( USkeletalMesh* SkeletalMesh, int32 DesiredLOD, bool bReregisterComponent /*= true*/ )
{
	IMeshReductionModule& ReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionModule>("MeshReductionInterface");
	IMeshReduction* MeshReduction = ReductionModule.GetSkeletalMeshReductionInterface();

	check (MeshReduction && MeshReduction->IsSupported());

	if (DesiredLOD == 0
		&& SkeletalMesh->GetLODInfo(DesiredLOD) != nullptr
		&& SkeletalMesh->GetLODInfo(DesiredLOD)->bHasBeenSimplified
		&& (!SkeletalMesh->GetImportedModel()->OriginalReductionSourceMeshData.IsValidIndex(0) || SkeletalMesh->GetImportedModel()->OriginalReductionSourceMeshData[0]->IsEmpty()))
	{
		//The base LOD was reduce and there is no valid data, we cannot regenerate this lod it must be re-import before
		FFormatNamedArguments Args;
		Args.Add(TEXT("SkeletalMeshName"), FText::FromString(SkeletalMesh->GetName()));
		Args.Add(TEXT("LODIndex"), FText::AsNumber(DesiredLOD));
		FText Message = FText::Format(NSLOCTEXT("UnrealEd", "MeshSimp_GenerateLODCannotGenerateMissingData", "Cannot generate LOD {LODIndex} for skeletal mesh '{SkeletalMeshName}'. This LOD must be re-import to create the necessary data"), Args);
		if (FApp::IsUnattended())
		{
			UE_LOG(LogLODUtilities, Warning, TEXT("%s"), *(Message.ToString()));
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, Message);
		}
		return;
	}

	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("DesiredLOD"), DesiredLOD);
		Args.Add(TEXT("SkeletalMeshName"), FText::FromString(SkeletalMesh->GetName()));
		const FText StatusUpdate = FText::Format(NSLOCTEXT("UnrealEd", "MeshSimp_GeneratingLOD_F", "Generating LOD{DesiredLOD} for {SkeletalMeshName}..."), Args);
		GWarn->BeginSlowTask(StatusUpdate, true);
	}

	if (SkeletalMesh->GetLODInfo(DesiredLOD) != nullptr)
	{
		FSkeletalMeshModel* SkeletalMeshResource = SkeletalMesh->GetImportedModel();
		FSkeletalMeshOptimizationSettings& Settings = SkeletalMesh->GetLODInfo(DesiredLOD)->ReductionSettings;

		if (SkeletalMeshResource->LODModels.IsValidIndex(DesiredLOD) && !SkeletalMesh->GetLODInfo(DesiredLOD)->bHasBeenSimplified)
		{
			FSkeletalMeshLODModel& SrcModel = SkeletalMeshResource->LODModels[DesiredLOD];
			while (DesiredLOD >= SkeletalMeshResource->OriginalReductionSourceMeshData.Num())
			{
				FReductionBaseSkeletalMeshBulkData *EmptyReductionData = new FReductionBaseSkeletalMeshBulkData();
				SkeletalMeshResource->OriginalReductionSourceMeshData.Add(EmptyReductionData);
			}
			check(SkeletalMeshResource->OriginalReductionSourceMeshData.IsValidIndex(DesiredLOD));
			//Make the copy of the data only once until the ImportedModel change (re-imported)
			if (SkeletalMeshResource->OriginalReductionSourceMeshData[DesiredLOD]->IsEmpty())
			{
				TMap<FString, TArray<FMorphTargetDelta>> BaseLODMorphTargetData;
				BaseLODMorphTargetData.Empty(SkeletalMesh->MorphTargets.Num());
				for (UMorphTarget *MorphTarget : SkeletalMesh->MorphTargets)
				{
					if (!MorphTarget->HasDataForLOD(DesiredLOD))
					{
						continue;
					}
					TArray<FMorphTargetDelta>& MorphDeltasArray = BaseLODMorphTargetData.FindOrAdd(MorphTarget->GetFullName());
					const FMorphTargetLODModel& BaseMorphModel = MorphTarget->MorphLODModels[DesiredLOD];
					//Iterate each original morph target source index to fill the NewMorphTargetDeltas array with the TargetMatchData.
					for (const FMorphTargetDelta& MorphDelta : BaseMorphModel.Vertices)
					{
						MorphDeltasArray.Add(MorphDelta);
					}
				}
				//Copy the original SkeletalMesh LODModel
				SkeletalMeshResource->OriginalReductionSourceMeshData[DesiredLOD]->SaveReductionData(SrcModel, BaseLODMorphTargetData);

				if (DesiredLOD == 0)
				{
					SkeletalMesh->GetLODInfo(DesiredLOD)->SourceImportFilename = SkeletalMesh->AssetImportData->GetFirstFilename();
				}
			}
		}
	}
	

	if (MeshReduction->ReduceSkeletalMesh(SkeletalMesh, DesiredLOD, bReregisterComponent))
	{
		check(SkeletalMesh->GetLODNum() >= 1);

		auto ApplyMorphTargetOption = [&SkeletalMesh, &DesiredLOD]()
		{
			FSkeletalMeshOptimizationSettings& ReductionSettings = SkeletalMesh->GetLODInfo(DesiredLOD)->ReductionSettings;
			//Apply morph to the new LOD. Force it if we reduce the base LOD, base LOD must apply the morph target
			if (ReductionSettings.bRemapMorphTargets)
			{
				ApplyMorphTargetsToLOD(SkeletalMesh, ReductionSettings.BaseLOD, DesiredLOD);
			}
			else
			{
				ClearGeneratedMorphTarget(SkeletalMesh, DesiredLOD);
			}
		};

		if (bReregisterComponent)
		{
			TComponentReregisterContext<USkinnedMeshComponent> ReregisterContext;
			SkeletalMesh->ReleaseResources();
			SkeletalMesh->ReleaseResourcesFence.Wait();

			ApplyMorphTargetOption();
			
			SkeletalMesh->PostEditChange();
			SkeletalMesh->InitResources();
		}
		else
		{
			ApplyMorphTargetOption();
		}
		SkeletalMesh->MarkPackageDirty();
	}
	else
	{
		// Simplification failed! Warn the user.
		FFormatNamedArguments Args;
		Args.Add(TEXT("SkeletalMeshName"), FText::FromString(SkeletalMesh->GetName()));
		const FText Message = FText::Format(NSLOCTEXT("UnrealEd", "MeshSimp_GenerateLODFailed_F", "An error occurred while simplifying the geometry for mesh '{SkeletalMeshName}'.  Consider adjusting simplification parameters and re-simplifying the mesh."), Args);
		FMessageDialog::Open(EAppMsgType::Ok, Message);
	}
	GWarn->EndSlowTask();
}

void FLODUtilities::SimplifySkeletalMeshLOD(FSkeletalMeshUpdateContext& UpdateContext, int32 DesiredLOD, bool bReregisterComponent /*= true*/)
{
	USkeletalMesh* SkeletalMesh = UpdateContext.SkeletalMesh;
	IMeshReductionModule& ReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionModule>("MeshReductionInterface");
	IMeshReduction* MeshReduction = ReductionModule.GetSkeletalMeshReductionInterface();

	if (MeshReduction && MeshReduction->IsSupported() && SkeletalMesh)
	{
		SimplifySkeletalMeshLOD(SkeletalMesh, DesiredLOD, bReregisterComponent);
		
		if (UpdateContext.OnLODChanged.IsBound())
		{
			//Notify calling system of change
			UpdateContext.OnLODChanged.ExecuteIfBound();
		}
	}
}

void FLODUtilities::RestoreSkeletalMeshLODImportedData(USkeletalMesh* SkeletalMesh, int32 LodIndex, bool bReregisterComponent /*= true*/)
{

	if (!SkeletalMesh->GetImportedModel()->OriginalReductionSourceMeshData.IsValidIndex(LodIndex) || SkeletalMesh->GetImportedModel()->OriginalReductionSourceMeshData[LodIndex]->IsEmpty())
	{
		//There is nothing to restore
		return;
	}
	FSkeletalMeshLODModel ImportedBaseLODModel;
	TMap<FString, TArray<FMorphTargetDelta>> ImportedBaseLODMorphTargetData;
	SkeletalMesh->GetImportedModel()->OriginalReductionSourceMeshData[LodIndex]->LoadReductionData(ImportedBaseLODModel, ImportedBaseLODMorphTargetData);
	{
		FSkeletalMeshUpdateContext UpdateContext;
		UpdateContext.SkeletalMesh = SkeletalMesh;

		TComponentReregisterContext<USkinnedMeshComponent> ReregisterContext;
		if (bReregisterComponent)
		{
			SkeletalMesh->ReleaseResources();
			SkeletalMesh->ReleaseResourcesFence.Wait();
		}
		//Copy the SkeletalMeshLODModel
		SkeletalMesh->GetImportedModel()->LODModels[LodIndex] = ImportedBaseLODModel;
		//Copy the morph target deltas
		bool bInitMorphTargetData = false;
		for (UMorphTarget *MorphTarget : SkeletalMesh->MorphTargets)
		{
			if (!ImportedBaseLODMorphTargetData.Contains(MorphTarget->GetFullName()))
			{
				continue;
			}
			TArray<FMorphTargetDelta>& ImportedDeltas = ImportedBaseLODMorphTargetData[MorphTarget->GetFullName()];

			MorphTarget->PopulateDeltas(ImportedDeltas, LodIndex, SkeletalMesh->GetImportedModel()->LODModels[LodIndex].Sections, false, false);
			bInitMorphTargetData |= SkeletalMesh->RegisterMorphTarget(MorphTarget, false);
		}
		SkeletalMesh->InitMorphTargetsAndRebuildRenderData();
		
		//Empty the bulkdata since we restore it
		SkeletalMesh->GetImportedModel()->OriginalReductionSourceMeshData[LodIndex]->EmptyBulkData();

		if (bReregisterComponent)
		{
			SkeletalMesh->PostEditChange();
			SkeletalMesh->InitResources();
		}

		if (UpdateContext.OnLODChanged.IsBound())
		{
			//Notify calling system of change
			UpdateContext.OnLODChanged.ExecuteIfBound();
		}
	}
}

void FLODUtilities::RefreshLODChange(const USkeletalMesh* SkeletalMesh)
{
	for (FObjectIterator Iter(USkeletalMeshComponent::StaticClass()); Iter; ++Iter)
	{
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(*Iter);
		if  (SkeletalMeshComponent->SkeletalMesh == SkeletalMesh)
		{
			// it needs to recreate IF it already has been created
			if (SkeletalMeshComponent->IsRegistered())
			{
				SkeletalMeshComponent->UpdateLODStatus();
				SkeletalMeshComponent->MarkRenderStateDirty();
			}
		}
	}
}
