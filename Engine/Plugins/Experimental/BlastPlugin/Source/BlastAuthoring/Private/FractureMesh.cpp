// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FractureMesh.h"
#include "Editor.h"
#include "MeshAttributes.h"
#include "MeshDescription.h"
#include "Engine/StaticMeshActor.h"
#include "Layers/ILayers.h"
#include "Materials/Material.h"
#include "DrawDebugHelpers.h"
#include "MeshUtility.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionObject.h"

#if PLATFORM_WINDOWS
#include "PxPhysicsAPI.h"
#include "NvBlast.h" 
#include "NvBlastAssert.h"
#include "NvBlastGlobals.h"
#include "NvBlastExtAuthoring.h"
#include "NvBlastExtAuthoringMesh.h"
#include "NvBlastExtAuthoringCutout.h"
#include "NvBlastExtAuthoringFractureTool.h"
#endif

#define LOCTEXT_NAMESPACE "FractureMesh"

DEFINE_LOG_CATEGORY(LogFractureMesh);

#if PLATFORM_WINDOWS
using namespace Nv::Blast;
using namespace physx;
#endif

namespace FractureMesh
{
	static TAutoConsoleVariable<int32> CVarEnableBlastDebugVisualization(TEXT("physics.Destruction.BlastDebugVisualization"), 0, TEXT("If enabled, the blast fracture output will be rendered using debug rendering. Note: this must be enabled BEFORE fracturing."));
}

static UWorld* FindEditorWorld()
{
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Editor)
			{
				return Context.World();
			}
		}
	}

	return NULL;
}

void UFractureMesh::FractureMesh(const UEditableMesh* SourceMesh, const FString& ParentName, const UMeshFractureSettings& FractureSettings, int32 FracturedChunkIndex, const FTransform& Transform, int RandomSeed, UGeometryCollection* FracturedGeometryCollection, TArray<FGeneratedFracturedChunk>& GeneratedChunksOut, TArray<int32>& DeletedChunksOut)
{
#if PLATFORM_WINDOWS
	const double CacheStartTime = FPlatformTime::Seconds();
	check(FractureSettings.CommonSettings);

	FractureRandomGenerator RandomGenerator(RandomSeed);
	Nv::Blast::FractureTool* BlastFractureTool = NvBlastExtAuthoringCreateFractureTool();

	check(BlastFractureTool);

	BlastFractureTool->setInteriorMaterialId(FracturedGeometryCollection->GetInteriorMaterialIndex());

	// convert mesh and assign to fracture tool
	Nv::Blast::Mesh* NewBlastMesh = nullptr;
	if (FracturedChunkIndex == -1)
		FMeshUtility::EditableMeshToBlastMesh(SourceMesh, NewBlastMesh);
	else
		FMeshUtility::EditableMeshToBlastMesh(SourceMesh, FracturedChunkIndex, NewBlastMesh);

	if (NewBlastMesh)
	{
		BlastFractureTool->setSourceMesh(NewBlastMesh);
		BlastFractureTool->setRemoveIslands(FractureSettings.CommonSettings->RemoveIslands);

		// init Voronoi Site Generator if required
		Nv::Blast::VoronoiSitesGenerator* SiteGenerator = nullptr;
		if (FractureSettings.CommonSettings->FractureMode <= EMeshFractureMode::Radial)
		{
			SiteGenerator = NvBlastExtAuthoringCreateVoronoiSitesGenerator(NewBlastMesh, &RandomGenerator);
		}

		// Where Voronoi sites are required
		switch (FractureSettings.CommonSettings->FractureMode)
		{
		case EMeshFractureMode::Uniform:
			check(FractureSettings.UniformSettings);
			SiteGenerator->uniformlyGenerateSitesInMesh(FractureSettings.UniformSettings->NumberVoronoiSites);
			break;

		case EMeshFractureMode::Clustered:
			check(FractureSettings.ClusterSettings);
			SiteGenerator->clusteredSitesGeneration(FractureSettings.ClusterSettings->NumberClusters, FractureSettings.ClusterSettings->SitesPerCluster, FractureSettings.ClusterSettings->ClusterRadius);
			break;

		case EMeshFractureMode::Radial:
			check(FractureSettings.RadialSettings);
			PxVec3 Center(FractureSettings.RadialSettings->Center.X, FractureSettings.RadialSettings->Center.Y, FractureSettings.RadialSettings->Center.Z);
			PxVec3 Normal(FractureSettings.RadialSettings->Normal.X, FractureSettings.RadialSettings->Normal.Y, FractureSettings.RadialSettings->Normal.Z);
			SiteGenerator->radialPattern(Center, Normal, FractureSettings.RadialSettings->Radius, FractureSettings.RadialSettings->AngularSteps, FractureSettings.RadialSettings->RadialSteps, FractureSettings.RadialSettings->AngleOffset, FractureSettings.RadialSettings->Variability);
			break;
		}

		const physx::PxVec3* VononoiSites = nullptr;
		uint32 SitesCount = 0;
		int32 ReturnCode = 0;
		bool ReplaceChunk = false;
		int ChunkID = 0;

		switch (FractureSettings.CommonSettings->FractureMode)
		{
		// Voronoi
		case EMeshFractureMode::Uniform:
		case EMeshFractureMode::Clustered:
		case EMeshFractureMode::Radial:
		{
			SitesCount = SiteGenerator->getVoronoiSites(VononoiSites);
			ReturnCode = BlastFractureTool->voronoiFracturing(ChunkID, SitesCount, VononoiSites, ReplaceChunk);
			if (ReturnCode != 0)
			{
				UE_LOG(LogFractureMesh, Error, TEXT("Mesh Slicing failed ReturnCode=%d"), ReturnCode);
			}
		}
		break;

		// Slicing
		case EMeshFractureMode::Slicing:
		{
			check(FractureSettings.SlicingSettings);

			Nv::Blast::SlicingConfiguration SlicingConfiguration;
			SlicingConfiguration.x_slices = FractureSettings.SlicingSettings->SlicesX;
			SlicingConfiguration.y_slices = FractureSettings.SlicingSettings->SlicesY;
			SlicingConfiguration.z_slices = FractureSettings.SlicingSettings->SlicesZ;
			SlicingConfiguration.angle_variations = FractureSettings.SlicingSettings->SliceAngleVariation;
			SlicingConfiguration.offset_variations = FractureSettings.SlicingSettings->SliceOffsetVariation;

			ReturnCode = BlastFractureTool->slicing(ChunkID, SlicingConfiguration, ReplaceChunk, &RandomGenerator);
			if (ReturnCode != 0)
			{
				UE_LOG(LogFractureMesh, Error, TEXT("Mesh Slicing failed ReturnCode=%d"), ReturnCode);
			}
		}
		break;

		// Plane Cut
		case EMeshFractureMode::PlaneCut:
		{
			check(FractureSettings.PlaneCutSettings);
			TArray<FTransform> Transforms;
			UGeometryCollection* GeometryCollectionObject = Cast<UGeometryCollection>(static_cast<UObject*>(SourceMesh->GetSubMeshAddress().MeshObjectPtr));
			FGeometryCollection* GeometryCollection = nullptr;

			if (GeometryCollectionObject)
			{
				TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
				GeometryCollection = GeometryCollectionPtr.Get();

				if (GeometryCollection)
				{
					GeometryCollectionAlgo::GlobalMatrices(GeometryCollection, Transforms);
				}
			}

			TArray<int32> ChunkIDs;
			ChunkIDs.Push(ChunkID);
			int CutNumber = 0;

			for (UPlaneCut& Cut : FractureSettings.PlaneCutSettings->PlaneCuts)
			{
				const NoiseConfiguration Noise;

				if (GeometryCollection)
				{
					for (int32 CID : ChunkIDs)
					{
						FVector TransNormal = Transforms[FracturedChunkIndex].InverseTransformVector(Cut.Normal);
						FVector TransPosition = Transforms[FracturedChunkIndex].InverseTransformPosition(Cut.Position);
						const physx::PxVec3 Normal(TransNormal.X, TransNormal.Y, TransNormal.Z);
						const physx::PxVec3 Position(TransPosition.X, TransPosition.Y, TransPosition.Z);
						ReturnCode = BlastFractureTool->cut(CID, Normal, Position, Noise, CutNumber != 0, &RandomGenerator);
					}
				}
				else
				{
					for (int32 CID : ChunkIDs)
					{
						const physx::PxVec3 Normal(Cut.Normal.X, Cut.Normal.Y, Cut.Normal.Z);
						const physx::PxVec3 Position(Cut.Position.X, Cut.Position.Y, Cut.Position.Z);
						ReturnCode = BlastFractureTool->cut(CID, Normal, Position, Noise, CutNumber != 0, &RandomGenerator);
					}
				}

				int32 NumChunks = static_cast<int32>(BlastFractureTool->getChunkCount());
				if (NumChunks > 2)
				{
					CutNumber++;
					ChunkIDs.Empty();

					// All generated chunks are candidates for any further cuts (however we must exclude the initial chunk from now on)
					for (int32 ChunkIndex = 0; ChunkIndex < NumChunks; ChunkIndex++)
					{
						int32 NewChunkID = BlastFractureTool->getChunkId(ChunkIndex);

						// don't try and fracture the initial chunk again
						if (NewChunkID != ChunkID)
						{
							ChunkIDs.Push(NewChunkID);
						}
					}
				}
			}

			// ReturnCode zero is a success, if we end with one chunk then the chunk we started with hasn't been split
			ReturnCode = !(BlastFractureTool->getChunkCount()>2);
		}
		break;

#ifdef CUTOUT_ENABLED
		// Bitmap Cutout
		case EMeshFractureMode::Cutout:
		{
			if (FractureSettings.CutoutSettings->CutoutTexture.IsValid())
			{
				check(FractureSettings.CutoutSettings);

				CutoutConfiguration CutoutConfig;

				TArray<uint8> RawData;
				int32 Width = 0;
				int32 Height = 0;
				ExtractDataFromTexture(FractureSettings.CutoutSettings->CutoutTexture, RawData, Width, Height);

				CutoutConfiguration cutoutConfig;
				{
					physx::PxVec3 axis = FractureSettings.CutoutSettings->Normal
						if (axis.isZero())
						{
							axis = PxVec3(0.f, 0.f, 1.f);
						}
					axis.normalize();
					float d = axis.dot(physx::PxVec3(0.f, 0.f, 1.f));
					if (d < (1e-6f - 1.0f))
					{
						cutoutConfig.transform.q = physx::PxQuat(physx::PxPi, PxVec3(1.f, 0.f, 0.f));
					}
					else if (d < 1.f)
					{
						float s = physx::PxSqrt((1 + d) * 2);
						float invs = 1 / s;
						auto c = axis.cross(PxVec3(0.f, 0.f, 1.f));
						cutoutConfig.transform.q = physx::PxQuat(c.x * invs, c.y * invs, c.z * invs, s * 0.5f);
						cutoutConfig.transform.q.normalize();
					}
					cutoutConfig.transform.p = PxVec3(0, 0, 0);// = point.getValue();
				}

				CutoutConfig.cutoutSet = NvBlastExtAuthoringCreateCutoutSet();
				//				CutoutConfig.scale = PxVec2(FractureSettings.CutoutSettings->Scale.X, FractureSettings.CutoutSettings->Scale.Y);
				//				CutoutConfig.transform = U2PTransform(FractureSettings.CutoutSettings->Transform);
				FQuat Rot = FractureSettings.CutoutSettings->Transform.GetRotation();
				PxQuat PQuat = PxQuat(Rot.X, Rot.Y, Rot.Z, Rot.W);
				FVector Loc = FractureSettings.CutoutSettings->Transform.GetLocation();
				PxVec3 PPos = PxVec3(Loc.X, Loc.Y, Loc.Z);

				CutoutConfig.transform = PxTransform(PPos, PQuat);

				bool Periodic = false;
				bool ExpandGaps = true;

				NvBlastExtAuthoringBuildCutoutSet(*CutoutConfig.cutoutSet, RawData.GetData(), Width, Height, FractureSettings.CutoutSettings->SegmentationErrorThreshold, FractureSettings.CutoutSettings->SnapThreshold, Periodic, ExpandGaps);

				ReturnCode = BlastFractureTool->cutout(ChunkID, CutoutConfig, false, &RandomGenerator);
				if (ReturnCode != 0)
				{
					UE_LOG(LogFractureMesh, Error, TEXT("Mesh Slicing failed ReturnCode=%d"), ReturnCode);
				}
			}
		}
		break;

		case EMeshFractureMode::Brick:
		{
			if (ReturnCode != 0)
			{
				UE_LOG(LogFractureMesh, Error, TEXT("Mesh Brick failed ReturnCode=%d"), ReturnCode);
			}
		}
		break;

#endif // CUTOUT_ENABLED

		default:
			UE_LOG(LogFractureMesh, Error, TEXT("Invalid Mesh Fracture Mode"));

		}

		if (ReturnCode == 0)
		{
			// triangulates cut surfaces and fixes up UVs
			BlastFractureTool->finalizeFracturing();

			// Makes a Geometry collection for each of fracture chunks
			GenerateChunkMeshes(BlastFractureTool, FractureSettings, FracturedChunkIndex, ParentName, Transform, NewBlastMesh, FracturedGeometryCollection, GeneratedChunksOut, DeletedChunksOut);

			float ProcessingTime = static_cast<float>(FPlatformTime::Seconds() - CacheStartTime);
			LogStatsAndTimings(NewBlastMesh, BlastFractureTool, Transform, FractureSettings, ProcessingTime);

			if (FractureMesh::CVarEnableBlastDebugVisualization.GetValueOnGameThread() != 0)
			{
				RenderDebugGraphics(BlastFractureTool, FractureSettings, Transform);
			}
		}

		// release tools
		if (SiteGenerator)
		{
			SiteGenerator->release();
		}
		if (NewBlastMesh)
		{
			NewBlastMesh->release();
		}
	}

	BlastFractureTool->release();
#endif
}

#if PLATFORM_WINDOWS
void UFractureMesh::GenerateChunkMeshes(Nv::Blast::FractureTool* BlastFractureTool, const UMeshFractureSettings& FractureSettings, int32 FracturedChunkIndex, const FString& ParentName, const FTransform& ParentTransform, Nv::Blast::Mesh* BlastMesh, UGeometryCollection* FracturedGeometryCollection, TArray<FGeneratedFracturedChunk>& GeneratedChunksOut, TArray<int32>& DeletedChunksOut)
{
	check(BlastFractureTool);
	check(BlastMesh);
	check(FracturedGeometryCollection);
	check(FractureSettings.CommonSettings);

	// -1 special case when fracturing a fresh static mesh
	if (FracturedChunkIndex <  0)
		FracturedChunkIndex = 0;
	FMeshUtility::AddBlastMeshToGeometryCollection(BlastFractureTool, FracturedChunkIndex, ParentName, ParentTransform, FracturedGeometryCollection, GeneratedChunksOut, DeletedChunksOut);
}
#endif

void UFractureMesh::FixupHierarchy(int32 FracturedChunkIndex, class UGeometryCollection* GeometryCollectionObject, FGeneratedFracturedChunk& GeneratedChunk, const FString& Name)
{
	TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
	FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get();

	TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", FGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FGeometryCollectionBoneNode > > HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", FGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FString> > BoneNamesArray = GeometryCollection->GetAttribute<FString>("BoneName", FGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", FGeometryCollection::TransformGroup);

	TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;
	TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;
	TManagedArray<FGeometryCollectionBoneNode >& Hierarchy = *HierarchyArray;
	TManagedArray<FString>& BoneNames = *BoneNamesArray;

	int32 LastTransformGroupIndex = GeometryCollection->NumElements(FGeometryCollection::TransformGroup) - 1;

	TManagedArray<FTransform> & TransformsOut = *GeometryCollection->Transform;
	// additional data to allow us to operate the exploded view slider in the editor
	ExplodedTransforms[LastTransformGroupIndex] = TransformsOut[LastTransformGroupIndex];
	ExplodedVectors[LastTransformGroupIndex] = GeneratedChunk.ChunkLocation;

	// bone hierarchy and chunk naming
	int32 ParentFractureLevel = Hierarchy[FracturedChunkIndex].Level;

	if (GeneratedChunk.FirstChunk)
	{
		// the root/un-fractured piece, fracture level 0, No parent bone
		Hierarchy[LastTransformGroupIndex].Level = 0;
		BoneNames[LastTransformGroupIndex] = Name;
	}
	else
	{
		// all of the chunk fragments, fracture level > 0, has valid parent bone
		Hierarchy[LastTransformGroupIndex].Level = ParentFractureLevel + 1;
	}

	Hierarchy[LastTransformGroupIndex].Parent = GeneratedChunk.ParentBone;
	Hierarchy[LastTransformGroupIndex].SetFlags(FGeometryCollectionBoneNode::ENodeFlags::FS_Geometry);

	if (GeneratedChunk.ParentBone >= 0)
	{
		Hierarchy[GeneratedChunk.ParentBone].Children.Add(LastTransformGroupIndex);
		Hierarchy[GeneratedChunk.ParentBone].ClearFlags(FGeometryCollectionBoneNode::ENodeFlags::FS_Geometry);
	}

	FGeometryCollectionClusteringUtility::RecursivelyUpdateChildBoneNames(FracturedChunkIndex, Hierarchy, BoneNames);
	FMeshUtility::ValidateGeometryCollectionState(GeometryCollectionObject);

}

#if PLATFORM_WINDOWS
void UFractureMesh::LogStatsAndTimings(const Nv::Blast::Mesh* BlastMesh, const Nv::Blast::FractureTool* BlastFractureTool, const FTransform& Transform, const UMeshFractureSettings& FractureSettings, float ProcessingTime)
{
	check(FractureSettings.CommonSettings);

	uint32 VertexCount = BlastMesh->getVerticesCount();
	uint32 EdgeCount = BlastMesh->getEdgesCount();
	uint32 FacetCount = BlastMesh->getFacetCount();

	FVector Scale = Transform.GetScale3D();
	UE_LOG(LogFractureMesh, Verbose, TEXT("Scaling %3.2f, %3.2f, %3.2f"), Scale.X, Scale.Y, Scale.Z);
	UE_LOG(LogFractureMesh, Verbose, TEXT("Mesh: VertCount=%d, EdgeCount=%d, FacetCount=%d"), VertexCount, EdgeCount, FacetCount);
	UE_LOG(LogFractureMesh, Verbose, TEXT("Fracture Chunk Count = %d"), BlastFractureTool->getChunkCount());
	if (ProcessingTime < 0.5f)
	{
		UE_LOG(LogFractureMesh, Verbose, TEXT("Fracture: Fracturing Time=%5.4f ms"), ProcessingTime*1000.0f);
	}
	else
	{
		UE_LOG(LogFractureMesh, Verbose, TEXT("Fracture: Fracturing Time=%5.4f seconds"), ProcessingTime);
	}
}
#endif

void UFractureMesh::ExtractDataFromTexture(const TWeakObjectPtr<UTexture> SourceTexture, TArray<uint8>& RawDataOut, int32& WidthOut, int32& HeightOut)
{
	// use the source art if it exists
	FTextureSource* TextureSource = nullptr;
	if ((SourceTexture != nullptr) && SourceTexture->Source.IsValid())
	{
		switch (SourceTexture->Source.GetFormat())
		{
		case TSF_G8:
		case TSF_BGRA8:
			TextureSource = &(SourceTexture->Source);
			break;
		default:
			break;
		};
	}

	if (TextureSource != nullptr)
	{
		TArray<uint8> TextureRawData;
		TextureSource->GetMipData(TextureRawData, 0);
		int32 BytesPerPixel = TextureSource->GetBytesPerPixel();
		ETextureSourceFormat PixelFormat = TextureSource->GetFormat();
		WidthOut = TextureSource->GetSizeX();
		HeightOut = TextureSource->GetSizeY();
		RawDataOut.SetNumZeroed(WidthOut * HeightOut);

		for (int Y = 0; Y < HeightOut; ++Y)
		{
			for (int X = 0; X < WidthOut; ++X)
			{
				int32 PixelByteOffset = (X + Y * WidthOut) * BytesPerPixel;
				const uint8* PixelPtr = TextureRawData.GetData() + PixelByteOffset;
				FColor Color;
				if (PixelFormat == TSF_BGRA8)
				{
					Color = *((FColor*)PixelPtr);
				}
				else
				{
					checkSlow(PixelFormat == TSF_G8);
					const uint8 Intensity = *PixelPtr;
					Color = FColor(Intensity, Intensity, Intensity, Intensity);
				}

				RawDataOut[Y * WidthOut + X] = Color.A;
			}
		}
	}
}

#if PLATFORM_WINDOWS
void UFractureMesh::RenderDebugGraphics(Nv::Blast::FractureTool* BlastFractureTool, const UMeshFractureSettings& FractureSettings, const FTransform& Transform)
{
	check(FractureSettings.CommonSettings);

	uint32 ChunkCount = BlastFractureTool->getChunkCount();
	for (uint32 ChunkIndex = 0; ChunkIndex < ChunkCount; ChunkIndex++)
	{
		const Nv::Blast::ChunkInfo& ChunkInfo = BlastFractureTool->getChunkInfo(ChunkIndex);
		Nv::Blast::Mesh* ChunkMesh = ChunkInfo.meshData;

		// only render the children
		bool DebugDrawParent = false;
		uint32 StartIndex = DebugDrawParent ? 0 : 1;
		if (ChunkIndex >= StartIndex)
		{
			DrawDebugBlastMesh(ChunkMesh, ChunkIndex, UMeshFractureSettings::ExplodedViewExpansion, Transform);
		}
	}
}

void UFractureMesh::DrawDebugBlastMesh(const Nv::Blast::Mesh* ChunkMesh, int ChunkIndex, float ExplodedViewAmount, const FTransform& Transform)
{
	UWorld* InWorld = FindEditorWorld();

	TArray<FColor> Colors = { FColor::Red, FColor::Green, FColor::Blue, FColor::Yellow, FColor::Magenta, FColor::Cyan, FColor::Black, FColor::Orange, FColor::Purple };
	FColor UseColor = Colors[ChunkIndex % 9];

	const physx::PxBounds3& Bounds = ChunkMesh->getBoundingBox();
	float MaxBounds = FMath::Max(FMath::Max(Bounds.getExtents().x, Bounds.getExtents().y), Bounds.getExtents().z);

	PxVec3 ApproxChunkCenter = Bounds.getCenter();

	uint32 NumEdges = ChunkMesh->getEdgesCount();
	const Nv::Blast::Edge* Edges = ChunkMesh->getEdges();
	const Nv::Blast::Vertex* Vertices = ChunkMesh->getVertices();
	for (uint32 i = 0; i < NumEdges; i++)
	{
		const Nv::Blast::Vertex& V1 = Vertices[Edges[i].s];
		const Nv::Blast::Vertex& V2 = Vertices[Edges[i].e];

		PxVec3 S = V1.p + ApproxChunkCenter*MaxBounds*5.0f;
		PxVec3 E = V2.p + ApproxChunkCenter*MaxBounds*5.0f;

		FVector Start = FVector(S.x, S.y, S.z)*MagicScaling;
		FVector End = FVector(E.x, E.y, E.z)*MagicScaling;

		FVector TStart = Transform.TransformPosition(Start);
		FVector TEnd = Transform.TransformPosition(End);
		DrawDebugLine(InWorld, TStart, TEnd, UseColor, true);
	}
}
#endif

#undef LOCTEXT_NAMESPACE
