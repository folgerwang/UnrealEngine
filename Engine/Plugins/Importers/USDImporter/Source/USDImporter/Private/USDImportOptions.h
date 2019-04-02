// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Factories/MaterialImportHelpers.h"
#include "USDImportOptions.generated.h"


UENUM(BlueprintType)
enum class EExistingActorPolicy : uint8
{
	/** Replaces existing actors with new ones */
	Replace,
	/** Update transforms on existing actors but do not replace actor the actor class or any other data */
	UpdateTransform,
	/** Ignore any existing actor with the same name */
	Ignore,

};

UENUM(BlueprintType)
enum class EExistingAssetPolicy : uint8
{
	/** Reimports existing assets */
	Reimport,

	/** Ignores existing assets and doesnt reimport them */
	Ignore,
};

UENUM(BlueprintType)
enum class EUsdMeshImportType : uint8
{
	StaticMesh,
};

UCLASS(config = EditorPerProjectUserSettings)
class UUSDImportOptions : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	/** Defines what should happen with existing actors */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category=Mesh)
	EUsdMeshImportType MeshImportType;

	/**
	 * If checked, To enforce unique asset paths, all assets will be created in directories that match with their prim path 
	 * e.g a USD path /root/myassets/myprim_mesh will generate the path in the game directory "/Game/myassets/" with a mesh asset called "myprim_mesh" within that path.
	 */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category=Mesh)
	bool bGenerateUniquePathPerUSDPrim;

	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category=Mesh)
	bool bApplyWorldTransformToGeometry;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, config, Category="Mesh|Materials")
	EMaterialSearchLocation MaterialSearchLocation;

	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category = Mesh)
	float Scale;

public:
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
};

UCLASS(config = EditorPerProjectUserSettings)
class UUSDSceneImportOptions : public UUSDImportOptions
{
	GENERATED_UCLASS_BODY()
public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif
public:
	/** If checked, all actors generated will have a world space transform and will not have any attachment hierarchy */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category=General)
	bool bFlattenHierarchy;

	/** Defines what should happen with existing actors */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category=General)
	EExistingActorPolicy ExistingActorPolicy;

	/** Whether or not to import custom properties and set their unreal equivalent on spawned actors */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category = General)
	bool bImportProperties;

	/** Whether or not to import mesh geometry or to just spawn actors using existing meshes */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category=Mesh)
	bool bImportMeshes;

	/** The path where new assets are imported */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category=Mesh, meta=(ContentDir, EditCondition = bImportMeshes))
	FDirectoryPath PathForAssets;
	 
	/** What should happen with existing assets */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category=Mesh, meta = (EditCondition=bImportMeshes))
	EExistingAssetPolicy ExistingAssetPolicy;

	/** 
	 * This setting determines what to do if more than one USD prim is found with the same name.  If this setting is true a unique name will be generated and a unique asset will be imported 
	 * If this is false, the first asset found is generated. Assets will be reused when spawning actors into the world.
	 */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category=Mesh, meta=(EditCondition=bImportMeshes))
	bool bGenerateUniqueMeshes;
};

UCLASS()
class UUSDBatchImportOptionsSubTask : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	/** Path in the USD stage to import from */
	UPROPERTY(BlueprintReadWrite, Category = Mesh)
	FString SourcePath;

	/** Path to import asset as */
	UPROPERTY(BlueprintReadWrite, Category = Mesh)
	FString DestPath;

	UPROPERTY(BlueprintReadWrite, Category = Mesh)
	FString ErrorMessage;
};

UCLASS(config = EditorPerProjectUserSettings)
class UUSDBatchImportOptions : public UUSDImportOptions
{
	GENERATED_UCLASS_BODY()
public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif
public:
		
	/** Whether or not to import mesh geometry or to just spawn actors using existing meshes */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category=Mesh)
	bool bImportMeshes;

	/** The path where new assets are imported */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category=Mesh, meta=(ContentDir, EditCondition = bImportMeshes))
	FDirectoryPath PathForAssets;
	 
	/** What should happen with existing assets */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category=Mesh, meta = (EditCondition=bImportMeshes))
	EExistingAssetPolicy ExistingAssetPolicy;

	/** 
	 * This setting determines what to do if more than one USD prim is found with the same name.  If this setting is true a unique name will be generated and a unique asset will be imported 
	 * If this is false, the first asset found is generated. Assets will be reused when spawning actors into the world.
	 */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category=Mesh, meta=(EditCondition=bImportMeshes))
	bool bGenerateUniqueMeshes;

	UPROPERTY(BlueprintReadWrite, Category = Mesh)
	TArray<UUSDBatchImportOptionsSubTask*> SubTasks;
};
