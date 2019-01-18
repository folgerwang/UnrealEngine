// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithImportOptions.h"
#include "Misc/Guid.h"
#include "Serialization/BulkData.h"

#include "DatasmithScene.generated.h"

class ULevelSequence;
class UMaterialInterface;
class UStaticMesh;
class UTexture;
class UWorld;

USTRUCT()
struct DATASMITHCONTENT_API FDatasmithSceneInput
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid GUID;

	UPROPERTY(EditAnywhere, Category = "Options", meta = (ShowOnlyInnerProperties))
	UDatasmithImportOptions* Options;
};

UCLASS()
class DATASMITHCONTENT_API UDatasmithScene : public UObject
{
	GENERATED_BODY()

public:
	UDatasmithScene();

	virtual ~UDatasmithScene();

#if WITH_EDITORONLY_DATA
	/** Array of data to import with associated options used by this Datasmith scene */
	// TODO: See how to conciliate AssetImportData and FDatasmithSceneInput
	UPROPERTY()
	TArray<FDatasmithSceneInput> Imports;

	/** Pointer to data preparation pipeline blueprint used to process input data */
	UPROPERTY()
	class UBlueprint* DataPrepRecipeBP;

	/** Importing data and options used for this Datasmith scene */
	UPROPERTY(EditAnywhere, Instanced, Category=ImportSettings)
	class UDatasmithSceneImportData* AssetImportData;

	UPROPERTY()
	int32 BulkDataVersion; // Need an external version number because loading of the bulk data is handled externally

	FByteBulkData DatasmithSceneBulkData;

	/** Map of all the static meshes related to this Datasmith Scene */
	UPROPERTY(VisibleAnywhere, Category="Datasmith", AdvancedDisplay)
	TMap< FName, TSoftObjectPtr< UStaticMesh > > StaticMeshes;

	/** Map of all the textures related to this Datasmith Scene */
	UPROPERTY(VisibleAnywhere, Category="Datasmith", AdvancedDisplay)
	TMap< FName, TSoftObjectPtr< UTexture > > Textures;

	/** Map of all the materials related to this Datasmith Scene */
	UPROPERTY(VisibleAnywhere, Category="Datasmith", AdvancedDisplay)
	TMap< FName, TSoftObjectPtr< UMaterialInterface > > Materials;

	/** Map of all the level sequences related to this Datasmith Scene */
	UPROPERTY(VisibleAnywhere, Category="Datasmith", AdvancedDisplay)
	TMap< FName, TSoftObjectPtr< ULevelSequence > > LevelSequences;
#endif // #if WITH_EDITORONLY_DATA

	/** Register the DatasmithScene to the PreWorldRename callback as needed*/
	void RegisterPreWorldRenameCallback();

#if WITH_EDITOR
private:
	/** Called before a world is renamed */
	void OnPreWorldRename(UWorld* World, const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags, bool& bShouldFailRename);

	bool bPreWorldRenameCallbackRegistered;
#endif

public:
	virtual void Serialize( FArchive& Archive ) override;
};
