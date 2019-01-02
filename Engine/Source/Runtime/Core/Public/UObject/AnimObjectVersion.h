// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

// Custom serialization version for changes made in Dev-Anim stream
struct CORE_API FAnimObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded,

		// Reworked how anim blueprint root nodes are recovered
		LinkTimeAnimBlueprintRootDiscovery,

		// Cached marker sync names on skeleton for editor
		StoreMarkerNamesOnSkeleton,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FAnimObjectVersion() {}
};
