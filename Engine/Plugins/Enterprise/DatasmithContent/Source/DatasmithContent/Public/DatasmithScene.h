// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Serialization/BulkData.h"

#include "DatasmithScene.generated.h"

UCLASS()
class DATASMITHCONTENT_API UDatasmithScene : public UObject
{
	GENERATED_BODY()

public:

#if WITH_EDITORONLY_DATA

	/** Importing data and options used for this Datasmith scene */
	UPROPERTY(EditAnywhere, Instanced, Category=ImportSettings)
	class UDatasmithSceneImportData* AssetImportData;

	UPROPERTY()
	int32 BulkDataVersion; // Need an external version number because loading of the bulk data is handled externally

	FByteBulkData DatasmithSceneBulkData;
#endif // #if WITH_EDITORONLY_DATA

public:
	virtual void Serialize( FArchive& Archive ) override;
};
