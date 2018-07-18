// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DatasmithScene.h"

#include "EngineUtils.h"
#include "UObject/EnterpriseObjectVersion.h"

#if WITH_EDITORONLY_DATA

enum
{
	DATASMITHSCENEBULKDATA_VER_INITIAL = 1, // Version 0 means we didn't have any bulk data
	DATASMITHSCENEBULKDATA_VER_CURRENT = DATASMITHSCENEBULKDATA_VER_INITIAL
};

#endif // #if WITH_EDITORONLY_DATA

void UDatasmithScene::Serialize( FArchive& Archive )
{
#if WITH_EDITORONLY_DATA
	if ( Archive.IsSaving() && !IsTemplate() )
	{
		BulkDataVersion = DATASMITHSCENEBULKDATA_VER_CURRENT; // Update BulkDataVersion to current version
	}
#endif // #if WITH_EDITORONLY_DATA

	Super::Serialize( Archive );

	Archive.UsingCustomVersion(FEnterpriseObjectVersion::GUID);

	// Serialize/Deserialize stripping flag to control serialization of bulk data
	bool bIsEditorDataIncluded = true;
	if (Archive.CustomVer(FEnterpriseObjectVersion::GUID) >= FEnterpriseObjectVersion::FixSerializationOfBulkAndExtraData)
	{
		FStripDataFlags StripFlags( Archive );
		bIsEditorDataIncluded = !StripFlags.IsEditorDataStripped();
	}

#if WITH_EDITORONLY_DATA
	if ( bIsEditorDataIncluded && BulkDataVersion >= DATASMITHSCENEBULKDATA_VER_INITIAL )
	{
		DatasmithSceneBulkData.Serialize( Archive, this );
	}
#endif // #if WITH_EDITORONLY_DATA
}
