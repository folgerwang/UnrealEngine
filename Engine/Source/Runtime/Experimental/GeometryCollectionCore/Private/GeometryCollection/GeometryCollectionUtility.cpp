// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection->cpp: FGeometryCollection methods.
=============================================================================*/

#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

DEFINE_LOG_CATEGORY_STATIC(FGeometryCollectionUtilityLogging, Log, All);

namespace GeometryCollection
{
	TSharedPtr<FGeometryCollection> MakeCubeElement(const FTransform& center, FVector Scale)
	{
		FGeometryCollection * RestCollection = new FGeometryCollection();

		int NumNewVertices = 8; // 8 vertices per cube
		int VerticesIndex = RestCollection->AddElements(NumNewVertices, FGeometryCollection::VerticesGroup);
		TSharedRef<TManagedArray<FVector> > VerticesRef = RestCollection->GetAttribute<FVector>("Vertex", FGeometryCollection::VerticesGroup);
		TSharedRef<TManagedArray<FVector> > NormalsRef = RestCollection->GetAttribute<FVector>("Normal", FGeometryCollection::VerticesGroup);
		TSharedRef<TManagedArray<FVector> > TangentURef = RestCollection->GetAttribute<FVector>("TangentU", FGeometryCollection::VerticesGroup);
		TSharedRef<TManagedArray<FVector> > TangentVRef = RestCollection->GetAttribute<FVector>("TangentV", FGeometryCollection::VerticesGroup);
		TSharedRef<TManagedArray<FVector2D> > UVsRef = RestCollection->GetAttribute<FVector2D>("UV", FGeometryCollection::VerticesGroup);
		TSharedRef<TManagedArray<FLinearColor> > ColorsRef = RestCollection->GetAttribute<FLinearColor>("Color", FGeometryCollection::VerticesGroup);

		int NumNewIndices = 2 * 6; // two triangles per face
		int IndicesIndex = RestCollection->AddElements(NumNewIndices, FGeometryCollection::FacesGroup);
		TSharedRef<TManagedArray<FIntVector> > IndicesRef = RestCollection->GetAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup);
		TSharedRef<TManagedArray<bool> > VisibleRef = RestCollection->GetAttribute<bool>("Visible", FGeometryCollection::FacesGroup);
		TSharedRef<TManagedArray<int32> > MaterialIndexRef = RestCollection->GetAttribute<int32>("MaterialIndex", FGeometryCollection::FacesGroup);
		TSharedRef<TManagedArray<int32> > MaterialIDRef = RestCollection->GetAttribute<int32>("MaterialID", FGeometryCollection::FacesGroup);

		int NumNewParticles = 1; // 1 particle for this geometry structure
		int ParticlesIndex = RestCollection->AddElements(NumNewParticles, FGeometryCollection::TransformGroup);
		TSharedRef<TManagedArray<FTransform> > TransformRef = RestCollection->GetAttribute<FTransform>("Transform", FGeometryCollection::TransformGroup);

		TManagedArray<FVector>& Vertices = *VerticesRef;
		TManagedArray<FVector>&  Normals = *NormalsRef;
		TManagedArray<FVector>&  TangentU = *TangentURef;
		TManagedArray<FVector>&  TangentV = *TangentVRef;
		TManagedArray<FVector2D>&  UVs = *UVsRef;
		TManagedArray<FLinearColor>&  Colors = *ColorsRef;
		TManagedArray<FIntVector>&  Indices = *IndicesRef;
		TManagedArray<bool>&  Visible = *VisibleRef;
		TManagedArray<int32>&  MaterialIndex = *MaterialIndexRef;
		TManagedArray<int32>&  MaterialID = *MaterialIDRef;
		TManagedArray<FTransform>&  Transform= *TransformRef;

		// set the particle information
		Transform[0] = center;
		Transform[0].NormalizeRotation();

		// set the vertex information
		int32 Index = 0;
		Vertices[0] = FVector(-Scale.X / 2.f, -Scale.Y / 2.f, -Scale.Z / 2.f);
		Vertices[1] = FVector(+Scale.X / 2.f, -Scale.Y / 2.f, -Scale.Z / 2.f);
		Vertices[2] = FVector(-Scale.X / 2.f, +Scale.Y / 2.f, -Scale.Z / 2.f);
		Vertices[3] = FVector(+Scale.X / 2.f, +Scale.Y / 2.f, -Scale.Z / 2.f);
		Vertices[4] = FVector(-Scale.X / 2.f, -Scale.Y / 2.f, +Scale.Z / 2.f);
		Vertices[5] = FVector(+Scale.X / 2.f, -Scale.Y / 2.f, +Scale.Z / 2.f);
		Vertices[6] = FVector(-Scale.X / 2.f, +Scale.Y / 2.f, +Scale.Z / 2.f);
		Vertices[7] = FVector(+Scale.X / 2.f, +Scale.Y / 2.f, +Scale.Z / 2.f);

		Normals[0] = FVector(-1.f, -1.f, -1.f).GetSafeNormal();
		Normals[1] = FVector(1.f, -1.f, -1.f).GetSafeNormal();
		Normals[2] = FVector(-1.f, 1.f, -1.f).GetSafeNormal();
		Normals[3] = FVector(1.f, 1.f, -1.f).GetSafeNormal();
		Normals[4] = FVector(-1.f, -1.f, 1.f).GetSafeNormal();
		Normals[5] = FVector(1.f, -1.f, 1.f).GetSafeNormal();
		Normals[6] = FVector(-1.f, 1.f, 1.f).GetSafeNormal();
		Normals[7] = FVector(1.f, 1.f, 1.f).GetSafeNormal();

		UVs[0] = FVector2D(0, 0);
		UVs[1] = FVector2D(1, 0);
		UVs[2] = FVector2D(0, 1);
		UVs[3] = FVector2D(1, 1);
		UVs[4] = FVector2D(0, 0);
		UVs[5] = FVector2D(1, 0);
		UVs[6] = FVector2D(0, 1);
		UVs[7] = FVector2D(1, 1);

		Colors[0] = FLinearColor::White;
		Colors[1] = FLinearColor::White;
		Colors[2] = FLinearColor::White;
		Colors[3] = FLinearColor::White;
		Colors[4] = FLinearColor::White;
		Colors[5] = FLinearColor::White;
		Colors[6] = FLinearColor::White;
		Colors[7] = FLinearColor::White;


		// set the index information

		// Bottom: Y = -1
		Indices[0] = FIntVector(Index + 5,Index + 1,Index);
		Indices[1] = FIntVector(Index,Index + 4,Index + 5);
		// Top: Y = 1
		Indices[2] = FIntVector(Index + 2,Index + 3,Index + 7);
		Indices[3] = FIntVector(Index + 7,Index + 6,Index + 2);
		// Back: Z = -1
		Indices[4] = FIntVector(Index + 3,Index + 2,Index);
		Indices[5] = FIntVector(Index,Index + 1,Index + 3);
		// Front: Z = 1
		Indices[6] = FIntVector(Index + 4,Index + 6,Index + 7);
		Indices[7] = FIntVector(Index + 7,Index + 5,Index + 4);
		// Left: X = -1
		Indices[8] = FIntVector(Index, Index + 2,Index + 6);
		Indices[9] = FIntVector(Index + 6,Index + 4,Index);
		// Right: X = 1
		Indices[10] = FIntVector(Index + 7,Index + 3,Index + 1);
		Indices[11] = FIntVector(Index + 1,Index + 5,Index + 7);

		for (int i = 0; i < 12;i++)
		{
			Visible[i] = true;

			// first 6 triangles are material 0 and the next material 1
			MaterialIndex[i] = i;
			MaterialID[i] = i < 6 ? 0 : 1;
		}

		for (int IndexIdx = 0; IndexIdx < 12; IndexIdx++)
		{
			FIntVector Tri = Indices[IndexIdx];
			for (int idx = 0; idx < 3; idx++)
			{
				const FVector Normal = Normals[Tri[idx]];
				const FVector Edge = (Vertices[Tri[(idx + 1) % 3]] - Vertices[Tri[idx]]);
				TangentU[Tri[idx]] = (Edge ^ Normal).GetSafeNormal();
				TangentV[Tri[idx]] = (Normal ^ TangentU[Tri[idx]]).GetSafeNormal();
			}
		}

		// GeometryGroup
		GeometryCollection::AddGeometryProperties(RestCollection);

		// add 2 material sections to simulate 2 materials on the object
		TSharedRef<TManagedArray<FGeometryCollectionSection> > SectionsRef = RestCollection->GetAttribute<FGeometryCollectionSection>("Sections", FGeometryCollection::MaterialGroup);
		TManagedArray<FGeometryCollectionSection>&  Sections = *SectionsRef;
		// the first 6 indices are material 0
		int Element = RestCollection->AddElements(1, FGeometryCollection::MaterialGroup);
		Sections[Element].MaterialID = 0;
		Sections[Element].FirstIndex = 0;
		Sections[Element].NumTriangles = 6;
		Sections[Element].MinVertexIndex = 0;
		Sections[Element].MaxVertexIndex = Vertices.Num() - 1;

		// the second 6 indices are material 1
		Element = RestCollection->AddElements(1, FGeometryCollection::MaterialGroup);
		Sections[Element].MaterialID = 1;
		Sections[Element].FirstIndex = 6 * 3;
		Sections[Element].NumTriangles = 6;
		Sections[Element].MinVertexIndex = 0;
		Sections[Element].MaxVertexIndex = Vertices.Num() - 1;

		return TSharedPtr<FGeometryCollection>(RestCollection);
	}


	void SetupCubeGridExample(TSharedPtr<FGeometryCollection> RestCollectionIn)
	{
		check(RestCollectionIn.IsValid());

		float domain = 10;
		FVector Stack(domain);
		float numElements = powf(domain, 3);

		float Length = 50.f;
		float Seperation = .2f;
		float Expansion = 1.f + Seperation;

		FVector Stackf((float)Stack[0], (float)Stack[1], (float)Stack[2]);
		FVector MinCorner = -Length * Expansion / 2.f * Stackf;


		for (int32 i = 0; i < Stack[0]; ++i)
		{
			for (int32 j = 0; j < Stack[1]; ++j)
			{
				for (int32 k = 0; k < Stack[2]; ++k)
				{
					FVector Delta(j % 2 == 1 ? Length / 2.f : 0.f, 0.f, j % 2 == 1 ? Length / 2.f : 0.f);
					FVector CenterOfMass = FVector(MinCorner[0] + Expansion * Length * i + Length * (Expansion / 2.f),
						MinCorner[0] + Expansion * Length * j + Length * (Expansion / 2.f),
						MinCorner[0] + Expansion * Length * k + Length * (Expansion / 2.f)) + Delta;
					TSharedPtr<FGeometryCollection> Element = MakeCubeElement(FTransform(CenterOfMass), FVector(Length) );
					RestCollectionIn->AppendGeometry(*Element);
				}
			}
		}
	}

	void SetupTwoClusteredCubesCollection(FGeometryCollection * Collection)
	{
		int32 ParentIndex = Collection->AddElements(1, FGeometryCollection::TransformGroup);
		int32 TransformIndex0 = Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 0.)), FVector(9, 0, 0)), FVector(1.0)));
		int32 TransformIndex1 = Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 0.)), FVector(-9, 0, 0)), FVector(1.0)));

		TManagedArray<int32>& VertexCount = *Collection->VertexCount;
		TManagedArray<int32>& VertexStart = *Collection->VertexStart;
		TManagedArray<FVector> & Vertex = *Collection->Vertex;
		
		TArray<int32> ReverseMap;
		GeometryCollectionAlgo::BuildTransformGroupToGeometryGroupMap(*Collection, ReverseMap);

		for (int32 i = VertexStart[ReverseMap[TransformIndex0]]; i < VertexStart[ReverseMap[TransformIndex0]] + VertexCount[ReverseMap[TransformIndex0]]; i++)
		{
			Vertex[i] += FVector(1, 0, 0);
		}
		for (int32 i = VertexStart[ReverseMap[TransformIndex1]]; i < VertexStart[ReverseMap[TransformIndex1]] + VertexCount[ReverseMap[TransformIndex1]]; i++)
		{
			Vertex[i] -= FVector(1, 0, 0);
		}

		TManagedArray<FString> & Names = *Collection->BoneName;
		Names[ParentIndex] = "Root";
		Names[TransformIndex0] = "RGB1";
		Names[TransformIndex1] = "RGB2";

		(*Collection->BoneHierarchy)[ParentIndex].Parent = FGeometryCollection::Invalid;
		(*Collection->BoneHierarchy)[ParentIndex].Children.Add(TransformIndex0);
		(*Collection->BoneHierarchy)[ParentIndex].Children.Add(TransformIndex1);
		(*Collection->BoneHierarchy)[TransformIndex0].Parent = ParentIndex;
		(*Collection->BoneHierarchy)[TransformIndex1].Parent = ParentIndex;
	}


	void SetupNestedBoneCollection(FGeometryCollection* Collection)
	{
		int32 TransformIndex0 = Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 90.)), FVector(0, 10, 0)), FVector(1.0)));
		int32 TransformIndex1 = Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 45.)), FVector(0, 10, 0)), FVector(1.0)));
		int32 TransformIndex2 = Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 45.)), FVector(0, 10, 0)), FVector(1.0)));
		int32 TransformIndex3 = Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(90.,0., 0)), FVector(0, 10, 0)), FVector(1.0)));
		int32 TransformIndex4 = Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(45.,45., 45.)), FVector(0, 10, 0)), FVector(1.0)));

		//  0
		//  ...1
		//  ......2
		//  .........3
		//  ............4
		(*Collection->BoneHierarchy)[TransformIndex0].Parent = FGeometryCollection::Invalid;
		(*Collection->BoneHierarchy)[TransformIndex0].Children.Add(TransformIndex1);
		(*Collection->BoneHierarchy)[TransformIndex1].Parent = TransformIndex0;
		(*Collection->BoneHierarchy)[TransformIndex1].Children.Add(TransformIndex2);
		(*Collection->BoneHierarchy)[TransformIndex2].Parent = TransformIndex1;
		(*Collection->BoneHierarchy)[TransformIndex2].Children.Add(TransformIndex3);
		(*Collection->BoneHierarchy)[TransformIndex3].Parent = TransformIndex2;
		(*Collection->BoneHierarchy)[TransformIndex3].Children.Add(TransformIndex4);
		(*Collection->BoneHierarchy)[TransformIndex4].Parent = TransformIndex3;
	}

	void AddGeometryProperties(FGeometryCollection * Collection)
	{
		if (Collection)
		{
			if (!Collection->NumElements(FGeometryCollection::GeometryGroup))
			{
				int32 NumVertices = Collection->Vertex->Num();
				if (NumVertices)
				{
					// transforms group
					TManagedArray<FTransform>& Transform = *Collection->Transform;
					// vertices group
					TManagedArray<int32> & BoneMap = *Collection->BoneMap;
					TManagedArray<FVector> & Vertex = *Collection->Vertex;
					// faces
					TManagedArray<FIntVector> & FaceIndices = *Collection->Indices;

					// geometry group
					TManagedArray<int32>& TransformIndex = *Collection->TransformIndex;
					TManagedArray<FBox>& BoundingBox = *Collection->BoundingBox;
					TManagedArray<float>& InnerRadius = *Collection->InnerRadius;
					TManagedArray<float>& OuterRadius = *Collection->OuterRadius;
					TManagedArray<int32>& VertexCount = *Collection->VertexCount;
					TManagedArray<int32>& VertexStart = *Collection->VertexStart;
					TManagedArray<int32>& FaceCount = *Collection->FaceCount;
					TManagedArray<int32>& FaceStart = *Collection->FaceStart;

					// gather unique geometries
					TSet<int32> TransformIndexOfGeometry;
					for (int32 BoneIdx = 0; BoneIdx < BoneMap.Num(); BoneIdx++)
					{
						TransformIndexOfGeometry.Add(BoneMap[BoneIdx]);
					}

					// reverse map
					TArray<int32> ReverseMap;
					ReverseMap.Init(FGeometryCollection::Invalid, Transform.Num());

					Collection->AddElements(TransformIndexOfGeometry.Num(), FGeometryCollection::GeometryGroup);
					TArray<int32> GeometryIndices = TransformIndexOfGeometry.Array();
					for (int32 Index = 0; Index < GeometryIndices.Num(); Index++)
					{
						ReverseMap[GeometryIndices[Index]] = Index;

						TransformIndex[Index] = GeometryIndices[Index];
						BoundingBox[Index].Init();
						InnerRadius[Index] = FLT_MAX;
						OuterRadius[Index] = -FLT_MAX;
						VertexStart[Index] = FGeometryCollection::Invalid;
						VertexCount[Index] = 0;
						FaceStart[Index] = FGeometryCollection::Invalid;
						FaceCount[Index] = 0;
					}

					// build vertex summary information
					TArray<FVector> Center;
					Center.Init(FVector(0), GeometryIndices.Num());
					int CurrentParticleIndex = FGeometryCollection::Invalid;
					for (int vdx = 0; vdx < Vertex.Num(); vdx++)
					{
						int32 ParticleIndex = BoneMap[vdx];
						check(ReverseMap[ParticleIndex] != FGeometryCollection::Invalid);
						int32 GeometryIndex = ReverseMap[ParticleIndex];

						if (VertexStart[GeometryIndex] == FGeometryCollection::Invalid)
						{
							// @todo(ContigiousVertices) : Files on disk are not contiguous, so until they are fixed just use the first set of vertices.
							//ensureMsgf(VertexCount[GeometryIndex] == 0, TEXT("Expected empty vertex count."));

							VertexStart[GeometryIndex] = vdx;
							CurrentParticleIndex = ParticleIndex;
						}
						if (ParticleIndex == CurrentParticleIndex)
						{
							VertexCount[GeometryIndex]++;
							BoundingBox[GeometryIndex] += Vertex[vdx];
						}
						// ensure contiguous particle indices
						// @todo(ContigiousVertices) : Files on disk are not contiguous, so until they are fixed just use the first set of vertices.
						// ensureMsgf(ParticleIndex == CurrentParticleIndex, TEXT("Expected contiguous particle indices in rigid body creation."));

						Center[GeometryIndex] += Vertex[vdx];
					}

					// build vertex centers
					for (int GeometryIndex = 0; GeometryIndex < GeometryIndices.Num(); GeometryIndex++)
					{
						if (VertexCount[GeometryIndex])
						{
							Center[GeometryIndex] / VertexCount[GeometryIndex];
						}
					}


					// build face summary information
					CurrentParticleIndex = FGeometryCollection::Invalid;
					for (int fdx = 0; fdx < FaceIndices.Num(); fdx++)
					{
						int32 vdx = FaceIndices[fdx][0];
						int32 ParticleIndex = BoneMap[vdx];
						check(ReverseMap[ParticleIndex] != FGeometryCollection::Invalid);
						int32 GeometryIndex = ReverseMap[ParticleIndex];

						if (FaceStart[GeometryIndex] == FGeometryCollection::Invalid)
						{
							// @todo(ContigiousVertices) : Files on disk are not contiguous, so until they are fixed just use the first set of vertices.
							//ensureMsgf(FaceCount[GeometryIndex] == 0, TEXT("Expected empty face count."));

							FaceStart[GeometryIndex] = fdx;
							CurrentParticleIndex = ParticleIndex;
						}
						if (ParticleIndex == CurrentParticleIndex)
						{
							FaceCount[GeometryIndex]++;
						}
						// ensure contiguous particle indices
						// @todo(ContigiousVertices) : Files on disk are not contiguous, so until they are fixed just use the first set of vertices.
						//ensureMsgf(ParticleIndex == CurrentParticleIndex, TEXT("Expected contiguous particle indices in rigid body creation."));
					}

					// find the inner and outer radius
					for (int vdx = 0; vdx < Vertex.Num(); vdx++)
					{
						int32 GeometryIndex = ReverseMap[BoneMap[vdx]]; // double indexing safe due to check in previous loop.
						float Delta = (Center[GeometryIndex] - Vertex[vdx]).Size();
						InnerRadius[GeometryIndex] = FMath::Min(InnerRadius[GeometryIndex], Delta);
						OuterRadius[GeometryIndex] = FMath::Max(OuterRadius[GeometryIndex], Delta);
					}

					// Inner/Outer centroid
					for (int fdx = 0; fdx <FaceIndices.Num(); fdx++)
					{
						int vdx = FaceIndices[fdx][0];
						int32 GeometryIndex = ReverseMap[BoneMap[vdx]]; // double indexing safe due to check in previous loop.

						FVector Centroid(0);
						for (int e = 0; e < 3; e++)
						{
							Centroid += Vertex[FaceIndices[fdx][e]];
						}
						Centroid /= 3;

						float Delta = (Center[GeometryIndex] - Centroid).Size();
						InnerRadius[GeometryIndex] = FMath::Min(InnerRadius[GeometryIndex], Delta);
						OuterRadius[GeometryIndex] = FMath::Max(OuterRadius[GeometryIndex], Delta);
					}

					// Inner/Outer edges
					for (int fdx = 0; fdx < FaceIndices.Num(); fdx++)
					{
						int vdx = FaceIndices[fdx][0];
						int32 GeometryIndex = ReverseMap[BoneMap[vdx]]; // double indexing safe due to check in previous loop.
						for (int e = 0; e < 3; e++)
						{
							int i = e, j = (e + 1) % 3;
							FVector Edge = Vertex[FaceIndices[fdx][i]] + 0.5*(Vertex[FaceIndices[fdx][j]] - Vertex[FaceIndices[fdx][i]]);
							float Delta = (Center[GeometryIndex] - Edge).Size();
							InnerRadius[GeometryIndex] = FMath::Min(InnerRadius[GeometryIndex], Delta);
							OuterRadius[GeometryIndex] = FMath::Max(OuterRadius[GeometryIndex], Delta);
						}
					}
				}
			}
		}
	}

	void MakeMaterialsContiguous(FGeometryCollection * Collection)
	{
		// if the material indices are not setup then they will all be zero, then this is an old asset needing updated
		if (Collection->NumElements(FGeometryCollection::FacesGroup) 
			&& (*Collection->MaterialIndex)[0] == (*Collection->MaterialIndex)[1] && (*Collection->MaterialIndex)[0] == 0)
		{
			int NumVisited = 0;
			// fill in the material IDs
			TManagedArray<FGeometryCollectionSection>& Section = *Collection->Sections;
			TManagedArray<int32>& MaterialID = *Collection->MaterialID;
			for (int i = 0; i < Section.Num(); i++)
			{
				int first = Section[i].FirstIndex / 3;
				int last = first + Section[i].NumTriangles;

				for (int FaceIdx = first; FaceIdx < last; FaceIdx++)
				{
					MaterialID[FaceIdx] = Section[i].MaterialID;
					NumVisited++;
				}

			}

			check(NumVisited == Collection->NumElements(FGeometryCollection::FacesGroup));

			// Reindex will update everything else that is required
			Collection->ReindexMaterials();
		}
	}

}


