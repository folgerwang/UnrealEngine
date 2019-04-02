// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Templates/ScopedPointer.h"
#include "MeshUtilities.h"
#include "MeshBuild.h"
#include "MeshSimplify.h"
#include "OverlappingCorners.h"
#include "Templates/UniquePtr.h"
#include "Features/IModularFeatures.h"
#include "IMeshReductionInterfaces.h"
#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "RenderUtils.h"
#include "Engine/StaticMesh.h"
#include "MeshDescriptionOperations.h"

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
		// Correct layout selection depends on the name "QuadricMeshReduction_{foo}"
		// e.g.
		// TArray<FString> SplitVersionString;
		// VersionString.ParseIntoArray(SplitVersionString, TEXT("_"), true);
		// bool bUseQuadricSimplier = SplitVersionString[0].Equals("QuadricMeshReduction");

		static FString Version = TEXT("QuadricMeshReduction_V1.0");
		return Version;
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
		
		TMap<FVertexID, FVertexID> VertexIDRemap;

		bool bWeldVertices = ReductionSettings.WeldingThreshold > 0.0f;
		if (bWeldVertices)
		{
			FMeshDescriptionOperations::BuildWeldedVertexIDRemap(InMesh, ReductionSettings.WeldingThreshold, VertexIDRemap);
		}

		TArray< TVertSimp< NumTexCoords > >	Verts;
		TArray< uint32 >					Indexes;

		TMap< int32, int32 > VertsMap;

		int32 NumFaces = 0;
		for (const FPolygonID PolygonID : InMesh.Polygons().GetElementIDs())
		{
			NumFaces += InMesh.GetPolygonTriangles(PolygonID).Num();
		}
		int32 NumWedges = NumFaces * 3;
		FStaticMeshDescriptionConstAttributeGetter InMeshAttribute(&InMesh);
		TVertexAttributesConstRef<FVector> InVertexPositions = InMeshAttribute.GetPositions();
		TVertexInstanceAttributesConstRef<FVector> InVertexNormals = InMeshAttribute.GetNormals();
		TVertexInstanceAttributesConstRef<FVector> InVertexTangents = InMeshAttribute.GetTangents();
		TVertexInstanceAttributesConstRef<float> InVertexBinormalSigns = InMeshAttribute.GetBinormalSigns();
		TVertexInstanceAttributesConstRef<FVector4> InVertexColors = InMeshAttribute.GetColors();
		TVertexInstanceAttributesConstRef<FVector2D> InVertexUVs = InMeshAttribute.GetUVs();
		TPolygonGroupAttributesConstRef<FName> InPolygonGroupMaterialNames = InMeshAttribute.GetPolygonGroupImportedMaterialSlotNames();

		TPolygonGroupAttributesRef<FName> OutPolygonGroupMaterialNames = OutReducedMesh.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

		int32 WedgeIndex = 0;
		for (const FPolygonID& PolygonID : InMesh.Polygons().GetElementIDs())
		{
			const FPolygonGroupID PolygonGroupID = InMesh.GetPolygonPolygonGroup(PolygonID);

			const TArray<FMeshTriangle>& PolygonTriangles = InMesh.GetPolygonTriangles(PolygonID);
			for (int32 TriangleIndex = 0; TriangleIndex < PolygonTriangles.Num(); ++TriangleIndex)
			{
				const FMeshTriangle& Triangle = PolygonTriangles[TriangleIndex];

				FVector CornerPositions[3];
				for (int32 TriVert = 0; TriVert < 3; ++TriVert)
				{
					const FVertexInstanceID VertexInstanceID = Triangle.GetVertexInstanceID(TriVert);
					const FVertexID TmpVertexID = InMesh.GetVertexInstanceVertex(VertexInstanceID);
					const FVertexID VertexID = bWeldVertices ? VertexIDRemap[TmpVertexID] : TmpVertexID;
					CornerPositions[TriVert] = InVertexPositions[VertexID];
				}

				// Don't process degenerate triangles.
				if( PointsEqual(CornerPositions[0], CornerPositions[1]) ||
					PointsEqual(CornerPositions[0], CornerPositions[2]) ||
					PointsEqual(CornerPositions[1], CornerPositions[2]) )
				{
					WedgeIndex += 3;
					continue;
				}

				int32 VertexIndices[3];
				for (int32 TriVert = 0; TriVert < 3; ++TriVert, ++WedgeIndex)
				{
					const FVertexInstanceID VertexInstanceID = Triangle.GetVertexInstanceID(TriVert);
					const int32 VertexInstanceValue = VertexInstanceID.GetValue();
					const FVector& VertexPosition = CornerPositions[TriVert];

					TVertSimp< NumTexCoords > NewVert;

					const TArray<FPolygonID>& VertexInstanceConnectedPolygons = InMesh.GetVertexInstanceConnectedPolygons( VertexInstanceID );
					if (VertexInstanceConnectedPolygons.Num() > 0)
					{
						const FPolygonID ConnectedPolygonID = VertexInstanceConnectedPolygons[0];
						NewVert.MaterialIndex = InMesh.GetPolygonPolygonGroup(ConnectedPolygonID).GetValue();
					}

					NewVert.Position = CornerPositions[TriVert];
					NewVert.Tangents[0] = InVertexTangents[ VertexInstanceID ];
					NewVert.Normal = InVertexNormals[ VertexInstanceID ];
					NewVert.Tangents[1] = FVector(0.0f);
					if (!NewVert.Normal.IsNearlyZero(SMALL_NUMBER) && !NewVert.Tangents[0].IsNearlyZero(SMALL_NUMBER))
					{
						NewVert.Tangents[1] = FVector::CrossProduct(NewVert.Normal, NewVert.Tangents[0]).GetSafeNormal() * InVertexBinormalSigns[ VertexInstanceID ];
					}

					// Fix bad tangents
					NewVert.Tangents[0] = NewVert.Tangents[0].ContainsNaN() ? FVector::ZeroVector : NewVert.Tangents[0];
					NewVert.Tangents[1] = NewVert.Tangents[1].ContainsNaN() ? FVector::ZeroVector : NewVert.Tangents[1];
					NewVert.Normal = NewVert.Normal.ContainsNaN() ? FVector::ZeroVector : NewVert.Normal;
					NewVert.Color = FLinearColor(InVertexColors[ VertexInstanceID ]);

					for (int32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++)
					{
						if (UVIndex < InVertexUVs.GetNumIndices())
						{
							NewVert.TexCoords[UVIndex] = InVertexUVs.Get(VertexInstanceID, UVIndex);
							InMeshNumTexCoords = FMath::Max(UVIndex + 1, InMeshNumTexCoords);
						}
						else
						{
							NewVert.TexCoords[UVIndex] = FVector2D::ZeroVector;
						}
					}

					// Make sure this vertex is valid from the start
					NewVert.Correct();
					

					//Never add duplicated vertex instance
					//Use WedgeIndex since OverlappingCorners has been built based on that
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
					VertexIndices[TriVert] = Index;
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
		int32 AbsoluteMinTris = 2;
		int32 TargetNumTriangles = (ReductionSettings.TerminationCriterion != EStaticMeshReductionTerimationCriterion::Vertices) ? FMath::Max(AbsoluteMinTris, FMath::CeilToInt(NumTris * ReductionSettings.PercentTriangles)) : AbsoluteMinTris;
		int32 TargetNumVertices = (ReductionSettings.TerminationCriterion != EStaticMeshReductionTerimationCriterion::Triangles) ? FMath::CeilToInt(NumVerts * ReductionSettings.PercentVertices) : 0;
		
		float MaxErrorSqr = MeshSimp->SimplifyMesh(MAX_FLT, TargetNumTriangles, TargetNumVertices);

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
				TArray<FVertexInstanceID> CornerInstanceIDs;
				CornerInstanceIDs.SetNum(3);

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

				// Insert a polygon into the mesh
				TArray<FEdgeID> NewEdgeIDs;
				const FPolygonID NewPolygonID = OutReducedMesh.CreatePolygon(MaterialPolygonGroupID, CornerInstanceIDs, &NewEdgeIDs);
				for (const FEdgeID NewEdgeID : NewEdgeIDs)
				{
					// @todo: set NewEdgeID edge hardness?
				}
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

	/**
	*	Returns true if mesh reduction is active. Active mean there will be a reduction of the vertices or triangle number
	*/
	virtual bool IsReductionActive(const struct FMeshReductionSettings &ReductionSettings) const
	{
		float Threshold_One = (1.0f - KINDA_SMALL_NUMBER);
		switch (ReductionSettings.TerminationCriterion)
		{
			case EStaticMeshReductionTerimationCriterion::Triangles:
			{
				return ReductionSettings.PercentTriangles < Threshold_One;
			}
			break;
			case EStaticMeshReductionTerimationCriterion::Vertices:
			{
				return ReductionSettings.PercentVertices < Threshold_One;
			}
			break;
			case EStaticMeshReductionTerimationCriterion::Any:
			{
				return ReductionSettings.PercentTriangles < Threshold_One || ReductionSettings.PercentVertices < Threshold_One;
			}
			break;
		}
		return false;
	}

	virtual bool IsReductionActive(const FSkeletalMeshOptimizationSettings &ReductionSettings) const
	{
		return false;
	}

	virtual ~FQuadricSimplifierMeshReduction() {}

	static FQuadricSimplifierMeshReduction* Create()
	{
		return new FQuadricSimplifierMeshReduction;
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
