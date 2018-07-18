// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MeshAttributes.h"

namespace MeshAttribute
{
	const FName Vertex::Position( "Position ");
	const FName Vertex::CornerSharpness( "CornerSharpness" );

	const FName VertexInstance::TextureCoordinate( "TextureCoordinate" );
	const FName VertexInstance::Normal( "Normal" );
	const FName VertexInstance::Tangent( "Tangent" );
	const FName VertexInstance::BinormalSign( "BinormalSign" );
	const FName VertexInstance::Color( "Color" );

	const FName Edge::IsHard( "IsHard" );
	const FName Edge::IsUVSeam( "IsUVSeam" );
	const FName Edge::CreaseSharpness( "CreaseSharpness" );

	const FName Polygon::Normal( "Normal" );
	const FName Polygon::Tangent( "Tangent" );
	const FName Polygon::Binormal( "Binormal" );
	const FName Polygon::Center( "Center" );

	const FName PolygonGroup::ImportedMaterialSlotName( "ImportedMaterialSlotName" );
	const FName PolygonGroup::EnableCollision( "EnableCollision" );
	const FName PolygonGroup::CastShadow( "CastShadow" );
}

