// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DatasmithScene.h"

#if WITH_EDITORONLY_DATA

enum
{
	DATASMITHSCENEBULKDATA_VER_INITIAL = 1, // Version 0 means we didn't have any bulk data
	DATASMITHSCENEBULKDATA_VER_CURRENT = DATASMITHSCENEBULKDATA_VER_INITIAL
};

void UDatasmithScene::Serialize( FArchive& Archive )
{
	if ( Archive.IsSaving() && !IsTemplate() )
	{
		BulkDataVersion = DATASMITHSCENEBULKDATA_VER_CURRENT; // Update BulkDataVersion to current version
	}

	Super::Serialize( Archive );

	if ( BulkDataVersion >= DATASMITHSCENEBULKDATA_VER_INITIAL )
	{
		DatasmithSceneBulkData.Serialize( Archive, this );
	}
}

#endif // #if WITH_EDITORONLY_DATA
