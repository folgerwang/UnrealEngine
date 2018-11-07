// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

// Custom serialization version for changes made in Dev-Enterprise stream
struct CORE_API FEnterpriseObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,

		// Conditional serialization of bulk (UDatasmithScene) and extra (UDatasmithStaticMeshCADImportData) data 
		FixSerializationOfBulkAndExtraData,

		// Extensibility updates for bookmarks
		BookmarkExtensibilityUpgrade,

		// Update FMediaFrameworkCaptureCameraViewportCameraOutputInfo with LazyObjectPtr
		MediaFrameworkUserDataLazyObject,
		
		// Live Live timecode synchronization updates
		LiveLinkTimeSynchronization,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

	FEnterpriseObjectVersion() = delete;
};
