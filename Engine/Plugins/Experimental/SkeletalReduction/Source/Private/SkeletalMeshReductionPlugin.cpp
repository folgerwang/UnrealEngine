// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"

#include "AnimationBlueprintLibrary.h"
#include "AnimationRuntime.h"
#include "Components/SkinnedMeshComponent.h"
#include "ComponentReregisterContext.h"
#include "Engine/MeshMerging.h" 
#include "Engine/StaticMesh.h"
#include "Features/IModularFeatures.h"
#include "ISkeletalMeshReduction.h"
#include "MeshBoneReduction.h"
#include "MeshMergeData.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "RawMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "SkeletalSimplifierMeshManager.h"
#include "SkeletalSimplifier.h"
#include "SkeletalMeshReductionSkinnedMesh.h"
#include "Stats/StatsMisc.h"
#include "Assets/ClothingAsset.h"


#define LOCTEXT_NAMESPACE "SkeletalMeshReduction"


class FQuadricSkeletalMeshReduction : public IMeshReduction
{
public:
	FQuadricSkeletalMeshReduction() 
	{};

	virtual ~FQuadricSkeletalMeshReduction()
	{}

	virtual const FString& GetVersionString() const override
	{
		// NB: The version string must be of the form "QuadricSkeletalMeshReduction_{foo}"
		// for the SkeletalMeshReductionSettingDetails to recognize this.
		// Version corresponds to VersionName in SkeletalReduction.uplugin.  
		static FString Version = TEXT("QuadricSkeletalMeshReduction_V0.1");
		return Version;
	}

	/**
	*	Returns true if mesh reduction is supported
	*/
	virtual bool IsSupported() const { return true; }

	/**
	*	Returns true if mesh reduction is active. Active mean there will be a reduction of the vertices or triangle number
	*/
	virtual bool IsReductionActive(const struct FMeshReductionSettings &ReductionSettings) const
	{
		return false;
	}

	virtual bool IsReductionActive(const FSkeletalMeshOptimizationSettings &ReductionSettings) const
	{
		float Threshold_One = (1.0f - KINDA_SMALL_NUMBER);
		float Threshold_Zero = (0.0f + KINDA_SMALL_NUMBER);
		switch (ReductionSettings.TerminationCriterion)
		{
			case SkeletalMeshTerminationCriterion::SMTC_NumOfTriangles:
			{
				return ReductionSettings.NumOfTrianglesPercentage < Threshold_One;
			}
			break;
			case SkeletalMeshTerminationCriterion::SMTC_NumOfVerts:
			{
				return ReductionSettings.NumOfVertPercentage < Threshold_One;
			}
			break;
			case SkeletalMeshTerminationCriterion::SMTC_TriangleOrVert:
			{
				return ReductionSettings.NumOfTrianglesPercentage < Threshold_One || ReductionSettings.NumOfVertPercentage < Threshold_One;
			}
			break;
			//Absolute count is consider has being always reduced
			case SkeletalMeshTerminationCriterion::SMTC_AbsNumOfVerts:
			case SkeletalMeshTerminationCriterion::SMTC_AbsNumOfTriangles:
			case SkeletalMeshTerminationCriterion::SMTC_AbsTriangleOrVert:
			{
				return true;
			}
			break;
		}

		return false;
	}

	/**
	* Reduces the provided skeletal mesh.
	* @returns true if reduction was successful.
	*/
	virtual bool ReduceSkeletalMesh( class USkeletalMesh* SkeletalMesh,
		                             int32 LODIndex,
		                             bool bReregisterComponent = true
	                                ) override ;


	/**
	* Reduces the raw mesh using the provided reduction settings.
	* @param OutReducedMesh - Upon return contains the reduced mesh.
	* @param OutMaxDeviation - Upon return contains the maximum distance by which the reduced mesh deviates from the original.
	* @param InMesh - The mesh to reduce.
	* @param ReductionSettings - Setting with which to reduce the mesh.
	*/
	virtual void ReduceMeshDescription( FMeshDescription& OutReducedMesh,
		                                float& OutMaxDeviation,
		                                const FMeshDescription& InMesh,
		                                const FOverlappingCorners& InOverlappingCorners,
		                                const struct FMeshReductionSettings& ReductionSettings
										) override {};

	


private:
	// -- internal structures used to for book-keeping


	/**
	* Holds data needed to create skeletal mesh skinning streams.
	*/
	struct  FSkeletalMeshData;

	/**
	* Useful in book-keeping ranges within an array.
	*/
	struct  FSectionRange;

	/**
	* Important bones when simplifying
	*/
	struct FImportantBones;

private:

	/**
	*  Reduce the skeletal mesh
	*  @param SkeletalMesh      - the mesh to be reduced.
	*  @param SkeletalMeshModel - the Model that belongs to the skeletal mesh, i.e. SkeletalMesh->GetImportedModel();
	*  @param LODIndex          - target index for the LOD   
	*/
	void ReduceSkeletalMesh( USkeletalMesh& SkeletalMesh, FSkeletalMeshModel& SkeletalMeshModel, int32 LODIndex ) const ;


	/**
	*  Reduce the skeletal mesh
	*  @param SrcLODModel       - Input mesh for simplification
	*  @param OutLODModel       - The result of simplification
	*  @param Bounds            - The bounds of the source geometry - can be used in computing simplification error threshold
	*  @param RefSkeleton       - The reference skeleton
	*  @param Settings          - Settings that control the reduction
	*  @param ImportantBones    - Bones and associated weight ( inhibit edge collapse of verts that rely on these bones)
	*  @param BoneMatrices      - Used to pose the mesh.
	*  @param LODIndex          - Target LOD index
	*/
	bool ReduceSkeletalLODModel( const FSkeletalMeshLODModel& SrcLODModel,
		                         FSkeletalMeshLODModel& OutLODModel,
		                         const FBoxSphereBounds& Bounds,
		                         const FReferenceSkeleton& RefSkeleton,
		                         const FSkeletalMeshOptimizationSettings& Settings,
								 const FImportantBones& ImportantBones,
		                         const TArray<FMatrix>& BoneMatrices,
		                         const int32 LODIndex ) const;

	/**
	* Remove the specified section from the mesh.
	* @param Model        - the model from which we remove the section
	* @param SectionIndex - the number of the section to remove
	*/
	bool RemoveMeshSection( FSkeletalMeshLODModel& Model, int32 SectionIndex ) const;


	/**
	* Generate a representation of the skined mesh in pose prescribed by Bone Weights and Matrices with attribute data on the verts for simplification
	* @param SrcLODModel    - the original model
	* @param BoneMatrices   - define the configuration of the model
	* @param LODIndex       - target index for the result.
	* @param OutSkinnedMesh - the posed mesh 
	*/
	void ConvertToFSkinnedSkeletalMesh( const FSkeletalMeshLODModel& SrcLODModel,
		                                const TArray<FMatrix>& BoneMatrices,
		                                const int32 LODIndex,
		                                SkeletalSimplifier::FSkinnedSkeletalMesh& OutSkinnedMesh ) const;

	/**
	* Generate a SkeletalMeshLODModel from a SkinnedSkeletalMesh and ReferenceSkeleton 
	* @param SkinnedMesh     - the source mesh
	* @param RefSkeleton    - reference skeleton
	* @param NewModel       - resulting MeshLODModel
	*/
	void ConvertToFSkeletalMeshLODModel( const SkeletalSimplifier::FSkinnedSkeletalMesh& SkinnedMesh,
		                                 const FReferenceSkeleton& RefSkeleton,
		                                 FSkeletalMeshLODModel& NewModel ) const;

	/**
	* Simplify the mesh
	* @param Settings         - the settings that control the simplifier
	* @param Bounds           - the radius of the source mesh
	* @param InOutSkinnedMesh - the skinned mesh 
	*/
	float SimplifyMesh( const FSkeletalMeshOptimizationSettings& Settings,
		                const FBoxSphereBounds& Bounds,
		                SkeletalSimplifier::FSkinnedSkeletalMesh& InOutSkinnedMesh ) const ;


	/**
	* Extract data in SOA form needed for the MeshUtilities.BuildSkeletalMesh
	* to build the new skeletal mesh.
	* @param SkinnedMesh      -  input format for the mesh
	* @param SkeletalMeshData - struct of array format needed for the mesh utilities
	*/
	void ExtractFSkeletalData( const SkeletalSimplifier::FSkinnedSkeletalMesh& SkinnedMesh,
		                       FSkeletalMeshData& SkeletalMeshData ) const;

	/**
	* Compute the UVBounds for the each channel on the mesh
	* @param Mesh     - Input mesh
	* @param UVBounds - Resulting UV bounds
	*/
	void ComputeUVBounds( const SkeletalSimplifier::FSkinnedSkeletalMesh& Mesh,
		                  FVector2D(&UVBounds)[2 * SkeletalSimplifier::MeshVertType::BasicAttrContainerType::NumUVs] ) const;

	/**
	* Clamp the UVs on the mesh
	* @param UVBounds - per channel bounds for UVs
	* @param Mesh     - Mesh to update. 
	*/
	void ClampUVBounds( const FVector2D(&UVBounds)[2 * SkeletalSimplifier::MeshVertType::BasicAttrContainerType::NumUVs],
		                SkeletalSimplifier::FSkinnedSkeletalMesh& Mesh ) const;

	/**
	* Reduce the number of bones on the Mesh to a max number.
	* This will re-normalize the weights.
	* @param Mesh            - the mesh to update
	* @param MaxBonesPerVert - the max number of bones on each vert. 
	*/
	void TrimBonesPerVert( SkeletalSimplifier::FSkinnedSkeletalMesh& Mesh, int32 MaxBonesPerVert ) const ;

	/**
	* If a vertex has one of the important bones as its' major bone, associated the ImportantBones.Weight
	*/
	void UpdateSpecializedVertWeights(const FImportantBones& ImportantBones, SkeletalSimplifier::FSkinnedSkeletalMesh& SkinnedSkeletalMesh) const;

};

struct  FQuadricSkeletalMeshReduction::FSkeletalMeshData
{
	TArray<SkeletalMeshImportData::FVertInfluence> Influences;
	TArray<SkeletalMeshImportData::FMeshWedge> Wedges;
	TArray<SkeletalMeshImportData::FMeshFace> Faces;
	TArray<FVector> Points;
	uint32 TexCoordCount;
};


struct FQuadricSkeletalMeshReduction::FSectionRange
{
	int32 Begin;
	int32 End;
};

struct FQuadricSkeletalMeshReduction::FImportantBones
{
	TArray<int32> Ids;
	float Weight;
};
/**
*  Required MeshReduction Interface.
*/
class FSkeletalMeshReduction : public ISkeletalMeshReduction
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;


	/** IMeshReductionModule interface.*/
	virtual class IMeshReduction* GetSkeletalMeshReductionInterface() override
	{
		return &SkeletalMeshReducer;
	}

	// not supported !
	virtual class IMeshReduction* GetStaticMeshReductionInterface()   override
	{
		return nullptr;
	}

	// not supported !
	virtual class IMeshMerging*   GetMeshMergingInterface()           override
	{
		return nullptr;
	}

	// not supported !
	virtual class IMeshMerging* GetDistributedMeshMergingInterface()  override
	{
		return nullptr;
	}


	virtual FString GetName() override
	{
		return FString("SkeletalMeshReduction");
	};

private:
	FQuadricSkeletalMeshReduction  SkeletalMeshReducer;
};

DEFINE_LOG_CATEGORY_STATIC(LogSkeletalMeshReduction, Log, All);

IMPLEMENT_MODULE(FSkeletalMeshReduction, SkeletalMeshReduction)


void FSkeletalMeshReduction::StartupModule()
{

	IModularFeatures::Get().RegisterModularFeature(IMeshReductionModule::GetModularFeatureName(), this);
}


void FSkeletalMeshReduction::ShutdownModule()
{

	IModularFeatures::Get().UnregisterModularFeature(IMeshReductionModule::GetModularFeatureName(), this);

}




bool FQuadricSkeletalMeshReduction::ReduceSkeletalMesh( USkeletalMesh* SkeletalMesh, int32 LODIndex, bool bReregisterComponent )
{
	check(SkeletalMesh);
	check(LODIndex >= 0);
	check(LODIndex <= SkeletalMesh->GetLODNum());

	FSkeletalMeshModel* SkeletalMeshResource = SkeletalMesh->GetImportedModel();
	check(SkeletalMeshResource);
	check(LODIndex <= SkeletalMeshResource->LODModels.Num());

	if (bReregisterComponent)
	{
		TComponentReregisterContext<USkinnedMeshComponent> ReregisterContext;
		SkeletalMesh->ReleaseResources();
		SkeletalMesh->ReleaseResourcesFence.Wait();

		ReduceSkeletalMesh(*SkeletalMesh, *SkeletalMeshResource, LODIndex);

		SkeletalMesh->PostEditChange();
		SkeletalMesh->InitResources();
	}
	else
	{
		ReduceSkeletalMesh(*SkeletalMesh, *SkeletalMeshResource, LODIndex);
	}

	return true;
}

bool FQuadricSkeletalMeshReduction::RemoveMeshSection(FSkeletalMeshLODModel& Model, int32 SectionIndex) const 
{
	// Need a valid section
	if (!Model.Sections.IsValidIndex(SectionIndex))
	{
		return false;
	}

	FSkelMeshSection& SectionToRemove = Model.Sections[SectionIndex];

	if (SectionToRemove.CorrespondClothAssetIndex != INDEX_NONE)
	{
		// Can't remove this, clothing currently relies on it
		return false;
	}

	const uint32 NumVertsToRemove   = SectionToRemove.GetNumVertices();
	const uint32 BaseVertToRemove   = SectionToRemove.BaseVertexIndex;
	const uint32 NumIndicesToRemove = SectionToRemove.NumTriangles * 3;
	const uint32 BaseIndexToRemove  = SectionToRemove.BaseIndex;


	// Strip indices
	Model.IndexBuffer.RemoveAt(BaseIndexToRemove, NumIndicesToRemove);

	Model.Sections.RemoveAt(SectionIndex);

	// Fixup indices above base vert
	for (uint32& Index : Model.IndexBuffer)
	{
		if (Index >= BaseVertToRemove)
		{
			Index -= NumVertsToRemove;
		}
	}

	Model.NumVertices -= NumVertsToRemove;

	// Fixup anything needing section indices
	for (FSkelMeshSection& Section : Model.Sections)
	{
		// Push back clothing indices
		if (Section.CorrespondClothAssetIndex > SectionIndex)
		{
			Section.CorrespondClothAssetIndex--;
		}

		// Removed indices, re-base further sections
		if (Section.BaseIndex > BaseIndexToRemove)
		{
			Section.BaseIndex -= NumIndicesToRemove;
		}

		// Remove verts, re-base further sections
		if (Section.BaseVertexIndex > BaseVertToRemove)
		{
			Section.BaseVertexIndex -= NumVertsToRemove;
		}
	}
	return true;
}


void FQuadricSkeletalMeshReduction::ConvertToFSkinnedSkeletalMesh( const FSkeletalMeshLODModel& SrcLODModel,
	                                                               const TArray<FMatrix>& BoneMatrices,
	                                                               const int32 LODIndex,
	                                                               SkeletalSimplifier::FSkinnedSkeletalMesh& OutSkinnedMesh) const
{



	auto ApplySkinning = [](const FMatrix& XForm, FSoftSkinVertex& Vertex)->void
	{
		// transform position
		FVector WeightedPosition = XForm.TransformPosition(Vertex.Position);
		FVector WeightedTangentX = XForm.TransformVector(Vertex.TangentX);
		FVector WeightedTangentY = XForm.TransformVector(Vertex.TangentY);
		FVector WeightedTangentZ = XForm.TransformVector(Vertex.TangentZ);

		
		Vertex.TangentX   = WeightedTangentX.GetSafeNormal();
		Vertex.TangentY   = WeightedTangentY.GetSafeNormal();
		uint8 WComponent  = Vertex.TangentZ.W;                    // NB: this looks bad.. the component of the vector is a float..
		Vertex.TangentZ   = WeightedTangentZ.GetSafeNormal();
		Vertex.TangentZ.W = WComponent;
		Vertex.Position   = WeightedPosition;
	};

	auto CreateSkinningMatrix = [&BoneMatrices](const FSoftSkinVertex& Vertex, const FSkelMeshSection& Section, bool& bValidBoneWeights)->FMatrix
	{
		// Compute the inverse of the total bone influence for this vertex.
		
		float InvTotalInfluence = 1.f / 255.f;   // expected default - anything else could indicate a problem with the asset.
		{
			int32 TotalInfluence = 0;

			for (int32 i = 0; i < MAX_TOTAL_INFLUENCES; ++i)
			{
				const uint8 BoneInfluence = Vertex.InfluenceWeights[i];
				TotalInfluence += BoneInfluence;
			}

			if (TotalInfluence != 255) // 255 is the expected value.  This logic just allows for graceful failure.
			{
				// Not expected value - record that.
				bValidBoneWeights = false;

				if (TotalInfluence == 0)
				{
					InvTotalInfluence = 0.f;
				}
				else
				{
					InvTotalInfluence = 1.f / float(TotalInfluence);
				}
			}
		}


		// Build the blended matrix 

		FMatrix BlendedMatrix(ForceInitToZero);

		int32 ValidInfluenceCount = 0;
		
		const TArray<uint16>& BoneMap = Section.BoneMap;

		for (int32 i = 0; i < MAX_TOTAL_INFLUENCES; ++i)
		{
			const uint16 BoneIndex    = Vertex.InfluenceBones[i];
			const uint8 BoneInfluence = Vertex.InfluenceWeights[i];

			// Accumulate the bone influence for this vert into the BlendedMatrix

			if (BoneInfluence > 0)
			{
				check(BoneIndex < BoneMap.Num());
				const uint16 SectionBoneId = BoneMap[BoneIndex]; // Third-party tool uses an additional indirection bode table here 
				const float  BoneWeight = BoneInfluence * InvTotalInfluence;  // convert to [0,1] float

				if (BoneMatrices.IsValidIndex(SectionBoneId))
				{
					ValidInfluenceCount++;
					const FMatrix BoneMatrix = BoneMatrices[SectionBoneId];
					BlendedMatrix += (BoneMatrix * BoneWeight);
				}
			}
		}

		// default identity matrix for the special case of the vertex having no valid transforms..

		if (ValidInfluenceCount == 0)
		{
			BlendedMatrix = FMatrix::Identity;
		}

		

		return BlendedMatrix;
	};



	// Copy the vertices into a single buffer

	TArray<FSoftSkinVertex> SoftSkinVertices;
	SrcLODModel.GetVertices(SoftSkinVertices);
	const int32 SectionCount = SrcLODModel.Sections.Num();

	// functor: true if this section should be included.

	auto SkipSection = [&SrcLODModel, LODIndex](int32 SectionIndex)->bool
	{
		if (SrcLODModel.Sections[SectionIndex].bDisabled)
		{
			return true;
		}
		int32 MaxLODIndex = SrcLODModel.Sections[SectionIndex].GenerateUpToLodIndex;
		return (MaxLODIndex != -1 && MaxLODIndex < LODIndex);
	};

	// Count the total number of verts, but only the number of triangles that
	// are used in sections we don't skip.
	// NB: This could result zero triangles, but a non-zero number of verts.
	//     i.e. we aren't going to try to compact the vertex array.

	TArray<FSectionRange> SectionRangeArray; // Keep track of the begin / end vertex in this section

	int32 VertexCount = 0;
	
	for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		const FSkelMeshSection& Section = SrcLODModel.Sections[SectionIndex];
		FSectionRange SectionRange;
		SectionRange.Begin = VertexCount;
		SectionRange.End   = VertexCount + Section.SoftVertices.Num();

		SectionRangeArray.Add(SectionRange);

		VertexCount = SectionRange.End;
	}

	// Verify that the model has an allowed number of textures
	const uint32 TexCoordCount = SrcLODModel.NumTexCoords;
	check(TexCoordCount <= MAX_TEXCOORDS);

	
	
	// Update the verts to the skinned location.

	for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		const FSkelMeshSection& Section  = SrcLODModel.Sections[SectionIndex];
		const FSectionRange VertexRange  = SectionRangeArray[SectionIndex];
		
		// Loop over the vertices in this section.
		bool bHasValidBoneWeights = true;
		for (int32 VertexIndex = VertexRange.Begin; VertexIndex < VertexRange.End; ++VertexIndex)
		{
			FSoftSkinVertex& SkinVertex = SoftSkinVertices[VertexIndex];

			// Use the bone weights for this vertex to create a blended matrix 
			const FMatrix BlendedMatrix = CreateSkinningMatrix(SkinVertex, Section, bHasValidBoneWeights);

			// Update this Skin Vertex to the correct location, normal, etc.
			ApplySkinning(BlendedMatrix, SkinVertex);

		}

		// Report any error with invalid bone weights
		if (!bHasValidBoneWeights && !SkipSection(SectionIndex))
		{
			UE_LOG(LogSkeletalMeshReduction, Warning, TEXT("Building LOD %d - Encountered questionable vertex weights in source."), LODIndex);
		}
	}

	// -- Make the index buffer; skipping the "SkipSections"

	// How many triangles?

	int32 NumTriangles = 0;
	for (int32 s = 0; s < SectionCount; ++s)
	{
		if (SkipSection(s))
		{
			continue;
		}
		NumTriangles += SrcLODModel.Sections[s].NumTriangles;
	}

	const int32 NumIndices = NumTriangles * 3;

	OutSkinnedMesh.Resize(NumTriangles, VertexCount);
	OutSkinnedMesh.SetTexCoordCount(TexCoordCount);

	// local names
	uint32* OutIndexBuffer                            = OutSkinnedMesh.IndexBuffer;
	SkeletalSimplifier::MeshVertType* OutVertexBuffer = OutSkinnedMesh.VertexBuffer;

	// Construct the index buffer

	{
		int32 tmpId = 0;
		for (int32 s = 0; s < SectionCount; ++s)
		{
			if (SkipSection(s))
			{
				continue;
			}
			const auto& SrcIndexBuffer = SrcLODModel.IndexBuffer;

			const FSkelMeshSection& Section = SrcLODModel.Sections[s];

			const uint32 FirstIndex = Section.BaseIndex;
			const uint32 LastIndex = FirstIndex + Section.NumTriangles * 3;

			for (uint32 i = FirstIndex; i < LastIndex; ++i)
			{
				const uint32 vertexId = SrcIndexBuffer[i];
				OutIndexBuffer[tmpId] = (int32)vertexId;
				tmpId++;
			}
		}
	}
	
	// Copy all the verts over.  NB: We don't skip any sections 
	// so the index buffer offsets will still be valid.
	// NB: we do clamp the UVs to +/- 1024

	for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		const FSkelMeshSection& Section = SrcLODModel.Sections[SectionIndex];
		const auto& BoneMap = Section.BoneMap;

		const FSectionRange VertexRange = SectionRangeArray[SectionIndex];

		for (int32 v = VertexRange.Begin; v < VertexRange.End; ++v)
		{
			const auto& SkinnedVertex = SoftSkinVertices[v];

			auto& BasicAttrs  = OutVertexBuffer[v].BasicAttributes;
			auto& SparseBones = OutVertexBuffer[v].SparseBones;

			BasicAttrs.Normal    = SkinnedVertex.TangentZ;
			BasicAttrs.Tangent   = SkinnedVertex.TangentX;
			BasicAttrs.BiTangent = SkinnedVertex.TangentY;

			for (uint32 t = 0; t < TexCoordCount; ++t)
			{
				BasicAttrs.TexCoords[t].X = FMath::Clamp(SkinnedVertex.UVs[t].X, -1024.f, 1024.f);
				BasicAttrs.TexCoords[t].Y = FMath::Clamp(SkinnedVertex.UVs[t].Y, -1024.f, 1024.f);
			}
			for (uint32 t = TexCoordCount; t < MAX_TEXCOORDS; ++t)
			{
				BasicAttrs.TexCoords[t].X = 0.f;
				BasicAttrs.TexCoords[t].Y = 0.f;
			}

			BasicAttrs.Color = SkinnedVertex.Color;
			OutVertexBuffer[v].Position = SkinnedVertex.Position;
			OutVertexBuffer[v].MaterialIndex = 0; // default to be over-written

			for (int32 i = 0; i < MAX_TOTAL_INFLUENCES; ++i)
			{
				int32 localBoneId = (int32)SkinnedVertex.InfluenceBones[i];
				const uint16 boneId = BoneMap[localBoneId];

				const uint8 Influence = SkinnedVertex.InfluenceWeights[i];
				double boneWeight = ((double)Influence) / 255.;

				// For right now, only store bone weights that are greater than zero,
				// by default the sparse data structure assumes a value of zero for 
				// any non-initialized bones.

				if (Influence > 0)
				{
					SparseBones.SetElement((int32)boneId, boneWeight);
				}
			}
		}
	}

	// store sectionID or MaterialID in the material id (there is a one to one mapping between them).

	for (int32 s = 0; s < SectionCount; ++s)
	{
		if (SkipSection(s))
		{
			continue;
		}
		uint16 MaterialId = SrcLODModel.Sections[s].MaterialIndex;

		const FSectionRange VertexRange = SectionRangeArray[s];

		for (int32 v = VertexRange.Begin; v < VertexRange.End; ++v)
		{
			//OutVertexBuffer[v].MaterialIndex = s;
			OutVertexBuffer[v].MaterialIndex = MaterialId;
		}
	}

	// Put the vertex in a "correct" state.
	//    "corrects" normals (ensures that they are orthonormal)
	//    re-orders the bones by weight (highest to lowest)

	for (int32 s = 0; s < SectionCount; ++s)
	{
		if (SkipSection(s))
		{
			continue;
		}

		const FSectionRange VertexRange = SectionRangeArray[s];

		for (int32 v = VertexRange.Begin; v < VertexRange.End; ++v)
		{
			OutVertexBuffer[v].Correct();
		}
	}

	// Compact the mesh to remove any unreferenced verts
	// and fix-up the index buffer

	OutSkinnedMesh.Compact();
}


void FQuadricSkeletalMeshReduction::UpdateSpecializedVertWeights(const FImportantBones& ImportantBones, SkeletalSimplifier::FSkinnedSkeletalMesh& SkinnedSkeletalMesh) const
{
	const float Weight = ImportantBones.Weight;

	int32 NumVerts = SkinnedSkeletalMesh.NumVertices();

	//If a vertex has one of the important bones as its' major bone, associated the ImportantBones.Weight
	for (int32 i = 0; i < NumVerts; ++i)
	{
		auto& Vert = SkinnedSkeletalMesh.VertexBuffer[i];
		const auto& Bones = Vert.GetSparseBones();
		if (!Bones.bIsEmpty())
		{
			auto CIter = Bones.GetData().CreateConstIterator();

			const int32 FirstBone = CIter.Key(); // Bones are ordered by descending weight

			if (ImportantBones.Ids.Contains(FirstBone))
			{
				Vert.SpecializedWeight = Weight;
			}
		}
		else
		{
			Vert.SpecializedWeight = 0.f;
		}
	}
}



void FQuadricSkeletalMeshReduction::TrimBonesPerVert( SkeletalSimplifier::FSkinnedSkeletalMesh& Mesh, int32 MaxBonesPerVert ) const
{
	// Loop over all the verts in the mesh, and reduce the bone count.

	SkeletalSimplifier::MeshVertType* VertexBuffer = Mesh.VertexBuffer;

	for (int32 i = 0, I = Mesh.NumVertices(); i < I; ++i)
	{
		SkeletalSimplifier::MeshVertType& Vertex = VertexBuffer[i];

		auto& SparseBones = Vertex.SparseBones;
		SparseBones.Correct(MaxBonesPerVert);
	}

}


void FQuadricSkeletalMeshReduction::ComputeUVBounds( const SkeletalSimplifier::FSkinnedSkeletalMesh& Mesh,
	                                                 FVector2D(&UVBounds)[2 * SkeletalSimplifier::MeshVertType::BasicAttrContainerType::NumUVs] ) const
{
	// Zero the bounds
	{
		const int32 NumUVs = SkeletalSimplifier::MeshVertType::BasicAttrContainerType::NumUVs;

		for (int32 i = 0; i < 2 * NumUVs; ++i)
		{
			UVBounds[i] = FVector2D(ForceInitToZero);
		}
	}


	{
		const int32 NumValidUVs = Mesh.TexCoordCount();
		for (int32 i = 0; i < NumValidUVs; ++i)
		{
			UVBounds[2 * i] = FVector2D(FLT_MAX, FLT_MAX);
			UVBounds[2 * i + 1] = FVector2D(-FLT_MAX, -FLT_MAX);
		}

		for (int32 i = 0; i < Mesh.NumVertices(); ++i)
		{
			const auto& Attrs = Mesh.VertexBuffer[i].BasicAttributes;
			for (int32 t = 0; t < NumValidUVs; ++t)
			{
				UVBounds[2 * t].X = FMath::Min(Attrs.TexCoords[t].X, UVBounds[2 * t].X);
				UVBounds[2 * t].Y = FMath::Min(Attrs.TexCoords[t].Y, UVBounds[2 * t].Y);

				UVBounds[2 * t + 1].X = FMath::Max(Attrs.TexCoords[t].X, UVBounds[2 * t + 1].X);
				UVBounds[2 * t + 1].Y = FMath::Max(Attrs.TexCoords[t].Y, UVBounds[2 * t + 1].Y);
			}
		}
	}
}

void FQuadricSkeletalMeshReduction::ClampUVBounds( const FVector2D(&UVBounds)[2 * SkeletalSimplifier::MeshVertType::BasicAttrContainerType::NumUVs],
	                                               SkeletalSimplifier::FSkinnedSkeletalMesh& Mesh ) const
{
	const int32 NumValidUVs = Mesh.TexCoordCount();


	for (int32 i = 0; i < Mesh.NumVertices(); ++i)
	{
		auto& Attrs = Mesh.VertexBuffer[i].BasicAttributes;
		for (int32 t = 0; t < NumValidUVs; ++t)
		{
			Attrs.TexCoords[t].X = FMath::Clamp(Attrs.TexCoords[t].X, UVBounds[2 * t].X, UVBounds[2 * t + 1].X);
			Attrs.TexCoords[t].Y = FMath::Clamp(Attrs.TexCoords[t].Y, UVBounds[2 * t].Y, UVBounds[2 * t + 1].Y);
		}
	}
}


float FQuadricSkeletalMeshReduction::SimplifyMesh( const FSkeletalMeshOptimizationSettings& Settings,
	                                               const FBoxSphereBounds& Bounds,
	                                               SkeletalSimplifier::FSkinnedSkeletalMesh& Mesh
                                                  ) const
{

	// Convert settings to weights and a termination criteria

	// Determine the stop criteria used

	const bool bUseVertexPercentCriterion = Settings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_NumOfVerts || Settings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_TriangleOrVert;
	const bool bUseTrianglePercentCriterion = Settings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_NumOfTriangles || Settings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_TriangleOrVert;

	const bool bUseMaxVertNumCriterion = Settings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_AbsNumOfVerts || Settings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_AbsTriangleOrVert;
	const bool bUseMaxTrisNumCriterion = Settings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_AbsNumOfTriangles || Settings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_AbsTriangleOrVert;

	// We can support a stopping criteria based on the MaxDistance the new vertex is from the plans of the source triangles.
	// but there seems to be no good use for this.  We are better off just using triangle count.
	const float MaxDist = FLT_MAX; // (Settings.ReductionMethod != SkeletalMeshOptimizationType::SMOT_NumOfTriangles) ? Settings.MaxDeviationPercentage * Bounds.SphereRadius : FLT_MAX;
	const int32 SrcTriNum = Mesh.NumIndices() / 3;
	const float TriangleRetainRatio = FMath::Clamp(Settings.NumOfTrianglesPercentage, 0.f, 1.f);
	const int32 TargetTriNum = (bUseTrianglePercentCriterion) ? FMath::CeilToInt(TriangleRetainRatio * SrcTriNum) : Settings.MaxNumOfTriangles;

	const int32 MinTriNumToRetain = (bUseTrianglePercentCriterion || bUseMaxTrisNumCriterion) ? FMath::Max(4, TargetTriNum) : 4;
	const float MaxCollapseCost = FLT_MAX;

	const int32 SrcVertNum = Mesh.NumVertices();
	const float VertRetainRatio = FMath::Clamp(Settings.NumOfVertPercentage, 0.f, 1.f);
	const int32 TargetVertNum = (bUseVertexPercentCriterion) ? FMath::CeilToInt(VertRetainRatio * SrcVertNum) : Settings.MaxNumOfVerts + 1;
	const int32 MinVerNumToRetain = (bUseVertexPercentCriterion || bUseMaxVertNumCriterion) ? FMath::Max(6, TargetVertNum) : 6;

	const float VolumeImportance      = FMath::Clamp(Settings.VolumeImportance, 0.f, 2.f);
	const bool bLockEdges             = Settings.bLockEdges;
	const bool bPreserveVolume        = (VolumeImportance > 1.e-4);
	const bool bEnforceBoneBoundaries = Settings.bEnforceBoneBoundaries;

	// Terminator tells the simplifier when to stop
	SkeletalSimplifier::FSimplifierTerminator Terminator(MinTriNumToRetain, SrcTriNum, MinVerNumToRetain, SrcVertNum, MaxCollapseCost, MaxDist);

	double NormalWeight    =16.00;
	double TangentWeight   = 0.10;
	double BiTangentWeight = 0.10;
	double UVWeight        = 0.50;
	double BoneWeight      = 0.25;
	double ColorWeight     = 0.10;
	/**
	// Magic table used to scale the default simplifier weights.
	// Numbers like these were used to express controls for the 3rd party tool
	const float ImportanceTable[] =
	{
		0.0f,	// OFF
		0.125f,	// Lowest
		0.35f,	// Low,
		1.0f,	// Normal
		2.8f,	// High
		8.0f,	// Highest
	};
	static_assert(ARRAY_COUNT(ImportanceTable) == SMOI_MAX, "Bad importance table size.");

	NormalWeight    *= ImportanceTable[Settings.ShadingImportance];
	TangentWeight   *= ImportanceTable[Settings.ShadingImportance];
	BiTangentWeight *= ImportanceTable[Settings.ShadingImportance];
	UVWeight        *= ImportanceTable[Settings.TextureImportance];
	BoneWeight      *= ImportanceTable[Settings.SkinningImportance];

	*/

	// Number of UV coords allocated.
	const int32 NumUVs = SkeletalSimplifier::MeshVertType::BasicAttrContainerType::NumUVs;

	FVector2D  UVBounds[2 * SkeletalSimplifier::MeshVertType::BasicAttrContainerType::NumUVs];
	ComputeUVBounds(Mesh, UVBounds);
	
	// Set up weights for the Basic Attributes (e.g. not the bones)
	SkeletalSimplifier::FMeshSimplifier::WeightArrayType  BasicAttrWeights; // by default constructs to zeros.
	{

		// Normal
		BasicAttrWeights[0] = NormalWeight;
		BasicAttrWeights[1] = NormalWeight;
		BasicAttrWeights[2] = NormalWeight;

		//Tangent
		BasicAttrWeights[3] = TangentWeight;
		BasicAttrWeights[4] = TangentWeight;
		BasicAttrWeights[5] = TangentWeight;
		//BiTangent
		BasicAttrWeights[6] = BiTangentWeight;
		BasicAttrWeights[7] = BiTangentWeight;
		BasicAttrWeights[8] = BiTangentWeight;

		//Color
		BasicAttrWeights[ 9] = ColorWeight; // r
		BasicAttrWeights[10] = ColorWeight; // b
		BasicAttrWeights[11] = ColorWeight; // b
		BasicAttrWeights[12] = ColorWeight; // alpha


		const int32 NumNonUVAttrs = 13;
		checkSlow(NumNonUVAttrs + NumUVs * 2 == BasicAttrWeights.Num());

		// Number of UVs actually used.
		const int32 NumValidUVs = Mesh.TexCoordCount();
		for (int32 i = 0; i < NumValidUVs; ++i)
		{
			FVector2D&  UVMin = UVBounds[2 * i];
			FVector2D&  UVMax = UVBounds[2 * i + 1];

			double URange = UVMax.X - UVMin.X;
			double VRange = UVMax.Y - UVMin.Y;

			double UWeight = (FMath::Abs(URange) > 1.e-5) ? UVWeight / URange : 0.;
			double VWeight = (FMath::Abs(VRange) > 1.e-5) ? UVWeight / VRange : 0.;

			BasicAttrWeights[NumNonUVAttrs + 2 * i    ] = UWeight; // U
			BasicAttrWeights[NumNonUVAttrs + 2 * i + 1] = VWeight; // V
		}

		for (int32 i = NumNonUVAttrs; i < NumNonUVAttrs + NumValidUVs * 2; ++i)
		{
			BasicAttrWeights[i] = UVWeight; // 0.5;
		}

		for (int32 i = NumNonUVAttrs + NumValidUVs * 2; i < NumNonUVAttrs + NumUVs * 2; ++i)
		{
			BasicAttrWeights[i] = 0.;
		}

	}

	// Additional parameters

	const bool bMergeCoincidentVertBones = true;
	const float EdgeWeightValue = 128.f;

	const float CoAlignmentLimit = FMath::Cos(45.f * PI / 180.); // 45 degrees limit

	// Create the simplifier

	SkeletalSimplifier::FMeshSimplifier  Simplifier(Mesh.VertexBuffer, (uint32)Mesh.NumVertices(),
		                                            Mesh.IndexBuffer, (uint32)Mesh.NumIndices(), 
		                                            CoAlignmentLimit, VolumeImportance, bPreserveVolume,  bEnforceBoneBoundaries);

	// The simplifier made a deep copy of the mesh.  

	Mesh.Empty();

	// Add additional control parameters to the simplifier.

	{
		// Set the edge weight that tries to keep UVseams from splitting.

		Simplifier.SetBoundaryConstraintWeight(EdgeWeightValue);

		// Set the weights for the dense attributes.

		Simplifier.SetAttributeWeights(BasicAttrWeights);

		// Set the bone weight.

		SkeletalSimplifier::FMeshSimplifier::SparseWeightContainerType BoneWeights(BoneWeight);
		Simplifier.SetSparseAttributeWeights(BoneWeights);


		if (bLockEdges)
		{
			// If locking the boundary, this has be be done before costs are computed.
			Simplifier.SetBoundaryLocked();
		}

	}

	// Do the actual simplification

	const float ResultError = Simplifier.SimplifyMesh(Terminator);

	// Resize the Mesh to hold the simplified result.

	Mesh.Resize(Simplifier.GetNumTris(), Simplifier.GetNumVerts());

	// Copy the simplified mesh back into Mesh

	Simplifier.OutputMesh(Mesh.VertexBuffer, Mesh.IndexBuffer, bMergeCoincidentVertBones, NULL);

	return ResultError;
}


void FQuadricSkeletalMeshReduction::ExtractFSkeletalData(const SkeletalSimplifier::FSkinnedSkeletalMesh& SkinnedMesh, FSkeletalMeshData& MeshData) const
{

	MeshData.TexCoordCount = SkinnedMesh.TexCoordCount();

	const int32 NumVerts   = SkinnedMesh.NumVertices();
	const int32 NumIndices = SkinnedMesh.NumIndices();
	const int32 NumTris    = NumIndices / 3;

	// Resize the MeshData.
	MeshData.Points.AddZeroed(NumVerts);
	MeshData.Faces.AddZeroed(NumTris);
	MeshData.Wedges.AddZeroed(NumIndices);

	TArray<FVector> PointNormals;
	TArray<uint32> PointList;
	TArray<uint32> PointInfluenceMap;  // index into MeshData.Influences.   Id = PointInfluenceMap[v];  first_influence_for_vert 'v' = MeshData.Influences[Id] 

	PointNormals.AddZeroed(NumVerts);

	PointList.Reserve(NumVerts);
	for (int32 i = 0; i < NumVerts; ++i)
	{
		PointList.Add(INDEX_NONE);
	}

	PointInfluenceMap.Reserve(NumVerts);
	for (int32 i = 0; i < NumVerts; ++i)
	{
		PointInfluenceMap.Add(INDEX_NONE);
	}

	
	// Per-vertex data
	
	for (uint32 v = 0; v < (uint32)NumVerts; ++v)
	{
		// The simplifier mesh vertex, has all the vertex attributes.

		const auto& SimpVertex = SkinnedMesh.VertexBuffer[v];
		
		// Copy location.
		
		MeshData.Points[v] = SimpVertex.GetPos();

		// Sort out the bones for this vert.

		PointInfluenceMap[v] = (uint32)MeshData.Influences.Num();

		// loop over the bones for this vertex, making weights.

		const auto& SparseBones = SimpVertex.GetSparseBones().GetData();

		int32 NumBonesAdded = 0;
		for (const auto& BoneData : SparseBones)
		{
			if (BoneData.Value > 0.)
			{
				SkeletalMeshImportData::FVertInfluence VertInfluence = { (float) BoneData.Value, v, (uint16) BoneData.Key};

				MeshData.Influences.Add(VertInfluence);
				NumBonesAdded++;
			}
		}
		
		// If no influences were added, add a default bone
		
		if (NumBonesAdded == 0)
		{
			SkeletalMeshImportData::FVertInfluence VertInfluence = { 0.f, v, (uint16)0 };

			MeshData.Influences.Add(VertInfluence);
	
		}
	}

	// loop over triangles.
	for (int32 t = 0; t < NumTris; ++t)
	{
		SkeletalMeshImportData::FMeshFace& Face = MeshData.Faces[t];

		
		uint32 MatId[3];

		// loop over the three corners for the triangle.
		// NB: We may have already visited these verts before..
		
		for (uint32 c = 0; c < 3; ++c)
		{
			const uint32 wedgeId = t * 3 + c;
			const uint32 vertId = SkinnedMesh.IndexBuffer[wedgeId];
			const auto& SimpVertex = SkinnedMesh.VertexBuffer[vertId];

			FVector WedgeNormal = SimpVertex.BasicAttributes.Normal;
			WedgeNormal.Normalize();

			Face.TangentX[c] = SimpVertex.BasicAttributes.Tangent;
			Face.TangentY[c] = SimpVertex.BasicAttributes.BiTangent;
			Face.TangentZ[c] = WedgeNormal;

			Face.iWedge[c] = wedgeId;

			MatId[c] = SimpVertex.MaterialIndex;

			//
			uint32 tmpVertId = vertId;
			FVector PointNormal = PointNormals[tmpVertId];

			if (PointNormal.SizeSquared() < KINDA_SMALL_NUMBER) // the array starts with 0'd out normals
			{
				PointNormals[tmpVertId] = WedgeNormal;
			}
			else // we have already visited this vert ..
			{
				while (FVector::DotProduct(PointNormal, WedgeNormal) - 1.f < -KINDA_SMALL_NUMBER)
				{
					tmpVertId = PointList[tmpVertId];
					if (tmpVertId == INDEX_NONE)
					{
						break;
					}
					checkSlow(tmpVertId < (uint32)PointList.Num());
					PointNormal = PointNormals[tmpVertId];
				}

				if (tmpVertId == INDEX_NONE)
				{
					// Add a copy of this point.. 
					FVector Point = MeshData.Points[vertId];
					tmpVertId = MeshData.Points.Add(Point);

					PointNormals.Add(WedgeNormal);

					uint32 nextVertId = PointList[vertId];
					PointList[vertId] = tmpVertId;
					PointList.Add(nextVertId);
					PointInfluenceMap.Add((uint32)MeshData.Influences.Num());

					int32 influenceId = PointInfluenceMap[vertId];
					while (MeshData.Influences[influenceId].VertIndex == vertId)
					{
						const auto& Influence = MeshData.Influences[influenceId];

						SkeletalMeshImportData::FVertInfluence VertInfluence = { Influence.Weight, tmpVertId, Influence.BoneIndex };
						MeshData.Influences.Add(VertInfluence);
						influenceId++;
					}
				}

			}

			// Populate the corresponding wedge.
			SkeletalMeshImportData::FMeshWedge& Wedge = MeshData.Wedges[wedgeId];
			Wedge.iVertex  = tmpVertId; // vertId;
			Wedge.Color    = SimpVertex.BasicAttributes.Color.ToFColor(true/** sRGB**/);
			for (int32 tcIdx = 0; tcIdx < MAX_TEXCOORDS; ++tcIdx)
			{
				Wedge.UVs[tcIdx] = SimpVertex.BasicAttributes.TexCoords[tcIdx];
			}

			

		}
		// The material id is only being stored on a per-vertex case..
		// but should be shared by all 3 verts in a triangle.

		Face.MeshMaterialIndex = (uint16)MatId[0];

	}

}


void FQuadricSkeletalMeshReduction::ConvertToFSkeletalMeshLODModel( const SkeletalSimplifier::FSkinnedSkeletalMesh& SkinnedMesh,
	                                                                const FReferenceSkeleton& RefSkeleton,
	                                                                FSkeletalMeshLODModel& NewModel
                                                                   ) const
{

	// Convert the mesh to a struct of arrays
	
	FSkeletalMeshData SkeletalMeshData;
	ExtractFSkeletalData(SkinnedMesh, SkeletalMeshData);


	// Create dummy map of 'point to original'
	TArray<int32> DummyMap;
	DummyMap.AddUninitialized(SkeletalMeshData.Points.Num());
	for (int32 PointIdx = 0; PointIdx < SkeletalMeshData.Points.Num(); PointIdx++)
	{
		DummyMap[PointIdx] = PointIdx;
	}

	// Make sure we do not recalculate normals
	IMeshUtilities::MeshBuildOptions Options;
	Options.bComputeNormals = false;
	Options.bComputeTangents = false;
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	// Create skinning streams for NewModel.
	MeshUtilities.BuildSkeletalMesh(
		NewModel,
		RefSkeleton,
		SkeletalMeshData.Influences,
		SkeletalMeshData.Wedges,
		SkeletalMeshData.Faces,
		SkeletalMeshData.Points,
		DummyMap,
		Options
	);

	// Set texture coordinate count on the new model.
	NewModel.NumTexCoords = SkeletalMeshData.TexCoordCount;
}




bool FQuadricSkeletalMeshReduction::ReduceSkeletalLODModel( const FSkeletalMeshLODModel& SrcModel,
	                                                        FSkeletalMeshLODModel& OutSkeletalMeshLODModel,
	                                                        const FBoxSphereBounds& Bounds,
	                                                        const FReferenceSkeleton& RefSkeleton,
	                                                        const FSkeletalMeshOptimizationSettings& Settings,
															const FImportantBones& ImportantBones,
	                                                        const TArray<FMatrix>& BoneMatrices,
	                                                        const int32 LODIndex
                                                           ) const
{
	// Parameters for Simplification etc
	const bool bUseVertexCriterion   = (Settings.TerminationCriterion != SMTC_NumOfTriangles && Settings.NumOfVertPercentage < 1.f);
	const bool bUseTriangleCriterion = (Settings.TerminationCriterion != SMTC_NumOfVerts && Settings.NumOfTrianglesPercentage < 1.f);
	const bool bProcessGeometry      = (bUseTriangleCriterion || bUseVertexCriterion);
	const bool bProcessBones         = (Settings.MaxBonesPerVertex < MAX_TOTAL_INFLUENCES);
	const bool bOptimizeMesh         = (bProcessGeometry || bProcessBones);


	
	// Generate a single skinned mesh form the SrcModel.  This mesh has per-vertex tangent space.

	SkeletalSimplifier::FSkinnedSkeletalMesh SkinnedSkeletalMesh;
	ConvertToFSkinnedSkeletalMesh(SrcModel, BoneMatrices, LODIndex, SkinnedSkeletalMesh);

	if (bOptimizeMesh)
	{
		if (ImportantBones.Ids.Num() > 0)
		{
			// Add specialized weights for verts associated with "important" bones.
			UpdateSpecializedVertWeights(ImportantBones, SkinnedSkeletalMesh);
		}

		// Capture the UV bounds from the source mesh.

		FVector2D  UVBounds[2 * SkeletalSimplifier::MeshVertType::BasicAttrContainerType::NumUVs];
		ComputeUVBounds(SkinnedSkeletalMesh, UVBounds);

		{
			// Use the bone-aware simplifier

			SimplifyMesh(Settings, Bounds, SkinnedSkeletalMesh);
		}

		// Clamp the UVs of the simplified mesh to match the source mesh.

		ClampUVBounds(UVBounds, SkinnedSkeletalMesh);


		// Reduce the number of bones per-vert

		const int32 MaxBonesPerVert = FMath::Clamp(Settings.MaxBonesPerVertex, 0, MAX_TOTAL_INFLUENCES);

		if (MaxBonesPerVert < MAX_TOTAL_INFLUENCES)
		{
			TrimBonesPerVert(SkinnedSkeletalMesh, MaxBonesPerVert);
		}
	}

	// Convert to SkeletalMeshLODModel. 
		
	ConvertToFSkeletalMeshLODModel(SkinnedSkeletalMesh, RefSkeleton, OutSkeletalMeshLODModel);

	bool bReturnValue =  (OutSkeletalMeshLODModel.NumVertices > 0);
	

	return bReturnValue;
}


void FQuadricSkeletalMeshReduction::ReduceSkeletalMesh(USkeletalMesh& SkeletalMesh, FSkeletalMeshModel& SkeletalMeshResource, int32 LODIndex) const
{
	check(LODIndex <= SkeletalMeshResource.LODModels.Num());

	//If the Current LOD is an import from file
	bool bOldLodWasFromFile = SkeletalMesh.IsValidLODIndex(LODIndex) && SkeletalMesh.GetLODInfo(LODIndex)->bHasBeenSimplified == false;

	//True if the LOD is added by this reduction
	bool bLODModelAdded = false;

	// Insert a new LOD model entry if needed.
	if (LODIndex == SkeletalMeshResource.LODModels.Num())
	{
		FSkeletalMeshLODModel* ModelPtr = NULL;
		SkeletalMeshResource.LODModels.Add(ModelPtr);
		bLODModelAdded = true;
	}

	// Copy over LOD info from LOD0 if there is no previous info.
	if (LODIndex == SkeletalMesh.GetLODNum())
	{
		// if there is no LOD, add one more
		SkeletalMesh.AddLODInfo();
	}


	// Swap in a new model, delete the old.

	FSkeletalMeshLODModel** LODModels = SkeletalMeshResource.LODModels.GetData();


	// get settings
	FSkeletalMeshLODInfo* LODInfo = SkeletalMesh.GetLODInfo(LODIndex);
	const FSkeletalMeshOptimizationSettings& Settings = LODInfo->ReductionSettings;
	

	// Struct to identify important bones.  Vertices associated with these bones
	// will have additional collapse weight added to them.

	FImportantBones  ImportantBones;
	{
		const TArray<FBoneReference>& BonesToPrioritize = LODInfo->BonesToPrioritize;
		const float BonePrioritizationWeight = LODInfo->WeightOfPrioritization;

		ImportantBones.Weight = BonePrioritizationWeight;
		for (const FBoneReference& BoneReference : BonesToPrioritize)
		{
			int32 BoneId = SkeletalMesh.RefSkeleton.FindRawBoneIndex(BoneReference.BoneName);

			// Q: should we exclude BoneId = 0?
			ImportantBones.Ids.AddUnique(BoneId);
		}
	}
	// select which mesh we're reducing from
	// use BaseLOD
	int32 BaseLOD = 0;
	FSkeletalMeshModel* SkelResource = SkeletalMesh.GetImportedModel();
	FSkeletalMeshLODModel* SrcModel = &SkelResource->LODModels[0];

	// only allow to set BaseLOD if the LOD is less than this
	if (Settings.BaseLOD > 0)
	{
		if (Settings.BaseLOD == LODIndex && (!SkelResource->OriginalReductionSourceMeshData.IsValidIndex(Settings.BaseLOD) || SkelResource->OriginalReductionSourceMeshData[Settings.BaseLOD]->IsEmpty()))
		{
			//Cannot reduce ourself if we are not imported
			UE_LOG(LogSkeletalMeshReduction, Warning, TEXT("Building LOD %d - Cannot generate LOD with himself if the LOD do not have imported Data. Using Base LOD 0 instead"), LODIndex);
		}
		else if (Settings.BaseLOD <= LODIndex && SkeletalMeshResource.LODModels.IsValidIndex(Settings.BaseLOD))
		{
			BaseLOD = Settings.BaseLOD;
			SrcModel = &SkeletalMeshResource.LODModels[BaseLOD];
		}
		else
		{
			// warn users
			UE_LOG(LogSkeletalMeshReduction, Warning, TEXT("Building LOD %d - Invalid Base LOD entered. Using Base LOD 0 instead"), LODIndex);
		}
	}
	
	auto FillClothingData = [&SkeletalMeshResource, &LODIndex, bLODModelAdded](int32 &EnableSectionNumber, TArray<bool> &SectionStatus)
	{
		EnableSectionNumber = 0;
		SectionStatus.Empty();
		if (!bLODModelAdded && SkeletalMeshResource.LODModels.IsValidIndex(LODIndex))
		{
			int32 SectionNumber = SkeletalMeshResource.LODModels[LODIndex].Sections.Num();
			SectionStatus.Reserve(SectionNumber);
			for (int32 SectionIndex = 0; SectionIndex < SectionNumber; ++SectionIndex)
			{
				SectionStatus.Add(!SkeletalMeshResource.LODModels[LODIndex].Sections[SectionIndex].bDisabled);
				if (SectionStatus[SectionIndex])
				{
					EnableSectionNumber++;
				}
			}
		}
	};

	// Unbind any existing clothing assets before we reimport the geometry
	TArray<ClothingAssetUtils::FClothingAssetMeshBinding> ClothingBindings;
	//Get a map of enable/disable sections
	int32 OriginalSectionNumberBeforeReduction = 0;
	TArray<bool> OriginalSectionEnableBeforeReduction;

	//Do not play with cloth if the LOD is added
	if (!bLODModelAdded)
	{
		//Store the clothBinding
		ClothingAssetUtils::GetMeshClothingAssetBindings(&SkeletalMesh, ClothingBindings, LODIndex);
		FillClothingData(OriginalSectionNumberBeforeReduction, OriginalSectionEnableBeforeReduction);
		//Unbind the Cloth for this LOD before we reduce it, we will put back the cloth after the reduction, if it still match the sections
		for (ClothingAssetUtils::FClothingAssetMeshBinding& Binding : ClothingBindings)
		{
			if (Binding.LODIndex == LODIndex)
			{
				Binding.Asset->UnbindFromSkeletalMesh(&SkeletalMesh, Binding.LODIndex);
			}
		}
	}

	bool bReducingSourceModel = false;
	//Reducing Base LOD, we need to use the temporary data so it can be iterative
	if (BaseLOD == LODIndex && SkelResource->OriginalReductionSourceMeshData.IsValidIndex(BaseLOD) && !SkelResource->OriginalReductionSourceMeshData[BaseLOD]->IsEmpty())
	{
		TMap<FString, TArray<FMorphTargetDelta>> TempLODMorphTargetData;
		SkelResource->OriginalReductionSourceMeshData[BaseLOD]->LoadReductionData(*SrcModel, TempLODMorphTargetData);
		bReducingSourceModel = true;
	}
	else
	{
		check(BaseLOD < LODIndex);
	}

	check(SrcModel);

	// now try bone reduction process if it's setup
	TMap<FBoneIndexType, FBoneIndexType> BonesToRemove;
	
	IMeshBoneReduction* MeshBoneReductionInterface = FModuleManager::Get().LoadModuleChecked<IMeshBoneReductionModule>("MeshBoneReduction").GetMeshBoneReductionInterface();

	TArray<FName> BoneNames;
	const int32 NumBones = SkeletalMesh.RefSkeleton.GetNum();
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		BoneNames.Add(SkeletalMesh.RefSkeleton.GetBoneName(BoneIndex));
	}

	// get the relative to ref pose matrices
	TArray<FMatrix> RelativeToRefPoseMatrices;
	RelativeToRefPoseMatrices.AddUninitialized(NumBones);
	// if it has bake pose, gets ref to local matrices using bake pose
	if (const UAnimSequence* BakePoseAnim = SkeletalMesh.GetLODInfo(LODIndex)->BakePose)
	{
		TArray<FTransform> BonePoses;
		UAnimationBlueprintLibrary::GetBonePosesForFrame(BakePoseAnim, BoneNames, 0, true, BonePoses, &SkeletalMesh);

		const FReferenceSkeleton& RefSkeleton = SkeletalMesh.RefSkeleton;
		const TArray<FTransform>& RefPoseInLocal = RefSkeleton.GetRefBonePose();

		// get component ref pose
		TArray<FTransform> RefPoseInCS;
		FAnimationRuntime::FillUpComponentSpaceTransforms(RefSkeleton, RefPoseInLocal, RefPoseInCS);

		// calculate component space bake pose
		TArray<FMatrix> ComponentSpacePose, ComponentSpaceRefPose, AnimPoseMatrices;
		ComponentSpacePose.AddUninitialized(NumBones);
		ComponentSpaceRefPose.AddUninitialized(NumBones);
		AnimPoseMatrices.AddUninitialized(NumBones);

		// to avoid scale issue, we use matrices here
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			ComponentSpaceRefPose[BoneIndex] = RefPoseInCS[BoneIndex].ToMatrixWithScale();
			AnimPoseMatrices[BoneIndex] = BonePoses[BoneIndex].ToMatrixWithScale();
		}

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
			if (ParentIndex != INDEX_NONE)
			{
				ComponentSpacePose[BoneIndex] = AnimPoseMatrices[BoneIndex] * ComponentSpacePose[ParentIndex];
			}
			else
			{
				ComponentSpacePose[BoneIndex] = AnimPoseMatrices[BoneIndex];
			}
		}

		// calculate relative to ref pose transform and convert to matrices
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			RelativeToRefPoseMatrices[BoneIndex] = ComponentSpaceRefPose[BoneIndex].Inverse() * ComponentSpacePose[BoneIndex];
		}
	}
	else
	{
		for (int32 Index = 0; Index < NumBones; ++Index)
		{
			RelativeToRefPoseMatrices[Index] = FMatrix::Identity;
		}
	}

	FSkeletalMeshLODModel* NewModel = new FSkeletalMeshLODModel();

	// Swap out the old model.  

	{
		FSkeletalMeshLODModel* Old = LODModels[LODIndex];

		LODModels[LODIndex] = NewModel;

		if (!bReducingSourceModel && Old)
		{
			delete Old;
		}
	}

	

	// Reduce LOD model with SrcMesh

	if (ReduceSkeletalLODModel(*SrcModel, *NewModel, SkeletalMesh.GetImportedBounds(), SkeletalMesh.RefSkeleton, Settings, ImportantBones, RelativeToRefPoseMatrices, LODIndex))
	{
		// Do any joint-welding / bone removal.

		if (MeshBoneReductionInterface != NULL && MeshBoneReductionInterface->GetBoneReductionData(&SkeletalMesh, LODIndex, BonesToRemove))
		{
			// fix up chunks to remove the bones that set to be removed
			for (int32 SectionIndex = 0; SectionIndex < NewModel->Sections.Num(); ++SectionIndex)
			{
				MeshBoneReductionInterface->FixUpSectionBoneMaps(NewModel->Sections[SectionIndex], BonesToRemove);
			}
		}

		if (bOldLodWasFromFile)
		{
			SkeletalMesh.GetLODInfo(LODIndex)->LODMaterialMap.Empty();
		}

		// If base lod has a customized LODMaterialMap and this LOD doesn't (could have if changes are applied instead of freshly generated, copy over the data into new new LOD

		if (SkeletalMesh.GetLODInfo(LODIndex)->LODMaterialMap.Num() == 0 && SkeletalMesh.GetLODInfo(BaseLOD)->LODMaterialMap.Num() != 0)
		{
			SkeletalMesh.GetLODInfo(LODIndex)->LODMaterialMap = SkeletalMesh.GetLODInfo(BaseLOD)->LODMaterialMap;
		}
		else
		{
			// Assuming the reducing step has set all material indices correctly, we double check if something went wrong
			// make sure we don't have more materials
			int32 TotalSectionCount = NewModel->Sections.Num();
			if (SkeletalMesh.GetLODInfo(LODIndex)->LODMaterialMap.Num() > TotalSectionCount)
			{
				SkeletalMesh.GetLODInfo(LODIndex)->LODMaterialMap = SkeletalMesh.GetLODInfo(BaseLOD)->LODMaterialMap;
				// Something went wrong during the reduce step during regenerate 					
				check(SkeletalMesh.GetLODInfo(BaseLOD)->LODMaterialMap.Num() == TotalSectionCount || SkeletalMesh.GetLODInfo(BaseLOD)->LODMaterialMap.Num() == 0);
			}
		}

		// Flag this LOD as having been simplified.
		SkeletalMesh.GetLODInfo(LODIndex)->bHasBeenSimplified = true;
		SkeletalMesh.bHasBeenSimplified = true;
	}
	else
	{
		//	Bulk data arrays need to be locked before a copy can be made.
		SrcModel->RawPointIndices.Lock(LOCK_READ_ONLY);
		SrcModel->LegacyRawPointIndices.Lock(LOCK_READ_ONLY);
		*NewModel = *SrcModel;
		SrcModel->RawPointIndices.Unlock();
		SrcModel->LegacyRawPointIndices.Unlock();

		// Do any joint-welding / bone removal.

		if (MeshBoneReductionInterface != NULL && MeshBoneReductionInterface->GetBoneReductionData(&SkeletalMesh, LODIndex, BonesToRemove))
		{
			// fix up chunks to remove the bones that set to be removed
			for (int32 SectionIndex = 0; SectionIndex < NewModel->Sections.Num(); ++SectionIndex)
			{
				MeshBoneReductionInterface->FixUpSectionBoneMaps(NewModel->Sections[SectionIndex], BonesToRemove);
			}
		}

		//Clean up some section data

		for (int32 SectionIndex = SrcModel->Sections.Num() - 1; SectionIndex >= 0; --SectionIndex)
		{
			//New model should be reset to -1 value
			NewModel->Sections[SectionIndex].GenerateUpToLodIndex = -1;
			int8 GenerateUpToLodIndex = SrcModel->Sections[SectionIndex].GenerateUpToLodIndex;
			if (GenerateUpToLodIndex != -1 && GenerateUpToLodIndex < LODIndex)
			{
				//Remove the section
				RemoveMeshSection(*NewModel, SectionIndex);
			}
		}

		SkeletalMesh.GetLODInfo(LODIndex)->LODMaterialMap = SkeletalMesh.GetLODInfo(BaseLOD)->LODMaterialMap;

		// Required bones are recalculated later on.

		NewModel->RequiredBones.Empty();
		SkeletalMesh.GetLODInfo(LODIndex)->bHasBeenSimplified = true;
		SkeletalMesh.bHasBeenSimplified = true;
	}
	
	if (!bLODModelAdded)
	{
		//Get the number of enabled section
		int32 SectionNumberAfterReduction = 0;
		TArray<bool> SectionEnableAfterReduction;
		FillClothingData(SectionNumberAfterReduction, SectionEnableAfterReduction);

		//Put back the clothing for this newly reduce LOD only if the section count match.
		if (ClothingBindings.Num() > 0 && OriginalSectionNumberBeforeReduction == SectionNumberAfterReduction)
		{
			TArray<int32> RemapSectionIndex;
			int32 SectionIndexTest = 0;
			for (int32 SectionIndexRef = 0; SectionIndexRef < OriginalSectionEnableBeforeReduction.Num(); SectionIndexRef++)
			{
				int32& RemapValue = RemapSectionIndex.Add_GetRef(INDEX_NONE);
				if (!OriginalSectionEnableBeforeReduction[SectionIndexRef])
				{
					continue;
				}
				for (; SectionIndexTest <= SectionIndexRef; SectionIndexTest++)
				{
					if (SectionEnableAfterReduction.IsValidIndex(SectionIndexTest) && SectionEnableAfterReduction[SectionIndexTest])
					{
						RemapValue = SectionIndexTest++;
						break;
					}
				}
			}

			for (ClothingAssetUtils::FClothingAssetMeshBinding& Binding : ClothingBindings)
			{
				int32 RemapBindingSectionIndex = RemapSectionIndex[Binding.SectionIndex];
				if (RemapBindingSectionIndex != INDEX_NONE && Binding.LODIndex == LODIndex && NewModel->Sections.IsValidIndex(RemapBindingSectionIndex))
				{
					Binding.Asset->BindToSkeletalMesh(&SkeletalMesh, Binding.LODIndex, RemapBindingSectionIndex, Binding.AssetInternalLodIndex, false);
				}
			}
		}
	}

	SkeletalMesh.CalculateRequiredBones(SkeletalMeshResource.LODModels[LODIndex], SkeletalMesh.RefSkeleton, &BonesToRemove);
}

#undef LOCTEXT_NAMESPACE
