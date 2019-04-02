// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheSceneProxy.h"
#include "MaterialShared.h"
#include "SceneManagement.h"
#include "EngineGlobals.h"
#include "Materials/Material.h"
#include "Engine/Engine.h"
#include "GeometryCacheComponent.h"
#include "GeometryCacheMeshData.h"
#include "GeometryCache.h"
#include "GeometryCacheTrackStreamable.h"
#include "GeometryCacheModule.h"
#include "GeometryCacheHelpers.h"
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"

DECLARE_CYCLE_STAT(TEXT("Gather Mesh Elements"), STAT_GeometryCacheSceneProxy_GetMeshElements, STATGROUP_GeometryCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("Triangle Count"), STAT_GeometryCacheSceneProxy_TriangleCount, STATGROUP_GeometryCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("Batch Count"), STAT_GeometryCacheSceneProxy_MeshBatchCount, STATGROUP_GeometryCache);
DECLARE_CYCLE_STAT(TEXT("Vertex Buffer Update"), STAT_VertexBufferUpdate, STATGROUP_GeometryCache);
DECLARE_CYCLE_STAT(TEXT("Index Buffer Update"), STAT_IndexBufferUpdate, STATGROUP_GeometryCache);
DECLARE_CYCLE_STAT(TEXT("Buffer Update Task"), STAT_BufferUpdateTask, STATGROUP_GeometryCache);

static TAutoConsoleVariable<int32> CVarOffloadUpdate(
	TEXT("GeometryCache.OffloadUpdate"),
	0,
	TEXT("Offloat some updates from the render thread to the workers & RHI threads."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarInterpolateFrames(
	TEXT("GeometryCache.InterpolateFrames"),
	1,
	TEXT("Interpolate between geometry cache frames (if topology allows this)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);


/**
* All vertex information except the position.
*/
struct FNoPositionVertex
{
	FVector2D TextureCoordinate[MAX_STATIC_TEXCOORDS];
	FPackedNormal TangentX;
	FPackedNormal TangentZ;
	FColor Color;
};

FGeometryCacheSceneProxy::FGeometryCacheSceneProxy(UGeometryCacheComponent* Component) : FPrimitiveSceneProxy(Component)
, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{
	Time = Component->GetAnimationTime();
	bLooping = Component->IsLooping();
	bAlwaysHasVelocity = true;
	PlaybackSpeed = (Component->IsPlaying()) ? Component->GetPlaybackSpeed() : 0.0f;

	// Copy each section
	const int32 NumTracks = Component->TrackSections.Num();
	Tracks.AddZeroed(NumTracks);
	for (int32 TrackIdx = 0; TrackIdx < NumTracks; TrackIdx++)
	{
		FTrackRenderData& SrcSection = Component->TrackSections[TrackIdx];
		UGeometryCacheTrackStreamable* StreamableTrack = Cast<UGeometryCacheTrackStreamable>(Component->GeometryCache->Tracks[TrackIdx]);
		check(StreamableTrack);

		const FGeometryCacheTrackStreamableSampleInfo& SampleInfo = StreamableTrack->GetSampleInfo(Time, bLooping);
		
		if (SampleInfo.NumVertices > 0)
		{
			FGeomCacheTrackProxy* NewSection = new FGeomCacheTrackProxy(GetScene().GetFeatureLevel());

			NewSection->Resource = StreamableTrack->GetRenderResource();
			NewSection->WorldMatrix = SrcSection.Matrix;
			NewSection->FrameIndex = -1;
			NewSection->UploadedSampleIndex = -1;
			NewSection->NextFrameIndex = -1;
			NewSection->NextFrameMeshData = nullptr;

			// Allocate verts

			
			NewSection->TangentXBuffer.Init(SampleInfo.NumVertices * sizeof(FPackedNormal));
			NewSection->TangentZBuffer.Init(SampleInfo.NumVertices * sizeof(FPackedNormal));
			NewSection->TextureCoordinatesBuffer.Init(SampleInfo.NumVertices * sizeof(FVector2D));
			NewSection->ColorBuffer.Init(SampleInfo.NumVertices * sizeof(FColor));
			

			//NewSection->VertexBuffer.Init(SampleInfo.NumVertices * sizeof(FNoPositionVertex));
			NewSection->PositionBuffers[0].Init(SampleInfo.NumVertices * sizeof(FVector));
			NewSection->PositionBuffers[1].Init(SampleInfo.NumVertices * sizeof(FVector));
			NewSection->CurrentPositionBufferIndex = -1;
			NewSection->PositionBufferFrameIndices[0] = NewSection->PositionBufferFrameIndices[1] = -1;
			NewSection->PositionBufferFrameTimes[0] = NewSection->PositionBufferFrameTimes[1] = -1.0f;

			// Allocate index buffer
			NewSection->IndexBuffer.NumIndices = SampleInfo.NumIndices;

			// Init vertex factory
			NewSection->VertexFactory.Init(&NewSection->PositionBuffers[0], &NewSection->PositionBuffers[1], &NewSection->TangentXBuffer, &NewSection->TangentZBuffer, &NewSection->TextureCoordinatesBuffer, &NewSection->ColorBuffer);

			// Enqueue initialization of render resource
			BeginInitResource(&NewSection->PositionBuffers[0]);
			BeginInitResource(&NewSection->PositionBuffers[1]);
			BeginInitResource(&NewSection->TangentXBuffer);
			BeginInitResource(&NewSection->TangentZBuffer);
			BeginInitResource(&NewSection->TextureCoordinatesBuffer);
			BeginInitResource(&NewSection->ColorBuffer);			
			BeginInitResource(&NewSection->IndexBuffer);
			BeginInitResource(&NewSection->VertexFactory);

			// Grab materials
			int32 Dummy = -1;
			NewSection->MeshData = new FGeometryCacheMeshData();
			NewSection->Resource->UpdateMeshData(Time, bLooping, Dummy, *NewSection->MeshData);
			NewSection->NextFrameMeshData = new FGeometryCacheMeshData();

			// Some basic sanity checks
			for (FGeometryCacheMeshBatchInfo& BatchInfo : NewSection->MeshData->BatchesInfo)
			{
				UMaterialInterface* Material = Component->GetMaterial(BatchInfo.MaterialIndex);
				if (Material == nullptr || !Material->CheckMaterialUsage_Concurrent(EMaterialUsage::MATUSAGE_GeometryCache))
				{
					Material = UMaterial::GetDefaultMaterial(MD_Surface);
				}

				NewSection->Materials.Add(Material);
			}

			// Save ref to new section
			Tracks[TrackIdx] = NewSection;
		}
	}

	if (IsRayTracingEnabled())
	{
		// Update at least once after the scene proxy has been constructed
		// Otherwise it is invisible until animation starts
		FGeometryCacheSceneProxy* SceneProxy = this;
		ENQUEUE_RENDER_COMMAND(FGeometryCacheUpdateAnimation)(
			[SceneProxy](FRHICommandListImmediate& RHICmdList)
			{
				SceneProxy->FrameUpdate();
			});

#if RHI_RAYTRACING
		{
			ENQUEUE_RENDER_COMMAND(FGeometryCacheInitRayTracingGeometry)(
				[SceneProxy](FRHICommandListImmediate& RHICmdList)
				{
					for (FGeomCacheTrackProxy* Section : SceneProxy->Tracks)
					{
						if (Section != nullptr)
						{
							FRayTracingGeometryInitializer Initializer;
							const int PositionBufferIndex = Section->CurrentPositionBufferIndex != -1 ? Section->CurrentPositionBufferIndex % 2 : 0;
							Initializer.PositionVertexBuffer = Section->PositionBuffers[PositionBufferIndex].VertexBufferRHI;
							Initializer.IndexBuffer = Section->IndexBuffer.IndexBufferRHI;
							Initializer.BaseVertexIndex = 0;
							Initializer.VertexBufferStride = sizeof(FVector);
							Initializer.VertexBufferByteOffset = 0;
							Initializer.TotalPrimitiveCount = Section->IndexBuffer.NumIndices / 3;
							Initializer.VertexBufferElementType = VET_Float3;
							Initializer.PrimitiveType = PT_TriangleList;
							Initializer.bFastBuild = false;

							Section->RayTracingGeometry.SetInitializer(Initializer);
							Section->RayTracingGeometry.InitResource();
						}
					}
				});
		}
	#endif
	}
}

FGeometryCacheSceneProxy::~FGeometryCacheSceneProxy()
{
	for (FGeomCacheTrackProxy* Section : Tracks)
	{
		if (Section != nullptr)
		{
			Section->TangentXBuffer.ReleaseResource();
			Section->TangentZBuffer.ReleaseResource();
			Section->TextureCoordinatesBuffer.ReleaseResource();
			Section->ColorBuffer.ReleaseResource();
			Section->IndexBuffer.ReleaseResource();
			Section->VertexFactory.ReleaseResource();
			Section->PositionBuffers[0].ReleaseResource();
			Section->PositionBuffers[1].ReleaseResource();
		#if RHI_RAYTRACING
			Section->RayTracingGeometry.ReleaseResource();
		#endif
			delete Section->MeshData;
			if (Section->NextFrameMeshData != nullptr)
				delete Section->NextFrameMeshData;
			delete Section;
		}
	}
	Tracks.Empty();
}

struct FRHICommandUpdateGeometryCacheBuffer : public FRHICommand<FRHICommandUpdateGeometryCacheBuffer>
{
	FGraphEventRef BufferGenerationCompleteFence;

	FVertexBufferRHIParamRef VertexBuffer;
	//void *VertexData;
	//uint32 VertexSize;
	TArray<uint8> VertexData;

	FIndexBufferRHIParamRef IndexBuffer;
	//void *IndexData;
	//uint32 IndexSize;
	TArray<uint8> IndexData;
	
	virtual ~FRHICommandUpdateGeometryCacheBuffer() {}
	
	FORCEINLINE_DEBUGGABLE FRHICommandUpdateGeometryCacheBuffer(
		FGraphEventRef& InBufferGenerationCompleteFence,
		FVertexBufferRHIParamRef InVertexBuffer,
		void *InVertexData,
		uint32 InVertexSize,
		FIndexBufferRHIParamRef InIndexBuffer,
		void *InIndexData,
		uint32 InIndexSize)
	:
		BufferGenerationCompleteFence(InBufferGenerationCompleteFence)
		, VertexBuffer(InVertexBuffer)
		, IndexBuffer(InIndexBuffer)
	{
		VertexData.SetNumUninitialized(InVertexSize);
		FMemory::Memcpy(VertexData.GetData(), InVertexData, InVertexSize);
		IndexData.SetNumUninitialized(InIndexSize);
		FMemory::Memcpy(IndexData.GetData(), InIndexData, InIndexSize);
	}

	/**
		This is scheduled by the render thread on the RHI thread and defers updating the buffers untill just before rendering.
		That way we can run the decoding/interpolation on the task graph.
		Completion of these tasks is marked by the BufferGenerationCompleteFence
	*/
	void Execute(FRHICommandListBase& CmdList)
	{
		//FTaskGraphInterface::Get().WaitUntilTaskCompletes(BufferGenerationCompleteFence, IsRunningRHIInSeparateThread() ? ENamedThreads::RHIThread : ENamedThreads::RenderThread);

		// Upload vertex data
		void* RESTRICT Data = (void* RESTRICT)GDynamicRHI->RHILockVertexBuffer(VertexBuffer, 0, VertexData.Num(), RLM_WriteOnly);
		FMemory::BigBlockMemcpy(Data, VertexData.GetData(), VertexData.Num());
		GDynamicRHI->RHIUnlockVertexBuffer(VertexBuffer);

		// Upload index data
		Data = (void* RESTRICT)GDynamicRHI->RHILockIndexBuffer(IndexBuffer, 0, IndexData.Num(), RLM_WriteOnly);
		FMemory::BigBlockMemcpy(Data, IndexData.GetData(), IndexData.Num());
		GDynamicRHI->RHIUnlockIndexBuffer(IndexBuffer);

		// Make sure to release refcounted things asap
		IndexBuffer = nullptr;
		VertexBuffer = nullptr;
		BufferGenerationCompleteFence = nullptr;
	}
};

class FGeometryCacheVertexFactoryUserDataWrapper : public FOneFrameResource
{
public:
	FGeometryCacheVertexFactoryUserData Data;
};

static float OneOver255 = 1.0f / 255.0f;

// Avoid converting from 8 bit normalized to float and back again.
inline FPackedNormal InterpolatePackedNormal(const FPackedNormal &A, const FPackedNormal &B, int32 ScaledFactor, int32 OneMinusScaledFactor)
{
	FPackedNormal result;
	result.Vector.X = (A.Vector.X * OneMinusScaledFactor + B.Vector.X * ScaledFactor) * OneOver255;
	result.Vector.Y = (A.Vector.Y * OneMinusScaledFactor + B.Vector.Y * ScaledFactor) * OneOver255;
	result.Vector.Z = (A.Vector.Z * OneMinusScaledFactor + B.Vector.Z * ScaledFactor) * OneOver255;
	result.Vector.W = (A.Vector.W * OneMinusScaledFactor + B.Vector.W * ScaledFactor) * OneOver255;
	return result;
}

// Avoid converting from 8 bit normalized to float and back again.
inline FColor InterpolatePackedColor(const FColor &A, const FColor &B, int32 ScaledFactor, int32 OneMinusScaledFactor)
{
	FColor result;
	result.R = (A.R * OneMinusScaledFactor + B.R * ScaledFactor) * OneOver255;
	result.G = (A.G * OneMinusScaledFactor + B.G * ScaledFactor) * OneOver255;
	result.B = (A.B * OneMinusScaledFactor + B.B * ScaledFactor) * OneOver255;
	result.A = (A.A * OneMinusScaledFactor + B.A * ScaledFactor) * OneOver255;
	return result;
}

SIZE_T FGeometryCacheSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FGeometryCacheSceneProxy::CreateMeshBatch(
	const FGeomCacheTrackProxy* TrackProxy, 
	const FGeometryCacheMeshBatchInfo& BatchInfo, 
	FGeometryCacheVertexFactoryUserDataWrapper& UserDataWrapper,
	FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer,
	FMeshBatch& Mesh) const
{
	FGeometryCacheVertexFactoryUserData &UserData = UserDataWrapper.Data;

	UserData.MeshExtension = FVector::OneVector;
	UserData.MeshOrigin = FVector::ZeroVector;

	const bool bHasMotionVectors = (
		TrackProxy->MeshData->VertexInfo.bHasMotionVectors &&
		TrackProxy->NextFrameMeshData->VertexInfo.bHasMotionVectors &&
		TrackProxy->MeshData->Positions.Num() == TrackProxy->MeshData->MotionVectors.Num())
		&& (TrackProxy->NextFrameMeshData->Positions.Num() == TrackProxy->NextFrameMeshData->MotionVectors.Num());

	if (!bHasMotionVectors)
	{
		UserData.MotionBlurDataExtension = FVector::OneVector;
		UserData.MotionBlurDataOrigin = FVector::ZeroVector;
		UserData.MotionBlurPositionScale = 0.0f;
	}
	else
	{
		UserData.MotionBlurDataExtension = FVector::OneVector * PlaybackSpeed;
		UserData.MotionBlurDataOrigin = FVector::ZeroVector;
		UserData.MotionBlurPositionScale = 1.0f;
	}

	if (IsRayTracingEnabled())
	{
		// No vertex manipulation is allowed in the vertex shader
		// Otherwise we need an additional compute shader pass to execute the vertex shader and dump to a staging buffer
		check(UserData.MeshExtension == FVector::OneVector);
		check(UserData.MeshOrigin == FVector::ZeroVector);
	}

	UserData.PositionBuffer = &TrackProxy->PositionBuffers[TrackProxy->CurrentPositionBufferIndex % 2];
	UserData.MotionBlurDataBuffer = &TrackProxy->PositionBuffers[(TrackProxy->CurrentPositionBufferIndex+1) % 2];

	FGeometryCacheVertexFactoryUniformBufferParameters UniformBufferParameters;

	UniformBufferParameters.MeshOrigin = UserData.MeshOrigin;
	UniformBufferParameters.MeshExtension = UserData.MeshExtension;
	UniformBufferParameters.MotionBlurDataOrigin = UserData.MotionBlurDataOrigin;
	UniformBufferParameters.MotionBlurDataExtension = UserData.MotionBlurDataExtension;
	UniformBufferParameters.MotionBlurPositionScale = UserData.MotionBlurPositionScale;

	UserData.UniformBuffer = FGeometryCacheVertexFactoryUniformBufferParametersRef::CreateUniformBufferImmediate(UniformBufferParameters, UniformBuffer_SingleFrame);
	TrackProxy->VertexFactory.CreateManualVertexFetchUniformBuffer(UserData.PositionBuffer, UserData.MotionBlurDataBuffer, UserData);

	// Draw the mesh.
	FMeshBatchElement& BatchElement = Mesh.Elements[0];
	BatchElement.IndexBuffer = &TrackProxy->IndexBuffer;
	Mesh.VertexFactory = &TrackProxy->VertexFactory;
	Mesh.SegmentIndex = 0;

	const FMatrix& LocalToWorldTransform = TrackProxy->WorldMatrix * GetLocalToWorld();

	DynamicPrimitiveUniformBuffer.Set(LocalToWorldTransform, LocalToWorldTransform, GetBounds(), GetLocalBounds(), true, false, UseEditorDepthTest());
	BatchElement.PrimitiveUniformBuffer = DynamicPrimitiveUniformBuffer.UniformBuffer.GetUniformBufferRHI();

	BatchElement.FirstIndex = BatchInfo.StartIndex;
	BatchElement.NumPrimitives = BatchInfo.NumTriangles;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = TrackProxy->MeshData->Positions.Num() - 1;
	BatchElement.VertexFactoryUserData = &UserDataWrapper.Data;
	Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
	Mesh.Type = PT_TriangleList;
	Mesh.DepthPriorityGroup = SDPG_World;
	Mesh.bCanApplyViewModeOverrides = false;
}

void FGeometryCacheSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	SCOPE_CYCLE_COUNTER(STAT_GeometryCacheSceneProxy_GetMeshElements);

	// Set up wire frame material (if needed)
	const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

	FColoredMaterialRenderProxy* WireframeMaterialInstance = nullptr;
	if (bWireframe)
	{
		WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : nullptr,
			FLinearColor(0, 0.5f, 1.f)
			);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
	}
	
	const bool bVisible = [&Views, VisibilityMap]()
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				return true;
			}
		}
		return false;
	}();

	if (bVisible)
	{
		if (!IsRayTracingEnabled())
		{
			// When ray tracing is disabled, update only when visible
			// This is the old behavior
			FrameUpdate();
		}

		// Iterate over all batches in all tracks and add them to all the relevant views	
		for (const FGeomCacheTrackProxy* TrackProxy : Tracks)
		{
			const FVisibilitySample& VisibilitySample = TrackProxy->Resource->GetTrack()->GetVisibilitySample(Time, bLooping);
			if (!VisibilitySample.bVisibilityState)
			{
				continue;
			}

			const int32 NumBatches = TrackProxy->MeshData->BatchesInfo.Num();

			for (int32 BatchIndex = 0; BatchIndex < NumBatches; ++BatchIndex)
			{
				const FGeometryCacheMeshBatchInfo BatchInfo = TrackProxy->MeshData->BatchesInfo[BatchIndex];

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (VisibilityMap & (1 << ViewIndex))
					{
						FMeshBatch& MeshBatch = Collector.AllocateMesh();

						FGeometryCacheVertexFactoryUserDataWrapper& UserDataWrapper = Collector.AllocateOneFrameResource<FGeometryCacheVertexFactoryUserDataWrapper>();
						FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
						CreateMeshBatch(TrackProxy, BatchInfo, UserDataWrapper, DynamicPrimitiveUniformBuffer, MeshBatch);

						// Apply view mode material overrides
						FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : TrackProxy->Materials[BatchIndex]->GetRenderProxy();
						MeshBatch.bWireframe = bWireframe;
						MeshBatch.MaterialRenderProxy = MaterialProxy;

						Collector.AddMesh(ViewIndex, MeshBatch);

						INC_DWORD_STAT_BY(STAT_GeometryCacheSceneProxy_TriangleCount, MeshBatch.Elements[0].NumPrimitives);
						INC_DWORD_STAT_BY(STAT_GeometryCacheSceneProxy_MeshBatchCount, 1);

					#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
						// Render bounds
						RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
					#endif
					}
				}
			}
		}
	}
}

#if RHI_RAYTRACING
void FGeometryCacheSceneProxy::GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances)
{
	for (FGeomCacheTrackProxy* TrackProxy : Tracks)
	{
		const FVisibilitySample& VisibilitySample = TrackProxy->Resource->GetTrack()->GetVisibilitySample(Time, bLooping);
		if (!VisibilitySample.bVisibilityState)
		{
			continue;
		}

		FRayTracingInstance RayTracingInstance;
		RayTracingInstance.Geometry = &TrackProxy->RayTracingGeometry;
		RayTracingInstance.InstanceTransforms.Add(GetLocalToWorld());

		for (int32 SegmentIndex = 0; SegmentIndex < TrackProxy->MeshData->BatchesInfo.Num(); ++SegmentIndex)
		{
			const FGeometryCacheMeshBatchInfo BatchInfo = TrackProxy->MeshData->BatchesInfo[SegmentIndex];
			FMeshBatch MeshBatch;

			FGeometryCacheVertexFactoryUserDataWrapper& UserDataWrapper = Context.RayTracingMeshResourceCollector.AllocateOneFrameResource<FGeometryCacheVertexFactoryUserDataWrapper>();
			FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Context.RayTracingMeshResourceCollector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
			CreateMeshBatch(TrackProxy, BatchInfo, UserDataWrapper, DynamicPrimitiveUniformBuffer, MeshBatch);

			MeshBatch.MaterialRenderProxy = TrackProxy->Materials[SegmentIndex]->GetRenderProxy();

			RayTracingInstance.Materials.Add(MeshBatch);
		}

		RayTracingInstance.BuildInstanceMaskAndFlags();

		OutRayTracingInstances.Add(RayTracingInstance);
	}
}
#endif

FPrimitiveViewRelevance FGeometryCacheSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = IsMovable() && Result.bOpaqueRelevance && Result.bRenderInMainPass;
	return Result;
}

bool FGeometryCacheSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}

bool FGeometryCacheSceneProxy::IsUsingDistanceCullFade() const
{
	return MaterialRelevance.bUsesDistanceCullFade;
}

uint32 FGeometryCacheSceneProxy::GetMemoryFootprint(void) const
{
	return(sizeof(*this) + GetAllocatedSize());
}

uint32 FGeometryCacheSceneProxy::GetAllocatedSize(void) const
{
	return(FPrimitiveSceneProxy::GetAllocatedSize());
}

void FGeometryCacheSceneProxy::UpdateAnimation(float NewTime, bool bNewLooping, bool bNewIsPlayingBackwards, float NewPlaybackSpeed)
{
	Time = NewTime;
	bLooping = bNewLooping;
	bIsPlayingBackwards = bNewIsPlayingBackwards;
	PlaybackSpeed = NewPlaybackSpeed;

	if (IsRayTracingEnabled())
	{
		// When ray tracing is enabled, update regardless of visibility
		FrameUpdate();

	#if RHI_RAYTRACING
		for (FGeomCacheTrackProxy* Section : Tracks)
		{
			if (Section != nullptr)
			{
				const int PositionBufferIndex = Section->CurrentPositionBufferIndex != -1 ? Section->CurrentPositionBufferIndex % 2 : 0;

				Section->RayTracingGeometry.Initializer.PositionVertexBuffer = Section->PositionBuffers[PositionBufferIndex].VertexBufferRHI;
				Section->RayTracingGeometry.Initializer.TotalPrimitiveCount = Section->IndexBuffer.NumIndices / 3;
				Section->RayTracingGeometry.UpdateRHI();
			}
		}
	#endif
	}
}

void FGeometryCacheSceneProxy::FrameUpdate() const
{
	for (FGeomCacheTrackProxy* TrackProxy : Tracks)
	{
		// Render out stored TrackProxy's
		if (TrackProxy != nullptr)
		{
			const FVisibilitySample& VisibilitySample = TrackProxy->Resource->GetTrack()->GetVisibilitySample(Time, bLooping);
			if (!VisibilitySample.bVisibilityState)
			{
				continue;
			}

			// Figure out which frame(s) we need to decode
			int32 FrameIndex;
			int32 NextFrameIndex;
			float InterpolationFactor;
			TrackProxy->Resource->GetTrack()->FindSampleIndexesFromTime(Time, bLooping, bIsPlayingBackwards, FrameIndex, NextFrameIndex, InterpolationFactor);
			bool bDecodedAnything = false; // Did anything new get decoded this frame
			bool bSeeked = false; // Is this frame a seek and thus the previous rendered frame's data invalid
			bool bDecoderError = false; // If we have a decoder error we don't interpolate and we don't update the vertex buffers
										// so essentially we just keep the last valid frame...

			// Compare this against the frames we got and keep some/all/none of them
			// This will work across frames but also within a frame if the mesh is in several views
			if (TrackProxy->FrameIndex != FrameIndex || TrackProxy->NextFrameIndex != NextFrameIndex)
			{
				// Normal case the next frame is the new current frame
				if (TrackProxy->NextFrameIndex == FrameIndex)
				{
					// Cycle the current and next frame double buffer
					FGeometryCacheMeshData *OldFrameMesh = TrackProxy->MeshData;
					TrackProxy->MeshData = TrackProxy->NextFrameMeshData;
					TrackProxy->NextFrameMeshData = OldFrameMesh;

					int32 OldFrameIndex = TrackProxy->FrameIndex;
					TrackProxy->FrameIndex = TrackProxy->NextFrameIndex;
					TrackProxy->NextFrameIndex = OldFrameIndex;

					// Decode the new next frame
					if (TrackProxy->Resource->DecodeMeshData(NextFrameIndex, *TrackProxy->NextFrameMeshData))
					{
						bDecodedAnything = true;
						// Only register this if we actually successfully decoded
						TrackProxy->NextFrameIndex = NextFrameIndex;
					}
					else
					{
						// Mark the frame as corrupted
						TrackProxy->NextFrameIndex = -1;
						bDecoderError = true;
					}
				}
				// Probably a seek or the mesh hasn't been visible in a while decode two frames
				else
				{
					if (TrackProxy->Resource->DecodeMeshData(FrameIndex, *TrackProxy->MeshData))
					{
						TrackProxy->NextFrameMeshData->Indices = TrackProxy->MeshData->Indices;
						if (TrackProxy->Resource->DecodeMeshData(NextFrameIndex, *TrackProxy->NextFrameMeshData))
						{
							TrackProxy->FrameIndex = FrameIndex;
							TrackProxy->NextFrameIndex = NextFrameIndex;
							bSeeked = true;
							bDecodedAnything = true;
						}
						else
						{
							// The first frame decoded fine but the second didn't 
							// we need to specially handle this
							TrackProxy->NextFrameIndex = -1;
							bDecoderError = true;
						}
					}
					else
					{
						TrackProxy->FrameIndex = -1;
						bDecoderError = true;
					}
				}
			}

			// Check if we can interpolate between the two frames we have available
			const bool bCanInterpolate = TrackProxy->Resource->IsTopologyCompatible(TrackProxy->FrameIndex, TrackProxy->NextFrameIndex);

			// Check if we have explicit motion vectors
			const bool bHasMotionVectors = (
				TrackProxy->MeshData->VertexInfo.bHasMotionVectors &&
				TrackProxy->NextFrameMeshData->VertexInfo.bHasMotionVectors &&
				TrackProxy->MeshData->Positions.Num() == TrackProxy->MeshData->MotionVectors.Num())
				&& (TrackProxy->NextFrameMeshData->Positions.Num() == TrackProxy->NextFrameMeshData->MotionVectors.Num());

			// Can we interpolate the vertex data?
			if (bCanInterpolate && !bDecoderError && CVarInterpolateFrames.GetValueOnRenderThread() != 0)
			{
				// Interpolate if the time has changed.
				// note: This is a bit precarious as this code is called multiple times per frame. This ensures
				// we only interpolate once (which is a nice optimization) but more importantly that we only
				// bump the CurrentPositionBufferIndex once per frame. This ensures that last frame's position
				// buffer is not overwritten.
				// If motion blur suddenly seems to stop working while it should be working it may be that the
				// CurrentPositionBufferIndex gets inadvertently bumped twice per frame essentially using the same
				// data for current and previous during rendering.
				if (TrackProxy->PositionBufferFrameTimes[TrackProxy->CurrentPositionBufferIndex % 2] != Time)
				{
					TArray<FVector> InterpolatedPositions;
					TArray<FPackedNormal> InterpolatedTangentX;
					TArray<FPackedNormal> InterpolatedTangentZ;
					TArray<FVector2D> InterpolatedUVs;
					TArray<FColor> InterpolatedColors;

					const int32 NumVerts = TrackProxy->MeshData->Positions.Num();

					InterpolatedPositions.AddUninitialized(NumVerts);
					InterpolatedTangentX.AddUninitialized(NumVerts);
					InterpolatedTangentZ.AddUninitialized(NumVerts);
					InterpolatedUVs.AddUninitialized(NumVerts);
					InterpolatedColors.AddUninitialized(NumVerts);

					TArray<FVector> InterpolatedMotionVectors;
					if (bHasMotionVectors)
					{
						InterpolatedMotionVectors.AddUninitialized(NumVerts);
					}


					const float OneMinusInterp = 1.0 - InterpolationFactor;
					const int32 InterpFixed = (int32)(InterpolationFactor * 255.0f);
					const int32 OneMinusInterpFixed = 255 - InterpFixed;

					for (int32 Index = 0; Index < NumVerts; ++Index)
					{
						const FVector& PositionA = TrackProxy->MeshData->Positions[Index];
						const FVector& PositionB = TrackProxy->NextFrameMeshData->Positions[Index];
						InterpolatedPositions[Index] = PositionA * OneMinusInterp + PositionB* InterpolationFactor;
					}

					for (int32 Index = 0; Index < NumVerts; ++Index)
					{
						// The following are already 8 bit so quantized enough we can do exact equal comparisons
						const FPackedNormal& TangentXA = TrackProxy->MeshData->TangentsX[Index];
						const FPackedNormal& TangentXB = TrackProxy->NextFrameMeshData->TangentsX[Index];
						const FPackedNormal& TangentZA = TrackProxy->MeshData->TangentsZ[Index];
						const FPackedNormal& TangentZB = TrackProxy->NextFrameMeshData->TangentsZ[Index];

						InterpolatedTangentX[Index] = InterpolatePackedNormal(TangentXA, TangentXB, InterpFixed, OneMinusInterpFixed);
						InterpolatedTangentZ[Index] = InterpolatePackedNormal(TangentZA, TangentZB, InterpFixed, OneMinusInterpFixed);
					}

					if (TrackProxy->MeshData->VertexInfo.bHasColor0) for (int32 Index = 0; Index < NumVerts; ++Index)
					{
						const FColor& ColorA = TrackProxy->MeshData->Colors[Index];
						const FColor& ColorB = TrackProxy->NextFrameMeshData->Colors[Index];
						InterpolatedColors[Index] = InterpolatePackedColor(ColorA, ColorB, InterpFixed, OneMinusInterpFixed);
					}

					if (TrackProxy->MeshData->VertexInfo.bHasUV0) for (int32 Index = 0; Index < NumVerts; ++Index)
					{
						const FVector2D& UVA = TrackProxy->MeshData->TextureCoordinates[Index];
						const FVector2D& UVB = TrackProxy->NextFrameMeshData->TextureCoordinates[Index];

						InterpolatedUVs[Index] = UVA * OneMinusInterp + UVB * InterpolationFactor;
					}

					if (bHasMotionVectors) for (int32 Index = 0; Index < NumVerts; ++Index)
					{
						InterpolatedMotionVectors[Index] = TrackProxy->MeshData->MotionVectors[Index] * OneMinusInterp +
							TrackProxy->NextFrameMeshData->MotionVectors[Index] * InterpolationFactor;
					}

					// Upload other non-motionblurred data
					if (!TrackProxy->MeshData->VertexInfo.bConstantIndices)
						TrackProxy->IndexBuffer.Update(TrackProxy->MeshData->Indices);

					if (TrackProxy->MeshData->VertexInfo.bHasTangentX)
						TrackProxy->TangentXBuffer.Update(InterpolatedTangentX);
					if (TrackProxy->MeshData->VertexInfo.bHasTangentZ)
						TrackProxy->TangentZBuffer.Update(InterpolatedTangentZ);

					if (TrackProxy->MeshData->VertexInfo.bHasUV0)
						TrackProxy->TextureCoordinatesBuffer.Update(InterpolatedUVs);

					if (TrackProxy->MeshData->VertexInfo.bHasColor0)
						TrackProxy->ColorBuffer.Update(InterpolatedColors);

					bool bIsCompatibleWithCachedFrame = TrackProxy->Resource->IsTopologyCompatible(
						TrackProxy->PositionBufferFrameIndices[TrackProxy->CurrentPositionBufferIndex % 2],
						TrackProxy->FrameIndex);

					if (!bHasMotionVectors)
					{
						// Initialize both buffers the first frame
						if (TrackProxy->CurrentPositionBufferIndex == -1 || !bIsCompatibleWithCachedFrame)
						{
							TrackProxy->PositionBuffers[0].Update(InterpolatedPositions);
							TrackProxy->PositionBuffers[1].Update(InterpolatedPositions);
							TrackProxy->CurrentPositionBufferIndex = 0;
							TrackProxy->PositionBufferFrameTimes[0] = Time;
							TrackProxy->PositionBufferFrameTimes[1] = Time;
							// We need to keep a frame index in order to ensure topology consistency. As we can interpolate 
							// FrameIndex and NextFrameIndex are certainly topo-compatible so it doesn't really matter which 
							// one we keep here. But wee keep NextFrameIndex as that is most useful to validate against
							// the frame coming up
							TrackProxy->PositionBufferFrameIndices[0] = TrackProxy->NextFrameIndex;
							TrackProxy->PositionBufferFrameIndices[1] = TrackProxy->NextFrameIndex;
						}
						else
						{
							TrackProxy->CurrentPositionBufferIndex++;
							TrackProxy->PositionBuffers[TrackProxy->CurrentPositionBufferIndex % 2].Update(InterpolatedPositions);
							TrackProxy->PositionBufferFrameTimes[TrackProxy->CurrentPositionBufferIndex % 2] = Time;
							TrackProxy->PositionBufferFrameIndices[TrackProxy->CurrentPositionBufferIndex % 2] = TrackProxy->NextFrameIndex;
						}
					}
					else
					{
						TrackProxy->CurrentPositionBufferIndex = 0;
						TrackProxy->PositionBuffers[0].Update(InterpolatedPositions);
						TrackProxy->PositionBuffers[1].Update(InterpolatedMotionVectors);
						TrackProxy->PositionBufferFrameIndices[0] = TrackProxy->FrameIndex;
						TrackProxy->PositionBufferFrameIndices[1] = -1;
						TrackProxy->PositionBufferFrameTimes[0] = Time;
						TrackProxy->PositionBufferFrameTimes[1] = Time;
					}
				}


			}
			else
			{
				// We just don't interpolate between frames if we got GPU to burn we could someday render twice and stipple fade between it :-D like with lods

				// Only bother uploading if anything changed or when the we failed to decode anything make sure update the gpu buffers regardless
				if (bDecodedAnything || bDecoderError)
				{
					//TrackProxy->IndexBuffer.Update(TrackProxy->MeshData->Indices);

					const int32 NumVertices = TrackProxy->MeshData->Positions.Num();

					if (TrackProxy->MeshData->VertexInfo.bHasTangentX)
						TrackProxy->TangentXBuffer.Update(TrackProxy->MeshData->TangentsX);
					if (TrackProxy->MeshData->VertexInfo.bHasTangentZ)
						TrackProxy->TangentZBuffer.Update(TrackProxy->MeshData->TangentsZ);

					if (!TrackProxy->MeshData->VertexInfo.bConstantIndices)
						TrackProxy->IndexBuffer.Update(TrackProxy->MeshData->Indices);

					if (TrackProxy->MeshData->VertexInfo.bHasUV0)
						TrackProxy->TextureCoordinatesBuffer.Update(TrackProxy->MeshData->TextureCoordinates);

					if (TrackProxy->MeshData->VertexInfo.bHasColor0)
						TrackProxy->ColorBuffer.Update(TrackProxy->MeshData->Colors);

					bool bIsCompatibleWithCachedFrame = TrackProxy->Resource->IsTopologyCompatible(
						TrackProxy->PositionBufferFrameIndices[TrackProxy->CurrentPositionBufferIndex % 2],
						TrackProxy->FrameIndex);

					if (!bHasMotionVectors)
					{
						// Initialize both buffers the first frame or when topology changed as we can't render
						// with a previous buffer referencing a buffer from another topology
						if (TrackProxy->CurrentPositionBufferIndex == -1 || !bIsCompatibleWithCachedFrame || bSeeked)
						{
							TrackProxy->PositionBuffers[0].Update(TrackProxy->MeshData->Positions);
							TrackProxy->PositionBuffers[1].Update(TrackProxy->MeshData->Positions);
							TrackProxy->CurrentPositionBufferIndex = 0;
							TrackProxy->PositionBufferFrameIndices[0] = TrackProxy->FrameIndex;
							TrackProxy->PositionBufferFrameIndices[1] = TrackProxy->FrameIndex;
						}
						// We still use the previous frame's buffer as a motion blur previous position. As interpolation is switched
						// off the actual time of this previous frame depends on the geometry cache framerate and playback speed
						// so the motion blur vectors may not really be anything relevant. Do we want to just disable motion blur? 
						// But as an optimization skipping interpolation when the cache fps is near to the actual game fps this is obviously nice...
						else
						{
							TrackProxy->CurrentPositionBufferIndex++;
							TrackProxy->PositionBuffers[TrackProxy->CurrentPositionBufferIndex % 2].Update(TrackProxy->MeshData->Positions);
							TrackProxy->PositionBufferFrameIndices[TrackProxy->CurrentPositionBufferIndex % 2] = TrackProxy->FrameIndex;
						}
					}
					else
					{
						TrackProxy->CurrentPositionBufferIndex = 0;
						TrackProxy->PositionBuffers[0].Update(TrackProxy->MeshData->Positions);
						TrackProxy->PositionBuffers[1].Update(TrackProxy->MeshData->MotionVectors);
						TrackProxy->PositionBufferFrameIndices[0] = TrackProxy->FrameIndex;
						TrackProxy->PositionBufferFrameIndices[1] = -1;
						TrackProxy->PositionBufferFrameTimes[0] = Time;
						TrackProxy->PositionBufferFrameTimes[1] = Time;
					}
				}
			}

		#if 0
			bool bOffloadUpdate = CVarOffloadUpdate.GetValueOnRenderThread() != 0;
			if (TrackProxy->SampleIndex != TrackProxy->UploadedSampleIndex)
			{
				TrackProxy->UploadedSampleIndex = TrackProxy->SampleIndex;

				if (bOffloadUpdate)
				{
					check(false);
					// Only update the size on this thread
					TrackProxy->IndexBuffer.UpdateSizeOnly(TrackProxy->MeshData->Indices.Num());
					TrackProxy->VertexBuffer.UpdateSizeTyped<FNoPositionVertex>(TrackProxy->MeshData->Vertices.Num());

					// Do the interpolation on a worker thread
					FGraphEventRef CompletionFence = FFunctionGraphTask::CreateAndDispatchWhenReady([]()
					{

					}, GET_STATID(STAT_BufferUpdateTask), NULL, ENamedThreads::AnyThread);

					// Queue a command on the RHI thread that waits for the interpolation job and then uploads them to the GPU
					FRHICommandListImmediate &RHICommandList = GetImmediateCommandList_ForRenderCommand();
					new (RHICommandList.AllocCommand<FRHICommandUpdateGeometryCacheBuffer>())FRHICommandUpdateGeometryCacheBuffer(
						CompletionFence,
						TrackProxy->VertexBuffer.VertexBufferRHI,
						TrackProxy->MeshData->Vertices.GetData(),
						TrackProxy->VertexBuffer.GetSizeInBytes(),
						TrackProxy->IndexBuffer.IndexBufferRHI,
						TrackProxy->MeshData->Indices.GetData(),
						TrackProxy->IndexBuffer.SizeInBytes());

					// Upload vertex data
					/*void* RESTRICT Data = (void* RESTRICT)GDynamicRHI->RHILockVertexBuffer(TrackProxy->VertexBuffer.VertexBufferRHI, 0, TrackProxy->VertexBuffer.SizeInBytes(), RLM_WriteOnly);
					FMemory::BigBlockMemcpy(Data, TrackProxy->MeshData->Vertices.GetData(), TrackProxy->VertexBuffer.SizeInBytes());
					GDynamicRHI->RHIUnlockVertexBuffer(TrackProxy->VertexBuffer.VertexBufferRHI);

					// Upload index data
					Data = (void* RESTRICT)GDynamicRHI->RHILockIndexBuffer(TrackProxy->IndexBuffer.IndexBufferRHI, 0, TrackProxy->IndexBuffer.SizeInBytes(), RLM_WriteOnly);
					FMemory::BigBlockMemcpy(Data, TrackProxy->MeshData->Indices.GetData(), TrackProxy->IndexBuffer.SizeInBytes());
					GDynamicRHI->RHIUnlockIndexBuffer(TrackProxy->IndexBuffer.IndexBufferRHI);*/
				}
				else
				{
					TrackProxy->IndexBuffer.Update(TrackProxy->MeshData->Indices);
					TrackProxy->VertexBuffer.Update(TrackProxy->MeshData->Vertices);

					// Initialize both buffers the first frame
					if (TrackProxy->CurrentPositionBufferIndex == -1)
					{
						TrackProxy->PositonBuffers[0].Update(TrackProxy->MeshData->Vertices);
						TrackProxy->PositonBuffers[1].Update(TrackProxy->MeshData->Vertices);
						TrackProxy->CurrentPositionBufferIndex = 0;
					}
					else
					{
						TrackProxy->CurrentPositionBufferIndex++;
						TrackProxy->PositonBuffers[TrackProxy->CurrentPositionBufferIndex%2].Update(TrackProxy->MeshData->Vertices);
					}
				}
			}
		#endif

		}
	}
}

void FGeometryCacheSceneProxy::UpdateSectionWorldMatrix(const int32 SectionIndex, const FMatrix& WorldMatrix)
{
	check(SectionIndex < Tracks.Num() && "Section Index out of range");
	Tracks[SectionIndex]->WorldMatrix = WorldMatrix;
}

void FGeometryCacheSceneProxy::ClearSections()
{
	Tracks.Empty();
}

FGeomCacheVertexFactory::FGeomCacheVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
	: FGeometryCacheVertexVertexFactory(InFeatureLevel)
{

}

void FGeomCacheVertexFactory::Init_RenderThread(const FVertexBuffer* PositionBuffer, const FVertexBuffer* MotionBlurDataBuffer, const FVertexBuffer* TangentXBuffer, const FVertexBuffer* TangentZBuffer, const FVertexBuffer* TextureCoordinateBuffer, const FVertexBuffer* ColorBuffer)
{
	check(IsInRenderingThread());

	// Initialize the vertex factory's stream components.
	FDataType NewData;
	NewData.PositionComponent = FVertexStreamComponent(PositionBuffer, 0, sizeof(FVector), VET_Float3);

	NewData.TextureCoordinates.Add(FVertexStreamComponent(TextureCoordinateBuffer, 0, sizeof(FVector2D), VET_Float2));
	NewData.TangentBasisComponents[0] = FVertexStreamComponent(TangentXBuffer, 0, sizeof(FPackedNormal), VET_PackedNormal);
	NewData.TangentBasisComponents[1] = FVertexStreamComponent(TangentZBuffer, 0, sizeof(FPackedNormal), VET_PackedNormal);
	NewData.ColorComponent = FVertexStreamComponent(ColorBuffer, 0, sizeof(FColor), VET_Color);
	NewData.MotionBlurDataComponent = FVertexStreamComponent(MotionBlurDataBuffer, 0, sizeof(FVector), VET_Float3);

	SetData(NewData);
}

void FGeomCacheVertexFactory::Init(const FVertexBuffer* PositionBuffer, const FVertexBuffer* MotionBlurDataBuffer, const FVertexBuffer* TangentXBuffer, const FVertexBuffer* TangentZBuffer, const FVertexBuffer* TextureCoordinateBuffer, const FVertexBuffer* ColorBuffer)
{
	if (IsInRenderingThread())
	{
		Init_RenderThread(PositionBuffer, MotionBlurDataBuffer, TangentXBuffer, TangentZBuffer, TextureCoordinateBuffer, ColorBuffer);
	}
	else
	{

		ENQUEUE_RENDER_COMMAND(InitGeomCacheVertexFactory)(
			[this, PositionBuffer, MotionBlurDataBuffer, TangentXBuffer, TangentZBuffer, TextureCoordinateBuffer, ColorBuffer](FRHICommandListImmediate& RHICmdList)
			{
			Init_RenderThread(PositionBuffer, MotionBlurDataBuffer, TangentXBuffer, TangentZBuffer, TextureCoordinateBuffer, ColorBuffer);
		});
		FlushRenderingCommands();
	}
}

void FGeomCacheIndexBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	void* Buffer = nullptr;
	IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(uint32), NumIndices * sizeof(uint32), BUF_Static | BUF_ShaderResource, CreateInfo, Buffer);
	RHIUnlockIndexBuffer(IndexBufferRHI);
}

void FGeomCacheIndexBuffer::Update(const TArray<uint32> &Indices)
{
	SCOPE_CYCLE_COUNTER(STAT_IndexBufferUpdate);

	check(IsInRenderingThread());

	void* Buffer = nullptr;

	// We only ever grow in size. Ok for now?
	if (Indices.Num() > NumIndices)
	{
		NumIndices = Indices.Num();
		FRHIResourceCreateInfo CreateInfo;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(uint32), NumIndices * sizeof(uint32), BUF_Static | BUF_ShaderResource, CreateInfo, Buffer);
	}
	else
	{
		// Copy the index data into the index buffer.
		Buffer = RHILockIndexBuffer(IndexBufferRHI, 0, Indices.Num() * sizeof(uint32), RLM_WriteOnly);
	}

	FMemory::Memcpy(Buffer, Indices.GetData(), Indices.Num() * sizeof(uint32));
	RHIUnlockIndexBuffer(IndexBufferRHI);
}

void FGeomCacheIndexBuffer::UpdateSizeOnly(int32 NewNumIndices)
{
	check(IsInRenderingThread());

	// We only ever grow in size. Ok for now?
	if (NewNumIndices > NumIndices)
	{
		FRHIResourceCreateInfo CreateInfo;
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint32), NewNumIndices * sizeof(uint32), BUF_Static | BUF_ShaderResource, CreateInfo);
		NumIndices = NewNumIndices;
	}
}

void FGeomCacheVertexBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	void* BufferData = nullptr;
	VertexBufferRHI = RHICreateAndLockVertexBuffer(SizeInBytes, BUF_Static | BUF_ShaderResource, CreateInfo, BufferData);
	RHIUnlockVertexBuffer(VertexBufferRHI);
}

void FGeomCacheVertexBuffer::UpdateRaw(const void *Data, int32 NumItems, int32 ItemSizeBytes, int32 ItemStrideBytes)
{
	SCOPE_CYCLE_COUNTER(STAT_VertexBufferUpdate);
	int32 NewSizeInBytes = ItemSizeBytes * NumItems;
	bool bCanMemcopy = ItemSizeBytes == ItemStrideBytes;

	void* VertexBufferData = nullptr;

	if (NewSizeInBytes > SizeInBytes)
	{
		SizeInBytes = NewSizeInBytes;
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(SizeInBytes, BUF_Static | BUF_ShaderResource, CreateInfo, VertexBufferData);
	}
	else
	{
		VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, SizeInBytes, RLM_WriteOnly);
	}

	if (bCanMemcopy)
	{
		FMemory::Memcpy(VertexBufferData, Data, NewSizeInBytes);
	}
	else
	{
		int8 *InBytes = (int8 *)Data;
		int8 *OutBytes = (int8 *)VertexBufferData;
		for (int32 ItemId=0; ItemId < NumItems; ItemId++)
		{
			FMemory::Memcpy(OutBytes, InBytes, ItemSizeBytes);
			InBytes += ItemStrideBytes;
			OutBytes += ItemSizeBytes;
		}
	}

	RHIUnlockVertexBuffer(VertexBufferRHI);
}

void FGeomCacheVertexBuffer::UpdateSize(int32 NewSizeInBytes)
{
	if (NewSizeInBytes > SizeInBytes)
	{
		SizeInBytes = NewSizeInBytes;
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer(SizeInBytes, BUF_Static | BUF_ShaderResource, CreateInfo);
	}
}
