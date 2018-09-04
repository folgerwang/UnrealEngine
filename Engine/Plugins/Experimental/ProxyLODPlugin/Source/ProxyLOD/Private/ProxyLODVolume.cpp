// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ProxyLODVolume.h"

#include "ProxyLODMeshAttrTransfer.h"
#include "ProxyLODMeshConvertUtils.h"
#include "ProxyLODMeshSDFConversions.h"
#include "ProxyLODMeshTypes.h"
#include "ProxyLODMeshUtilities.h"

#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h> // for Spatial Query
#include <openvdb/tools/MeshToVolume.h> // for MeshToVolume
#include <openvdb/tools/VolumeToMesh.h> // for VolumeToMesh

typedef openvdb::math::Transform	OpenVDBTransform;

class FProxyLODVolumeImpl : public IProxyLODVolume
{
public:
	FProxyLODVolumeImpl()
		: VoxelSize(0.0)
	{
	}

	~FProxyLODVolumeImpl()
	{
		SDFVolume.reset();
		SrcPolyIndexGrid.reset();
		Sampler.reset();
	}

	bool Initialize(const TArray<FMeshMergeData>& Geometry, float Accuracy)
	{
		FRawMeshArrayAdapter SrcGeometryAdapter(Geometry);
		OpenVDBTransform::Ptr XForm = OpenVDBTransform::createLinearTransform(Accuracy);
		SrcGeometryAdapter.SetTransform(XForm);

		VoxelSize = SrcGeometryAdapter.GetTransform().voxelSize()[0];

		SrcPolyIndexGrid = openvdb::Int32Grid::create();

		if (!ProxyLOD::MeshArrayToSDFVolume(SrcGeometryAdapter, SDFVolume, SrcPolyIndexGrid.get()))
		{
			SrcPolyIndexGrid.reset();
			return false;
		}

		Sampler.reset(new openvdb::tools::GridSampler<openvdb::FloatGrid, openvdb::tools::PointSampler>(*SDFVolume));

		return true;
	}

	virtual double GetVoxelSize() const override
	{
		return VoxelSize;
	}

	virtual FVector3i GetBBoxSize() const override
	{
		if (SDFVolume == nullptr)
		{
			return FVector3i(0,0,0);
		}

		openvdb::math::Coord VolumeBBoxSize = SDFVolume->evalActiveVoxelDim();

		return FVector3i(VolumeBBoxSize.x(), VolumeBBoxSize.y(), VolumeBBoxSize.z());
	}

	virtual void CloseGaps(const double GapRadius, const int32 MaxDilations) override
	{
		ProxyLOD::CloseGaps(SDFVolume, GapRadius, MaxDilations);
	}

	virtual float QueryDistance(const FVector& Point) const override
	{
		return Sampler->wsSample(openvdb::Vec3R(Point.X, Point.Y, Point.Z));
	}

	virtual void ConvertToRawMesh(FRawMesh& OutRawMesh) const override
	{
		// Mesh types that will be shared by various stages.
		FAOSMesh AOSMeshedVolume;
		ProxyLOD::SDFVolumeToMesh(SDFVolume, 0.0, 0.0, AOSMeshedVolume);

		FVertexDataMesh VertexDataMesh;
		ProxyLOD::ConvertMesh(AOSMeshedVolume, VertexDataMesh);

		ProxyLOD::ConvertMesh(VertexDataMesh, OutRawMesh);
	}

	void ExpandNarrowBand(float ExteriorWidth, float InteriorWidth)
	{
		using namespace openvdb::tools;

		FRawMesh RawMesh;
		ConvertToRawMesh(RawMesh);
		FRawMeshAdapter MeshAdapter(RawMesh, SDFVolume->transform());

		openvdb::FloatGrid::Ptr NewSDFVolume;
		openvdb::Int32Grid::Ptr NewSrcPolyIndexGrid;

		try
		{
			NewSrcPolyIndexGrid = openvdb::Int32Grid::create();
			NewSDFVolume = openvdb::tools::meshToVolume<openvdb::FloatGrid>(MeshAdapter, MeshAdapter.GetTransform(), ExteriorWidth / VoxelSize, InteriorWidth / VoxelSize, 0, NewSrcPolyIndexGrid.get());

			SDFVolume = NewSDFVolume;
			SrcPolyIndexGrid = NewSrcPolyIndexGrid;

			// reduce memory footprint, increase the spareness.
			openvdb::tools::pruneLevelSet(SDFVolume->tree(), float(ExteriorWidth / VoxelSize), float(-InteriorWidth / VoxelSize));
		}
		catch (std::bad_alloc&)
		{
			NewSDFVolume.reset();
			NewSrcPolyIndexGrid.reset();
			return;
		}

		Sampler.reset(new openvdb::tools::GridSampler<openvdb::FloatGrid, openvdb::tools::PointSampler>(*SDFVolume));
	}

private:
	openvdb::FloatGrid::Ptr SDFVolume;
	openvdb::Int32Grid::Ptr SrcPolyIndexGrid;
	openvdb::tools::GridSampler<openvdb::FloatGrid, openvdb::tools::PointSampler>::Ptr Sampler;
	double VoxelSize;
};

int32 IProxyLODVolume::FVector3i::MinIndex() const
{
	return (int32)openvdb::math::MinIndex(openvdb::math::Coord(X, Y, X));
}

TUniquePtr<IProxyLODVolume> IProxyLODVolume::CreateSDFVolumeFromMeshArray(const TArray<FMeshMergeData>& Geometry, float Step)
{
	TUniquePtr<FProxyLODVolumeImpl> Volume = MakeUnique<FProxyLODVolumeImpl>();

	if (Volume == nullptr || !Volume->Initialize(Geometry, Step))
	{
		return nullptr;
	}

	return Volume;
}
