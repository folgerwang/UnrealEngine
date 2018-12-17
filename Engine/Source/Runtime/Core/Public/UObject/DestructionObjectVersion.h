// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

// Custom serialization version for changes made in Dev-Destruction stream
struct CORE_API FDestructionObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,

		// Added timestamped caches for geometry component to handle transform sampling instead of per-frame
		AddedTimestampedGeometryComponentCache,

		// Added functionality to strip unnecessary data from geometry collection caches
		AddedCacheDataReduction,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FDestructionObjectVersion() {}
};
