// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MeshTypes.h"

DEFINE_LOG_CATEGORY( LogMeshDescription );


const FElementID FElementID::Invalid( TNumericLimits<uint32>::Max() );
const FVertexID FVertexID::Invalid( TNumericLimits<uint32>::Max() );
const FVertexInstanceID FVertexInstanceID::Invalid( TNumericLimits<uint32>::Max() );
const FEdgeID FEdgeID::Invalid( TNumericLimits<uint32>::Max() );
const FPolygonGroupID FPolygonGroupID::Invalid( TNumericLimits<uint32>::Max() );
const FPolygonID FPolygonID::Invalid( TNumericLimits<uint32>::Max() );

#if 0
const FName UEditableMeshAttribute::VertexPositionName( "VertexPosition" );
const FName UEditableMeshAttribute::VertexCornerSharpnessName( "VertexCornerSharpness" );
const FName UEditableMeshAttribute::VertexNormalName( "VertexNormal" );
const FName UEditableMeshAttribute::VertexTangentName( "VertexTangent" );
const FName UEditableMeshAttribute::VertexBinormalSignName( "VertexBinormalSign" );
const FName UEditableMeshAttribute::VertexTextureCoordinateName( "VertexTextureCoordinate" );
const FName UEditableMeshAttribute::VertexColorName( "VertexColor" );
const FName UEditableMeshAttribute::EdgeIsHardName( "EdgeIsHard" );
const FName UEditableMeshAttribute::EdgeCreaseSharpnessName( "EdgeCreaseSharpness" );
const FName UEditableMeshAttribute::PolygonNormalName( "PolygonNormal" );
const FName UEditableMeshAttribute::PolygonCenterName( "PolygonCenter" );
#endif
