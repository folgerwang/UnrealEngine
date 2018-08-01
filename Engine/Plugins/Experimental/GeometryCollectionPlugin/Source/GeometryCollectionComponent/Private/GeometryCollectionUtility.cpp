// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection->cpp: UGeometryCollection methods.
=============================================================================*/

#include "GeometryCollectionUtility.h"
#include "Templates/SharedPointer.h"
DEFINE_LOG_CATEGORY_STATIC(FGeometryCollectionUtilityLogging, Log, All);

namespace GeometryCollection
{
	TSharedPtr<UGeometryCollection> MakeCubeElement(const FTransform& center, float Scale)
	{
		UGeometryCollection * RestCollection = NewObject<UGeometryCollection>();

		int NumNewVertices = 8; // 8 vertices per cube
		int VerticesIndex = RestCollection->AddElements(NumNewVertices, UGeometryCollection::VerticesGroup);
		TSharedRef<TManagedArray<FVector> > VerticesRef = RestCollection->GetAttribute<FVector>("Vertex", UGeometryCollection::VerticesGroup);
		TSharedRef<TManagedArray<FVector> > NormalsRef = RestCollection->GetAttribute<FVector>("Normal", UGeometryCollection::VerticesGroup);
		TSharedRef<TManagedArray<FVector> > TangentURef = RestCollection->GetAttribute<FVector>("TangentU", UGeometryCollection::VerticesGroup);
		TSharedRef<TManagedArray<FVector> > TangentVRef = RestCollection->GetAttribute<FVector>("TangentV", UGeometryCollection::VerticesGroup);
		TSharedRef<TManagedArray<FVector2D> > UVsRef = RestCollection->GetAttribute<FVector2D>("UV", UGeometryCollection::VerticesGroup);
		TSharedRef<TManagedArray<FLinearColor> > ColorsRef = RestCollection->GetAttribute<FLinearColor>("Color", UGeometryCollection::VerticesGroup);

		int NumNewIndices = 2 * 6; // two triangles per face
		int IndicesIndex = RestCollection->AddElements(NumNewIndices, UGeometryCollection::GeometryGroup);
		TSharedRef<TManagedArray<FIntVector> > IndicesRef = RestCollection->GetAttribute<FIntVector>("Indices", UGeometryCollection::GeometryGroup);
		TSharedRef<TManagedArray<bool> > VisibleRef = RestCollection->GetAttribute<bool>("Visible", UGeometryCollection::GeometryGroup);

		int NumNewParticles = 1; // 1 particle for this geometry structure
		int ParticlesIndex = RestCollection->AddElements(NumNewParticles, UGeometryCollection::TransformGroup);
		TSharedRef<TManagedArray<FTransform> > TransformRef = RestCollection->GetAttribute<FTransform>("Transform", UGeometryCollection::TransformGroup);

		TManagedArray<FVector>& Vertices= *VerticesRef;
		TManagedArray<FVector>&  Normals= *NormalsRef;
		TManagedArray<FVector>&  TangentU= *TangentURef;
		TManagedArray<FVector>&  TangentV= *TangentVRef;
		TManagedArray<FVector2D>&  UVs= *UVsRef;
		TManagedArray<FLinearColor>&  Colors= *ColorsRef;
		TManagedArray<FIntVector>&  Indices= *IndicesRef;
		TManagedArray<bool>&  Visible= *VisibleRef;
		TManagedArray<FTransform>&  Transform= *TransformRef;


		// set the particle information
		Transform[0] = center;

		// set the vertex information
		int32 Index = 0;
		Vertices[0] = FVector(-Scale / 2.f, -Scale / 2.f, -Scale / 2.f);
		Vertices[1] = FVector(+Scale / 2.f, -Scale / 2.f, -Scale / 2.f);
		Vertices[2] = FVector(-Scale / 2.f, +Scale / 2.f, -Scale / 2.f);
		Vertices[3] = FVector(+Scale / 2.f, +Scale / 2.f, -Scale / 2.f);
		Vertices[4] = FVector(-Scale / 2.f, -Scale / 2.f, +Scale / 2.f);
		Vertices[5] = FVector(+Scale / 2.f, -Scale / 2.f, +Scale / 2.f);
		Vertices[6] = FVector(-Scale / 2.f, +Scale / 2.f, +Scale / 2.f);
		Vertices[7] = FVector(+Scale / 2.f, +Scale / 2.f, +Scale / 2.f);

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
		return TSharedPtr<UGeometryCollection>(RestCollection);
	}


	void SetupCubeGridExample(TSharedPtr<UGeometryCollection> RestCollectionIn)
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
					TSharedPtr<UGeometryCollection> Element = MakeCubeElement(FTransform(CenterOfMass), Length);
					RestCollectionIn->AppendGeometry(*Element);
				}
			}
		}
	}
}


