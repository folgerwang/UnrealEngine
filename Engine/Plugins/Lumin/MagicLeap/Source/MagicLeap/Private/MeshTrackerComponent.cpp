// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MeshTrackerComponent.h"
#include "MagicLeapHMD.h"
#include "AppFramework.h"
#include "MagicLeapMath.h"
#include "AppEventHandler.h"

#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#if WITH_EDITOR
#include "Editor.h"
#endif
#include "MRMeshComponent.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_meshing.h>
#include <ml_data_array.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

static TAutoConsoleVariable<float> CVarFakeMeshTrackerData(
	TEXT("vr.MagicLeap.FakeMeshTrackerData"),
	0.0f,
	TEXT("If True MeshTrackerComponent will generate some simple fake mesh data.\n"),
	ECVF_Default);

class FMeshTrackerImpl : public MagicLeap::IAppEventHandler
{
public:
	FMeshTrackerImpl()
#if WITH_MLSDK
		: Tracker(ML_INVALID_HANDLE)
		, bUpdateRequested(false)
#endif //WITH_MLSDK
	{
#if WITH_MLSDK
		UnrealToMLMeshTypeMap.Add(EMeshType::Full, MLMeshingType_Full);
		UnrealToMLMeshTypeMap.Add(EMeshType::Blocks, MLMeshingType_Blocks);
		UnrealToMLMeshTypeMap.Add(EMeshType::PointCloud, MLMeshingType_PointCloud);
#endif //WITH_MLSDK
	};

#if WITH_MLSDK
	MLMeshingSettings CreateSettings(const UMeshTrackerComponent& MeshTracker)
	{
		float WorldToMetersScale = 100.0f;
		if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
		{
			const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
			if (AppFramework.IsInitialized())
			{
				WorldToMetersScale = AppFramework.GetWorldToMetersScale();
			}
		}

		MLMeshingSettings Settings;
		FMemory::Memset(&Settings, 0, sizeof(Settings));

		// Convert all of the times from float seconds -> uint64 nanoseconds.
		Settings.meshing_poll_time = static_cast<uint64_t>(MeshTracker.MeshingPollTime * 1e9);

		Settings.mesh_type = UnrealToMLMeshTypeMap[MeshTracker.MeshType];

		Settings.bounds_center = MagicLeap::ToMLVector(BoundsCenter, WorldToMetersScale);
		Settings.bounds_rotation = MagicLeap::ToMLQuat(BoundsRotation);
		Settings.bounds_extents = MagicLeap::ToMLVector(MeshTracker.BoundingVolume->GetScaledBoxExtent(), WorldToMetersScale);

		// MagicLeap::ToMLVector() causes the Z component to be negated.
		// The bounds were thus invalid and resulted in everything being meshed. 
		// This provides the content devs with an option to ignore the bounding volume at will.
		// TODO: Can this be improved?
		if (!MeshTracker.IgnoreBoundingVolume)
		{
			Settings.bounds_extents.x = FMath::Abs<float>(Settings.bounds_extents.x);
			Settings.bounds_extents.y = FMath::Abs<float>(Settings.bounds_extents.y);
			Settings.bounds_extents.z = FMath::Abs<float>(Settings.bounds_extents.z);
		}

		Settings.target_number_triangles = static_cast<uint32>(MeshTracker.TargetNumberTriangles);
		Settings.target_number_triangles_per_block = static_cast<uint32>(MeshTracker.TargetNumberTriangles);

		Settings.enable_meshing = MeshTracker.ScanWorld;
		Settings.index_order_ccw = false;
		Settings.fill_holes = MeshTracker.FillGaps;
		Settings.fill_hole_length = MeshTracker.PerimeterOfGapsToFill / WorldToMetersScale;
		Settings.compute_normals = MeshTracker.RequestNormals;
		Settings.planarize = MeshTracker.Planarize;
		Settings.remove_disconnected_components = MeshTracker.RemoveDisconnectedSections;
		Settings.disconnected_component_area = MeshTracker.DisconnectedSectionArea / (WorldToMetersScale * WorldToMetersScale);
		Settings.request_vertex_confidence = MeshTracker.RequestVertexConfidence;
		Settings.remove_mesh_skirt = MeshTracker.RemoveOverlappingTriangles;

		return Settings;
	};

	bool SettingsChanged(const MLMeshingSettings& lhs, const MLMeshingSettings& rhs, const UMeshTrackerComponent& MeshTracker)
	{
		return (lhs.meshing_poll_time != rhs.meshing_poll_time ||
			lhs.mesh_type != rhs.mesh_type ||
			MagicLeap::ToFVector(lhs.bounds_extents, 1.0f) != MagicLeap::ToFVector(rhs.bounds_extents, 1.0f) ||
			lhs.target_number_triangles != rhs.target_number_triangles ||
			lhs.target_number_triangles_per_block != rhs.target_number_triangles_per_block ||
			lhs.enable_meshing != rhs.enable_meshing ||
			lhs.fill_holes != rhs.fill_holes ||
			lhs.planarize != rhs.planarize ||
			lhs.remove_disconnected_components != rhs.remove_disconnected_components ||
			lhs.disconnected_component_area != rhs.disconnected_component_area ||
			lhs.request_vertex_confidence != rhs.request_vertex_confidence ||
			lhs.remove_mesh_skirt != rhs.remove_mesh_skirt ||
			FVector(lhs.bounds_center.xyz.x - rhs.bounds_center.xyz.x,
				lhs.bounds_center.xyz.y - rhs.bounds_center.xyz.y,
				lhs.bounds_center.xyz.z - rhs.bounds_center.xyz.z).Size() > MeshTracker.MinDistanceRescan
			);
		// TODO: (njain) Account for a minimum change in bounds_rotation and bounds_extents.
	}
#endif //WITH_MLSDK

	void OnAppPause() override
	{
#if WITH_MLSDK
		bWasSystemEnabledOnPause = CurrentSettings.enable_meshing;

		if (!bWasSystemEnabledOnPause)
		{
			UE_LOG(LogMagicLeap, Log, TEXT("Mesh tracking was not enabled at time of application pause."));
		}
		else
		{
			if (!MLHandleIsValid(Tracker))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Mesh tracker was invalid on application pause."));
			}
			else
			{
				CurrentSettings.enable_meshing = false;

				if (!MLMeshingUpdate(Tracker, &CurrentSettings))
				{
					UE_LOG(LogMagicLeap, Error, TEXT("Failed to disable mesh tracker on application pause."));
				}
				else
				{
					UE_LOG(LogMagicLeap, Log, TEXT("Mesh tracker paused until app resumes."));
				}
			}
		}
#endif //WITH_MLSDK
	}

	void OnAppResume() override
	{
#if WITH_MLSDK
		if (!bWasSystemEnabledOnPause)
		{
			UE_LOG(LogMagicLeap, Log, TEXT("Not resuming mesh tracker as it was not enabled at time of application pause."));
		}
		else
		{
			if (!MLHandleIsValid(Tracker))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Mesh tracker was invalid on application resume."));
			}
			else
			{
				CurrentSettings.enable_meshing = true;

				if (!MLMeshingUpdate(Tracker, &CurrentSettings))
				{
					UE_LOG(LogMagicLeap, Error, TEXT("Failed to re-enable mesh tracker on application resume."));
				}
				else
				{
					UE_LOG(LogMagicLeap, Log, TEXT("Mesh tracker re-enabled on application resume."));
				}
			}
		}
#endif //WITH_MLSDK
	}

public:
#if WITH_MLSDK
	MLCoordinateFrameUID CoordinateFrame;
	MLHandle Tracker;
	MLMeshingStaticData Data;

	MLDataArrayDiff GroupDiff;
	int32 SectionCounter;

	MLMeshingSettings CurrentSettings;

	struct MeshCache
	{
		MLDataArrayDiff Diff;
		int32 Section;
	};

	TMap<uint64, MeshCache> MeshHandleCacheMap;
#endif //WITH_MLSDK

	// Keep a copy of the mesh data here.  MRMeshComponent will use it from the game and render thread.
	struct FMLCachedMeshData
	{
		typedef TSharedPtr<FMLCachedMeshData, ESPMode::ThreadSafe> SharedPtr;
		
		FMeshTrackerImpl* Owner = nullptr;
		
		IMRMesh::FBrickId BrickId = 0;
		TArray<FVector> Vertices;
		TArray<uint32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UV0;
		TArray<FColor> VertexColors;
		TArray<FPackedNormal> Tangents;
		TArray<float> Confidence;

		void Recycle(SharedPtr& MeshData)
		{
			check(Owner);
			FMeshTrackerImpl* TempOwner = Owner;
			Owner = nullptr;
	
			BrickId = 0;
			Vertices.Reset();
			Triangles.Reset();
			Normals.Reset();
			UV0.Reset();
			VertexColors.Reset();
			Tangents.Reset();
			Confidence.Reset();
	
			TempOwner->FreeMeshDataCache(MeshData);
		}

		void Init(FMeshTrackerImpl* InOwner)
		{
			check(!Owner);
			Owner = InOwner;
		}
	};


	// This receipt will be kept in the FSendBrickDataArgs to ensure the cached data outlives MRMeshComponent use of it.
	class FMeshTrackerComponentBrickDataReceipt : public IMRMesh::FBrickDataReceipt
	{
	public:
		FMeshTrackerComponentBrickDataReceipt(FMLCachedMeshData::SharedPtr& MeshData) :
			CachedMeshData(MeshData)
		{
		}
		~FMeshTrackerComponentBrickDataReceipt() override
		{
			CachedMeshData->Recycle(CachedMeshData);
		}
	private:
		FMLCachedMeshData::SharedPtr CachedMeshData;
	};
	
	FMLCachedMeshData::SharedPtr AquireMeshDataCache()
	{
		if (FreeCachedMeshDatas.Num() > 0)
		{
			FScopeLock ScopeLock(&FreeCachedMeshDatasMutex);
			FMLCachedMeshData::SharedPtr CachedMeshData(FreeCachedMeshDatas.Pop(false));
			CachedMeshData->Init(this);
			return CachedMeshData;
		}
		else
		{
			FMLCachedMeshData::SharedPtr CachedMeshData(new FMLCachedMeshData());
			CachedMeshData->Init(this);
			CachedMeshDatas.Add(CachedMeshData);
			return CachedMeshData;
		}
	}

	void FreeMeshDataCache(FMLCachedMeshData::SharedPtr& DataCache)
	{
		FScopeLock ScopeLock(&FreeCachedMeshDatasMutex);
		FreeCachedMeshDatas.Add(DataCache);
	}

	FVector BoundsCenter;
	FQuat BoundsRotation;

#if WITH_MLSDK
	bool bUpdateRequested;
#endif //WITH_MLSDK

	bool Create(const UMeshTrackerComponent& MeshTracker)
	{
#if WITH_MLSDK
		if (!MLHandleIsValid(Tracker))
		{
			// Create the tracker on demand.
			UE_LOG(LogMagicLeap, Display, TEXT("Creating Mesh Tracker"));
			MLMeshingSettings Settings = CreateSettings(MeshTracker);
			Tracker = MLMeshingCreate(&Settings);

			if (!MLHandleIsValid(Tracker))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Could not create mesh tracker."));
				return false;
			}

			CurrentSettings = Settings;
			FMemory::Memset(&GroupDiff, 0, sizeof(MLDataArrayDiff));
			SectionCounter = 0;
			// TODO: pull out of current scope and handle separately.
			if (MLMeshingGetStaticData(Tracker, &Data))
			{
				CoordinateFrame = Data.frame;
			}
		}
#endif //WITH_MLSDK
		return true;
	}

	void Destroy()
	{
#if WITH_MLSDK
		if (MLHandleIsValid(Tracker))
		{
			bool bResult = MLMeshingDestroy(Tracker);
			if (!bResult)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Error destroying mesh tracker."));
			}
			Tracker = ML_INVALID_HANDLE;
		}
#endif //WITH_MLSDK
	}

private:
#if WITH_MLSDK
	TMap<EMeshType, MLMeshingType> UnrealToMLMeshTypeMap;
#endif //WITH_MLSDK

	// A free list to recycle the CachedMeshData instances.  
	TArray<FMLCachedMeshData::SharedPtr> CachedMeshDatas;
	TArray<FMLCachedMeshData::SharedPtr> FreeCachedMeshDatas;
	FCriticalSection FreeCachedMeshDatasMutex; //The free list may be pushed/popped from multiple threads.
};

UMeshTrackerComponent::UMeshTrackerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, VertexColorFromConfidenceZero(FLinearColor::Red)
	, VertexColorFromConfidenceOne(FLinearColor::Blue)
	, Impl(new FMeshTrackerImpl())
{

	// Make sure this component ticks
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	bAutoActivate = true;

	BoundingVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundingVolume"));
	BoundingVolume->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	BoundingVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundingVolume->SetCanEverAffectNavigation(false);
	BoundingVolume->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
	BoundingVolume->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	BoundingVolume->SetGenerateOverlapEvents(false);
	// Recommended default box extents for meshing - 10m (5m radius)
	BoundingVolume->SetBoxExtent(FVector(1000, 1000, 1000), false);

	BlockVertexColors.Add(FColor::Blue);
	BlockVertexColors.Add(FColor::Red);
	BlockVertexColors.Add(FColor::Green);
	BlockVertexColors.Add(FColor::Yellow);
	BlockVertexColors.Add(FColor::Cyan);
	BlockVertexColors.Add(FColor::Magenta);

#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::PrePIEEnded.AddUObject(this, &UMeshTrackerComponent::PrePIEEnded);
	}
#endif
}

UMeshTrackerComponent::~UMeshTrackerComponent()
{
	delete Impl;
}

void UMeshTrackerComponent::ConnectMRMesh(UMRMeshComponent* InMRMeshPtr)
{
	if (MRMesh)
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("MeshTrackerComponent already has a MRMesh connected.  Ignoring this connect."));
		return;
	}
	else if (InMRMeshPtr->IsConnected())
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("MRMesh is already connected to a UMeshTrackerComponent. Ignoring this connect."));
		return;
	}
	else
	{
		InMRMeshPtr->SetConnected(true);
		MRMesh = InMRMeshPtr;
	}
}

void UMeshTrackerComponent::DisconnectMRMesh(class UMRMeshComponent* InMRMeshPtr)
{
	if (!MRMesh)
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("MeshTrackerComponent MRMesh is already disconnected. Ignoring this diconnect."));
		return;
	}
	else if (InMRMeshPtr != MRMesh)
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("MeshTrackerComponent MRMesh given is not the MRMesh connected. Ignoring this diconnect."));
		return;
	}
	else
	{
		check(MRMesh->IsConnected());
		MRMesh->SetConnected(false);
	}
	MRMesh = nullptr;
}

#if WITH_EDITOR
void UMeshTrackerComponent::PostEditChangeProperty(FPropertyChangedEvent& e)
{
#if WITH_MLSDK
	if (MLHandleIsValid(Impl->Tracker) && e.Property != nullptr)
	{
		MLMeshingSettings Settings = Impl->CreateSettings(*this);
		MLMeshingUpdate(Impl->Tracker, &Settings);
	}
#endif //WITH_MLSDK

	Super::PostEditChangeProperty(e);
}
#endif

bool UMeshTrackerComponent::ForceMeshUpdate()
{
#if WITH_MLSDK
	if (MLHandleIsValid(Impl->Tracker))
	{
		Impl->bUpdateRequested = MLMeshingRefresh(Impl->Tracker);
	}
	return Impl->bUpdateRequested;
#else
	return true;
#endif //WITH_MLSDK
}

void UMeshTrackerComponent::LogMLDataArray(const MLDataArray& Data) const
{
#if WITH_MLSDK
	UE_LOG(LogMagicLeap, Log, TEXT("  Data:"));
	const uint64 timestamp = Data.timestamp;
	const uint32 stream_count = Data.stream_count;
	UE_LOG(LogMagicLeap, Log, TEXT("    timestamp:    %llu"), timestamp);
	UE_LOG(LogMagicLeap, Log, TEXT("    stream_count: %i"), stream_count);
	for (uint32 i = 0; i < stream_count; ++i)
	{
		const MLDataArrayStream& stream = Data.streams[i];
		UE_LOG(LogMagicLeap, Log, TEXT("      stream: %i"), i);
		UE_LOG(LogMagicLeap, Log, TEXT("        type: %i"), (int32)stream.type);
		UE_LOG(LogMagicLeap, Log, TEXT("        count: %i"), stream.count);
		UE_LOG(LogMagicLeap, Log, TEXT("        data_size: %i"), stream.data_size);
	}
#endif //WITH_MLSDK
}

void UMeshTrackerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
#if WITH_MLSDK

	if (!MRMesh)
	{
		return;
	}

	static const auto FakeMeshTrackerDataCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("vr.MagicLeap.FakeMeshTrackerData"));
	if (FakeMeshTrackerDataCVar != nullptr && FakeMeshTrackerDataCVar->GetInt())
	{
		TickWithFakeData();
		return;
	}

	if (!GEngine->XRSystem.IsValid() || !GEngine->XRSystem->GetHMDDevice())
	{
		return;
	}
	const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
	if (!AppFramework.IsInitialized())
	{
		return;
	}

	if (!Impl->Create(*this))
	{
		return;
	}

	// Update the bounding box.
	FTransform PoseInverse = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this).Inverse();
	PoseInverse.ConcatenateRotation(BoundingVolume->GetComponentQuat());
	Impl->BoundsCenter = PoseInverse.TransformPosition(BoundingVolume->GetComponentLocation());
	Impl->BoundsRotation = PoseInverse.GetRotation();

	// Update the tracker on demand.
	MLMeshingSettings NewSettings = Impl->CreateSettings(*this);
	// Hack because MLMeshSettings does not support the comparison operator.
	if (Impl->SettingsChanged(Impl->CurrentSettings, NewSettings, *this))
	{
		UE_LOG(LogMagicLeap, Display, TEXT("Updating Mesh Tracker Settings"));
		if (Impl->CurrentSettings.mesh_type != NewSettings.mesh_type && MRMesh != nullptr)
		{
			MRMesh->Clear();
		}
		MLMeshingUpdate(Impl->Tracker, &NewSettings);
		Impl->CurrentSettings = NewSettings;
	}

	// Update the general position of the object so the mesh is in the right position.
	float WorldToMetersScale = AppFramework.GetWorldToMetersScale();

	EFailReason FailReason = EFailReason::None;
	FTransform Pose = FTransform::Identity;
	if (AppFramework.GetTransform(Impl->Data.frame, Pose, FailReason))
	{
		if (MRMesh)
		{
			MRMesh->SendRelativeTransform(Pose);
		}
	}
	else if (FailReason == EFailReason::NaNsInTransform)
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("NaNs in mesh frame transform."));
	}

	// Only draw meshes during the scan phase in order to maintain a steady framerate.
	if (ScanWorld || Impl->bUpdateRequested)
	{
		// Collect current mesh handles.
		TSet<uint64> CurrentMeshHandleSet;

		// Lock group data array.
		MLDataArray GroupData;
		MLDataArrayLockResult mainDataArray = MLDataArrayTryLock(Impl->Data.meshes, &GroupData, &Impl->GroupDiff);
		if (mainDataArray == MLDataArrayLockResult_New)
		{
			// Work with the locked group MLDataArray.
			{
				//UE_LOG(LogMagicLeap, Log, TEXT("UMeshTrackerComponent::TickComponent found new group data."));
				//LogMLDataArray(GroupData);

				Impl->bUpdateRequested = false;
				MLDataArrayHandle* groupBegin = GroupData.streams[0].handle_array;
				MLDataArrayHandle* groupEnd = GroupData.streams[0].handle_array + GroupData.streams[0].count;

				for (MLDataArrayHandle* groupIterator = groupBegin; groupIterator != groupEnd; ++groupIterator)
				{
					CurrentMeshHandleSet.Add(*groupIterator);
				}

				// Unlock group data array.
				MLDataArrayUnlock(Impl->Data.meshes);
			}

			// Clear unnecessary meshes.
			TMap<uint64, FMeshTrackerImpl::MeshCache> NewMeshHandleCacheMap;
			for (const auto& MeshHandleCachePair : Impl->MeshHandleCacheMap)
			{
				if (!CurrentMeshHandleSet.Contains(MeshHandleCachePair.Key))
				{
					// We don't add any sections to the procedural mesh component for point clouds.
					if (MeshType != EMeshType::PointCloud)
					{
						// MRMeshComponent may reference this data from multiple threads
						const static TArray<FVector> EmptyVertices;
						const static TArray<FVector2D> EmptyUVs;
						const static TArray<FPackedNormal> EmptyTangents;
						const static TArray<FColor> EmptyVertexColors;
						const static TArray<uint32> EmptyTriangles;
						if (MRMesh)
						{
							static_cast<IMRMesh*>(MRMesh)->SendBrickData(IMRMesh::FSendBrickDataArgs
							{
								nullptr,
								MeshHandleCachePair.Key,
								EmptyVertices,
								EmptyUVs,
								EmptyTangents,
								EmptyVertexColors,
								EmptyTriangles
							}
							);
						}
					}

					OnMeshTrackerUpdated.Broadcast(MeshHandleCachePair.Value.Section, TArray<FVector>(), TArray<int32>(), TArray<FVector>(), TArray<float>());
				}
				else
				{
					NewMeshHandleCacheMap.Add(MeshHandleCachePair.Key, MeshHandleCachePair.Value);
				}
			}
			Impl->MeshHandleCacheMap = NewMeshHandleCacheMap;

			// Add new mesh handles.
			for (auto CurrentMeshHandle : CurrentMeshHandleSet)
			{
				if (!Impl->MeshHandleCacheMap.Contains(CurrentMeshHandle))
				{
					FMeshTrackerImpl::MeshCache CurrentMeshCache;
					FMemory::Memset(&CurrentMeshCache.Diff, 0, sizeof(MLDataArrayDiff));
					CurrentMeshCache.Section = (Impl->SectionCounter)++;
					Impl->MeshHandleCacheMap.Add(CurrentMeshHandle, CurrentMeshCache);
					// We don't add any sections to the procedural mesh component for point clouds.
					// If number of sections is more than number of materials for this component, set the first section's material as the new section's material.
				}
			}
		}

		for (auto& MeshData : Impl->MeshHandleCacheMap)
		{
			// Lock mesh data array.
			uint64 CurrentMeshHandle = MeshData.Key;
			MLDataArray CurrentMeshData;
			MLDataArrayLockResult meshDataArray = MLDataArrayTryLock(CurrentMeshHandle, &CurrentMeshData, &MeshData.Value.Diff);
			if (meshDataArray == MLDataArrayLockResult_New)
			{
				// Collect current mesh data.
				FMeshTrackerImpl::FMLCachedMeshData::SharedPtr CurrentMeshDataCache = Impl->AquireMeshDataCache();
				CurrentMeshDataCache->BrickId = CurrentMeshHandle;

				// Work with the locked mesh MLDataArray.
				{
					//UE_LOG(LogMagicLeap, Log, TEXT("UMeshTrackerComponent::TickComponent found new mesh data."));
					//LogMLDataArray(CurrentMeshData);

					Impl->bUpdateRequested = false;

					// TODO: Copying the position and normal arrays could be a mem copy if we're sure about the memory layout of things.

					// Read position stream
					{
						const uint32 PositionCount = CurrentMeshData.streams[Impl->Data.position_stream_index].count;
						CurrentMeshDataCache->Vertices.Reserve(PositionCount);
						for (uint32 i = 0; i < PositionCount; ++i)
						{
							const auto &position = CurrentMeshData.streams[Impl->Data.position_stream_index].xyz_array[i];
							CurrentMeshDataCache->Vertices.Add(MagicLeap::ToFVector(position, WorldToMetersScale));
						}
					}

					// Read triangle (indices) stream
					if (Impl->Data.triangle_index_stream_index == MLDataArray_InvalidStreamIndex || CurrentMeshData.streams[Impl->Data.triangle_index_stream_index].uint_array == nullptr)
					{
						const int32 TriangleCount = CurrentMeshData.streams[Impl->Data.triangle_index_stream_index].count;
						CurrentMeshDataCache->Triangles.AddZeroed(TriangleCount);
					}
					else
					{
						CurrentMeshDataCache->Triangles.Append(CurrentMeshData.streams[Impl->Data.triangle_index_stream_index].uint_array, CurrentMeshData.streams[Impl->Data.triangle_index_stream_index].count);
					}

					// Read normal stream
					{
						uint32 NormalsCount = 0;
						if (Impl->Data.normal_stream_index != MLDataArray_InvalidStreamIndex && CurrentMeshData.streams[Impl->Data.normal_stream_index].type != MLDataArrayType_None)
						{
							NormalsCount = CurrentMeshData.streams[Impl->Data.normal_stream_index].count;
							CurrentMeshDataCache->Normals.Reserve(NormalsCount);
							for (uint32 i = 0; i < NormalsCount; ++i)
							{
								const auto &normal = CurrentMeshData.streams[Impl->Data.normal_stream_index].xyz_array[i];
								CurrentMeshDataCache->Normals.Add(MagicLeap::ToFVector(normal, 1.0f));
							}
						}
						else
						{
							// Vulkan requires that tangent data exist, so we provide fake normals and tangents even if it does not

							NormalsCount = CurrentMeshDataCache->Vertices.Num();
							CurrentMeshDataCache->Normals.Reserve(NormalsCount);
							for (uint32 i = 0; i < NormalsCount; ++i)
							{
								FVector Normal = CurrentMeshDataCache->Vertices[i];
								Normal.Normalize();
								CurrentMeshDataCache->Normals.Add(Normal);
							}
						}

						// Also write normals into tangents
						CurrentMeshDataCache->Tangents.Reserve(NormalsCount * 2);
						for (uint32 i = 0; i < NormalsCount; ++i)
						{
							const FVector Normal = CurrentMeshDataCache->Normals[i];
							const FVector NonNormal = Normal.X < Normal.Z ? FVector(0, 0, 1) : FVector(0, 1, 0);
							const FVector TangentX = FVector::CrossProduct(Normal, NonNormal);
							CurrentMeshDataCache->Tangents.Add(TangentX);
							CurrentMeshDataCache->Tangents.Add(Normal);
						}

					}

					// Read confidence stream
					if (Impl->Data.confidence_stream_index != MLDataArray_InvalidStreamIndex && CurrentMeshData.streams[Impl->Data.confidence_stream_index].type != MLDataArrayType_None)
					{
						const uint32 ConfidenceCount = CurrentMeshData.streams[Impl->Data.confidence_stream_index].count;
						CurrentMeshDataCache->Confidence.Reserve(ConfidenceCount);
						for (uint32 i = 0; i < ConfidenceCount; ++i)
						{
							const float Confidence = CurrentMeshData.streams[Impl->Data.confidence_stream_index].float_array[i];
							CurrentMeshDataCache->Confidence.Add(Confidence);
						}
					}

					// Write VertexColor
					{
						switch (VertexColorMode)
						{
						case EMLMeshVertexColorMode::Confidence:
						{
							const uint32 ConfidenceCount = CurrentMeshDataCache->Confidence.Num();
							const uint32 PositionCount = CurrentMeshDataCache->Vertices.Num();
							// Length of VertexColor array needs to be same as length of Position array.
							CurrentMeshDataCache->VertexColors.Reserve(PositionCount);
							for (uint32 i = 0; i < ConfidenceCount; ++i)
							{
								const float Confidence = CurrentMeshDataCache->Confidence[i];
								const FLinearColor VertexColor = FMath::Lerp(VertexColorFromConfidenceZero, VertexColorFromConfidenceOne, Confidence);
								CurrentMeshDataCache->VertexColors.Add(VertexColor.ToFColor(false));
							}
							for (uint32 i = ConfidenceCount; i < PositionCount; ++i)
							{
								CurrentMeshDataCache->VertexColors.Add(VertexColorFromConfidenceZero.ToFColor(false));
							}
							break;
						}
						case EMLMeshVertexColorMode::Block:
						{
							const uint32 VertexCount = CurrentMeshDataCache->Vertices.Num();
							const uint32 ColorCount = BlockVertexColors.Num();
							const FColor VertexColor = ColorCount > 0 ? BlockVertexColors[MeshData.Value.Section % ColorCount] : FColor::White;
							if (BlockVertexColors.Num() == 0)
							{
								UE_LOG(LogMagicLeap, Warning, TEXT("MeshTracker is in EMLMeshVertexColorMode::Block, but has no BlockVertexColors set.  Using White for all blocks."));
							}
							for (uint32 i = 0; i < VertexCount; ++i)
							{
								CurrentMeshDataCache->VertexColors.Add(VertexColor);
							}
							break;
						}
						case EMLMeshVertexColorMode::None:
						{
							// Vulkan requires that we fill everything in.
							const uint32 VertexCount = CurrentMeshDataCache->Vertices.Num();
							for (uint32 i = 0; i < VertexCount; ++i)
							{
								CurrentMeshDataCache->VertexColors.Add(FColor::White);
							}
							break;
						}
						default:
							check(false);
						}
					}

					// Unlock mesh data array.
					MLDataArrayUnlock(CurrentMeshHandle);
				}

				// Write UVs
				{
					const uint32 VertexCount = CurrentMeshDataCache->Vertices.Num();
					for (uint32 i = 0; i < VertexCount; ++i)
					{
						const float FakeCoord = (float)i / (float)VertexCount;
						CurrentMeshDataCache->UV0.Add(FVector2D(FakeCoord, FakeCoord));
					}
				}

				// We don't add any sections to the procedural mesh component for point clouds.
				if (MeshType != EMeshType::PointCloud)
				{
					// Create or Update. Create on existing mesh section will overwrite that section.

					if (MRMesh)
					{
						static_cast<IMRMesh*>(MRMesh)->SendBrickData(IMRMesh::FSendBrickDataArgs
						{
							TSharedPtr<IMRMesh::FBrickDataReceipt, ESPMode::ThreadSafe>(new FMeshTrackerImpl::FMeshTrackerComponentBrickDataReceipt(CurrentMeshDataCache)),
							CurrentMeshDataCache->BrickId,
							CurrentMeshDataCache->Vertices,
							CurrentMeshDataCache->UV0,
							CurrentMeshDataCache->Tangents,
							CurrentMeshDataCache->VertexColors,
							CurrentMeshDataCache->Triangles
						}
						);
					}
				}
				if (OnMeshTrackerUpdated.IsBound())
				{
					// Hack because blueprints don't support uint32.
					TArray<int32> Triangles(reinterpret_cast<const int32*>(CurrentMeshDataCache->Triangles.GetData()), CurrentMeshDataCache->Triangles.Num());
					OnMeshTrackerUpdated.Broadcast(MeshData.Value.Section, CurrentMeshDataCache->Vertices, Triangles, CurrentMeshDataCache->Normals, CurrentMeshDataCache->Confidence);
				}
			}
		}
	}
#endif //WITH_MLSDK
}

void UMeshTrackerComponent::TickWithFakeData()
{
#if WITH_MLSDK
	// fake only works at default worldscale
	float WorldToMetersScale = 100;

	FTransform Pose = FTransform::Identity;
	if (MRMesh)
	{
		MRMesh->SendRelativeTransform(Pose);
	}

	// Only draw meshes during the scan phase in order to maintain a steady framerate.
	if (ScanWorld || Impl->bUpdateRequested)
	{
		// Collect current mesh handles.
		TSet<uint64> CurrentMeshHandleSet;

		// Figure out new fake data mode, so we can cycle through a few different setups.
		static int period = 60;
		static int count = 0;
		count = (count + 1) % (period * 2);
		bool bTest = count < period;
		static bool bOldTest = false;

		static int modes = 3;
		bool bModeChanged = false;
		static int oldMode = -1;
		int newMode = 0;
		if (bOldTest != bTest)
		{
			bOldTest = bTest;
			bModeChanged = true;

			newMode = (oldMode + 1) % modes;
			oldMode = newMode;
		}
		
		if (bModeChanged)
		{
			UE_LOG(LogMagicLeap, Display, TEXT("TickWithFakeData() mode changed to %i"), newMode);
			Impl->bUpdateRequested = false;

			if (newMode != 0)
			{
				CurrentMeshHandleSet.Add(newMode);
			}

			// Clear unnecessary meshes.
			TMap<uint64, FMeshTrackerImpl::MeshCache> NewMeshHandleCacheMap;
			for (const auto& MeshHandleCachePair : Impl->MeshHandleCacheMap)
			{
				if (!CurrentMeshHandleSet.Contains(MeshHandleCachePair.Key))
				{
					// We don't add any sections to the procedural mesh component for point clouds.
					//if (MeshType != EMeshType::PointCloud)
					{
						static TArray<FVector> EmptyVertices;
						const static TArray<FVector2D> EmptyUVs;
						const static TArray<FPackedNormal> EmptyTangents;
						static TArray<FColor> EmptyVertexColors;
						static TArray<uint32> EmptyTriangles;
						if (MRMesh)
						{
							UE_LOG(LogMagicLeap, Display, TEXT("TickWithFakeData() sending empty brick to remove %llu"), MeshHandleCachePair.Key);
							static_cast<IMRMesh*>(MRMesh)->SendBrickData(IMRMesh::FSendBrickDataArgs
							{
								nullptr,
								MeshHandleCachePair.Key,
								EmptyVertices,
								EmptyUVs,
								EmptyTangents,
								EmptyVertexColors,
								EmptyTriangles
							}
							);
						}
					}
					OnMeshTrackerUpdated.Broadcast(MeshHandleCachePair.Value.Section, TArray<FVector>(), TArray<int32>(), TArray<FVector>(), TArray<float>());
				}
				else
				{
					NewMeshHandleCacheMap.Add(MeshHandleCachePair.Key, MeshHandleCachePair.Value);
				}
			}
			Impl->MeshHandleCacheMap = NewMeshHandleCacheMap;

			// Add new mesh handles.
			for (auto CurrentMeshHandle : CurrentMeshHandleSet)
			{
				if (!Impl->MeshHandleCacheMap.Contains(CurrentMeshHandle))
				{
					FMeshTrackerImpl::MeshCache CurrentMeshCache;
					memset(&CurrentMeshCache.Diff, 0, sizeof(MLDataArrayDiff));
					CurrentMeshCache.Section = (Impl->SectionCounter)++;
					Impl->MeshHandleCacheMap.Add(CurrentMeshHandle, CurrentMeshCache);
				}
			}
		}

		for (auto& MeshData : Impl->MeshHandleCacheMap)
		{
			// Lock mesh data array.
			uint64 CurrentMeshHandle = MeshData.Key;
			if (bModeChanged && CurrentMeshHandle == newMode)
			{
				Impl->bUpdateRequested = false;
				// Collect current mesh data.
				FMeshTrackerImpl::FMLCachedMeshData::SharedPtr CurrentMeshDataCache = Impl->AquireMeshDataCache();
				CurrentMeshDataCache->BrickId = CurrentMeshHandle;

				FVector Origin = FVector((CurrentMeshHandle + 1) * 50, 0, 0);
				static FVector Extents(20.0f, 20.0f, 50.0f);

				const int32 VertCount = 8;
				CurrentMeshDataCache->Vertices.Reserve(VertCount);

				CurrentMeshDataCache->Vertices.Add(FVector(Origin.X + Extents.X, Origin.Y - Extents.Y, Origin.Z + Extents.Z));  // 0
				CurrentMeshDataCache->Vertices.Add(FVector(Origin.X + Extents.X, Origin.Y + Extents.Y, Origin.Z + Extents.Z));  // 1
				CurrentMeshDataCache->Vertices.Add(FVector(Origin.X + Extents.X, Origin.Y + Extents.Y, Origin.Z - Extents.Z));  // 2
				CurrentMeshDataCache->Vertices.Add(FVector(Origin.X + Extents.X, Origin.Y - Extents.Y, Origin.Z - Extents.Z));  // 3
				CurrentMeshDataCache->Vertices.Add(FVector(Origin.X - Extents.X, Origin.Y - Extents.Y, Origin.Z + Extents.Z));  // 4
				CurrentMeshDataCache->Vertices.Add(FVector(Origin.X - Extents.X, Origin.Y + Extents.Y, Origin.Z + Extents.Z));  // 5
				CurrentMeshDataCache->Vertices.Add(FVector(Origin.X - Extents.X, Origin.Y + Extents.Y, Origin.Z - Extents.Z));  // 6
				CurrentMeshDataCache->Vertices.Add(FVector(Origin.X - Extents.X, Origin.Y - Extents.Y, Origin.Z - Extents.Z));  // 7

				CurrentMeshDataCache->UV0.Reserve(VertCount);
				CurrentMeshDataCache->VertexColors.Reserve(VertCount);
				for (int i = 0; i < VertCount; ++i)
				{
					int imod = i % 10;
					CurrentMeshDataCache->VertexColors.Emplace(FColor::MakeRandomColor());
					CurrentMeshDataCache->UV0.Add(FVector2D(imod*0.1f, imod*0.1f));
				}
				
				// Vulkan requires that tangent data exist

				CurrentMeshDataCache->Normals.Reserve(VertCount);
				for (uint32 i = 0; i < VertCount; ++i)
				{
					CurrentMeshDataCache->Normals.Add(FVector(0,0,1));
				}

				// Also write normals into tangents
				CurrentMeshDataCache->Tangents.Reserve(VertCount * 2);
				for (uint32 i = 0; i < VertCount; ++i)
				{
					const FVector Normal = CurrentMeshDataCache->Normals[i];
					const FVector NonNormal = Normal.X < Normal.Z ? FVector(0, 0, 1) : FVector(0, 1, 0);
					const FVector TangentX = FVector::CrossProduct(Normal, NonNormal);
					CurrentMeshDataCache->Tangents.Add(TangentX);
					CurrentMeshDataCache->Tangents.Add(Normal);
				}
				
				CurrentMeshDataCache->Triangles.Reserve(6 * 2 * 3);
				// Read triangle (indices) stream
				// This makes half the triangles of a box, one per side.
				//CurrentMeshDataCache->Triangles.Add(0);
				//CurrentMeshDataCache->Triangles.Add(1);
				//CurrentMeshDataCache->Triangles.Add(2);

				CurrentMeshDataCache->Triangles.Add(0);
				CurrentMeshDataCache->Triangles.Add(2);
				CurrentMeshDataCache->Triangles.Add(3);

				//CurrentMeshDataCache->Triangles.Add(0);
				//CurrentMeshDataCache->Triangles.Add(4);
				//CurrentMeshDataCache->Triangles.Add(1);

				CurrentMeshDataCache->Triangles.Add(1);
				CurrentMeshDataCache->Triangles.Add(4);
				CurrentMeshDataCache->Triangles.Add(5);

				//CurrentMeshDataCache->Triangles.Add(7);
				//CurrentMeshDataCache->Triangles.Add(5);
				//CurrentMeshDataCache->Triangles.Add(4);

				CurrentMeshDataCache->Triangles.Add(6);
				CurrentMeshDataCache->Triangles.Add(5);
				CurrentMeshDataCache->Triangles.Add(7);

				//CurrentMeshDataCache->Triangles.Add(7);
				//CurrentMeshDataCache->Triangles.Add(3);
				//CurrentMeshDataCache->Triangles.Add(2);

				CurrentMeshDataCache->Triangles.Add(7);
				CurrentMeshDataCache->Triangles.Add(2);
				CurrentMeshDataCache->Triangles.Add(6);

				//CurrentMeshDataCache->Triangles.Add(7);
				//CurrentMeshDataCache->Triangles.Add(4);
				//CurrentMeshDataCache->Triangles.Add(0);

				CurrentMeshDataCache->Triangles.Add(7);
				CurrentMeshDataCache->Triangles.Add(0);
				CurrentMeshDataCache->Triangles.Add(3);

				//CurrentMeshDataCache->Triangles.Add(1);
				//CurrentMeshDataCache->Triangles.Add(5);
				//CurrentMeshDataCache->Triangles.Add(6);

				CurrentMeshDataCache->Triangles.Add(2);
				CurrentMeshDataCache->Triangles.Add(1);
				CurrentMeshDataCache->Triangles.Add(6);


				// We don't add any sections to the mesh component for point clouds.
				if (MeshType != EMeshType::PointCloud)
				{
					if (MRMesh)
					{
						UE_LOG(LogMagicLeap, Error, TEXT("TickWithFakeData() Sending fake brick %llu."), CurrentMeshDataCache->BrickId);
						static_cast<IMRMesh*>(MRMesh)->SendBrickData(IMRMesh::FSendBrickDataArgs
						{
							TSharedPtr<IMRMesh::FBrickDataReceipt, ESPMode::ThreadSafe>(new FMeshTrackerImpl::FMeshTrackerComponentBrickDataReceipt(CurrentMeshDataCache)),
							CurrentMeshDataCache->BrickId,
							CurrentMeshDataCache->Vertices,
							CurrentMeshDataCache->UV0,
							CurrentMeshDataCache->Tangents,
							CurrentMeshDataCache->VertexColors,
							CurrentMeshDataCache->Triangles
						}
						);
					}
				}
				if (OnMeshTrackerUpdated.IsBound())
				{
					// Hack because blueprints don't support uint32.
					TArray<int32> Triangles(reinterpret_cast<const int32*>(CurrentMeshDataCache->Triangles.GetData()), CurrentMeshDataCache->Triangles.Num());
					OnMeshTrackerUpdated.Broadcast(MeshData.Value.Section, CurrentMeshDataCache->Vertices, Triangles, CurrentMeshDataCache->Normals, CurrentMeshDataCache->Confidence);
				}
			}
		}
	}
#endif //WITH_MLSDK
}

void UMeshTrackerComponent::BeginDestroy()
{
	if (MRMesh != nullptr)
	{
		DisconnectMRMesh(MRMesh);
	}
	Super::BeginDestroy();
}

void UMeshTrackerComponent::FinishDestroy()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::PrePIEEnded.RemoveAll(this);
	}
#endif
	Impl->Destroy();
	Super::FinishDestroy();
}

#if WITH_EDITOR
void UMeshTrackerComponent::PrePIEEnded(bool bWasSimulatingInEditor)
{
	Impl->Destroy();
}
#endif
