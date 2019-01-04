// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

// Custom serialization version for all packages containing EditableMesh asset types
struct FEditableMeshCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the module
		BeforeCustomVersionWasAdded = 0,

		// Added subdivision level count and texture coordinate count to UEditableMesh format
		TextureCoordinateAndSubdivisionCounts,

		// SparseArrays now have custom serialization, preserving the gaps
		CustomSparseArraySerialization,

		// Added vertex color support
		WithVertexColors,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

	FEditableMeshCustomVersion() = delete;
};
