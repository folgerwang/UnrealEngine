// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"

#include "DatasmithImportOptions.generated.h"

class FJsonObject;

UENUM()
enum class EDatasmithImportSearchPackagePolicy : uint8
{
	/** Search only in current package */
	Current UMETA(DisplayName = "Current", DisplayValue = "Current"),

	/** Search in all packages */
	All,
};

UENUM()
enum class EDatasmithImportAssetConflictPolicy : uint8
{
	/** Replace existing asset with new one */
	Replace,

	/** Update existing asset with new values */
	Update,

	/** Use existing asset instead of creating new one */
	Use,

	/** Skip new asset */
	Ignore,
};

UENUM()
enum class EDatasmithImportActorPolicy : uint8
{
	/** Import new actors, update and delete existing actors. Doesn't recreate actors that exist in the source both not in the destination. */
	Update,

	/** Same as update but recreates deleted actors so that the source and destination are the same. */
	Full,

	/** Skip importing a certain type of actors */
	Ignore,
};

UENUM()
enum class EDatasmithImportMaterialQuality : uint8
{
	UseNoFresnelCurves,

	UseSimplifierFresnelCurves,

	UseRealFresnelCurves,
};

UENUM()
enum class EDatasmithImportLightmapMin : uint8
{
	LIGHTMAP_16		UMETA(DisplayName = "16"),
	LIGHTMAP_32		UMETA(DisplayName = "32"),
	LIGHTMAP_64		UMETA(DisplayName = "64"),
	LIGHTMAP_128	UMETA(DisplayName = "128"),
	LIGHTMAP_256	UMETA(DisplayName = "256"),
	LIGHTMAP_512	UMETA(DisplayName = "512"),
};

UENUM()
enum class EDatasmithImportLightmapMax : uint8
{
	LIGHTMAP_64		UMETA(DisplayName = "64"),
	LIGHTMAP_128	UMETA(DisplayName = "128"),
	LIGHTMAP_256	UMETA(DisplayName = "256"),
	LIGHTMAP_512	UMETA(DisplayName = "512"),
	LIGHTMAP_1024	UMETA(DisplayName = "1024"),
	LIGHTMAP_2048	UMETA(DisplayName = "2048"),
	LIGHTMAP_4096	UMETA(DisplayName = "4096")
};

UENUM()
enum class EDatasmithImportScene : uint8
{
	NewLevel		UMETA(DisplayName = "Create New Level", ToolTip = "Create a new Level and spawn the actors after the import."),

	CurrentLevel	UMETA(DisplayName = "Merge to Current Level", ToolTip = "Use the current Level to spawn the actors after the import."),

	AssetsOnly		UMETA(DisplayName = "Assets Only", ToolTip = "Do not modify the Level after import. No actor will be created (including the Blueprint if requested by the ImportHierarchy"),
};

UENUM()
enum class EDatasmithImportHierarchy : uint8
{
	UseMultipleActors	UMETA(DisplayName = "One StaticMeshActor per Geometric Object", ToolTip = "Create an StaticMeshActor for every node in the hierarchy of the model."),

	UseSingleActor		UMETA(DisplayName = "Single StaticMeshActor with Components", ToolTip = "Create one root StaticMeshActor then one component for every node in the hierarchy of the model. Recommended to import udatasmith files."),

	UseOneBlueprint		UMETA(DisplayName = "Blueprint", ToolTip = "Create one root blueprint then one component for every node in the hierarchy of the model. Recommended to import CAD files."),
};

UENUM()
enum class EDatasmithCADStitchingTechnique : uint8
{
	StitchingNone = 0,
	StitchingHeal,
	StitchingSew,
};

USTRUCT(BlueprintType)
struct DATASMITHCONTENT_API FDatasmithAssetImportOptions
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName PackagePath;
};

USTRUCT(BlueprintType)
struct DATASMITHCONTENT_API FDatasmithStaticMeshImportOptions
{
	GENERATED_USTRUCT_BODY()

	FDatasmithStaticMeshImportOptions();

	/** Minimum resolution for auto-generated lightmap UVs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Lightmap)
	EDatasmithImportLightmapMin MinLightmapResolution;

	/** Maximum resolution for auto-generated lightmap UVs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Lightmap)
	EDatasmithImportLightmapMax MaxLightmapResolution;

	UPROPERTY(BlueprintReadWrite, Category = Lightmap, Transient)
	bool bGenerateLightmapUVs;

	UPROPERTY(BlueprintReadWrite, Category = Mesh, Transient)
	bool bRemoveDegenerates;

public:
	static int32 ConvertLightmapEnumToValue( EDatasmithImportLightmapMin EnumValue );
	static int32 ConvertLightmapEnumToValue( EDatasmithImportLightmapMax EnumValue );

	bool operator == (const FDatasmithStaticMeshImportOptions& Other) const
	{
		return (MinLightmapResolution == Other.MinLightmapResolution && MaxLightmapResolution == Other.MaxLightmapResolution && bRemoveDegenerates == Other.bRemoveDegenerates);
	}
};

USTRUCT(BlueprintType)
struct DATASMITHCONTENT_API FDatasmithReimportOptions
{
	GENERATED_BODY()

public:
	FDatasmithReimportOptions();

	/** Specifies whether or not to update Datasmith Scene Actors in the current Level */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SyncCurrentLevelActors", meta = (DisplayName = "Datasmith Scene Actors"))
	bool bUpdateActors;

	/** Specifies whether or not to add back Actors you've deleted from the current Level */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SyncCurrentLevelActors", meta = (DisplayName = "Re-Spawn Deleted Actors", EditCondition = "bUpdateActors"))
	bool bRespawnDeletedActors;
};

USTRUCT(BlueprintType)
struct DATASMITHCONTENT_API FDatasmithImportBaseOptions
{
	GENERATED_USTRUCT_BODY()

	FDatasmithImportBaseOptions();

	/** Specifies where to put the content */
	UPROPERTY(BlueprintReadWrite, Category = Import, Transient)
	EDatasmithImportScene SceneHandling; // Not displayed, not saved

	/** Specifies whether or not to import geometry */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = Process, meta = (DisplayName = "Geometry"))
	bool bIncludeGeometry;

	/** Specifies whether or not to import materials and textures */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = Process, meta = (DisplayName = "Materials & Textures"))
	bool bIncludeMaterial;

	/** Specifies whether or not to import lights */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = Process, meta = (DisplayName = "Lights"))
	bool bIncludeLight;

	/** Specifies whether or not to import cameras */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = Process, meta = (DisplayName = "Cameras"))
	bool bIncludeCamera;

	/** Specifies whether or not to import animations */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = Process, meta = (DisplayName = "Animations"))
	bool bIncludeAnimation;

	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = Process, meta = (ShowOnlyInnerProperties))
	FDatasmithAssetImportOptions AssetOptions;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = Process, meta = (ShowOnlyInnerProperties))
	FDatasmithStaticMeshImportOptions StaticMeshOptions;

	bool CanIncludeAnimation() const { return bIncludeGeometry || bIncludeCamera || bIncludeLight; }
};

USTRUCT(BlueprintType)
struct DATASMITHCONTENT_API FDatasmithTessellationOptions
{
	GENERATED_BODY()

	FDatasmithTessellationOptions()
		: ChordTolerance(0.2f)
		, MaxEdgeLength(0.0f)
		, NormalTolerance(20.0f)
		, StitchingTechnique(EDatasmithCADStitchingTechnique::StitchingSew)
	{

	}

	/**
	 * Maximum distance between any point on a triangle generated by the tessellation process and the actual surface.
	 * The lower the value the more triangles.
	 * Default value is 0.2.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Geometry & Tessellation Options", meta = (Units = cm, ToolTip = "Maximum distance between any generated triangle and the original surface. Smaller values make more triangles.", ClampMin = "0.0"))
	float ChordTolerance;

	/**
	 * Maximum length of edges of triangles generated by the tessellation process.
	 * The length is in scene/model unit. The smaller the more triangles are generated.
	 * Value of 0 means no constraint on length of edges
	 * Default value is 0.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Geometry & Tessellation Options", meta = (Units = cm, DisplayName = "Max Edge Length", ToolTip = "Maximum length of any edge in the generated triangles. Smaller values make more triangles.", ClampMin = "0.0"))
	float MaxEdgeLength;

	/**
	 * Maximum angle between the normal of two triangles generated by the tessellation process.
	 * The angle is expressed in degree. The smaller the more triangles are generated.
	 * Default value is 20 degrees.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Geometry & Tessellation Options", meta = (Units = deg, ToolTip = "Maximum angle between adjacent triangles. Smaller values make more triangles.", ClampMin = "0.0", ClampMax = "90.0"))
	float NormalTolerance;


	/**
	 * Stitching technique applied on neighbouring surfaces before tessellation.
	 * None : No stitching applied. This is the default.
	 * Sewing : Connects surfaces which physically share a boundary but not topologically within a set of objects.
	 *          This technique can modify the structure of the model by removing and adding objects.  
	 * Healing : Connects surfaces which physically share a boundary but not topologically within an object.
	 * The techniques are using the chord tolerance to determine if two surfaces should be stitched.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Geometry & Tessellation Options", meta = (ToolTip = "Stitching technique applied on model before tessellation. Sewing could impact number of objects."))
	EDatasmithCADStitchingTechnique StitchingTechnique;

public:
	bool operator == (const FDatasmithTessellationOptions& Other) const
	{
		return (FMath::IsNearlyEqual(ChordTolerance, Other.ChordTolerance) && FMath::IsNearlyEqual(MaxEdgeLength, Other.MaxEdgeLength) && FMath::IsNearlyEqual(NormalTolerance, Other.NormalTolerance) && StitchingTechnique == Other.StitchingTechnique);
	}
};

UCLASS(config = EditorPerProjectUserSettings, HideCategories = ("NotVisible"))
class DATASMITHCONTENT_API UDatasmithImportOptions : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** Specifies where to search for assets */
	UPROPERTY(Transient, AdvancedDisplay)
	EDatasmithImportSearchPackagePolicy SearchPackagePolicy; // Not displayed. Kept for future use

	/** Specifies what to do when material conflicts */
	UPROPERTY(Transient, AdvancedDisplay)
	EDatasmithImportAssetConflictPolicy MaterialConflictPolicy; // Not displayed. Kept for future use

	/** Specifies what to do when texture conflicts */
	UPROPERTY(Transient, AdvancedDisplay)
	EDatasmithImportAssetConflictPolicy TextureConflictPolicy; // Not displayed. Kept for future use

	/** Specifies what to do when actor conflicts */
	UPROPERTY(Transient, AdvancedDisplay)
	EDatasmithImportActorPolicy StaticMeshActorImportPolicy; // Not displayed. Kept for future use

	/** Specifies what to do when light conflicts */
	UPROPERTY(Transient, AdvancedDisplay)
	EDatasmithImportActorPolicy LightImportPolicy; // Not displayed. Kept for future use

	/** Specifies what to do when material conflicts */
	UPROPERTY(Transient, AdvancedDisplay)
	EDatasmithImportActorPolicy CameraImportPolicy; // Not displayed. Kept for future use

	/** Specifies what to do when actor conflicts */
	UPROPERTY(Transient, AdvancedDisplay)
	EDatasmithImportActorPolicy OtherActorImportPolicy; // Not displayed. Kept for future use

	/** Specifies what to do when material conflicts */
	UPROPERTY(Transient, AdvancedDisplay)
	EDatasmithImportMaterialQuality MaterialQuality; // Not displayed. Kept for future use

	/** Specifies how to import the model's hierarchy */
	EDatasmithImportHierarchy HierarchyHandling;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Options", meta = (ShowOnlyInnerProperties))
	FDatasmithImportBaseOptions BaseOptions;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "NotVisible", meta = (ShowOnlyInnerProperties))
	FDatasmithTessellationOptions TessellationOptions;

	/** Options specific to the reimport process */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "NotVisible", meta = (ShowOnlyInnerProperties))
	FDatasmithReimportOptions ReimportOptions;

	/** Name of the imported file without its path */
	UPROPERTY(BlueprintReadWrite, Category = "NotVisible")
	FString FileName;
	
	/** Full path of the imported file */
	UPROPERTY(BlueprintReadWrite, Category = "NotVisible")
	FString FilePath;
	
	/** Whether to use or not the same options when loading multiple files. Default false */
	bool bUseSameOptions;

	void UpdateNotDisplayedConfig( bool bIsAReimport );

	//~ UObject interface
#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif //WITH_EDITOR
	//~ End UObject interface
};
