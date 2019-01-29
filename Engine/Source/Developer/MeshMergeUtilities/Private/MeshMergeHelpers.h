// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/MeshMerging.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class USplineMeshComponent;
class UBodySetup;
class ALandscapeProxy;
struct FSectionInfo;
struct FMeshDescription;
struct FStaticMaterial;
struct FRawMeshExt;
struct FStaticMeshLODResources;
struct FKAggregateGeom;
class UInstancedStaticMeshComponent;

class MESHMERGEUTILITIES_API FMeshMergeHelpers
{
public:
	/** Extracting section info data from static, skeletal mesh (components) */
	static void ExtractSections(const UStaticMeshComponent* Component, int32 LODIndex, TArray<FSectionInfo>& OutSections);
	static void ExtractSections(const USkeletalMeshComponent* Component, int32 LODIndex, TArray<FSectionInfo>& OutSections);
	static void ExtractSections(const UStaticMesh* StaticMesh, int32 LODIndex, TArray<FSectionInfo>& OutSections);

	/** Expanding instance data from instanced static mesh components */
	static void ExpandInstances(const UInstancedStaticMeshComponent* InInstancedStaticMeshComponent, FMeshDescription& InOutRawMesh, TArray<FSectionInfo>& InOutSections);

	/** Extracting mesh data in FMeshDescription form from static, skeletal mesh (components) */
	static void RetrieveMesh(const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, FMeshDescription& RawMesh, bool bPropagateVertexColours);
	static void RetrieveMesh(USkeletalMeshComponent* SkeletalMeshComponent, int32 LODIndex, FMeshDescription& RawMesh, bool bPropagateVertexColours);
	static void RetrieveMesh(const UStaticMesh* StaticMesh, int32 LODIndex, FMeshDescription& RawMesh);
	
	/** Exports static mesh LOD render data to a RawMesh */
	static void ExportStaticMeshLOD(const FStaticMeshLODResources& StaticMeshLOD, FMeshDescription& OutRawMesh, const TArray<FStaticMaterial>& Materials);

	/** Checks whether or not the texture coordinates are outside of 0-1 UV ranges */
	static bool CheckWrappingUVs(const TArray<FVector2D>& UVs);
	static bool CheckWrappingUVs(const FMeshDescription& MeshDescription, int32 UVChannelIndex);

	/** Culls away triangles which are inside culling volumes or completely underneath the landscape */
	static void CullTrianglesFromVolumesAndUnderLandscapes(const UWorld* World, const FBoxSphereBounds& Bounds, FMeshDescription& InOutRawMesh);
	
	/** Propagates deformation along spline to raw mesh data */
	static void PropagateSplineDeformationToRawMesh(const USplineMeshComponent* InSplineMeshComponent, FMeshDescription &OutRawMesh);
	
	/** Propagates deformation along spline to physics geometry data */
	static void PropagateSplineDeformationToPhysicsGeometry(USplineMeshComponent* SplineMeshComponent, FKAggregateGeom& InOutPhysicsGeometry);

	/** Transforms raw mesh data using InTransform*/
	static void TransformRawMeshVertexData(const FTransform& InTransform, FMeshDescription &OutRawMesh);

	/** Retrieves all culling landscapes and volumes as FMeshDescription structures. Note the caller is responsible for deleting the heap data managed by CullingRawMeshes */
	static void RetrieveCullingLandscapeAndVolumes(UWorld* InWorld, const FBoxSphereBounds& EstimatedMeshProxyBounds, const TEnumAsByte<ELandscapeCullingPrecision::Type> PrecisionType, TArray<FMeshDescription*>& CullingRawMeshes);

	/** Transforms physics geometry data using InTransform */
	static void TransformPhysicsGeometry(const FTransform& InTransform, const bool bBakeConvexTransform, struct FKAggregateGeom& AggGeom);
	
	/** Extract physics geometry data from a body setup */
	static void ExtractPhysicsGeometry(UBodySetup* InBodySetup, const FTransform& ComponentToWorld, const bool bBakeConvexTransform, struct FKAggregateGeom& OutAggGeom);

	/** Ensure that UV is in valid 0-1 UV ranges */
	static FVector2D GetValidUV(const FVector2D& UV);
	
	/** Calculates UV coordinates bounds for the given Raw Mesh	*/
	static void CalculateTextureCoordinateBoundsForRawMesh(const FMeshDescription& InRawMesh, TArray<FBox2D>& OutBounds);

	/** Propagates vertex painted colors from the StaticMeshComponent instance to RawMesh */
	static bool PropagatePaintedColorsToRawMesh(const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, FMeshDescription& RawMesh);

	/** Checks whether or not the landscape proxy is hit given a ray start and end */
	static bool IsLandscapeHit(const FVector& RayOrigin, const FVector& RayEndPoint, const UWorld* World, const TArray<ALandscapeProxy*>& LandscapeProxies, FVector& OutHitLocation);
	
	/** Appends a FMeshDescription to another instance */
	static void AppendRawMesh(FMeshDescription& InTarget, const FMeshDescription& InSource);

	/** Merges imposter meshes into a raw mesh. */
	static void MergeImpostersToRawMesh(TArray<const UStaticMeshComponent*> ImposterComponents, FMeshDescription& InRawMesh, const FVector& InPivot, int32 BaseMaterialIndex, TArray<UMaterialInterface*>& OutImposterMaterials);
};
