// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Templates/ScopedPointer.h"
#include "MeshUtilities.h"
#include "MeshBuild.h"
#include "RawMesh.h"
#include "MeshSimplify.h"
#include "OverlappingCorners.h"
#include "Templates/UniquePtr.h"
#include "Features/IModularFeatures.h"
#include "IMeshReductionInterfaces.h"
#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "RenderUtils.h"

class FQuadricSimplifierMeshReductionModule : public IMeshReductionModule
{
public:
	virtual ~FQuadricSimplifierMeshReductionModule() {}

	// IModuleInterface interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IMeshReductionModule interface.
	virtual class IMeshReduction* GetStaticMeshReductionInterface() override;
	virtual class IMeshReduction* GetSkeletalMeshReductionInterface() override;
	virtual class IMeshMerging* GetMeshMergingInterface() override;
	virtual class IMeshMerging* GetDistributedMeshMergingInterface() override;	
	virtual FString GetName() override;
};


DEFINE_LOG_CATEGORY_STATIC(LogQuadricSimplifier, Log, All);
IMPLEMENT_MODULE(FQuadricSimplifierMeshReductionModule, QuadricMeshReduction);

template< uint32 NumTexCoords >
class TVertSimp
{
	typedef TVertSimp< NumTexCoords > VertType;
public:
	uint32			MaterialIndex;
	FVector			Position;
	FVector			Normal;
	FVector			Tangents[2];
	FLinearColor	Color;
	FVector2D		TexCoords[ NumTexCoords ];

	uint32			GetMaterialIndex() const	{ return MaterialIndex; }
	FVector&		GetPos()					{ return Position; }
	const FVector&	GetPos() const				{ return Position; }
	float*			GetAttributes()				{ return (float*)&Normal; }
	const float*	GetAttributes() const		{ return (const float*)&Normal; }

	void		Correct()
	{
		Normal.Normalize();
		Tangents[0] -= ( Tangents[0] * Normal ) * Normal;
		Tangents[0].Normalize();
		Tangents[1] -= ( Tangents[1] * Normal ) * Normal;
		Tangents[1] -= ( Tangents[1] * Tangents[0] ) * Tangents[0];
		Tangents[1].Normalize();
		Color = Color.GetClamped();
	}

	bool		Equals(	const VertType& a ) const
	{
		if( MaterialIndex != a.MaterialIndex ||
			!PointsEqual(  Position,	a.Position ) ||
			!NormalsEqual( Tangents[0],	a.Tangents[0] ) ||
			!NormalsEqual( Tangents[1],	a.Tangents[1] ) ||
			!NormalsEqual( Normal,		a.Normal ) ||
			!Color.Equals( a.Color ) )
		{
			return false;
		}

		// UVs
		for( int32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++ )
		{
			if( !UVsEqual( TexCoords[ UVIndex ], a.TexCoords[ UVIndex ] ) )
			{
				return false;
			}
		}

		return true;
	}

	bool		operator==(	const VertType& a ) const
	{
		if( MaterialIndex	!= a.MaterialIndex ||
			Position		!= a.Position ||
			Normal			!= a.Normal ||
			Tangents[0]		!= a.Tangents[0] ||
			Tangents[1]		!= a.Tangents[1] ||
			Color			!= a.Color )
		{
			return false;
		}

		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			if( TexCoords[i] != a.TexCoords[i] )
			{
				return false;
			}
		}
		return true;
	}

	VertType	operator+( const VertType& a ) const
	{
		VertType v;
		v.MaterialIndex	= MaterialIndex;
		v.Position		= Position + a.Position;
		v.Normal		= Normal + a.Normal;
		v.Tangents[0]	= Tangents[0] + a.Tangents[0];
		v.Tangents[1]	= Tangents[1] + a.Tangents[1];
		v.Color			= Color + a.Color;

		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			v.TexCoords[i] = TexCoords[i] + a.TexCoords[i];
		}
		return v;
	}

	VertType	operator-( const VertType& a ) const
	{
		VertType v;
		v.MaterialIndex	= MaterialIndex;
		v.Position		= Position - a.Position;
		v.Normal		= Normal - a.Normal;
		v.Tangents[0]	= Tangents[0] - a.Tangents[0];
		v.Tangents[1]	= Tangents[1] - a.Tangents[1];
		v.Color			= Color - a.Color;
		
		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			v.TexCoords[i] = TexCoords[i] - a.TexCoords[i];
		}
		return v;
	}

	VertType	operator*( const float a ) const
	{
		VertType v;
		v.MaterialIndex	= MaterialIndex;
		v.Position		= Position * a;
		v.Normal		= Normal * a;
		v.Tangents[0]	= Tangents[0] * a;
		v.Tangents[1]	= Tangents[1] * a;
		v.Color			= Color * a;
		
		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			v.TexCoords[i] = TexCoords[i] * a;
		}
		return v;
	}

	VertType	operator/( const float a ) const
	{
		float ia = 1.0f / a;
		return (*this) * ia;
	}
};

class FQuadricSimplifierMeshReduction : public IMeshReduction
{
public:
	virtual const FString& GetVersionString() const override
	{
		static FString Version = TEXT("1.0");
		return Version;
	}

	virtual void Reduce(
		FRawMesh& OutReducedMesh,
		float& OutMaxDeviation,
		const FRawMesh& InMesh,
		const FOverlappingCorners& InOverlappingCorners,
		const FMeshReductionSettings& InSettings
		) override
	{
		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

		TArray<FVector> VertexPositions;
		TArray<uint32> Indices;
		if (InSettings.WeldingThreshold > 0.0f)
		{
			WeldVertexPositions(InMesh, InSettings.WeldingThreshold, VertexPositions, Indices);
		}

		const uint32 NumTexCoords = MAX_STATIC_TEXCOORDS;

		TArray< TVertSimp< NumTexCoords > >	Verts;
		TArray< uint32 >					Indexes;

		TMap< int32, int32 > VertsMap;

		int32 NumWedges = InMesh.WedgeIndices.Num();
		int32 NumFaces = NumWedges / 3;

		// Process each face, build vertex buffer and index buffer
		for (int32 FaceIndex = 0; FaceIndex < NumFaces; FaceIndex++)
		{
			FVector Positions[3];
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				if (InSettings.WeldingThreshold > 0.0f)
				{
					Positions[CornerIndex] = VertexPositions[Indices[FaceIndex * 3 + CornerIndex]];
				}
				else
				{
					Positions[CornerIndex] = InMesh.VertexPositions[InMesh.WedgeIndices[FaceIndex * 3 + CornerIndex]];
				}
			}

			// Don't process degenerate triangles.
			if( PointsEqual( Positions[0], Positions[1] ) ||
				PointsEqual( Positions[0], Positions[2] ) ||
				PointsEqual( Positions[1], Positions[2] ) )
			{
				continue;
			}

			int32 VertexIndices[3];
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				int32 WedgeIndex = FaceIndex * 3 + CornerIndex;

				TVertSimp< NumTexCoords > NewVert;
				NewVert.MaterialIndex	= InMesh.FaceMaterialIndices[ FaceIndex ];
				NewVert.Position		= Positions[ CornerIndex ];
				NewVert.Tangents[0]		= InMesh.WedgeTangentX[ WedgeIndex ];
				NewVert.Tangents[1]		= InMesh.WedgeTangentY[ WedgeIndex ];
				NewVert.Normal			= InMesh.WedgeTangentZ[ WedgeIndex ];

				// Fix bad tangents
				NewVert.Tangents[0] = NewVert.Tangents[0].ContainsNaN() ? FVector::ZeroVector : NewVert.Tangents[0];
				NewVert.Tangents[1] = NewVert.Tangents[1].ContainsNaN() ? FVector::ZeroVector : NewVert.Tangents[1];
				NewVert.Normal		= NewVert.Normal.ContainsNaN()		? FVector::ZeroVector : NewVert.Normal;

				if( InMesh.WedgeColors.Num() == NumWedges )
				{
					NewVert.Color = FLinearColor::FromSRGBColor( InMesh.WedgeColors[ WedgeIndex ] );
				}
				else
				{
					NewVert.Color = FLinearColor::Transparent;
				}

				for( int32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++ )
				{
					if( InMesh.WedgeTexCoords[ UVIndex ].Num() == NumWedges )
					{
						NewVert.TexCoords[ UVIndex ] = InMesh.WedgeTexCoords[ UVIndex ][ WedgeIndex ];
					}
					else
					{
						NewVert.TexCoords[ UVIndex ] = FVector2D::ZeroVector;
					}
				}

				// Make sure this vertex is valid from the start
				NewVert.Correct();

				const TArray<int32>& DupVerts = InOverlappingCorners.FindIfOverlapping(WedgeIndex);

				int32 Index = INDEX_NONE;
				for (int32 k = 0; k < DupVerts.Num(); k++)
				{
					if( DupVerts[k] >= WedgeIndex )
					{
						// the verts beyond me haven't been placed yet, so these duplicates are not relevant
						break;
					}

					int32* Location = VertsMap.Find( DupVerts[k] );
					if( Location )
					{
						TVertSimp< NumTexCoords >& FoundVert = Verts[ *Location ];

						if( NewVert.Equals( FoundVert ) )
						{
							Index = *Location;
							break;
						}
					}
				}
				if( Index == INDEX_NONE )
				{
					Index = Verts.Add( NewVert );
					VertsMap.Add( WedgeIndex, Index );
				}
				VertexIndices[ CornerIndex ] = Index;
			}

			// Reject degenerate triangles.
			if( VertexIndices[0] == VertexIndices[1] ||
				VertexIndices[1] == VertexIndices[2] ||
				VertexIndices[0] == VertexIndices[2] )
			{
				continue;
			}

			Indexes.Add( VertexIndices[0] );
			Indexes.Add( VertexIndices[1] );
			Indexes.Add( VertexIndices[2] );
		}

		uint32 NumVerts = Verts.Num();
		uint32 NumIndexes = Indexes.Num();
		uint32 NumTris = NumIndexes / 3;

		static_assert( NumTexCoords == 8, "NumTexCoords changed, fix AttributeWeights" );
		const uint32 NumAttributes = ( sizeof( TVertSimp< NumTexCoords > ) - sizeof( uint32 ) - sizeof( FVector ) ) / sizeof(float);
		float AttributeWeights[] =
		{
			16.0f, 16.0f, 16.0f,	// Normal
			0.1f, 0.1f, 0.1f,		// Tangent[0]
			0.1f, 0.1f, 0.1f,		// Tangent[1]
			0.1f, 0.1f, 0.1f, 0.1f,	// Color
			0.5f, 0.5f,				// TexCoord[0]
			0.5f, 0.5f,				// TexCoord[1]
			0.5f, 0.5f,				// TexCoord[2]
			0.5f, 0.5f,				// TexCoord[3]
			0.5f, 0.5f,				// TexCoord[4]
			0.5f, 0.5f,				// TexCoord[5]
			0.5f, 0.5f,				// TexCoord[6]
			0.5f, 0.5f,				// TexCoord[7]
		};
		float* ColorWeights = AttributeWeights + 3 + 3 + 3;
		float* TexCoordWeights = ColorWeights + 4;

		// Zero out weights that aren't used
		{
			if( InMesh.WedgeColors.Num() != NumWedges )
			{
				ColorWeights[0] = 0.0f;
				ColorWeights[1] = 0.0f;
				ColorWeights[2] = 0.0f;
				ColorWeights[3] = 0.0f;
			}

			for( int32 TexCoordIndex = 0; TexCoordIndex < NumTexCoords; TexCoordIndex++ )
			{
				if( InMesh.WedgeTexCoords[ TexCoordIndex ].Num() != NumWedges )
				{
					TexCoordWeights[ 2 * TexCoordIndex + 0 ] = 0.0f;
					TexCoordWeights[ 2 * TexCoordIndex + 1 ] = 0.0f;
				}
				else if (InMesh.WedgeTexCoords[TexCoordIndex].Num() > 0)
				{
					// Normalize TexCoordWeights using min/max TexCoord range, with assumption that value ranges above 2 aren't standard UV values

					float MinVal = +FLT_MAX;
					float MaxVal = -FLT_MAX;

					for (int32 VertexIndex = 0; VertexIndex < InMesh.WedgeTexCoords[TexCoordIndex].Num(); ++VertexIndex)
					{
						MinVal = FMath::Min(MinVal, InMesh.WedgeTexCoords[TexCoordIndex][VertexIndex].X);
						MinVal = FMath::Min(MinVal, InMesh.WedgeTexCoords[TexCoordIndex][VertexIndex].Y);
						MaxVal = FMath::Max(MaxVal, InMesh.WedgeTexCoords[TexCoordIndex][VertexIndex].X);
						MaxVal = FMath::Max(MaxVal, InMesh.WedgeTexCoords[TexCoordIndex][VertexIndex].Y);
					}

					TexCoordWeights[2 * TexCoordIndex + 0] = 1.0f / FMath::Max(2.0f, MaxVal - MinVal);
					TexCoordWeights[2 * TexCoordIndex + 1] = 1.0f / FMath::Max(2.0f, MaxVal - MinVal);
				}
			}
		}
		
		TMeshSimplifier< TVertSimp< NumTexCoords >, NumAttributes >* MeshSimp = new TMeshSimplifier< TVertSimp< NumTexCoords >, NumAttributes >( Verts.GetData(), NumVerts, Indexes.GetData(), NumIndexes );

		MeshSimp->SetAttributeWeights( AttributeWeights );
		//MeshSimp->SetBoundaryLocked();
		MeshSimp->InitCosts();

		float MaxErrorSqr = MeshSimp->SimplifyMesh( MAX_FLT, NumTris * InSettings.PercentTriangles );

		NumVerts = MeshSimp->GetNumVerts();
		NumTris = MeshSimp->GetNumTris();
		NumIndexes = NumTris * 3;

		MeshSimp->OutputMesh( Verts.GetData(), Indexes.GetData() );
		delete MeshSimp;

		//Reorder the face to use the material in the correct order
		TArray<int32> ReduceMeshUsedMaterialIndex;
		bool bDoRemap = false;
		for (uint32 TriIndex = 0; TriIndex < NumTris; TriIndex++)
		{
			int32 ReduceMaterialIndex = Verts[Indexes[3 * TriIndex]].MaterialIndex;
			int32 FinalMaterialIndex = ReduceMeshUsedMaterialIndex.AddUnique(ReduceMaterialIndex);
			bDoRemap |= ReduceMaterialIndex != FinalMaterialIndex;
		}
		if (bDoRemap)
		{
			int32 MaximumIndex = 0;
			int32 MaxMaterialIndex = FMath::Max(ReduceMeshUsedMaterialIndex, &MaximumIndex);
			TArray<TArray<int32>> MaterialSectionIndexes;
			//We need to add up to the maximum material index
			MaterialSectionIndexes.AddDefaulted(MaxMaterialIndex+1);
			//Reorder the Indexes according to the remap array
			//First, sort them by section in the material index order
			for (uint32 IndexesIndex = 0; IndexesIndex < NumIndexes; IndexesIndex++)
			{
				int32 ReduceMaterialIndex = Verts[Indexes[IndexesIndex]].MaterialIndex;
				MaterialSectionIndexes[ReduceMaterialIndex].Add(Indexes[IndexesIndex]);
			}
			//Update the Indexes array by placing all triangles in section index order
			//This will make sure that the reduce LOD mesh will have the same material order
			//as the reference LOD, even if some sections disappear because all triangles was remove.
			int32 IndexOffset = 0;
			for (const TArray<int32>& RemapSectionIndexes : MaterialSectionIndexes)
			{
				for (int32 IndexOfIndex = 0; IndexOfIndex < RemapSectionIndexes.Num(); ++IndexOfIndex)
				{
					int32 SortedIndex = IndexOfIndex + IndexOffset;
					Indexes[SortedIndex] = RemapSectionIndexes[IndexOfIndex];
				}
				IndexOffset += RemapSectionIndexes.Num();
			}
		}


		OutMaxDeviation = FMath::Sqrt( MaxErrorSqr ) / 8.0f;

		{
			// Output FRawMesh
			OutReducedMesh.VertexPositions.Empty( NumVerts );
			OutReducedMesh.VertexPositions.AddUninitialized( NumVerts );
			for( uint32 i= 0; i < NumVerts; i++ )
			{
				OutReducedMesh.VertexPositions[i] = Verts[i].Position;
			}

			OutReducedMesh.WedgeIndices.Empty( NumIndexes );
			OutReducedMesh.WedgeIndices.AddUninitialized( NumIndexes );

			for( uint32 i = 0; i < NumIndexes; i++ )
			{
				OutReducedMesh.WedgeIndices[i] = Indexes[i];
			}

			OutReducedMesh.WedgeTangentX.Empty( NumIndexes );
			OutReducedMesh.WedgeTangentY.Empty( NumIndexes );
			OutReducedMesh.WedgeTangentZ.Empty( NumIndexes );
			OutReducedMesh.WedgeTangentX.AddUninitialized( NumIndexes );
			OutReducedMesh.WedgeTangentY.AddUninitialized( NumIndexes );
			OutReducedMesh.WedgeTangentZ.AddUninitialized( NumIndexes );
			for( uint32 i= 0; i < NumIndexes; i++ )
			{
				OutReducedMesh.WedgeTangentX[i] = Verts[ Indexes[i] ].Tangents[0];
				OutReducedMesh.WedgeTangentY[i] = Verts[ Indexes[i] ].Tangents[1];
				OutReducedMesh.WedgeTangentZ[i] = Verts[ Indexes[i] ].Normal;
			}

			if( InMesh.WedgeColors.Num() == NumWedges )
			{
				OutReducedMesh.WedgeColors.Empty( NumIndexes );
				OutReducedMesh.WedgeColors.AddUninitialized( NumIndexes );
				for( uint32 i= 0; i < NumIndexes; i++ )
				{
					OutReducedMesh.WedgeColors[i] = Verts[ Indexes[i] ].Color.ToFColor(true);
				}
			}
			else
			{
				OutReducedMesh.WedgeColors.Empty();
			}

			for( int32 TexCoordIndex = 0; TexCoordIndex < NumTexCoords; TexCoordIndex++ )
			{
				if( InMesh.WedgeTexCoords[ TexCoordIndex ].Num() == NumWedges )
				{
					OutReducedMesh.WedgeTexCoords[ TexCoordIndex ].Empty( NumIndexes );
					OutReducedMesh.WedgeTexCoords[ TexCoordIndex ].AddUninitialized( NumIndexes );
					for( uint32 i= 0; i < NumIndexes; i++ )
					{
						OutReducedMesh.WedgeTexCoords[ TexCoordIndex ][i] = Verts[ Indexes[i] ].TexCoords[ TexCoordIndex ];
					}
				}
				else
				{
					OutReducedMesh.WedgeTexCoords[ TexCoordIndex ].Empty();
				}
			}

			OutReducedMesh.FaceMaterialIndices.Empty( NumTris );
			OutReducedMesh.FaceMaterialIndices.AddUninitialized( NumTris );
			for( uint32 i= 0; i < NumTris; i++ )
			{
				OutReducedMesh.FaceMaterialIndices[i] = Verts[ Indexes[3*i] ].MaterialIndex;
			}

			OutReducedMesh.FaceSmoothingMasks.Empty( NumTris );
			OutReducedMesh.FaceSmoothingMasks.AddZeroed( NumTris );

			Verts.Empty();
			Indexes.Empty();
		}
	}

	virtual void ReduceMeshDescription(
		FMeshDescription& OutReducedMesh,
		float& OutMaxDeviation,
		const FMeshDescription& InMesh,
		const FOverlappingCorners& InOverlappingCorners,
		const struct FMeshReductionSettings& ReductionSettings
	) override
	{
		check(&InMesh != &OutReducedMesh);	// can't reduce in-place

		const uint32 NumTexCoords = MAX_STATIC_TEXCOORDS;
		int32 InMeshNumTexCoords = 1;

		TArray< TVertSimp< NumTexCoords > >	Verts;
		TArray< uint32 >					Indexes;

		TMap< int32, int32 > VertsMap;

		int32 NumFaces = 0;
		for (const FPolygonID PolygonID : InMesh.Polygons().GetElementIDs())
		{
			NumFaces += InMesh.GetPolygonTriangles(PolygonID).Num();
		}
		int32 NumWedges = NumFaces * 3;

		TVertexAttributesConstRef<FVector> InVertexPositions = InMesh.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
		TVertexInstanceAttributesConstRef<FVector> InVertexNormals = InMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
		TVertexInstanceAttributesConstRef<FVector> InVertexTangents = InMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
		TVertexInstanceAttributesConstRef<float> InVertexBinormalSigns = InMesh.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
		TVertexInstanceAttributesConstRef<FVector4> InVertexColors = InMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
		TVertexInstanceAttributesConstRef<FVector2D> InVertexUVs = InMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
		TPolygonGroupAttributesConstRef<FName> InPolygonGroupMaterialNames = InMesh.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

		TPolygonGroupAttributesRef<FName> OutPolygonGroupMaterialNames = OutReducedMesh.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

		int32 FaceIndex = 0;
		for (const FPolygonID& PolygonID : InMesh.Polygons().GetElementIDs())
		{
			const TArray<FMeshTriangle>& Triangles = InMesh.GetPolygonTriangles(PolygonID);

			FVertexInstanceID VertexInstanceIDs[3];
			FVertexID VertexIDs[3];
			FVector Positions[3];

			for (const FMeshTriangle MeshTriangle : Triangles)
			{
				int32 CurrentFaceIndex = FaceIndex;
				//Increment face index here because there is many continue in this for loop
				++FaceIndex;
				for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
				{
					VertexInstanceIDs[CornerIndex] = MeshTriangle.GetVertexInstanceID(CornerIndex);
					VertexIDs[CornerIndex] = InMesh.GetVertexInstanceVertex(VertexInstanceIDs[CornerIndex]);
					Positions[CornerIndex] = InVertexPositions[VertexIDs[CornerIndex]];
				}

				// Don't process degenerate triangles.
				if (PointsEqual(Positions[0], Positions[1]) ||
					PointsEqual(Positions[0], Positions[2]) ||
					PointsEqual(Positions[1], Positions[2]))
				{
					continue;
				}

				int32 VertexIndices[3];
				for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
				{
					int32 WedgeIndex = CurrentFaceIndex * 3 + CornerIndex;

					TVertSimp< NumTexCoords > NewVert;

					const TArray<FPolygonID>& VertexInstanceConnectedPolygons = InMesh.GetVertexInstanceConnectedPolygons(VertexInstanceIDs[CornerIndex]);
					if (VertexInstanceConnectedPolygons.Num() > 0)
					{
						const FPolygonID ConnectedPolygonID = VertexInstanceConnectedPolygons[0];
						NewVert.MaterialIndex = InMesh.GetPolygonPolygonGroup(ConnectedPolygonID).GetValue();
						// @todo: check with Alexis: OK to conflate material index with polygon group ID? (what if there are gaps in the polygon group array?)
					}

					NewVert.Position = Positions[CornerIndex];
					NewVert.Tangents[0] = InVertexTangents[VertexInstanceIDs[CornerIndex]];
					NewVert.Normal = InVertexNormals[VertexInstanceIDs[CornerIndex]];
					NewVert.Tangents[1] = FVector(0.0f);
					if (!NewVert.Normal.IsNearlyZero(SMALL_NUMBER) && !NewVert.Tangents[0].IsNearlyZero(SMALL_NUMBER))
					{
						NewVert.Tangents[1] = FVector::CrossProduct(NewVert.Normal, NewVert.Tangents[0]).GetSafeNormal() * InVertexBinormalSigns[VertexInstanceIDs[CornerIndex]];
					}

					// Fix bad tangents
					NewVert.Tangents[0] = NewVert.Tangents[0].ContainsNaN() ? FVector::ZeroVector : NewVert.Tangents[0];
					NewVert.Tangents[1] = NewVert.Tangents[1].ContainsNaN() ? FVector::ZeroVector : NewVert.Tangents[1];
					NewVert.Normal = NewVert.Normal.ContainsNaN() ? FVector::ZeroVector : NewVert.Normal;
					NewVert.Color = FLinearColor(InVertexColors[VertexInstanceIDs[CornerIndex]]);

					for (int32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++)
					{
						if (UVIndex < InVertexUVs.GetNumIndices())
						{
							NewVert.TexCoords[UVIndex] = InVertexUVs.Get(VertexInstanceIDs[CornerIndex], UVIndex);
							InMeshNumTexCoords = FMath::Max(UVIndex+1, InMeshNumTexCoords);
						}
						else
						{
							NewVert.TexCoords[UVIndex] = FVector2D::ZeroVector;
						}
					}

					// Make sure this vertex is valid from the start
					NewVert.Correct();

					const TArray<int32>& DupVerts = InOverlappingCorners.FindIfOverlapping(WedgeIndex);

					int32 Index = INDEX_NONE;
					for (int32 k = 0; k < DupVerts.Num(); k++)
					{
						if (DupVerts[k] >= WedgeIndex)
						{
							// the verts beyond me haven't been placed yet, so these duplicates are not relevant
							break;
						}

						int32* Location = VertsMap.Find(DupVerts[k]);
						if (Location)
						{
							TVertSimp< NumTexCoords >& FoundVert = Verts[*Location];

							if (NewVert.Equals(FoundVert))
							{
								Index = *Location;
								break;
							}
						}
					}
					if (Index == INDEX_NONE)
					{
						Index = Verts.Add(NewVert);
						VertsMap.Add(WedgeIndex, Index);
					}
					VertexIndices[CornerIndex] = Index;
				}

				// Reject degenerate triangles.
				if (VertexIndices[0] == VertexIndices[1] ||
					VertexIndices[1] == VertexIndices[2] ||
					VertexIndices[0] == VertexIndices[2])
				{
					continue;
				}

				Indexes.Add(VertexIndices[0]);
				Indexes.Add(VertexIndices[1]);
				Indexes.Add(VertexIndices[2]);
			}
		}

		uint32 NumVerts = Verts.Num();
		uint32 NumIndexes = Indexes.Num();
		uint32 NumTris = NumIndexes / 3;

		static_assert(NumTexCoords == 8, "NumTexCoords changed, fix AttributeWeights");
		const uint32 NumAttributes = (sizeof(TVertSimp< NumTexCoords >) - sizeof(uint32) - sizeof(FVector)) / sizeof(float);
		float AttributeWeights[] =
		{
			16.0f, 16.0f, 16.0f,	// Normal
			0.1f, 0.1f, 0.1f,		// Tangent[0]
			0.1f, 0.1f, 0.1f,		// Tangent[1]
			0.1f, 0.1f, 0.1f, 0.1f,	// Color
			0.5f, 0.5f,				// TexCoord[0]
			0.5f, 0.5f,				// TexCoord[1]
			0.5f, 0.5f,				// TexCoord[2]
			0.5f, 0.5f,				// TexCoord[3]
			0.5f, 0.5f,				// TexCoord[4]
			0.5f, 0.5f,				// TexCoord[5]
			0.5f, 0.5f,				// TexCoord[6]
			0.5f, 0.5f,				// TexCoord[7]
		};
		float* ColorWeights = AttributeWeights + 3 + 3 + 3;
		float* TexCoordWeights = ColorWeights + 4;

		// Zero out weights that aren't used
		{
			//TODO Check if we have vertex color

			for (int32 TexCoordIndex = 0; TexCoordIndex < NumTexCoords; TexCoordIndex++)
			{
				if (TexCoordIndex >= InVertexUVs.GetNumIndices())
				{
					TexCoordWeights[2 * TexCoordIndex + 0] = 0.0f;
					TexCoordWeights[2 * TexCoordIndex + 1] = 0.0f;
				}
			}
		}

		TMeshSimplifier< TVertSimp< NumTexCoords >, NumAttributes >* MeshSimp = new TMeshSimplifier< TVertSimp< NumTexCoords >, NumAttributes >(Verts.GetData(), NumVerts, Indexes.GetData(), NumIndexes);

		MeshSimp->SetAttributeWeights(AttributeWeights);
		//MeshSimp->SetBoundaryLocked();
		MeshSimp->InitCosts();
		//We need a minimum of 2 triangles, to see the object on both side. If we use one, we will end up with zero triangle when we will remove a shared edge
		float MaxErrorSqr = MeshSimp->SimplifyMesh(MAX_FLT, FMath::Max(2, int32(NumTris * ReductionSettings.PercentTriangles)));

		NumVerts = MeshSimp->GetNumVerts();
		NumTris = MeshSimp->GetNumTris();
		NumIndexes = NumTris * 3;

		MeshSimp->OutputMesh(Verts.GetData(), Indexes.GetData());
		delete MeshSimp;

		OutMaxDeviation = FMath::Sqrt(MaxErrorSqr) / 8.0f;

		{
			//Empty the destination mesh
			OutReducedMesh.PolygonGroups().Reset();
			OutReducedMesh.Polygons().Reset();
			OutReducedMesh.Edges().Reset();
			OutReducedMesh.VertexInstances().Reset();
			OutReducedMesh.Vertices().Reset();

			//Fill the PolygonGroups from the InMesh
			for (const FPolygonGroupID& PolygonGroupID : InMesh.PolygonGroups().GetElementIDs())
			{
				OutReducedMesh.CreatePolygonGroupWithID(PolygonGroupID);
				OutPolygonGroupMaterialNames[PolygonGroupID] = InPolygonGroupMaterialNames[PolygonGroupID];
			}

			TVertexAttributesRef<FVector> OutVertexPositions = OutReducedMesh.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);

			//Fill the vertex array
			for (int32 VertexIndex = 0; VertexIndex < (int32)NumVerts; ++VertexIndex)
			{
				FVertexID AddedVertexId = OutReducedMesh.CreateVertex();
				OutVertexPositions[AddedVertexId] = Verts[VertexIndex].Position;
				check(AddedVertexId.GetValue() == VertexIndex);
			}

			TMap<int32, FPolygonGroupID> PolygonGroupMapping;

			TVertexInstanceAttributesRef<FVector> OutVertexNormals = OutReducedMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
			TVertexInstanceAttributesRef<FVector> OutVertexTangents = OutReducedMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
			TVertexInstanceAttributesRef<float> OutVertexBinormalSigns = OutReducedMesh.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
			TVertexInstanceAttributesRef<FVector4> OutVertexColors = OutReducedMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
			TVertexInstanceAttributesRef<FVector2D> OutVertexUVs = OutReducedMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

			//Specify the number of texture coords in this mesh description
			OutVertexUVs.SetNumIndices(InMeshNumTexCoords);

			//Vertex instances and Polygons
			for (int32 TriangleIndex = 0; TriangleIndex < (int32)NumTris; TriangleIndex++)
			{
				FVertexInstanceID CornerInstanceIDs[3];
				FVertexID CornerVerticesIDs[3];
				for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
				{
					int32 VertexInstanceIndex = TriangleIndex * 3 + CornerIndex;
					const FVertexInstanceID VertexInstanceID(VertexInstanceIndex);
					CornerInstanceIDs[CornerIndex] = VertexInstanceID;
					int32 ControlPointIndex = Indexes[VertexInstanceIndex];
					const FVertexID VertexID(ControlPointIndex);
					//FVector VertexPosition = OutReducedMesh.GetVertex(VertexID).VertexPosition;
					CornerVerticesIDs[CornerIndex] = VertexID;
					FVertexInstanceID AddedVertexInstanceId = OutReducedMesh.CreateVertexInstance(VertexID);
					//Make sure the Added vertex instance ID is matching the expected vertex instance ID
					check(AddedVertexInstanceId == VertexInstanceID);
					check(AddedVertexInstanceId.GetValue() == VertexInstanceIndex);

					//NTBs information
					OutVertexTangents[AddedVertexInstanceId] = Verts[Indexes[VertexInstanceIndex]].Tangents[0];
					OutVertexBinormalSigns[AddedVertexInstanceId] = GetBasisDeterminantSign(Verts[Indexes[VertexInstanceIndex]].Tangents[0].GetSafeNormal(), Verts[Indexes[VertexInstanceIndex]].Tangents[1].GetSafeNormal(), Verts[Indexes[VertexInstanceIndex]].Normal.GetSafeNormal());
					OutVertexNormals[AddedVertexInstanceId] = Verts[Indexes[VertexInstanceIndex]].Normal;

					//Vertex Color
					OutVertexColors[AddedVertexInstanceId] = Verts[Indexes[VertexInstanceIndex]].Color;

					//Texture coord
					for (int32 TexCoordIndex = 0; TexCoordIndex < InMeshNumTexCoords; TexCoordIndex++)
					{
						OutVertexUVs.Set(AddedVertexInstanceId, TexCoordIndex, Verts[Indexes[VertexInstanceIndex]].TexCoords[TexCoordIndex]);
					}
				}
				
				// material index
				int32 MaterialIndex = Verts[Indexes[3 * TriangleIndex]].MaterialIndex;
				FPolygonGroupID MaterialPolygonGroupID = FPolygonGroupID::Invalid;
				if (!PolygonGroupMapping.Contains(MaterialIndex))
				{
					FPolygonGroupID PolygonGroupID(MaterialIndex);
					check(InMesh.PolygonGroups().IsValid(PolygonGroupID));
					MaterialPolygonGroupID = OutReducedMesh.PolygonGroups().Num() > MaterialIndex ? PolygonGroupID : OutReducedMesh.CreatePolygonGroup();

					// Copy all attributes from the base polygon group to the new polygon group
					InMesh.PolygonGroupAttributes().ForEach(
						[&OutReducedMesh, PolygonGroupID, MaterialPolygonGroupID](const FName Name, const auto ArrayRef)
						{
							for (int32 Index = 0; Index < ArrayRef.GetNumIndices(); ++Index)
							{
								// Only copy shared attribute values, since input mesh description can differ from output mesh description
								const auto& Value = ArrayRef.Get(PolygonGroupID, Index);
								if (OutReducedMesh.PolygonGroupAttributes().HasAttribute(Name))
								{
									OutReducedMesh.PolygonGroupAttributes().SetAttribute(MaterialPolygonGroupID, Name, Index, Value);
								}
							}
						}
					);
					PolygonGroupMapping.Add(MaterialIndex, MaterialPolygonGroupID);
				}
				else
				{
					MaterialPolygonGroupID = PolygonGroupMapping[MaterialIndex];
				}

				// Create polygon edges
				TArray<FMeshDescription::FContourPoint> Contours;
				{
					// Add the edges of this triangle
					for (uint32 TriangleEdgeNumber = 0; TriangleEdgeNumber < 3; ++TriangleEdgeNumber)
					{
						int32 ContourPointIndex = Contours.AddDefaulted();
						FMeshDescription::FContourPoint& ContourPoint = Contours[ContourPointIndex];
						//Find the matching edge ID
						uint32 CornerIndices[2];
						CornerIndices[0] = (TriangleEdgeNumber + 0) % 3;
						CornerIndices[1] = (TriangleEdgeNumber + 1) % 3;

						FVertexID EdgeVertexIDs[2];
						EdgeVertexIDs[0] = CornerVerticesIDs[CornerIndices[0]];
						EdgeVertexIDs[1] = CornerVerticesIDs[CornerIndices[1]];

						FEdgeID MatchEdgeId = OutReducedMesh.GetVertexPairEdge(EdgeVertexIDs[0], EdgeVertexIDs[1]);
						if (MatchEdgeId == FEdgeID::Invalid)
						{
							MatchEdgeId = OutReducedMesh.CreateEdge(EdgeVertexIDs[0], EdgeVertexIDs[1]);
							// @todo: set edge hardness?
						}
						ContourPoint.EdgeID = MatchEdgeId;
						ContourPoint.VertexInstanceID = CornerInstanceIDs[CornerIndices[0]];
					}
				}

				// Insert a polygon into the mesh
				const FPolygonID NewPolygonID = OutReducedMesh.CreatePolygon(MaterialPolygonGroupID, Contours);
				const int32 NewTriangleIndex = OutReducedMesh.GetPolygonTriangles(NewPolygonID).AddDefaulted();
				FMeshTriangle& NewTriangle = OutReducedMesh.GetPolygonTriangles(NewPolygonID)[NewTriangleIndex];
				for (int32 TriangleVertexIndex = 0; TriangleVertexIndex < 3; ++TriangleVertexIndex)
				{
					const FVertexInstanceID VertexInstanceID = CornerInstanceIDs[TriangleVertexIndex];
					NewTriangle.SetVertexInstanceID(TriangleVertexIndex, VertexInstanceID);
				}
			}
			Verts.Empty();
			Indexes.Empty();

			//Remove the unused polygon group (reduce can remove all polygons from a group)
			TArray<FPolygonGroupID> ToDeletePolygonGroupIDs;
			for (const FPolygonGroupID& PolygonGroupID : OutReducedMesh.PolygonGroups().GetElementIDs())
			{
				FMeshPolygonGroup& PolygonGroup = OutReducedMesh.GetPolygonGroup(PolygonGroupID);
				if (PolygonGroup.Polygons.Num() == 0)
				{
					ToDeletePolygonGroupIDs.Add(PolygonGroupID);
				}
			}
			for (const FPolygonGroupID& PolygonGroupID : ToDeletePolygonGroupIDs)
			{
				OutReducedMesh.DeletePolygonGroup(PolygonGroupID);
			}
		}
	}

	virtual bool ReduceSkeletalMesh(
		USkeletalMesh* SkeletalMesh,
		int32 LODIndex,
		bool bReregisterComponent = true
		) override
	{
		return false;
	}

	virtual bool IsSupported() const override
	{
		return true;
	}

	virtual ~FQuadricSimplifierMeshReduction() {}

	static FQuadricSimplifierMeshReduction* Create()
	{
		return new FQuadricSimplifierMeshReduction;
	}
private:
	void WeldVertexPositions(const FRawMesh& InMesh, const float WeldingThreshold, TArray<FVector>& OutVertexPositions, TArray<uint32>& OutIndices)
	{
		//The remap use to fix the indices after welding the vertex position buffer
		TArray<int32> VertexRemap;
		//Initialize some arrays
		VertexRemap.AddZeroed(InMesh.VertexPositions.Num());
		for (int32 VertexIndexRef = 0; VertexIndexRef < InMesh.VertexPositions.Num(); ++VertexIndexRef)
		{
			VertexRemap[VertexIndexRef] = INDEX_NONE;
		}
		OutVertexPositions.Reserve(InMesh.VertexPositions.Num());
		//Weld overlapping vertex position
		for (int32 VertexIndexRef = 0; VertexIndexRef < InMesh.VertexPositions.Num(); ++VertexIndexRef)
		{
			//Skip already remap vertex
			if (VertexRemap[VertexIndexRef] != INDEX_NONE)
			{
				continue;
			}
			const FVector& PositionA = InMesh.VertexPositions[VertexIndexRef];
			//Add this vertex to the new vertex buffer
			VertexRemap[VertexIndexRef] = OutVertexPositions.Add(InMesh.VertexPositions[VertexIndexRef]);
			//Find vertex to weld, search forward VertexIndexRef
			for (int32 VertexIndex = VertexIndexRef + 1; VertexIndex < InMesh.VertexPositions.Num(); ++VertexIndex)
			{
				//skip already remap vertex
				if (VertexRemap[VertexIndex] != INDEX_NONE)
				{
					continue;
				}
				const FVector& PositionB = InMesh.VertexPositions[VertexIndex];
				if (PositionA.Equals(PositionB, WeldingThreshold))
				{
					//Remap this vertex to the "reference remapped vertex"
					VertexRemap[VertexIndex] = VertexRemap[VertexIndexRef];
				}
			}
		}
		//Remap the indices to the new vertex position buffer
		OutIndices.AddZeroed(InMesh.WedgeIndices.Num());
		for (int32 WedgeIndex = 0; WedgeIndex < InMesh.WedgeIndices.Num(); ++WedgeIndex)
		{
			int32 VertexIndex = InMesh.WedgeIndices[WedgeIndex];
			OutIndices[WedgeIndex] = VertexRemap[VertexIndex] == INDEX_NONE ? VertexIndex : VertexRemap[VertexIndex];
		}
	}
};
TUniquePtr<FQuadricSimplifierMeshReduction> GQuadricSimplifierMeshReduction;

void FQuadricSimplifierMeshReductionModule::StartupModule()
{
	GQuadricSimplifierMeshReduction.Reset(FQuadricSimplifierMeshReduction::Create());
	IModularFeatures::Get().RegisterModularFeature(IMeshReductionModule::GetModularFeatureName(), this);
}

void FQuadricSimplifierMeshReductionModule::ShutdownModule()
{
	GQuadricSimplifierMeshReduction = nullptr;
	IModularFeatures::Get().UnregisterModularFeature(IMeshReductionModule::GetModularFeatureName(), this);
}

IMeshReduction* FQuadricSimplifierMeshReductionModule::GetStaticMeshReductionInterface()
{
	return GQuadricSimplifierMeshReduction.Get();
}

IMeshReduction* FQuadricSimplifierMeshReductionModule::GetSkeletalMeshReductionInterface()
{
	return nullptr;
}

IMeshMerging* FQuadricSimplifierMeshReductionModule::GetMeshMergingInterface()
{
	return nullptr;
}

class IMeshMerging* FQuadricSimplifierMeshReductionModule::GetDistributedMeshMergingInterface()
{
	return nullptr;
}

FString FQuadricSimplifierMeshReductionModule::GetName()
{
	return FString("QuadricMeshReduction");	
}
