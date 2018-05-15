// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// @todo mesh description: for now, these attributes are hardcoded in the MeshDescription module.
// It would be better if these were defined in separate modules, one per mesh type which uses MeshDescription.
// This would give an extensible and modular structure whereby a mesh type defines its valid attributes and adapters.
namespace MeshAttribute
{
	namespace Vertex
	{
		extern MESHDESCRIPTION_API const FName Position;
		extern MESHDESCRIPTION_API const FName CornerSharpness;
	}

	namespace VertexInstance
	{
		extern MESHDESCRIPTION_API const FName TextureCoordinate;
		extern MESHDESCRIPTION_API const FName Normal;
		extern MESHDESCRIPTION_API const FName Tangent;
		extern MESHDESCRIPTION_API const FName BinormalSign;
		extern MESHDESCRIPTION_API const FName Color;
	}

	namespace Edge
	{
		extern MESHDESCRIPTION_API const FName IsHard;
		extern MESHDESCRIPTION_API const FName IsUVSeam;
		extern MESHDESCRIPTION_API const FName CreaseSharpness;
	}

	namespace Polygon
	{
		extern MESHDESCRIPTION_API const FName Normal;
		extern MESHDESCRIPTION_API const FName Tangent;
		extern MESHDESCRIPTION_API const FName Binormal;
		extern MESHDESCRIPTION_API const FName Center;
	}

	namespace PolygonGroup
	{
		extern MESHDESCRIPTION_API const FName ImportedMaterialSlotName;
		extern MESHDESCRIPTION_API const FName EnableCollision;
		extern MESHDESCRIPTION_API const FName CastShadow;
	}
}
