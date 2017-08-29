// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshDescription.h"


UMeshDescription::UMeshDescription()
{
	// Add basic vertex attributes
	VertexAttributes().RegisterAttribute<FVector>(UEditableMeshAttribute::VertexPosition());
	VertexAttributes().RegisterAttribute<float>(UEditableMeshAttribute::VertexCornerSharpness());

	// Add basic vertex instance attributes
	VertexInstanceAttributes().RegisterAttribute<FVector2D>(UEditableMeshAttribute::VertexTextureCoordinate(), 2 );
	VertexInstanceAttributes().RegisterAttribute<FVector>(UEditableMeshAttribute::VertexNormal());
	VertexInstanceAttributes().RegisterAttribute<FVector>(UEditableMeshAttribute::VertexTangent());
	VertexInstanceAttributes().RegisterAttribute<float>(UEditableMeshAttribute::VertexBinormalSign());
	VertexInstanceAttributes().RegisterAttribute<FVector4>(UEditableMeshAttribute::VertexColor());

	// Add basic edge attributes
	EdgeAttributes().RegisterAttribute<bool>(UEditableMeshAttribute::EdgeIsHard());
	EdgeAttributes().RegisterAttribute<float>(UEditableMeshAttribute::EdgeCreaseSharpness());

	// Add basic polygon attributes
	PolygonAttributes().RegisterAttribute<FVector>(UEditableMeshAttribute::PolygonNormal());
	PolygonAttributes().RegisterAttribute<FVector>( "PolygonTangent" );
	PolygonAttributes().RegisterAttribute<FVector>( "PolygonBinormal" );
	PolygonAttributes().RegisterAttribute<FVector>(UEditableMeshAttribute::PolygonCenter());

	// Add basic polygon group attributes
	PolygonGroupAttributes().RegisterAttribute<FStringAssetReference>( "MaterialAsset" );
	PolygonGroupAttributes().RegisterAttribute<FName>( "MaterialSlotName" );
	PolygonGroupAttributes().RegisterAttribute<FName>( "ImportedMaterialSlotName" );
	PolygonGroupAttributes().RegisterAttribute<bool>( "EnableCollision" );
	PolygonGroupAttributes().RegisterAttribute<bool>( "CastShadow" );
}


void UMeshDescription::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar << VertexArray;
	Ar << VertexInstanceArray;
	Ar << EdgeArray;
	Ar << PolygonArray;
	Ar << PolygonGroupArray;

	Ar << VertexAttributesSet;
	Ar << VertexInstanceAttributesSet;
	Ar << EdgeAttributesSet;
	Ar << PolygonAttributesSet;
	Ar << PolygonGroupAttributesSet;
}


void UMeshDescription::PostLoad()
{
}


void UMeshDescription::Compact( FElementIDRemappings& OutRemappings )
{
	VertexArray.Compact( OutRemappings.NewVertexIndexLookup );
	VertexInstanceArray.Compact( OutRemappings.NewVertexInstanceIndexLookup );
	EdgeArray.Compact( OutRemappings.NewEdgeIndexLookup );
	PolygonArray.Compact( OutRemappings.NewPolygonIndexLookup );
	PolygonGroupArray.Compact( OutRemappings.NewPolygonGroupIndexLookup );

	// @todo mesh description: compact attribute arrays according to these remappings
	//

	FixUpElementIDs( OutRemappings );
}


void UMeshDescription::Remap( const FElementIDRemappings& Remappings )
{
	VertexArray.Remap( Remappings.NewVertexIndexLookup );
	VertexInstanceArray.Remap( Remappings.NewVertexInstanceIndexLookup );
	EdgeArray.Remap( Remappings.NewEdgeIndexLookup );
	PolygonArray.Remap( Remappings.NewPolygonIndexLookup );
	PolygonGroupArray.Remap( Remappings.NewPolygonGroupIndexLookup );

	// @todo mesh description: remap attribute arrays according to these remappings
	//

	FixUpElementIDs( Remappings );
}


void UMeshDescription::FixUpElementIDs( const FElementIDRemappings& Remappings )
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

		for( FMeshPolygonContour& HoleContour : Polygon.HoleContours )
		{
			for( FVertexInstanceID& VertexInstanceID : HoleContour.VertexInstanceIDs )
			{
				VertexInstanceID = Remappings.GetRemappedVertexInstanceID( VertexInstanceID );
			}
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
