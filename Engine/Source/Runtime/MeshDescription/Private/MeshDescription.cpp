// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "Serialization/BulkDataReader.h"
#include "Serialization/BulkDataWriter.h"


void UDEPRECATED_MeshDescription::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		UE_LOG( LogLoad, Error, TEXT( "UMeshDescription about to be deprecated - please resave %s" ), *GetPathName() );
	}
	// Discard the contents
	FMeshDescription MeshDescription;
	Ar << MeshDescription;
}


FArchive& operator<<( FArchive& Ar, FMeshDescription& MeshDescription )
{
	Ar.UsingCustomVersion( FReleaseObjectVersion::GUID );
	Ar.UsingCustomVersion( FEditorObjectVersion::GUID );

	if( Ar.IsLoading() && Ar.CustomVer( FReleaseObjectVersion::GUID ) < FReleaseObjectVersion::MeshDescriptionNewSerialization )
	{
		UE_LOG( LogLoad, Warning, TEXT( "Deprecated serialization format" ) );
	}

	Ar << MeshDescription.VertexArray;
	Ar << MeshDescription.VertexInstanceArray;
	Ar << MeshDescription.EdgeArray;
	Ar << MeshDescription.PolygonArray;
	Ar << MeshDescription.PolygonGroupArray;

	Ar << MeshDescription.VertexAttributesSet;
	Ar << MeshDescription.VertexInstanceAttributesSet;
	Ar << MeshDescription.EdgeAttributesSet;
	Ar << MeshDescription.PolygonAttributesSet;
	Ar << MeshDescription.PolygonGroupAttributesSet;

	if( Ar.IsLoading() && Ar.CustomVer( FReleaseObjectVersion::GUID ) >= FReleaseObjectVersion::MeshDescriptionNewSerialization )
	{
		// Populate vertex instance IDs for vertices
		for( const FVertexInstanceID VertexInstanceID : MeshDescription.VertexInstanceArray.GetElementIDs() )
		{
			const FVertexID VertexID = MeshDescription.GetVertexInstanceVertex( VertexInstanceID );
			MeshDescription.VertexArray[ VertexID ].VertexInstanceIDs.Add( VertexInstanceID );
		}

		// Populate edge IDs for vertices
		for( const FEdgeID EdgeID : MeshDescription.EdgeArray.GetElementIDs() )
		{
			const FVertexID VertexID0 = MeshDescription.GetEdgeVertex( EdgeID, 0 );
			const FVertexID VertexID1 = MeshDescription.GetEdgeVertex( EdgeID, 1 );
			MeshDescription.VertexArray[ VertexID0 ].ConnectedEdgeIDs.Add( EdgeID );
			MeshDescription.VertexArray[ VertexID1 ].ConnectedEdgeIDs.Add( EdgeID );
		}

		// Populate polygon IDs for vertex instances, edges and polygon groups
		for( const FPolygonID PolygonID : MeshDescription.PolygonArray.GetElementIDs() )
		{
			auto PopulatePolygonIDs = [ &MeshDescription, PolygonID ]( const TArray<FVertexInstanceID>& VertexInstanceIDs )
			{
				const int32 NumVertexInstances = VertexInstanceIDs.Num();
				for( int32 Index = 0; Index < NumVertexInstances; ++Index )
				{
					const FVertexInstanceID VertexInstanceID0 = VertexInstanceIDs[ Index ];
					const FVertexInstanceID VertexInstanceID1 = VertexInstanceIDs[ ( Index + 1 ) % NumVertexInstances ];
					const FVertexID VertexID0 = MeshDescription.GetVertexInstanceVertex( VertexInstanceID0 );
					const FVertexID VertexID1 = MeshDescription.GetVertexInstanceVertex( VertexInstanceID1 );
					const FEdgeID EdgeID = MeshDescription.GetVertexPairEdge( VertexID0, VertexID1 );
					MeshDescription.VertexInstanceArray[ VertexInstanceID0 ].ConnectedPolygons.Add( PolygonID );
					MeshDescription.EdgeArray[ EdgeID ].ConnectedPolygons.Add( PolygonID );
				}
			};

			PopulatePolygonIDs( MeshDescription.GetPolygonPerimeterVertexInstances( PolygonID ) );

			const FPolygonGroupID PolygonGroupID = MeshDescription.PolygonArray[ PolygonID ].PolygonGroupID;
			MeshDescription.PolygonGroupArray[ PolygonGroupID ].Polygons.Add( PolygonID );

			// We don't serialize triangles; instead the polygon gets retriangulated on load
			MeshDescription.ComputePolygonTriangulation( PolygonID, MeshDescription.PolygonArray[ PolygonID ].Triangles );
		}
	}

	return Ar;
}


void FMeshDescription::Empty()
{
	VertexArray.Reset();
	VertexInstanceArray.Reset();
	EdgeArray.Reset();
	PolygonArray.Reset();
	PolygonGroupArray.Reset();

	//Empty all attributes
	VertexAttributesSet.Initialize( 0 );
	VertexInstanceAttributesSet.Initialize( 0 );
	EdgeAttributesSet.Initialize( 0 );
	PolygonAttributesSet.Initialize( 0 );
	PolygonGroupAttributesSet.Initialize( 0 );
}


bool FMeshDescription::IsEmpty() const
{
	return VertexArray.GetArraySize() == 0 &&
		   VertexInstanceArray.GetArraySize() == 0 &&
		   EdgeArray.GetArraySize() == 0 &&
		   PolygonArray.GetArraySize() == 0 &&
		   PolygonGroupArray.GetArraySize() == 0;
}


void FMeshDescription::Compact( FElementIDRemappings& OutRemappings )
{
	VertexArray.Compact( OutRemappings.NewVertexIndexLookup );
	VertexInstanceArray.Compact( OutRemappings.NewVertexInstanceIndexLookup );
	EdgeArray.Compact( OutRemappings.NewEdgeIndexLookup );
	PolygonArray.Compact( OutRemappings.NewPolygonIndexLookup );
	PolygonGroupArray.Compact( OutRemappings.NewPolygonGroupIndexLookup );

	RemapAttributes( OutRemappings );
	FixUpElementIDs( OutRemappings );
}


void FMeshDescription::Remap( const FElementIDRemappings& Remappings )
{
	VertexArray.Remap( Remappings.NewVertexIndexLookup );
	VertexInstanceArray.Remap( Remappings.NewVertexInstanceIndexLookup );
	EdgeArray.Remap( Remappings.NewEdgeIndexLookup );
	PolygonArray.Remap( Remappings.NewPolygonIndexLookup );
	PolygonGroupArray.Remap( Remappings.NewPolygonGroupIndexLookup );

	RemapAttributes( Remappings );
	FixUpElementIDs( Remappings );
}


void FMeshDescription::FixUpElementIDs( const FElementIDRemappings& Remappings )
{
	for( const FVertexID VertexID : VertexArray.GetElementIDs() )
	{
		FMeshVertex& Vertex = VertexArray[ VertexID ];

		// Fix up vertex instance index references in vertices array
		for( FVertexInstanceID& VertexInstanceID : Vertex.VertexInstanceIDs )
		{
			VertexInstanceID = Remappings.GetRemappedVertexInstanceID( VertexInstanceID );
		}

		// Fix up edge index references in the vertex array
		for( FEdgeID& EdgeID : Vertex.ConnectedEdgeIDs )
		{
			EdgeID = Remappings.GetRemappedEdgeID( EdgeID );
		}
	}

	// Fix up vertex index references in vertex instance array
	for( const FVertexInstanceID VertexInstanceID : VertexInstanceArray.GetElementIDs() )
	{
		FMeshVertexInstance& VertexInstance = VertexInstanceArray[ VertexInstanceID ];

		VertexInstance.VertexID = Remappings.GetRemappedVertexID( VertexInstance.VertexID );

		for( FPolygonID& PolygonID : VertexInstance.ConnectedPolygons )
		{
			PolygonID = Remappings.GetRemappedPolygonID( PolygonID );
		}
	}

	for( const FEdgeID EdgeID : EdgeArray.GetElementIDs() )
	{
		FMeshEdge& Edge = EdgeArray[ EdgeID ];

		// Fix up vertex index references in Edges array
		for( int32 Index = 0; Index < 2; Index++ )
		{
			Edge.VertexIDs[ Index ] = Remappings.GetRemappedVertexID( Edge.VertexIDs[ Index ] );
		}

		// Fix up references to section indices
		for( FPolygonID& ConnectedPolygon : Edge.ConnectedPolygons )
		{
			ConnectedPolygon = Remappings.GetRemappedPolygonID( ConnectedPolygon );
		}
	}

	for( const FPolygonID PolygonID : PolygonArray.GetElementIDs() )
	{
		FMeshPolygon& Polygon = PolygonArray[ PolygonID ];

		// Fix up references to vertex indices in section polygons' contours
		for( FVertexInstanceID& VertexInstanceID : Polygon.PerimeterContour.VertexInstanceIDs )
		{
			VertexInstanceID = Remappings.GetRemappedVertexInstanceID( VertexInstanceID );
		}

		for( FMeshTriangle& Triangle : Polygon.Triangles )
		{
			for( int32 TriangleVertexNumber = 0; TriangleVertexNumber < 3; ++TriangleVertexNumber )
			{
				const FVertexInstanceID OriginalVertexInstanceID = Triangle.GetVertexInstanceID( TriangleVertexNumber );
				const FVertexInstanceID NewVertexInstanceID = Remappings.GetRemappedVertexInstanceID( OriginalVertexInstanceID );
				Triangle.SetVertexInstanceID( TriangleVertexNumber, NewVertexInstanceID );
			}
		}

		Polygon.PolygonGroupID = Remappings.GetRemappedPolygonGroupID( Polygon.PolygonGroupID );
	}

	for( const FPolygonGroupID PolygonGroupID : PolygonGroupArray.GetElementIDs() )
	{
		FMeshPolygonGroup& PolygonGroup = PolygonGroupArray[ PolygonGroupID ];

		for( FPolygonID& Polygon : PolygonGroup.Polygons )
		{
			Polygon = Remappings.GetRemappedPolygonID( Polygon );
		}
	}
}


void FMeshDescription::CreatePolygon_Internal( const FPolygonID PolygonID, const FPolygonGroupID PolygonGroupID, const TArray<FContourPoint>& Perimeter )
{
	FMeshPolygon& Polygon = PolygonArray[ PolygonID ];

	Polygon.PerimeterContour.VertexInstanceIDs.Reset( Perimeter.Num() );
	for( const FContourPoint ContourPoint : Perimeter )
	{
		const FVertexInstanceID VertexInstanceID = ContourPoint.VertexInstanceID;
		const FEdgeID EdgeID = ContourPoint.EdgeID;

		Polygon.PerimeterContour.VertexInstanceIDs.Add( VertexInstanceID );
		check( !VertexInstanceArray[ VertexInstanceID ].ConnectedPolygons.Contains( PolygonID ) );
		VertexInstanceArray[ VertexInstanceID ].ConnectedPolygons.Add( PolygonID );

		check( !EdgeArray[ EdgeID ].ConnectedPolygons.Contains( PolygonID ) );
		EdgeArray[ EdgeID ].ConnectedPolygons.Add( PolygonID );
	}

	Polygon.PolygonGroupID = PolygonGroupID;
	PolygonGroupArray[ PolygonGroupID ].Polygons.Add( PolygonID );

	PolygonAttributesSet.Insert( PolygonID );
}


void FMeshDescription::CreatePolygon_Internal( const FPolygonID PolygonID, const FPolygonGroupID PolygonGroupID, const TArray<FVertexInstanceID>& VertexInstanceIDs, TArray<FEdgeID>* OutEdgeIDs )
{
	if( OutEdgeIDs )
	{
		OutEdgeIDs->Empty();
	}

	FMeshPolygon& Polygon = PolygonArray[ PolygonID ];
	const int32 NumVertices = VertexInstanceIDs.Num();
	Polygon.PerimeterContour.VertexInstanceIDs.Reset( NumVertices );

	for( int32 Index = 0; Index < NumVertices; ++Index )
	{
		const FVertexInstanceID VertexInstanceID = VertexInstanceIDs[ Index ];
		const FVertexInstanceID NextVertexInstanceID = VertexInstanceIDs[ ( Index + 1 == NumVertices ) ? 0 : Index + 1 ];

		Polygon.PerimeterContour.VertexInstanceIDs.Add( VertexInstanceID );
		check( !VertexInstanceArray[ VertexInstanceID ].ConnectedPolygons.Contains( PolygonID ) );
		VertexInstanceArray[ VertexInstanceID ].ConnectedPolygons.Add( PolygonID );

		const FVertexID VertexID0 = GetVertexInstanceVertex( VertexInstanceID );
		const FVertexID VertexID1 = GetVertexInstanceVertex( NextVertexInstanceID );

		FEdgeID EdgeID = GetVertexPairEdge( VertexID0, VertexID1 );
		if( EdgeID == FEdgeID::Invalid )
		{
			EdgeID = CreateEdge( VertexID0, VertexID1 );
			if( OutEdgeIDs )
			{
				OutEdgeIDs->Add( EdgeID );
			}
		}

		check( !EdgeArray[ EdgeID ].ConnectedPolygons.Contains( PolygonID ) );
		EdgeArray[ EdgeID ].ConnectedPolygons.Add( PolygonID );
	}

	Polygon.PolygonGroupID = PolygonGroupID;
	PolygonGroupArray[ PolygonGroupID ].Polygons.Add( PolygonID );

	PolygonAttributesSet.Insert( PolygonID );
}



void FMeshDescription::RemapAttributes( const FElementIDRemappings& Remappings )
{
	VertexAttributesSet.Remap( Remappings.NewVertexIndexLookup );
	VertexInstanceAttributesSet.Remap( Remappings.NewVertexInstanceIndexLookup );
	EdgeAttributesSet.Remap( Remappings.NewEdgeIndexLookup );
	PolygonAttributesSet.Remap( Remappings.NewPolygonIndexLookup );
	PolygonGroupAttributesSet.Remap( Remappings.NewPolygonGroupIndexLookup );
}


bool FMeshDescription::IsVertexOrphaned( const FVertexID VertexID ) const
{
	for( const FVertexInstanceID VertexInstanceID : GetVertex( VertexID ).VertexInstanceIDs )
	{
		if( GetVertexInstance( VertexInstanceID ).ConnectedPolygons.Num() > 0 )
		{
			return false;
		}
	}

	return true;
}


void FMeshDescription::GetPolygonPerimeterVertices( const FPolygonID PolygonID, TArray<FVertexID>& OutPolygonPerimeterVertexIDs ) const
{
	const FMeshPolygon& Polygon = GetPolygon( PolygonID );

	OutPolygonPerimeterVertexIDs.SetNumUninitialized( Polygon.PerimeterContour.VertexInstanceIDs.Num(), false );

	int32 Index = 0;
	for( const FVertexInstanceID VertexInstanceID : Polygon.PerimeterContour.VertexInstanceIDs )
	{
		const FMeshVertexInstance& VertexInstance = VertexInstanceArray[ VertexInstanceID ];
		OutPolygonPerimeterVertexIDs[ Index ] = VertexInstance.VertexID;
		Index++;
	}
}


/** Given three direction vectors, indicates if A and B are on the same 'side' of Vec. */
bool FMeshDescription::VectorsOnSameSide( const FVector& Vec, const FVector& A, const FVector& B, const float SameSideDotProductEpsilon )
{
	const FVector CrossA = FVector::CrossProduct( Vec, A );
	const FVector CrossB = FVector::CrossProduct( Vec, B );
	float DotWithEpsilon = SameSideDotProductEpsilon + FVector::DotProduct( CrossA, CrossB );
	return !FMath::IsNegativeFloat( DotWithEpsilon );
}


/** Util to see if P lies within triangle created by A, B and C. */
bool FMeshDescription::PointInTriangle( const FVector& A, const FVector& B, const FVector& C, const FVector& P, const float InsideTriangleDotProductEpsilon )
{
	// Cross product indicates which 'side' of the vector the point is on
	// If its on the same side as the remaining vert for all edges, then its inside.	
	if ( VectorsOnSameSide( B - A, P - A, C - A, InsideTriangleDotProductEpsilon ) &&
		 VectorsOnSameSide( C - B, P - B, A - B, InsideTriangleDotProductEpsilon ) &&
		 VectorsOnSameSide( A - C, P - C, B - C, InsideTriangleDotProductEpsilon ) )
	{
		return true;
	}
	else
	{
		return false;
	}
}


FPlane FMeshDescription::ComputePolygonPlane( const FPolygonID PolygonID ) const
{
	// NOTE: This polygon plane computation code is partially based on the implementation of "Newell's method" from Real-Time 
	//       Collision Detection by Christer Ericson, published by Morgan Kaufmann Publishers, (c) 2005 Elsevier Inc

	// @todo mesheditor perf: For polygons that are just triangles, use a cross product to get the normal fast!
	// @todo mesheditor perf: We could skip computing the plane distance when we only need the normal
	// @todo mesheditor perf: We could cache these computed polygon normals; or just use the normal of the first three vertices' triangle if it is satisfactory in all cases
	// @todo mesheditor: For non-planar polygons, the result can vary. Ideally this should use the actual polygon triangulation as opposed to the arbitrary triangulation used here.

	FVector Centroid = FVector::ZeroVector;
	FVector Normal = FVector::ZeroVector;

	static TArray<FVertexID> PerimeterVertexIDs;
	GetPolygonPerimeterVertices( PolygonID, /* Out */ PerimeterVertexIDs );

	// @todo Maybe this shouldn't be in FMeshDescription but in a utility class, as it references a specific attribute name
	TVertexAttributesConstRef<FVector> VertexPositions = VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

	// Use 'Newell's Method' to compute a robust 'best fit' plane from the vertices of this polygon
	for ( int32 VertexNumberI = PerimeterVertexIDs.Num() - 1, VertexNumberJ = 0; VertexNumberJ < PerimeterVertexIDs.Num(); VertexNumberI = VertexNumberJ, VertexNumberJ++ )
	{
		const FVertexID VertexIDI = PerimeterVertexIDs[ VertexNumberI ];
		const FVector PositionI = VertexPositions[ VertexIDI ];

		const FVertexID VertexIDJ = PerimeterVertexIDs[ VertexNumberJ ];
		const FVector PositionJ = VertexPositions[ VertexIDJ ];

		Centroid += PositionJ;

		Normal.X += ( PositionJ.Y - PositionI.Y ) * ( PositionI.Z + PositionJ.Z );
		Normal.Y += ( PositionJ.Z - PositionI.Z ) * ( PositionI.X + PositionJ.X );
		Normal.Z += ( PositionJ.X - PositionI.X ) * ( PositionI.Y + PositionJ.Y );
	}

	Normal.Normalize();

	// Construct a plane from the normal and centroid
	return FPlane( Normal, FVector::DotProduct( Centroid, Normal ) / ( float )PerimeterVertexIDs.Num() );
}


FVector FMeshDescription::ComputePolygonNormal( const FPolygonID PolygonID ) const
{
	// @todo mesheditor: Polygon normals are now computed and cached when changes are made to a polygon.
	// In theory, we can just return that cached value, but we need to check that there is nothing which relies on the value being correct before
	// the cache is updated at the end of a modification.
	const FPlane PolygonPlane = ComputePolygonPlane( PolygonID );
	const FVector PolygonNormal( PolygonPlane.X, PolygonPlane.Y, PolygonPlane.Z );
	return PolygonNormal;
}


void FMeshDescription::ComputePolygonTriangulation(const FPolygonID PolygonID, TArray<FMeshTriangle>& OutTriangles)
{
	// NOTE: This polygon triangulation code is partially based on the ear cutting algorithm described on
	//       page 497 of the book "Real-time Collision Detection", published in 2005.

	struct Local
	{
		// Returns true if the triangle formed by the specified three positions has a normal that is facing the opposite direction of the reference normal
		static inline bool IsTriangleFlipped(const FVector ReferenceNormal, const FVector VertexPositionA, const FVector VertexPositionB, const FVector VertexPositionC)
		{
			const FVector TriangleNormal = FVector::CrossProduct(
				VertexPositionC - VertexPositionA,
				VertexPositionB - VertexPositionA).GetSafeNormal();
			return (FVector::DotProduct(ReferenceNormal, TriangleNormal) <= 0.0f);
		}

	};


	OutTriangles.Reset();

	// @todo mesheditor: Perhaps should always attempt to triangulate by splitting polygons along the shortest edge, for better determinism.

	//	const FMeshPolygon& Polygon = GetPolygon( PolygonID );
	const TArray<FVertexInstanceID>& PolygonVertexInstanceIDs = GetPolygonPerimeterVertexInstances(PolygonID);

	// Polygon must have at least three vertices/edges
	const int32 PolygonVertexCount = PolygonVertexInstanceIDs.Num();
	check(PolygonVertexCount >= 3);

	// If perimeter has 3 vertices, just copy content of perimeter out 
	if (PolygonVertexCount == 3)
	{
		OutTriangles.Emplace();
		FMeshTriangle& Triangle = OutTriangles.Last();

		Triangle.SetVertexInstanceID(0, PolygonVertexInstanceIDs[0]);
		Triangle.SetVertexInstanceID(1, PolygonVertexInstanceIDs[1]);
		Triangle.SetVertexInstanceID(2, PolygonVertexInstanceIDs[2]);

		return;
	}

	// First figure out the polygon normal.  We need this to determine which triangles are convex, so that
	// we can figure out which ears to clip
	const FVector PolygonNormal = ComputePolygonNormal(PolygonID);

	// Make a simple linked list array of the previous and next vertex numbers, for each vertex number
	// in the polygon.  This will just save us having to iterate later on.
	static TArray<int32> PrevVertexNumbers, NextVertexNumbers;
	static TArray<FVector> VertexPositions;

	{
		const TVertexAttributesRef<FVector> MeshVertexPositions = VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
		PrevVertexNumbers.SetNumUninitialized(PolygonVertexCount, false);
		NextVertexNumbers.SetNumUninitialized(PolygonVertexCount, false);
		VertexPositions.SetNumUninitialized(PolygonVertexCount, false);

		for (int32 VertexNumber = 0; VertexNumber < PolygonVertexCount; ++VertexNumber)
		{
			PrevVertexNumbers[VertexNumber] = VertexNumber - 1;
			NextVertexNumbers[VertexNumber] = VertexNumber + 1;

			VertexPositions[VertexNumber] = MeshVertexPositions[GetVertexInstanceVertex(PolygonVertexInstanceIDs[VertexNumber])];
		}
		PrevVertexNumbers[0] = PolygonVertexCount - 1;
		NextVertexNumbers[PolygonVertexCount - 1] = 0;
	}

	int32 EarVertexNumber = 0;
	int32 EarTestCount = 0;
	for (int32 RemainingVertexCount = PolygonVertexCount; RemainingVertexCount >= 3; )
	{
		bool bIsEar = true;

		// If we're down to only a triangle, just treat it as an ear.  Also, if we've tried every possible candidate
		// vertex looking for an ear, go ahead and just treat the current vertex as an ear.  This can happen when 
		// vertices are colinear or other degenerate cases.
		if (RemainingVertexCount > 3 && EarTestCount < RemainingVertexCount)
		{
			const FVector PrevVertexPosition = VertexPositions[PrevVertexNumbers[EarVertexNumber]];
			const FVector EarVertexPosition = VertexPositions[EarVertexNumber];
			const FVector NextVertexPosition = VertexPositions[NextVertexNumbers[EarVertexNumber]];

			// Figure out whether the potential ear triangle is facing the same direction as the polygon
			// itself.  If it's facing the opposite direction, then we're dealing with a concave triangle
			// and we'll skip it for now.
			if (!Local::IsTriangleFlipped(
				PolygonNormal,
				PrevVertexPosition,
				EarVertexPosition,
				NextVertexPosition))
			{
				int32 TestVertexNumber = NextVertexNumbers[NextVertexNumbers[EarVertexNumber]];

				do
				{
					// Test every other remaining vertex to make sure that it doesn't lie inside our potential ear
					// triangle.  If we find a vertex that's inside the triangle, then it cannot actually be an ear.
					const FVector TestVertexPosition = VertexPositions[TestVertexNumber];
					if (PointInTriangle(
						PrevVertexPosition,
						EarVertexPosition,
						NextVertexPosition,
						TestVertexPosition,
						SMALL_NUMBER))
					{
						bIsEar = false;
						break;
					}

					TestVertexNumber = NextVertexNumbers[TestVertexNumber];
				} while (TestVertexNumber != PrevVertexNumbers[EarVertexNumber]);
			}
			else
			{
				bIsEar = false;
			}
		}

		if (bIsEar)
		{
			// OK, we found an ear!  Let's save this triangle in our output buffer.
			{
				OutTriangles.Emplace();
				FMeshTriangle& Triangle = OutTriangles.Last();

				Triangle.SetVertexInstanceID(0, PolygonVertexInstanceIDs[PrevVertexNumbers[EarVertexNumber]]);
				Triangle.SetVertexInstanceID(1, PolygonVertexInstanceIDs[EarVertexNumber]);
				Triangle.SetVertexInstanceID(2, PolygonVertexInstanceIDs[NextVertexNumbers[EarVertexNumber]]);
			}

			// Update our linked list.  We're effectively cutting off the ear by pointing the ear vertex's neighbors to
			// point at their next sequential neighbor, and reducing the remaining vertex count by one.
			{
				NextVertexNumbers[PrevVertexNumbers[EarVertexNumber]] = NextVertexNumbers[EarVertexNumber];
				PrevVertexNumbers[NextVertexNumbers[EarVertexNumber]] = PrevVertexNumbers[EarVertexNumber];
				--RemainingVertexCount;
			}

			// Move on to the previous vertex in the list, now that this vertex was cut
			EarVertexNumber = PrevVertexNumbers[EarVertexNumber];

			EarTestCount = 0;
		}
		else
		{
			// The vertex is not the ear vertex, because it formed a triangle that either had a normal which pointed in the opposite direction
			// of the polygon, or at least one of the other polygon vertices was found to be inside the triangle.  Move on to the next vertex.
			EarVertexNumber = NextVertexNumbers[EarVertexNumber];

			// Keep track of how many ear vertices we've tested, so that if we exhaust all remaining vertices, we can
			// fall back to clipping the triangle and adding it to our mesh anyway.  This is important for degenerate cases.
			++EarTestCount;
		}
	}

	check(OutTriangles.Num() > 0);
}


void FMeshDescription::TriangulateMesh()
{
	// Perform triangulation directly into mesh polygons
	for( const FPolygonID PolygonID : Polygons().GetElementIDs() )
	{
		FMeshPolygon& Polygon = PolygonArray[ PolygonID ];
		ComputePolygonTriangulation( PolygonID, Polygon.Triangles );
	}
}


bool FMeshDescription::ComputePolygonTangentsAndNormals(const FPolygonID PolygonID
	, float ComparisonThreshold
	, const TVertexAttributesRef<FVector> VertexPositions
	, const TVertexInstanceAttributesRef<FVector2D> VertexUVs
	, TPolygonAttributesRef<FVector> PolygonNormals
	, TPolygonAttributesRef<FVector> PolygonTangents
	, TPolygonAttributesRef<FVector> PolygonBinormals
	, TPolygonAttributesRef<FVector> PolygonCenters)
{
	bool bValidNTBs = true;
	// Calculate the center of this polygon
	FVector Center = FVector::ZeroVector;
	const TArray<FVertexInstanceID>& VertexInstanceIDs = GetPolygonPerimeterVertexInstances(PolygonID);
	for (const FVertexInstanceID VertexInstanceID : VertexInstanceIDs)
	{
		Center += VertexPositions[GetVertexInstanceVertex(VertexInstanceID)];
	}
	Center /= float(VertexInstanceIDs.Num());

	// Calculate the tangent basis for the polygon, based on the average of all constituent triangles
	FVector Normal = FVector::ZeroVector;
	FVector Tangent = FVector::ZeroVector;
	FVector Binormal = FVector::ZeroVector;

	for (const FMeshTriangle& Triangle : GetPolygonTriangles(PolygonID))
	{
		const FVertexID VertexID0 = GetVertexInstanceVertex(Triangle.VertexInstanceID0);
		const FVertexID VertexID1 = GetVertexInstanceVertex(Triangle.VertexInstanceID1);
		const FVertexID VertexID2 = GetVertexInstanceVertex(Triangle.VertexInstanceID2);

		const FVector DPosition1 = VertexPositions[VertexID1] - VertexPositions[VertexID0];
		const FVector DPosition2 = VertexPositions[VertexID2] - VertexPositions[VertexID0];

		const FVector2D DUV1 = VertexUVs[Triangle.VertexInstanceID1] - VertexUVs[Triangle.VertexInstanceID0];
		const FVector2D DUV2 = VertexUVs[Triangle.VertexInstanceID2] - VertexUVs[Triangle.VertexInstanceID0];

		// We have a left-handed coordinate system, but a counter-clockwise winding order
		// Hence normal calculation has to take the triangle vectors cross product in reverse.
		FVector TmpNormal = FVector::CrossProduct(DPosition2, DPosition1);
		if (!TmpNormal.IsNearlyZero(ComparisonThreshold) && !TmpNormal.ContainsNaN())
		{
			Normal += TmpNormal;
			// ...and tangent space seems to be right-handed.
			const float DetUV = FVector2D::CrossProduct(DUV1, DUV2);
			const float InvDetUV = (DetUV == 0.0f) ? 0.0f : 1.0f / DetUV;

			Tangent += (DPosition1 * DUV2.Y - DPosition2 * DUV1.Y) * InvDetUV;
			Binormal += (DPosition2 * DUV1.X - DPosition1 * DUV2.X) * InvDetUV;
		}
		else
		{
			//The polygon is degenerated
			bValidNTBs = false;
		}
	}

	PolygonNormals[PolygonID] = Normal.GetSafeNormal();
	PolygonTangents[PolygonID] = Tangent.GetSafeNormal();
	PolygonBinormals[PolygonID] = Binormal.GetSafeNormal();
	PolygonCenters[PolygonID] = Center;
	
	return bValidNTBs;
}


void FMeshDescription::ComputePolygonTangentsAndNormals(const TArray<FPolygonID>& PolygonIDs, float ComparisonThreshold)
{
	const TVertexAttributesRef<FVector> VertexPositions = VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	const TVertexInstanceAttributesRef<FVector2D> VertexUVs = VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
	TPolygonAttributesRef<FVector> PolygonNormals = PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Normal);
	TPolygonAttributesRef<FVector> PolygonTangents = PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Tangent);
	TPolygonAttributesRef<FVector> PolygonBinormals = PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Binormal);
	TPolygonAttributesRef<FVector> PolygonCenters = PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Center);

	TArray<FPolygonID> DegeneratePolygonIDs;
	for (const FPolygonID PolygonID : PolygonIDs)
	{
		if (!ComputePolygonTangentsAndNormals(PolygonID, ComparisonThreshold, VertexPositions, VertexUVs, PolygonNormals, PolygonTangents, PolygonBinormals, PolygonCenters))
		{
			DegeneratePolygonIDs.Add(PolygonID);
		}
	}
	//Remove degenerated polygons
	//Delete the degenerated polygons. The array is fill only if the remove degenerated option is turn on.
	if (DegeneratePolygonIDs.Num() > 0)
	{
		TArray<FEdgeID> OrphanedEdges;
		TArray<FVertexInstanceID> OrphanedVertexInstances;
		TArray<FPolygonGroupID> OrphanedPolygonGroups;
		TArray<FVertexID> OrphanedVertices;
		for (FPolygonID PolygonID : DegeneratePolygonIDs)
		{
			DeletePolygon(PolygonID, &OrphanedEdges, &OrphanedVertexInstances, &OrphanedPolygonGroups);
		}
		for (FPolygonGroupID PolygonGroupID : OrphanedPolygonGroups)
		{
			DeletePolygonGroup(PolygonGroupID);
		}
		for (FVertexInstanceID VertexInstanceID : OrphanedVertexInstances)
		{
			DeleteVertexInstance(VertexInstanceID, &OrphanedVertices);
		}
		for (FEdgeID EdgeID : OrphanedEdges)
		{
			DeleteEdge(EdgeID, &OrphanedVertices);
		}
		for (FVertexID VertexID : OrphanedVertices)
		{
			DeleteVertex(VertexID);
		}
		//Compact and Remap IDs so we have clean ID from 0 to n since we just erase some polygons
		//The render build need to have compact ID
		FElementIDRemappings RemappingInfos;
		Compact(RemappingInfos);
	}
}


void FMeshDescription::ComputePolygonTangentsAndNormals(float ComparisonThreshold)
{
	TArray<FPolygonID> PolygonsToComputeNTBs;
	PolygonsToComputeNTBs.Reserve(Polygons().Num());
	for (const FPolygonID& PolygonID : Polygons().GetElementIDs())
	{
		PolygonsToComputeNTBs.Add(PolygonID);
	}
	ComputePolygonTangentsAndNormals(PolygonsToComputeNTBs, ComparisonThreshold);
}


void FMeshDescription::GetConnectedSoftEdges(const FVertexID VertexID, TArray<FEdgeID>& OutConnectedSoftEdges) const
{
	OutConnectedSoftEdges.Reset();

	TEdgeAttributesConstRef<bool> EdgeHardnesses = EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
	for (const FEdgeID ConnectedEdgeID : GetVertex(VertexID).ConnectedEdgeIDs)
	{
		if (!EdgeHardnesses[ConnectedEdgeID])
		{
			OutConnectedSoftEdges.Add(ConnectedEdgeID);
		}
	}
}


void FMeshDescription::GetPolygonsInSameSoftEdgedGroupAsPolygon(const FPolygonID PolygonID, const TArray<FPolygonID>& CandidatePolygonIDs, const TArray<FEdgeID>& SoftEdgeIDs, TArray<FPolygonID>& OutPolygonIDs) const
{
	// The aim of this method is:
	// - given a polygon ID,
	// - given a set of candidate polygons connected to the same vertex (which should include the polygon ID),
	// - given a set of soft edges connected to the same vertex,
	// return the polygon IDs which form an adjacent run without crossing a hard edge.

	OutPolygonIDs.Reset();

	// Maintain a list of polygon IDs to be examined. Adjacents are added to the list if suitable.
	// Add the start poly here.
	static TArray<FPolygonID> PolygonsToCheck;
	PolygonsToCheck.Reset(CandidatePolygonIDs.Num());
	PolygonsToCheck.Add(PolygonID);

	int32 Index = 0;
	while (Index < PolygonsToCheck.Num())
	{
		const FPolygonID PolygonToCheck = PolygonsToCheck[Index];
		Index++;

		if (CandidatePolygonIDs.Contains(PolygonToCheck))
		{
			OutPolygonIDs.Add(PolygonToCheck);

			// Now look at its adjacent polygons. If they are joined by a soft edge which includes the vertex we're interested in, we want to consider them.
			// We take a shortcut by doing this process in reverse: we already know all the soft edges we are interested in, so check if any of them
			// have the current polygon as an adjacent.
			for (const FEdgeID SoftEdgeID : SoftEdgeIDs)
			{
				const TArray<FPolygonID>& EdgeConnectedPolygons = GetEdgeConnectedPolygons(SoftEdgeID);
				if (EdgeConnectedPolygons.Contains(PolygonToCheck))
				{
					for (const FPolygonID AdjacentPolygon : EdgeConnectedPolygons)
					{
						// Only add new polygons which haven't yet been added to the list. This prevents circular runs of polygons triggering infinite loops.
						PolygonsToCheck.AddUnique(AdjacentPolygon);
					}
				}
			}
		}
	}
}


void FMeshDescription::GetVertexConnectedPolygonsInSameSoftEdgedGroup(const FVertexID VertexID, const FPolygonID PolygonID, TArray<FPolygonID>& OutPolygonIDs) const
{
	// The aim here is to determine which polygons form part of the same soft edged group as the polygons attached to this vertex.
	// They should all contribute to the final vertex instance normal.

	// Get all polygons connected to this vertex.
	static TArray<FPolygonID> ConnectedPolygons;
	GetVertexConnectedPolygons(VertexID, ConnectedPolygons);

	// Cache a list of all soft edges which share this vertex.
	// We're only interested in finding adjacent polygons which are not the other side of a hard edge.
	static TArray<FEdgeID> ConnectedSoftEdges;
	GetConnectedSoftEdges(VertexID, ConnectedSoftEdges);

	GetPolygonsInSameSoftEdgedGroupAsPolygon(PolygonID, ConnectedPolygons, ConnectedSoftEdges, OutPolygonIDs);
}


float FMeshDescription::GetPolygonCornerAngleForVertex(const FPolygonID PolygonID, const FVertexID VertexID) const
{
	const FMeshPolygon& Polygon = GetPolygon(PolygonID);

	// Lambda function which returns the inner angle at a given index on a polygon contour
	auto GetContourAngle = [this](const FMeshPolygonContour& Contour, const int32 ContourIndex)
	{
		const int32 NumVertices = Contour.VertexInstanceIDs.Num();

		const int32 PrevIndex = (ContourIndex + NumVertices - 1) % NumVertices;
		const int32 NextIndex = (ContourIndex + 1) % NumVertices;

		const FVertexID PrevVertexID = GetVertexInstanceVertex(Contour.VertexInstanceIDs[PrevIndex]);
		const FVertexID ThisVertexID = GetVertexInstanceVertex(Contour.VertexInstanceIDs[ContourIndex]);
		const FVertexID NextVertexID = GetVertexInstanceVertex(Contour.VertexInstanceIDs[NextIndex]);

		TVertexAttributesConstRef<FVector> VertexPositions = VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);

		const FVector PrevVertexPosition = VertexPositions[PrevVertexID];
		const FVector ThisVertexPosition = VertexPositions[ThisVertexID];
		const FVector NextVertexPosition = VertexPositions[NextVertexID];

		const FVector Direction1 = (PrevVertexPosition - ThisVertexPosition).GetSafeNormal();
		const FVector Direction2 = (NextVertexPosition - ThisVertexPosition).GetSafeNormal();

		return FMath::Acos(FVector::DotProduct(Direction1, Direction2));
	};

	const FVertexInstanceArray& VertexInstancesRef = VertexInstances();
	auto IsVertexInstancedFromThisVertex = [&VertexInstancesRef, VertexID](const FVertexInstanceID VertexInstanceID)
	{
		return VertexInstancesRef[VertexInstanceID].VertexID == VertexID;
	};

	// First look for the vertex instance in the perimeter
	int32 ContourIndex = Polygon.PerimeterContour.VertexInstanceIDs.IndexOfByPredicate(IsVertexInstancedFromThisVertex);
	if (ContourIndex != INDEX_NONE)
	{
		// Return the internal angle if found
		return GetContourAngle(Polygon.PerimeterContour, ContourIndex);
	}

	// Found nothing; return 0
	return 0.0f;
}


void FMeshDescription::ComputeTangentsAndNormals(const FVertexInstanceID VertexInstanceID
	, EComputeNTBsOptions ComputeNTBsOptions
	, const TPolygonAttributesRef<FVector> PolygonNormals
	, const TPolygonAttributesRef<FVector> PolygonTangents
	, const TPolygonAttributesRef<FVector> PolygonBinormals
	, TVertexInstanceAttributesRef<FVector> VertexNormals
	, TVertexInstanceAttributesRef<FVector> VertexTangents
	, TVertexInstanceAttributesRef<float> VertexBinormalSigns)
{
	bool bComputeNormals = !!(ComputeNTBsOptions & EComputeNTBsOptions::Normals);
	bool bComputeTangents = !!(ComputeNTBsOptions & EComputeNTBsOptions::Tangents);
	bool bUseWeightedNormals = !!(ComputeNTBsOptions & EComputeNTBsOptions::WeightedNTBs);

	FVector Normal = FVector::ZeroVector;
	FVector Tangent = FVector::ZeroVector;
	FVector Binormal = FVector::ZeroVector;

	FVector& NormalRef = VertexNormals[VertexInstanceID];
	FVector& TangentRef = VertexTangents[VertexInstanceID];
	float& BinormalRef = VertexBinormalSigns[VertexInstanceID];

	if (!bComputeNormals && !bComputeTangents)
	{
		//Nothing to compute
		return;
	}

	const FVertexID VertexID = GetVertexInstanceVertex(VertexInstanceID);

	if (bComputeNormals || NormalRef.IsNearlyZero())
	{
		// Get all polygons connected to this vertex instance
		static TArray<FPolygonID> AllConnectedPolygons;
		const TArray<FPolygonID>& VertexInstanceConnectedPolygons = GetVertexInstanceConnectedPolygons(VertexInstanceID);
		check(VertexInstanceConnectedPolygons.Num() > 0);
		// Add also any in the same smoothing group connected to a different vertex instance
		// (as they still have influence over the normal).
		GetVertexConnectedPolygonsInSameSoftEdgedGroup(VertexID, VertexInstanceConnectedPolygons[0], AllConnectedPolygons);
		// The vertex instance normal is computed as a sum of all connected polygons' normals, weighted by the angle they make with the vertex
		for (const FPolygonID ConnectedPolygonID : AllConnectedPolygons)
		{
			const float Angle = bUseWeightedNormals ? GetPolygonCornerAngleForVertex(ConnectedPolygonID, VertexID) : 1.0f;

			Normal += PolygonNormals[ConnectedPolygonID] * Angle;

			// If this polygon is actually connected to the vertex instance we're processing, also include its contributions towards the tangent
			if (VertexInstanceConnectedPolygons.Contains(ConnectedPolygonID))
			{
				Tangent += PolygonTangents[ConnectedPolygonID] * Angle;
				Binormal += PolygonBinormals[ConnectedPolygonID] * Angle;
			}
		}
		// Normalize Normal
		Normal = Normal.GetSafeNormal();
	}
	else
	{
		//We use existing normals so just use all polygons having a vertex instance at the same location sharing the same normals
		Normal = NormalRef;
		TArray<FVertexInstanceID> VertexInstanceIDs = GetVertexVertexInstances(VertexID);
		for (const FVertexInstanceID& ConnectedVertexInstanceID : VertexInstanceIDs)
		{
			if (ConnectedVertexInstanceID != VertexInstanceID && !VertexNormals[ConnectedVertexInstanceID].Equals(Normal))
			{
				continue;
			}

			const TArray<FPolygonID>& ConnectedPolygons = GetVertexInstanceConnectedPolygons(ConnectedVertexInstanceID);
			for (const FPolygonID ConnectedPolygonID : ConnectedPolygons)
			{
				const float Angle = bUseWeightedNormals ? GetPolygonCornerAngleForVertex(ConnectedPolygonID, VertexID) : 1.0f;

				// If this polygon is actually connected to the vertex instance we're processing, also include its contributions towards the tangent
				Tangent += PolygonTangents[ConnectedPolygonID] * Angle;
				Binormal += PolygonBinormals[ConnectedPolygonID] * Angle;
			}
		}
	}
	
	
	float BinormalSign = 1.0f;
	if (bComputeTangents)
	{
		// Make Tangent orthonormal to Normal.
		// This is a quicker method than normalizing Tangent, taking the cross product Normal X Tangent, and then a further cross product with that result
		Tangent = (Tangent - Normal * FVector::DotProduct(Normal, Tangent)).GetSafeNormal();

		// Calculate binormal sign
		BinormalSign = (FVector::DotProduct(FVector::CrossProduct(Normal, Tangent), Binormal) < 0.0f) ? -1.0f : 1.0f;
	}

	//Set the value that need to be set
	if (NormalRef.IsNearlyZero())
	{
		NormalRef = Normal;
	}
	if (bComputeTangents)
	{
		if (TangentRef.IsNearlyZero())
		{
			TangentRef = Tangent;
		}
		if (FMath::IsNearlyZero(BinormalRef))
		{
			BinormalRef = BinormalSign;
		}
	}
}


void FMeshDescription::ComputeTangentsAndNormals(const TArray<FVertexInstanceID>& VertexInstanceIDs, EComputeNTBsOptions ComputeNTBsOptions)
{
	const TPolygonAttributesRef<FVector> PolygonNormals = PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Normal);
	const TPolygonAttributesRef<FVector> PolygonTangents = PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Tangent);
	const TPolygonAttributesRef<FVector> PolygonBinormals = PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Binormal);

	TVertexInstanceAttributesRef<FVector> VertexNormals = VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector> VertexTangents = VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesRef<float> VertexBinormalSigns = VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);

	for (const FVertexInstanceID VertexInstanceID : VertexInstanceIDs)
	{
		ComputeTangentsAndNormals(VertexInstanceID, ComputeNTBsOptions, PolygonNormals, PolygonTangents, PolygonBinormals, VertexNormals, VertexTangents, VertexBinormalSigns);
	}
}


void FMeshDescription::ComputeTangentsAndNormals(EComputeNTBsOptions ComputeNTBsOptions)
{
	const TPolygonAttributesRef<FVector> PolygonNormals = PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Normal);
	const TPolygonAttributesRef<FVector> PolygonTangents = PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Tangent);
	const TPolygonAttributesRef<FVector> PolygonBinormals = PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Binormal);

	TVertexInstanceAttributesRef<FVector> VertexNormals = VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector> VertexTangents = VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesRef<float> VertexBinormalSigns = VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);

	for (const FVertexInstanceID VertexInstanceID : VertexInstances().GetElementIDs())
	{
		ComputeTangentsAndNormals(VertexInstanceID, ComputeNTBsOptions, PolygonNormals, PolygonTangents, PolygonBinormals, VertexNormals, VertexTangents, VertexBinormalSigns);
	}
}


void FMeshDescription::DetermineEdgeHardnessesFromVertexInstanceNormals( const float Tolerance )
{
	const TVertexInstanceAttributesRef<FVector> VertexNormals = VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Normal );
	TEdgeAttributesRef<bool> EdgeHardnesses = EdgeAttributes().GetAttributesRef<bool>( MeshAttribute::Edge::IsHard );

	// Holds unique vertex instance IDs for a given edge vertex
	// @todo: use TMemStackAllocator or similar to avoid expensive allocations
	TArray<FVertexInstanceID> UniqueVertexInstanceIDs;

	for( const FEdgeID EdgeID : Edges().GetElementIDs() )
	{
		// Get list of polygons connected to this edge
		const TArray<FPolygonID>& ConnectedPolygonIDs = GetEdgeConnectedPolygons( EdgeID );
		if( ConnectedPolygonIDs.Num() == 0 )
		{
			// What does it mean if an edge has no connected polygons? For now we just skip it
			continue;
		}

		// Assume by default that the edge is soft - but as soon as any vertex instance belonging to a connected polygon
		// has a distinct normal from the others (within the given tolerance), we mark it as hard.
		// The exception is if an edge has exactly one connected polygon: in this case we automatically deem it a hard edge. 
		bool bEdgeIsHard = ( ConnectedPolygonIDs.Num() == 1 );

		// Examine vertices on each end of the edge, if we haven't yet identified it as 'hard'
		for( int32 VertexIndex = 0; !bEdgeIsHard && VertexIndex < 2; ++VertexIndex )
		{
			const FVertexID VertexID = GetEdgeVertex( EdgeID, VertexIndex );

			const int32 ReservedElements = 4;
			UniqueVertexInstanceIDs.Reset( ReservedElements );

			// Get a list of all vertex instances for this vertex which form part of any polygon connected to the edge
			for( const FVertexInstanceID VertexInstanceID : GetVertexVertexInstances( VertexID ) )
			{
				for( const FPolygonID PolygonID : GetVertexInstanceConnectedPolygons( VertexInstanceID ) )
				{
					if( ConnectedPolygonIDs.Contains( PolygonID ) )
					{
						UniqueVertexInstanceIDs.AddUnique( VertexInstanceID );
						break;
					}
				}
			}
			check( UniqueVertexInstanceIDs.Num() > 0 );

			// First unique vertex instance is used as a reference against which the others are compared.
			// (not a perfect approach: really the 'median' should be used as a reference)
			const FVector ReferenceNormal = VertexNormals[ UniqueVertexInstanceIDs[ 0 ] ];
			for( int32 Index = 1; Index < UniqueVertexInstanceIDs.Num(); ++Index )
			{
				if( !VertexNormals[ UniqueVertexInstanceIDs[ Index ] ].Equals( ReferenceNormal, Tolerance ) )
				{
					bEdgeIsHard = true;
					break;
				}
			}
		}

		EdgeHardnesses[ EdgeID ] = bEdgeIsHard;
	}
}


void FMeshDescription::DetermineUVSeamsFromUVs( const int32 UVIndex, const float Tolerance )
{
	const TVertexInstanceAttributesRef<FVector2D> VertexUVs = VertexInstanceAttributes().GetAttributesRef<FVector2D>( MeshAttribute::VertexInstance::TextureCoordinate );
	TEdgeAttributesRef<bool> EdgeUVSeams = EdgeAttributes().GetAttributesRef<bool>( MeshAttribute::Edge::IsUVSeam );

	// Holds unique vertex instance IDs for a given edge vertex
	// @todo: use TMemStackAllocator or similar to avoid expensive allocations
	TArray<FVertexInstanceID> UniqueVertexInstanceIDs;

	for( const FEdgeID EdgeID : Edges().GetElementIDs() )
	{
		// Get list of polygons connected to this edge
		const TArray<FPolygonID>& ConnectedPolygonIDs = GetEdgeConnectedPolygons( EdgeID );
		if( ConnectedPolygonIDs.Num() == 0 )
		{
			// What does it mean if an edge has no connected polygons? For now we just skip it
			continue;
		}

		// Assume by default that the edge is not a UV seam - but as soon as any vertex instance belonging to a connected polygon
		// has a distinct UV from the others (within the given tolerance), we mark it as a UV seam.
		bool bEdgeIsUVSeam = false;

		// Examine vertices on each end of the edge, if we haven't yet identified it as a UV seam
		for( int32 VertexIndex = 0; !bEdgeIsUVSeam && VertexIndex < 2; ++VertexIndex )
		{
			const FVertexID VertexID = GetEdgeVertex( EdgeID, VertexIndex );

			const int32 ReservedElements = 4;
			UniqueVertexInstanceIDs.Reset( ReservedElements );

			// Get a list of all vertex instances for this vertex which form part of any polygon connected to the edge
			for( const FVertexInstanceID VertexInstanceID : GetVertexVertexInstances( VertexID ) )
			{
				for( const FPolygonID PolygonID : GetVertexInstanceConnectedPolygons( VertexInstanceID ) )
				{
					if( ConnectedPolygonIDs.Contains( PolygonID ) )
					{
						UniqueVertexInstanceIDs.AddUnique( VertexInstanceID );
						break;
					}
				}
			}
			check( UniqueVertexInstanceIDs.Num() > 0 );

			// First unique vertex instance is used as a reference against which the others are compared.
			// (not a perfect approach: really the 'median' should be used as a reference)
			const FVector2D ReferenceUV = VertexUVs.Get( UniqueVertexInstanceIDs[ 0 ], UVIndex );
			for( int32 Index = 1; Index < UniqueVertexInstanceIDs.Num(); ++Index )
			{
				if( !VertexUVs.Get( UniqueVertexInstanceIDs[ Index ], UVIndex ).Equals( ReferenceUV, Tolerance ) )
				{
					bEdgeIsUVSeam = true;
					break;
				}
			}
		}

		EdgeUVSeams[ EdgeID ] = bEdgeIsUVSeam;
	}
}


void FMeshDescription::GetPolygonsInSameChartAsPolygon( const FPolygonID PolygonID, TArray<FPolygonID>& OutPolygonIDs )
{
	const TEdgeAttributesRef<bool> EdgeUVSeams = EdgeAttributes().GetAttributesRef<bool>( MeshAttribute::Edge::IsUVSeam );
	const int32 NumPolygons = Polygons().Num();

	// This holds the results - all polygon IDs which are in the same UV chart
	OutPolygonIDs.Reset( NumPolygons );

	// This holds all the polygons we need to check, and those we have already checked so we don't add duplicates
	// @todo: use TMemStackAllocator or similar to avoid expensive allocations
	TArray<FPolygonID> PolygonsToCheck;
	PolygonsToCheck.Reset( NumPolygons );

	// Add the initial polygon
	PolygonsToCheck.Add( PolygonID );

	int32 Index = 0;
	while( Index < PolygonsToCheck.Num() )
	{
		// Process the next polygon to be checked. If it's in this list, we already know it's one of the results. Now we have to check the neighbors.
		const FPolygonID PolygonToCheck = PolygonsToCheck[ Index ];
		OutPolygonIDs.Add( PolygonToCheck );
		Index++;

		// Iterate through edges of the polygon
		const TArray<FVertexInstanceID>& VertexInstanceIDs = GetPolygonPerimeterVertexInstances( PolygonToCheck );
		FVertexID LastVertexID = GetVertexInstanceVertex( VertexInstanceIDs.Last() );
		for( const FVertexInstanceID VertexInstanceID : VertexInstanceIDs )
		{
			const FVertexID VertexID = GetVertexInstanceVertex( VertexInstanceID );
			const FEdgeID EdgeID = GetVertexPairEdge( VertexID, LastVertexID );
			if( EdgeID != FEdgeID::Invalid && !EdgeUVSeams[ EdgeID ] )
			{
				// If it's a valid edge and not a UV seam, check its connected polygons
				const TArray<FPolygonID>& ConnectedPolygonIDs = GetEdgeConnectedPolygons( EdgeID );
				for( const FPolygonID ConnectedPolygonID : ConnectedPolygonIDs )
				{
					// Add polygons which aren't the one being checked, and haven't already been added to the list
					if( ConnectedPolygonID != PolygonToCheck && !PolygonsToCheck.Contains( ConnectedPolygonID ) )
					{
						PolygonsToCheck.Add( ConnectedPolygonID );
					}
				}
			}
			LastVertexID = VertexID;
		}
	}
}


void FMeshDescription::GetAllCharts( TArray<TArray<FPolygonID>>& OutCharts )
{
	// @todo: OutCharts: array of array doesn't seem like a really efficient data structure. Also templatize on allocator?

	const int32 NumPolygons = Polygons().Num();

	// Maintain a record of the polygons which have already been entered into a chart
	// @todo: use TMemStackAllocator or similar to avoid expensive allocations
	TSet<FPolygonID> ConsumedPolygons;
	ConsumedPolygons.Reserve( NumPolygons );

	for( const FPolygonID PolygonID : Polygons().GetElementIDs() )
	{
		if( !ConsumedPolygons.Contains( PolygonID ) )
		{
			OutCharts.Emplace();
			TArray<FPolygonID>& Chart = OutCharts.Last();
			GetPolygonsInSameChartAsPolygon( PolygonID, Chart );

			// Mark all polygons in the chart as 'consumed'. Note that the chart will also contain the initial polygon.
			for( const FPolygonID ChartPolygon : Chart )
			{
				ConsumedPolygons.Add( ChartPolygon );
			}
		}
	}
}


void FMeshDescription::ReversePolygonFacing(const FPolygonID PolygonID)
{
	//Build a reverse perimeter
	FMeshPolygon& Polygon = GetPolygon(PolygonID);
	for (int32 i = 0; i < Polygon.PerimeterContour.VertexInstanceIDs.Num() / 2; ++i)
	{
		Polygon.PerimeterContour.VertexInstanceIDs.Swap(i, Polygon.PerimeterContour.VertexInstanceIDs.Num() - i - 1);
	}
	
	//Triangulate the polygon since we reverse the indices
	ComputePolygonTriangulation(PolygonID, Polygon.Triangles);
}


void FMeshDescription::ReverseAllPolygonFacing()
{
	// Perform triangulation directly into mesh polygons
	for (const FPolygonID PolygonID : Polygons().GetElementIDs())
	{
		ReversePolygonFacing(PolygonID);
	}
}


#if WITH_EDITORONLY_DATA

void FMeshDescriptionBulkData::Serialize( FArchive& Ar, UObject* Owner )
{
	Ar.UsingCustomVersion( FEditorObjectVersion::GUID );

	if( Ar.IsTransacting() )
	{
		// If transacting, keep these members alive the other side of an undo, otherwise their values will get lost
		CustomVersions.Serialize( Ar );
		Ar << bBulkDataUpdated;
	}
	else
	{
		if( Ar.IsLoading() )
		{
			// If loading, take a copy of the package custom version container, so it can be applied when unpacking
			// MeshDescription from the bulk data.
			CustomVersions = Ar.GetCustomVersions();
		}
		else if( Ar.IsSaving() )
		{
			// If the bulk data hasn't been updated since this was loaded, there's a possibility that it has old versioning.
			// Explicitly load and resave the FMeshDescription so that its version is in sync with the FMeshDescriptionBulkData.
			if( !bBulkDataUpdated )
			{
				FMeshDescription MeshDescription;
				LoadMeshDescription( MeshDescription );
				SaveMeshDescription( MeshDescription );
			}
		}
	}

	BulkData.Serialize( Ar, Owner );

	if( Ar.IsLoading() && Ar.CustomVer( FEditorObjectVersion::GUID ) < FEditorObjectVersion::MeshDescriptionBulkDataGuid )
	{
		FPlatformMisc::CreateGuid( Guid );
	}
	else
	{
		Ar << Guid;
	}
}


void FMeshDescriptionBulkData::SaveMeshDescription( FMeshDescription& MeshDescription )
{
	BulkData.RemoveBulkData();

	if( !MeshDescription.IsEmpty() )
	{
		const bool bIsPersistent = true;
		FBulkDataWriter Ar( BulkData, bIsPersistent );
		Ar << MeshDescription;
	}

	FPlatformMisc::CreateGuid( Guid );

	// Mark the MeshDescriptionBulkData as having been updated.
	// This means we know that its version is up-to-date.
	bBulkDataUpdated = true;
}


void FMeshDescriptionBulkData::LoadMeshDescription( FMeshDescription& MeshDescription )
{
	MeshDescription.Empty();

	if( BulkData.GetElementCount() > 0 )
	{
		// Get a lock on the bulk data and read it into the mesh description
		{
			const bool bIsPersistent = true;
			FBulkDataReader Ar( BulkData, bIsPersistent );

			// Propagate the custom version information from the package to the bulk data, so that the MeshDescription
			// is serialized with the same versioning.
			Ar.SetCustomVersions( CustomVersions );
			Ar << MeshDescription;
		}
		// Unlock bulk data when we leave scope

		// Throw away the bulk data allocation as we don't need it now we have its contents as a FMeshDescription
		// @todo: revisit this
//		BulkData.UnloadBulkData();
	}
}


void FMeshDescriptionBulkData::Empty()
{
	BulkData.RemoveBulkData();
}


FString FMeshDescriptionBulkData::GetIdString() const
{
	return Guid.ToString();
}

#endif // #if WITH_EDITORONLY_DATA
