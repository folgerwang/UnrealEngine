// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Texture.h"
#include "MeshFractureSettings.generated.h"

//
/** Mesh fracture pattern modes */
UENUM()
enum class EMeshFractureMode : uint8
{
	/** Standard Voronoi */
	Uniform UMETA(DisplayName = "Uniform Voronoi"),

	/** Clustered Voronoi */
	Clustered UMETA(DisplayName = "Clustered Voronoi"),

	/** Radial Voronoi */
	Radial UMETA(DisplayName = "Radial Voronoi"),

	/** Slicing algorithm - non-voronoi */
	Slicing UMETA(DisplayName = "Slicing"),

	/** Simple Plane Slice - non-voronoi */
	PlaneCut UMETA(DisplayName = "Plane Cut"),

#ifdef CUTOUT_ENABLED
	/** Bitmap Cutout Slicing algorithm - non-voronoi */
	Cutout UMETA(DisplayName = "Bitmap Cutout"),

	/** Special case Brick Cutout Slicing algorithm - non-voronoi */
	Brick UMETA(DisplayName = "Brick"),
#endif
};

//
/** Mesh fracture levels - lazy way to get a drop down list from UI */
UENUM()
enum class EMeshFractureLevel : uint8
{
	AllLevels UMETA(DisplayName = "All Levels"),

	Level0 UMETA(DisplayName = "Level0"),

	Level1 UMETA(DisplayName = "Level1"),

	Level2 UMETA(DisplayName = "Level2"),

	Level3 UMETA(DisplayName = "Level3"),

	Level4 UMETA(DisplayName = "Level4"),

	Level5 UMETA(DisplayName = "Level5"),

	Level6 UMETA(DisplayName = "Level6"),
};

//
/** Exploded View Mode */
UENUM()
enum class EExplodedViewMode : uint8
{
	/** Levels split at different times */
	SplitLevels UMETA(DisplayName = "As Levels"),

	/** All levels split at the same time linearly */
	Linear UMETA(DisplayName = "Linear")
};

//
/** Colorize View Mode */
UENUM()
enum class EFractureColorizeMode : uint8
{
	/** Fracture colorization turned off */
	ColorOff UMETA(DisplayName = "Colors off"),

	/** Random colored fracture pieces */
	ColorRandom UMETA(DisplayName = "Random Color"),

	/** Colored based on bone hierarchy level */
	ColorLevels UMETA(DisplayName = "Color Levels")
};

//
/** Selection Mode */
UENUM()
enum class EFractureSelectionMode : uint8
{
	ChunkSelect UMETA(DisplayName = "Chunk Select"),

	ClusterSelect UMETA(DisplayName = "Cluster Select"),

	LevelSelect UMETA(DisplayName = "Level Select")

};

UENUM()
enum class EViewResetType : uint8
{
	RESET_ALL,
	RESET_TRANSFORMS
};

UCLASS(config = EditorPerProjectUserSettings)
class BLASTAUTHORING_API UCommonFractureSettings
	: public UObject
{
	GENERATED_BODY()
public:
	UCommonFractureSettings();

	/** In Editor Fracture Viewing mode */
	UPROPERTY(EditAnywhere, Category = CommonFracture)
		EMeshFractureLevel ViewMode;

	/** Enable bone color mode */
	UPROPERTY(EditAnywhere, Category = CommonFracture)
		bool ShowBoneColors;

	/** Delete Source mesh when fracturing & generating a Geometry Collection */
	UPROPERTY(EditAnywhere, Category = CommonFracture)
		bool DeleteSourceMesh;

	/** Fracture mode */
	UPROPERTY(EditAnywhere, Category = CommonFracture)
		EMeshFractureMode FractureMode;

	/** Cleanup mesh option */
	UPROPERTY(EditAnywhere, Category = CommonFracture)
		bool RemoveIslands;

	/** Random number generator seed for repeatability */
	UPROPERTY(EditAnywhere, Category = CommonFracture, meta = (DisplayName = "Random Seed", UIMin = "2", UIMax = "1000", ClampMin = "2", ClampMax = "1000"))
		int RandomSeed;

	int8 GetFractureLevelNumber()
	{
		return static_cast<int8>(ViewMode) - static_cast<int8>(EMeshFractureLevel::Level0);
	}
};


UCLASS(config = EditorPerProjectUserSettings)
class BLASTAUTHORING_API UUniformFractureSettings
	: public UObject
{
	GENERATED_BODY()
public:
	UUniformFractureSettings();

	/** Number of Voronoi sites - Uniform Voronoi Method */
	UPROPERTY(EditAnywhere, Category = UniformVoronoi, meta = (DisplayName = "# Voronoi Sites", UIMin = "2", UIMax = "5000", ClampMin = "2", ClampMax = "5000"))
		int NumberVoronoiSites;
};

UCLASS(config = EditorPerProjectUserSettings)
class BLASTAUTHORING_API UClusterFractureSettings
	: public UObject
{
	GENERATED_BODY()
public:
	UClusterFractureSettings(); 

	/** Number of Clusters - Clustered Voronoi Method */
	UPROPERTY(EditAnywhere, Category = ClusteredVoronoi)
		float NumberClusters;

	/** Sites per of Clusters - Clustered Voronoi Method */
	UPROPERTY(EditAnywhere, Category = ClusteredVoronoi)
		float SitesPerCluster;

	/** Clusters Radius - Clustered Voronoi Method */
	UPROPERTY(EditAnywhere, Category = ClusteredVoronoi)
		float ClusterRadius;
};

UCLASS(config = EditorPerProjectUserSettings)
class BLASTAUTHORING_API URadialFractureSettings
	: public UObject
{
	GENERATED_BODY()
public:
	URadialFractureSettings();

	// Radial Voronoi
	/** Center of generated pattern */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi)
		FVector Center;

	/** Normal to plane in which sites are generated */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi)
		FVector Normal;

	/** Pattern radius */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi)
		float Radius;

	/** Number of angular steps */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi)
		int AngularSteps;

	/** Number of radial steps */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi)
		int RadialSteps;

	/** Angle offset at each radial step */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi)
		float AngleOffset;

	/** Randomness of sites distribution */
	UPROPERTY(EditAnywhere, Category = RadialVoronoi)
		float Variability;

};

UCLASS(config = EditorPerProjectUserSettings)
class BLASTAUTHORING_API USlicingFractureSettings
	: public UObject
{
	GENERATED_BODY()
public:
	USlicingFractureSettings();

	/** Num Slices X axis - Slicing Method */
	UPROPERTY(EditAnywhere, Category = Slicing)
		int SlicesX;

	/** Num Slices Y axis - Slicing Method */
	UPROPERTY(EditAnywhere, Category = Slicing)
		int SlicesY;

	/** Num Slices Z axis - Slicing Method */
	UPROPERTY(EditAnywhere, Category = Slicing)
		int SlicesZ;

	/** Slicing Angle Variation - Slicing Method [0..1] */
	UPROPERTY(EditAnywhere, Category = Slicing)
		float SliceAngleVariation;

	/** Slicing Offset Variation - Slicing Method [0..1] */
	UPROPERTY(EditAnywhere, Category = Slicing)
		float SliceOffsetVariation;
};


struct BLASTAUTHORING_API UPlaneCut 
{
public:
	UPlaneCut();

	/** Position on cutting plane */
	FVector Position;

	/** Normal of cutting plane */
	FVector Normal;
};


UCLASS(config = EditorPerProjectUserSettings)
class BLASTAUTHORING_API UPlaneCutFractureSettings
	: public UObject
{
	GENERATED_BODY()
public:
	UPlaneCutFractureSettings();

	/** Multiple plane cuts */
	TArray<UPlaneCut> PlaneCuts;

};


// #todo: Noise configuration
UCLASS(config = EditorPerProjectUserSettings)
class BLASTAUTHORING_API UCutoutFractureSettings
	: public UObject
{
	GENERATED_BODY()
public:
	UCutoutFractureSettings();

	/**
	Transform for initial pattern position and orientation.
	By default 2d pattern lies in XY plane (Y is up) the center of pattern is (0, 0)
	*/
	UPROPERTY(EditAnywhere, Category = Cutout)
	FTransform Transform;

	/**
	Scale for pattern. Unscaled pattern has size (1, 1).
	For negative scale pattern will be placed at the center of chunk and scaled with max distance between points of its AABB
	*/
	UPROPERTY(EditAnywhere, Category = Cutout)
	FVector2D Scale;

	/**
	If relative transform is set - position will be displacement vector from chunk's center. Otherwise from global origin.
	*/
	UPROPERTY(EditAnywhere, Category = Cutout)
	bool IsRelativeTransform;

	/**
	The pixel distance at which neighboring cutout vertices and segments may be snapped into alignment. By default set it to 1
	*/
	UPROPERTY(EditAnywhere, Category = Cutout)
	float SnapThreshold;

	/**
	Reduce the number of vertices on curve until segmentation error is smaller than this value. By default set it to 0.001
	*/
	UPROPERTY(EditAnywhere, Category = Cutout)
	float SegmentationErrorThreshold;

	/** Cutout axis */
//	UPROPERTY(EditAnywhere, Category = Cutout)
//		FVector CutoutAxis;

	/** Cutout bitmap */
	UPROPERTY(EditAnywhere, Category = Cutout)
	TWeakObjectPtr<UTexture2D> CutoutTexture;
};


// #todo: custom brick gracture pattern
UCLASS(config = EditorPerProjectUserSettings)
class BLASTAUTHORING_API UBrickFractureSettings
	: public UObject
{
	GENERATED_BODY()
public:
	UBrickFractureSettings();

	/** Num Slices X axis - Slicing Method */
	UPROPERTY(EditAnywhere, Category = Brick)
		int SlicesX;

	/** Num Slices Y axis - Slicing Method */
	UPROPERTY(EditAnywhere, Category = Brick)
		int SlicesY;

	/** Num Slices Z axis - Slicing Method */
	UPROPERTY(EditAnywhere, Category = Brick)
		int SlicesZ;
};


UCLASS(config = EditorPerProjectUserSettings)
class BLASTAUTHORING_API UMeshFractureSettings
	: public UObject
{
	GENERATED_BODY()

public:

	UMeshFractureSettings();
	~UMeshFractureSettings();

	// general
	UPROPERTY(EditAnywhere, Category = FractureCommon)
	UCommonFractureSettings* CommonSettings;

	// Uniform Voronoi
	UPROPERTY(EditAnywhere, Category = UniformVoronoi)
	UUniformFractureSettings* UniformSettings;

	// Clustered Voronoi
	UPROPERTY(EditAnywhere, Category = ClusteredVoronoi)
	UClusterFractureSettings* ClusterSettings;

	// Radial Voronoi
	UPROPERTY(EditAnywhere, Category = RadialVoronoi)
	URadialFractureSettings* RadialSettings;

	// Slicing
	UPROPERTY(EditAnywhere, Category = Slicing)
	USlicingFractureSettings* SlicingSettings;

	// Plane Cut
	UPROPERTY(EditAnywhere, Category = PlaneCut)
	UPlaneCutFractureSettings* PlaneCutSettings;

	// Cutout
	UPROPERTY(EditAnywhere, Category = Cutout)
	UCutoutFractureSettings* CutoutSettings;

	// Brick
	UPROPERTY(EditAnywhere, Category = Cutout)
	UBrickFractureSettings* BrickSettings;

	// UI slider is provided for this debug functionality
	static float ExplodedViewExpansion;
};