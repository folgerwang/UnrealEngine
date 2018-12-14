// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionRenderLevelSetActor.h"


#include "Chaos/ArrayND.h"
#include "Chaos/Vector.h"

using namespace Chaos;

DEFINE_LOG_CATEGORY_STATIC(LSR_LOG, Log, All);

AGeometryCollectionRenderLevelSetActor::AGeometryCollectionRenderLevelSetActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), SurfaceTolerance(0.01f), Isovalue(0.f), Enabled(true), RenderVolumeBoundingBox(false), DynRayMarchMaterial(NULL), StepSizeMult(1.f)
{
	PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessComponent0"));
	RootComponent = PostProcessComponent;

	// set initial values
	// @todo: Need to make this work based on if the module is loaded
#if 0
	TargetVolumeTexture = LoadObject<UVolumeTexture>(NULL, TEXT("/GeometryCollectionPlugin/VolumeVisualization/Textures/VolumeToRender"), NULL, LOAD_None, NULL);
	UMaterialInterface* MaterialInterface = LoadObject<UMaterialInterface>(NULL, TEXT("/GeometryCollectionPlugin/VolumeVisualization/Materials/M_VolumeRenderSphereTracePP"), NULL, LOAD_None, NULL);
	RayMarchMaterial = MaterialInterface ? MaterialInterface->GetBaseMaterial() : nullptr;
#else
	TargetVolumeTexture = nullptr;
	RayMarchMaterial = nullptr;
#endif
}

void AGeometryCollectionRenderLevelSetActor::BeginPlay()
{
	Super::BeginPlay();

	// make sure to set enabled on the post process
	PostProcessComponent->bEnabled = Enabled;
	PostProcessComponent->bUnbound = true;
}

#if WITH_EDITOR
void AGeometryCollectionRenderLevelSetActor::PostEditChangeProperty(struct FPropertyChangedEvent& e)
{
	Super::PostEditChangeProperty(e);

	// sync all rendering properties each time a param changes.
	// @todo: optimize to only update parameters when rendering-specific ones are edited
	SyncMaterialParameters();

}
#endif

void AGeometryCollectionRenderLevelSetActor::SyncMaterialParameters()
{
	if (!RayMarchMaterial)
	{
		return;
	}

	// make dynamic material instance if it hasn't been created yet
	if (!DynRayMarchMaterial) {
		DynRayMarchMaterial = UMaterialInstanceDynamic::Create(RayMarchMaterial, this);

		// add the blendable with our post process material
		PostProcessComponent->AddOrUpdateBlendable(DynRayMarchMaterial);
	}

	// Sync all render parameters to our material
	DynRayMarchMaterial->SetScalarParameterValue("Surface Tolerance", SurfaceTolerance);
	DynRayMarchMaterial->SetScalarParameterValue("Isovalue", Isovalue);
	
	
	DynRayMarchMaterial->SetScalarParameterValue("Step Size Mult", StepSizeMult);
	DynRayMarchMaterial->SetScalarParameterValue("Voxel Size", VoxelSize);

	DynRayMarchMaterial->SetVectorParameterValue("Min Bounds", MinBBoxCorner);
	DynRayMarchMaterial->SetVectorParameterValue("Max Bounds", MaxBBoxCorner);

	DynRayMarchMaterial->SetVectorParameterValue("WorldToLocalc0", FLinearColor(WorldToLocal.GetColumn(0)));
	DynRayMarchMaterial->SetVectorParameterValue("WorldToLocalc1", FLinearColor(WorldToLocal.GetColumn(1)));
	DynRayMarchMaterial->SetVectorParameterValue("WorldToLocalc2", FLinearColor(WorldToLocal.GetColumn(2)));
	DynRayMarchMaterial->SetVectorParameterValue("WorldToLocalTranslation", FLinearColor(WorldToLocal.GetOrigin()));	

	DynRayMarchMaterial->SetTextureParameterValue("Volume To Render", TargetVolumeTexture);

	DynRayMarchMaterial->SetScalarParameterValue("Debug BBox", (float) RenderVolumeBoundingBox);

	PostProcessComponent->bEnabled = Enabled;
}

void AGeometryCollectionRenderLevelSetActor::SyncLevelSetTransform(const FTransform &LocalToWorld)
{
	if (!RayMarchMaterial)
	{
		return;
	}

	WorldToLocal = LocalToWorld.Inverse().ToMatrixWithScale();
	DynRayMarchMaterial->SetVectorParameterValue("WorldToLocalc0", FLinearColor(WorldToLocal.GetColumn(0)));
	DynRayMarchMaterial->SetVectorParameterValue("WorldToLocalc1", FLinearColor(WorldToLocal.GetColumn(1)));
	DynRayMarchMaterial->SetVectorParameterValue("WorldToLocalc2", FLinearColor(WorldToLocal.GetColumn(2)));
	DynRayMarchMaterial->SetVectorParameterValue("WorldToLocalTranslation", FLinearColor(WorldToLocal.GetOrigin()));
}

bool AGeometryCollectionRenderLevelSetActor::SetLevelSetToRender(const Chaos::TLevelSet<float, 3> &LevelSet, const FTransform &LocalToWorld)
{
	// error case when the target volume texture isn't set
	if (TargetVolumeTexture == NULL) {
		UE_LOG(LSR_LOG, Warning, TEXT("Target UVolumeTexture is null on %s"), *GetFullName());
		return false;
	}

	// get refs to the grid structures
	const TArrayND<float, 3> &LevelSetPhiArray = LevelSet.GetPhiArray();
	const TArrayND<TVector<float, 3>, 3> &LevelSetNormalsArray = LevelSet.GetNormalsArray();
	const TUniformGrid<float, 3> &LevelSetGrid = LevelSet.GetGrid();

	const TVector<int32, 3> &Counts = LevelSetGrid.Counts();
	
	// set bounding box
	MinBBoxCorner = LevelSetGrid.MinCorner();
	MaxBBoxCorner = LevelSetGrid.MaxCorner();
	WorldToLocal = LocalToWorld.Inverse().ToMatrixWithScale();

	// @todo: do we need to deal with non square voxels?
	VoxelSize = LevelSetGrid.Dx().X;

	// Error case when the voxel size is sufficiently small
	if (VoxelSize < 1e-5) {
		UE_LOG(LSR_LOG, Warning, TEXT("Voxel size is zero on %s"), *GetFullName());
		return false;
	}

	// lambda for querying the level set information
	// @note: x and z swap for volume textures to match TlevelSet
	// @todo: we could encode voxel ordering more nicely in the UVolumeTexture
	auto QueryVoxel = [&](const int32 x, const int32 y, const int32 z, FFloat16 *ret)
	{
		float sd = LevelSetPhiArray(TVector<int32, 3>(z, y, x));
		TVector<float, 3> n = LevelSetNormalsArray(TVector<int32, 3>(z, y, x));
		n.Normalize();

		// @note: x and z swap for volume textures to render correctly
		ret[0] = n.X;
		ret[1] = n.Y;
		ret[2] = n.Z;
		ret[3] = sd;
	};

	// fill volume texture from level set
	// @note: we swap z and x to match level set in world space
	bool success = TargetVolumeTexture->UpdateSourceFromFunction(QueryVoxel, Counts.Z, Counts.Y, Counts.X);

	if (!success) {
		UE_LOG(LSR_LOG, Warning, TEXT("Couldn't create target volume texture from TLevelSet with %s"), *GetFullName());
		return false;
	}

	// set all parameters on our dynamic material instance to sync state
	SyncMaterialParameters();
	
	UE_LOG(LSR_LOG, Log, TEXT("Volume Bounds: %s - %s -- Volume Dims: %d %d %d -- Voxel Size: %f -- World To Local: %s"), *MinBBoxCorner.ToString(), *MaxBBoxCorner.ToString(), Counts.X, Counts.Y, Counts.Z, VoxelSize, *WorldToLocal.ToString());

	return true;
}