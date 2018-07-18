// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/AssetUserData.h"
#include "Templates/SubclassOf.h"

#include "DatasmithAssetUserData.generated.h"

/** Asset user data that can be used with Datasmith on Actors and other objects  */
UCLASS(BlueprintType, meta = (ScriptName = "DatasmithUserData", DisplayName = "Datasmith User Data"))
class DATASMITHCONTENT_API UDatasmithAssetUserData : public UAssetUserData
{
	GENERATED_BODY()

public:

	// Meta-data are available at runtime in game, i.e. used in blueprint to display build-boarded information
	typedef TMap<FName, FString> FMetaDataContainer;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Datasmith User Data", meta = (ScriptName = "Metadata", DisplayName = "Metadata"))
	TMap<FName, FString> MetaData;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TMap< TSubclassOf< class UDatasmithObjectTemplate >, UDatasmithObjectTemplate* > ObjectTemplates;
#endif
};
