// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "Engine/StaticMesh.h"
#include "Engine/MeshMerging.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodySetupEnums.h"
#include "UVMapSettings.h"

#include "EditorStaticMeshLibrary.generated.h"

class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FEditorScriptingMeshReductionSettings
{
	GENERATED_BODY()

	FEditorScriptingMeshReductionSettings()
		: PercentTriangles(0.5f)
		, ScreenSize(0.5f)
	{ }

	// Percentage of triangles to keep. Ranges from 0.0 to 1.0: 1.0 = no reduction, 0.0 = no triangles.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float PercentTriangles;

	// ScreenSize to display this LOD. Ranges from 0.0 to 1.0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float ScreenSize;
};

USTRUCT(BlueprintType)
struct FEditorScriptingMeshReductionOptions
{
	GENERATED_BODY()

	FEditorScriptingMeshReductionOptions()
		: bAutoComputeLODScreenSize(true)
	{ }

	// If true, the screen sizes at which LODs swap are computed automatically
	// @note that this is displayed as 'Auto Compute LOD Distances' in the UI
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bAutoComputeLODScreenSize;

	// Array of reduction settings to apply to each new LOD mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FEditorScriptingMeshReductionSettings> ReductionSettings;
};

/** Types of Collision Construct that are generated **/
UENUM(BlueprintType)
enum class EScriptingCollisionShapeType : uint8
{
	Box,
	Sphere,
	Capsule,
	NDOP10_X,
	NDOP10_Y,
	NDOP10_Z,
	NDOP18,
	NDOP26
};

/**
 * Utility class to altering and analyzing a StaticMesh and use the common functionalities of the Mesh Editor.
 * The editor should not be in play in editor mode.
 */
UCLASS()
class EDITORSCRIPTINGUTILITIES_API UEditorStaticMeshLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Remove then add LODs on a static mesh.
	 * The static mesh must have at least LOD 0.
	 * The LOD 0 of the static mesh is kept after removal.
	 * The build settings of LOD 0 will be applied to all subsequent LODs.
	 * @param	StaticMesh				Mesh to process.
	 * @param	ReductionOptions		Options on how to generate LODs on the mesh.
	 * @return the number of LODs generated on the input mesh.
	 * An negative value indicates that the reduction could not be performed. See log for explanation.
	 * No action will be performed if ReductionOptions.ReductionSettings is empty
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static int32 SetLods(UStaticMesh* StaticMesh, const FEditorScriptingMeshReductionOptions& ReductionOptions);

	/**
	 * Adds or create a LOD at DestinationLodIndex using the geometry from SourceStaticMesh SourceLodIndex
	 * @param	DestinationStaticMesh		The static mesh to set the LOD in.
	 * @param	DestinationLodIndex			The index of the LOD to set.
	 * @param	SourceStaticMesh			The static mesh to get the LOD from.
	 * @param	SourceLodIndex				The index of the LOD to get.
	 * @param	bReuseExistingMaterialSlots	If true, sections from SourceStaticMesh will be remapped to match the material slots of DestinationStaticMesh
											when they have the same material assigned. If false, all material slots of SourceStaticMesh will be appended in DestinationStaticMesh.
	 * @return	The index of the LOD that was set. It can be different than DestinationLodIndex if it wasn't a valid index.
	 *			A negative value indicates that the LOD was not set. See log for explanation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static int32 SetLodFromStaticMesh(UStaticMesh* DestinationStaticMesh, int32 DestinationLodIndex, UStaticMesh* SourceStaticMesh, int32 SourceLodIndex, bool bReuseExistingMaterialSlots);

	/**
	 * Get number of LODs present on a static mesh.
	 * @param	StaticMesh				Mesh to process.
	 * @return the number of LODs present on the input mesh.
	 * An negative value indicates that the command could not be executed. See log for explanation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static int32 GetLodCount(UStaticMesh* StaticMesh);

	/**
	 * Remove LODs on a static mesh except LOD 0.
	 * @param	StaticMesh			Mesh to remove LOD from.
	 * @return A boolean indicating if the removal was successful, true, or not.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static bool RemoveLods(UStaticMesh* StaticMesh);

	/**
	 * Get an array of LOD screen sizes for evaluation.
	 * @param	StaticMesh			Mesh to process.
	 * @return array of LOD screen sizes.
	 */
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | StaticMesh")
	static TArray<float> GetLodScreenSizes(UStaticMesh* StaticMesh);

public:
	/**
	 * Add simple collisions to a static mesh.
	 * This method replicates what is done when invoking menu entries "Collision > Add [...] Simplified Collision" in the Mesh Editor.
	 * @param	StaticMesh				Mesh to generate simple collision for.
	 * @param	ShapeType				Options on which simple collision to add to the mesh.
	 * @return An integer indicating the index of the collision newly created.
	 * A negative value indicates the addition failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static int32 AddSimpleCollisions(UStaticMesh* StaticMesh, const EScriptingCollisionShapeType ShapeType);

	/**
	 * Get number of simple collisions present on a static mesh.
	 * @param	StaticMesh				Mesh to query on.
	 * @return An integer representing the number of simple collisions on the input static mesh.
	 * An negative value indicates that the command could not be executed. See log for explanation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static int32 GetSimpleCollisionCount(UStaticMesh* StaticMesh);

	/**
	 * Get the Collision Trace behavior of a static mesh
	 * @param	StaticMesh				Mesh to query on.
	 * @return the Collision Trace behavior.
	 */
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | StaticMesh")
	static TEnumAsByte<ECollisionTraceFlag> GetCollisionComplexity(UStaticMesh* StaticMesh);

	/**
	 * Get number of convex collisions present on a static mesh.
	 * @param	StaticMesh				Mesh to query on.
	 * @return An integer representing the number of convex collisions on the input static mesh.
	 * An negative value indicates that the command could not be executed. See log for explanation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static int32 GetConvexCollisionCount(UStaticMesh* StaticMesh);

	/**
	 * Add a convex collision to a static mesh.
	 * Any existing collisions will be removed from the static mesh.
	 * This method replicates what is done when invoking menu entry "Collision > Auto Convex Collision" in the Mesh Editor.
	 * @param	StaticMesh				Static mesh to add convex collision on.
	 * @param	HullCount				Maximum number of convex pieces that will be created. Must be positive.
	 * @param	MaxHullVerts			Maximum number of vertices allowed for any generated convex hull.
	 * @param	HullPrecision			Number of voxels to use when generating collision. Must be positive.
	 * @return A boolean indicating if the addition was successful or not.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static bool SetConvexDecompositionCollisions(UStaticMesh* StaticMesh, int32 HullCount, int32 MaxHullVerts, int32 HullPrecision);

	/**
	 * Remove collisions from a static mesh.
	 * This method replicates what is done when invoking menu entries "Collision > Remove Collision" in the Mesh Editor.
	 * @param	StaticMesh			Static mesh to remove collisions from.
	 * @return A boolean indicating if the removal was successful or not.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static bool RemoveCollisions(UStaticMesh* StaticMesh);

	/**
	 * Enables/disables mesh section collision for a specific LOD.
	 * @param	StaticMesh			Static mesh to Enables/disables collisions from.
	 * @param	bCollisionEnabled	If the collision is enabled or not.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	SectionIndex		Index of the StaticMesh Section.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static void EnableSectionCollision(UStaticMesh* StaticMesh, bool bCollisionEnabled, int32 LODIndex, int32 SectionIndex);

	/**
	 * Checks if a specific LOD mesh section has collision.
	 * @param	StaticMesh			Static mesh to remove collisions from.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	SectionIndex		Index of the StaticMesh Section.
	 * @return True is the collision is enabled for the specified LOD of the StaticMesh section.
	 */
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | StaticMesh")
	static bool IsSectionCollisionEnabled(UStaticMesh* StaticMesh, int32 LODIndex, int32 SectionIndex);

public:

	/**
	 * Enables/disables mesh section shadow casting for a specific LOD.
	 * @param	StaticMesh			Static mesh to Enables/disables shadow casting from.
	 * @param	bCastShadow			If the section should cast shadow.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	SectionIndex		Index of the StaticMesh Section.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static void EnableSectionCastShadow(UStaticMesh* StaticMesh, bool bCastShadow, int32 LODIndex, int32 SectionIndex);

	/** Check whether a static mesh has vertex colors */
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | StaticMesh")
	static bool HasVertexColors(UStaticMesh* StaticMesh);

	/** Check whether a static mesh component has vertex colors */
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | StaticMesh")
	static bool HasInstanceVertexColors(UStaticMeshComponent* StaticMeshComponent);

	/** Set Generate Lightmap UVs for StaticMesh */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta=(ScriptName="SetGenerateLightmapUv"))
	static bool SetGenerateLightmapUVs(UStaticMesh* StaticMesh, bool bGenerateLightmapUVs);

	/** Get number of StaticMesh verts for an LOD */
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | StaticMesh")
	static int32 GetNumberVerts(UStaticMesh* StaticMesh, int32 LODIndex);

	/** Sets StaticMeshFlag bAllowCPUAccess  */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static void SetAllowCPUAccess(UStaticMesh* StaticMesh, bool bAllowCPUAccess);

public:

	/**
	 * Returns the number of UV channels for the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh to query.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @return the number of UV channels.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static int32 GetNumUVChannels(UStaticMesh* StaticMesh, int32 LODIndex);

	/**
	 * Adds an empty UV channel at the end of the existing channels on the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh on which to add a UV channel.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @return true if a UV channel was added.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static bool AddUVChannel(UStaticMesh* StaticMesh, int32 LODIndex);

	/**
	 * Inserts an empty UV channel at the specified channel index on the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh on which to insert a UV channel.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	UVChannelIndex		Index where to insert the UV channel.
	 * @return true if a UV channel was added.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static bool InsertUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex);

	/**
	 * Removes the UV channel at the specified channel index on the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh on which to remove the UV channel.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	UVChannelIndex		Index where to remove the UV channel.
	 * @return true if the UV channel was removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static bool RemoveUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex);

	/**
	 * Generates planar UV mapping in the specified UV channel on the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh on which to generate the UV mapping.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	UVChannelIndex		Channel where to save the UV mapping.
	 * @param	Position			Position of the center of the projection gizmo.
	 * @param	Orientation			Rotation to apply to the projection gizmo.
	 * @param	Tiling				The UV tiling to use to generate the UV mapping.
	 * @return true if the UV mapping was generated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static bool GeneratePlanarUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex, const FVector& Position, const FRotator& Orientation, const FVector2D& Tiling);

	/**
	 * Generates cylindrical UV mapping in the specified UV channel on the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh on which to generate the UV mapping.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	UVChannelIndex		Channel where to save the UV mapping.
	 * @param	Position			Position of the center of the projection gizmo.
	 * @param	Orientation			Rotation to apply to the projection gizmo.
	 * @param	Tiling				The UV tiling to use to generate the UV mapping.
	 * @return true if the UV mapping was generated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static bool GenerateCylindricalUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex, const FVector& Position, const FRotator& Orientation, const FVector2D& Tiling);

	/**
	 * Generates box UV mapping in the specified UV channel on the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh on which to generate the UV mapping.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	UVChannelIndex		Channel where to save the UV mapping.
	 * @param	Position			Position of the center of the projection gizmo.
	 * @param	Orientation			Rotation to apply to the projection gizmo.
	 * @param	Size				The size of the box projection gizmo.
	 * @return true if the UV mapping was generated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh")
	static bool GenerateBoxUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex, const FVector& Position, const FRotator& Orientation, const FVector& Size);
};
