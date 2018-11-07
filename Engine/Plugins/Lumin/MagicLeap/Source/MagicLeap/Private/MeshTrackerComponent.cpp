// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MeshTrackerComponent.h"
#include "IMagicLeapHMD.h"
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
#include <ml_meshing2.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

#if WITH_MLSDK
// Map an Unreal meshing LOD to the corresponding ML meshing LOD
FORCEINLINE MLMeshingLOD MLToUnreal_MeshLOD(EMeshLOD UnrealMeshLOD)
{
	switch (UnrealMeshLOD)
	{
		case EMeshLOD::Minimum:
			return MLMeshingLOD_Minimum;
		case EMeshLOD::Medium:
			return MLMeshingLOD_Medium;
		case EMeshLOD::Maximum:
			return MLMeshingLOD_Maximum;
	}
	check(false);
	return MLMeshingLOD_Minimum;
}

// Makes MLCoordinateFrameUID a hashable type for use with TMap and TSet
template<typename ValueType> struct TMLCoordinateFrameUIDKeyFuncs : 
	BaseKeyFuncs<TPair<MLCoordinateFrameUID, ValueType>, MLCoordinateFrameUID>
{
private:
	typedef BaseKeyFuncs<TPair<MLCoordinateFrameUID, ValueType>, MLCoordinateFrameUID> Super;
public:
	typedef typename Super::ElementInitType ElementInitType;
	typedef typename Super::KeyInitType KeyInitType;

	static KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element.Key;
	}
	static bool Matches(KeyInitType A, KeyInitType B)
	{
		return A.data[0] == B.data[0] && A.data[1] == B.data[1];
	}
	static uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(static_cast<uint64>(Key.data[0])) ^ ~GetTypeHash(static_cast<uint64>(Key.data[1]));
	}
};
#endif //WITH_MLSDK

class FMeshTrackerImpl : public MagicLeap::IAppEventHandler
{
public:
	FMeshTrackerImpl()
#if WITH_MLSDK
		: MeshTracker(ML_INVALID_HANDLE)
		, MeshBrickIndex(0)
		, CurrentMeshInfoRequest(ML_INVALID_HANDLE)
		, CurrentMeshRequest(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
	{
	};

#if WITH_MLSDK
	MLMeshingSettings CreateSettings(const UMeshTrackerComponent& MeshTrackerComponent)
	{
		MLMeshingSettings Settings;

		MLMeshingInitSettings(&Settings);

		float WorldToMetersScale = 100.0f;
		if (IMagicLeapPlugin::Get().IsMagicLeapHMDValid())
		{
			const FAppFramework& AppFramework = static_cast<FMagicLeapHMD *>
				(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
			if (AppFramework.IsInitialized())
			{
				WorldToMetersScale = AppFramework.GetWorldToMetersScale();
			}
		}

		if (MeshTrackerComponent.MeshType == EMeshType::PointCloud)
		{
			Settings.flags |= MLMeshingFlags_PointCloud;
		}
		if (MeshTrackerComponent.RequestNormals)
		{
			Settings.flags |= MLMeshingFlags_ComputeNormals;
		}
		if (MeshTrackerComponent.RequestVertexConfidence)
		{
			Settings.flags |= MLMeshingFlags_ComputeConfidence;
		}
		if (MeshTrackerComponent.Planarize)
		{
			Settings.flags |= MLMeshingFlags_Planarize;
		}
		if (MeshTrackerComponent.RemoveOverlappingTriangles)
		{
			Settings.flags |= MLMeshingFlags_RemoveMeshSkirt;
		}

		Settings.fill_hole_length = MeshTrackerComponent.PerimeterOfGapsToFill / WorldToMetersScale;
		Settings.disconnected_component_area = MeshTrackerComponent.
			DisconnectedSectionArea / (WorldToMetersScale * WorldToMetersScale);

		return Settings;
	};

#endif //WITH_MLSDK

	void OnAppPause() override
	{
	}

	void OnAppResume() override
	{
	}

public:
#if WITH_MLSDK
	// Handle to ML mesh tracker
	MLHandle MeshTracker;

	// Next ID for bricks created with MR Mesh
	int32 MeshBrickIndex;

	// Handle to ML mesh info request
	MLHandle CurrentMeshInfoRequest;

	// Handle to ML mesh request
	MLHandle CurrentMeshRequest;

	// Current ML meshing settings
	MLMeshingSettings CurrentMeshSettings;

	// List of ML mesh block IDs and states
	TArray<MLMeshingBlockRequest> MeshBlockRequests;

	// Map of ML mesh block IDs to MR Mesh brick IDs
	TMap<MLCoordinateFrameUID, uint64, FDefaultSetAllocator, 
		TMLCoordinateFrameUIDKeyFuncs<uint64>> MeshBrickCache;

#endif //WITH_MLSDK

	// Keep a copy of the mesh data here.  MRMeshComponent will use it from the game and render thread.
	struct FMLCachedMeshData
	{
		typedef TSharedPtr<FMLCachedMeshData, ESPMode::ThreadSafe> SharedPtr;
		
		FMeshTrackerImpl* Owner = nullptr;
		
		IMRMesh::FBrickId BrickId = 0;
		TArray<FVector> OffsetVertices;
		TArray<FVector> WorldVertices;
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
			OffsetVertices.Reset();
			WorldVertices.Reset();
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

	bool Create(const UMeshTrackerComponent& MeshTrackerComponent)
	{
#if WITH_MLSDK
		if (!MLHandleIsValid(MeshTracker))
		{
			// Create the tracker on demand.
			//UE_LOG(LogMagicLeap, Log, TEXT("Creating Mesh MeshTracker"));
			
			CurrentMeshSettings = CreateSettings(MeshTrackerComponent);

			MLResult Result = MLMeshingCreateClient(&MeshTracker, &CurrentMeshSettings);

			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLMeshingCreateClient failed: %s."), 
					UTF8_TO_TCHAR(MLGetResultString(Result)));
				return false;
			}

			MeshBrickIndex = 0;
		}
#endif //WITH_MLSDK
		return true;
	}

	void Destroy()
	{
#if WITH_MLSDK
		if (MLHandleIsValid(MeshTracker))
		{
			MLResult Result = MLMeshingDestroyClient(&MeshTracker);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, 
					TEXT("MLMeshingDestroyClient failed: %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
			}
			MeshTracker = ML_INVALID_HANDLE;
		}
#endif //WITH_MLSDK
	}

private:

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
	if (!InMRMeshPtr)
	{
		UE_LOG(LogMagicLeap, Warning,
			TEXT("MRMesh given is not valid. Ignoring this connect."));
		return;
	}
	else if (MRMesh)
	{
		UE_LOG(LogMagicLeap, Warning, 
			TEXT("MeshTrackerComponent already has a MRMesh connected.  Ignoring this connect."));
		return;
	}
	else if (InMRMeshPtr->IsConnected())
	{
		UE_LOG(LogMagicLeap, Warning, 
			TEXT("MRMesh is already connected to a UMeshTrackerComponent. Ignoring this connect."));
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
		UE_LOG(LogMagicLeap, Warning, 
			TEXT("MeshTrackerComponent MRMesh is already disconnected. Ignoring this disconnect."));
		return;
	}
	else if (InMRMeshPtr != MRMesh)
	{
		UE_LOG(LogMagicLeap, Warning, 
			TEXT("MeshTrackerComponent MRMesh given is not the MRMesh connected. "
				 "Ignoring this disconnect."));
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
	if (MLHandleIsValid(Impl->MeshTracker) && e.Property != nullptr)
	{
		MLMeshingSettings MeshSettings = Impl->CreateSettings(*this);

		// Just brute compare
		if (0 != memcmp(&Impl->CurrentMeshSettings, &MeshSettings, sizeof(MeshSettings)))
		{
			UE_LOG(LogMagicLeap, Log, 
				TEXT("PostEditChangeProperty is changing MLMeshingSettings"));

			auto Result = MLMeshingUpdateSettings(Impl->MeshTracker, &Impl->CurrentMeshSettings);

			if (MLResult_Ok != Result)
			{
				UE_LOG(LogMagicLeap, Error, 
					TEXT("MLMeshingUpdateSettings failed: %s"), UTF8_TO_TCHAR(MLGetResultString(Result)));
			}
			else
			{
				Impl->CurrentMeshSettings = MeshSettings;
			}
		}
	}
#endif //WITH_MLSDK

	Super::PostEditChangeProperty(e);
}
#endif

void UMeshTrackerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if WITH_MLSDK
	if (!MRMesh)
	{
		return;
	}

	if (!IMagicLeapPlugin::Get().IsMagicLeapHMDValid())
	{
		return;
	}

	const FAppFramework& AppFramework = 
		static_cast<FMagicLeapHMD *>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
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

	float WorldToMetersScale = AppFramework.GetWorldToMetersScale();

	// Make sure MR Mesh is at 0,0,0 (verts received from ML meshing are in world space)
	MRMesh->SendRelativeTransform(FTransform::Identity);

	if (ScanWorld)
	{
		// Request mesh info
		if (Impl->CurrentMeshInfoRequest == ML_INVALID_HANDLE && Impl->CurrentMeshRequest == ML_INVALID_HANDLE)
		{
			MLMeshingExtents Extents = {};
			Extents.center = MagicLeap::ToMLVector(Impl->BoundsCenter, WorldToMetersScale);
			Extents.rotation = MagicLeap::ToMLQuat(Impl->BoundsRotation);
			Extents.extents = MagicLeap::ToMLVector(BoundingVolume->GetScaledBoxExtent(), WorldToMetersScale);

			// MagicLeap::ToMLVector() is, as the name implies, meant for vectors (not extents) and using it
			// causes the Z component to be negated. We'll abs all of them for safety.
			Extents.extents.x = FMath::Abs<float>(Extents.extents.x);
			Extents.extents.y = FMath::Abs<float>(Extents.extents.y);
			Extents.extents.z = FMath::Abs<float>(Extents.extents.z);

			auto Result = MLMeshingRequestMeshInfo(Impl->MeshTracker, &Extents, &Impl->CurrentMeshInfoRequest);
			if (MLResult_Ok != Result)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLMeshingRequestMeshInfo failed: %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
				Impl->CurrentMeshInfoRequest = ML_INVALID_HANDLE;
			}
		}

		// Request block meshes for current mesh info and block list
		if (Impl->CurrentMeshRequest == ML_INVALID_HANDLE && Impl->MeshBlockRequests.Num() > 0)
		{
			MLMeshingMeshRequest MeshRequest = {};
			MeshRequest.request_count = static_cast<int>(Impl->MeshBlockRequests.Num());
			MeshRequest.data = Impl->MeshBlockRequests.GetData();
			auto Result = MLMeshingRequestMesh(Impl->MeshTracker, &MeshRequest, &Impl->CurrentMeshRequest);
			if (MLResult_Ok != Result)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLMeshingRequestMesh failed: %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
				Impl->CurrentMeshInfoRequest = ML_INVALID_HANDLE;
			}
		}

		// Request IDs and states of block meshes
		if (Impl->CurrentMeshInfoRequest != ML_INVALID_HANDLE)
		{
			MLMeshingMeshInfo MeshInfo = {};

			auto Result = MLMeshingGetMeshInfoResult(Impl->MeshTracker, Impl->CurrentMeshInfoRequest, &MeshInfo);
			if (MLResult_Ok != Result)
			{
				// Just silently wait for pending result
				if (MLResult_Pending != Result)
				{
					UE_LOG(LogMagicLeap, Error, 
						TEXT("MLMeshingGetMeshInfoResult failed: %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
				}
			}
			else
			{
				// Clear our stored block requests
				Impl->MeshBlockRequests.Empty();

				//UE_LOG(LogMagicLeap, Log, TEXT("Mesh tracker received %d mesh block infos"), MeshInfo.data_count);

				for (uint32_t MeshInfoIndex = 0; MeshInfoIndex < MeshInfo.data_count; ++ MeshInfoIndex)
				{
					const auto &MeshInfoData = MeshInfo.data[MeshInfoIndex];

					MLMeshingBlockRequest BlockRequest = {};

					BlockRequest.id = MeshInfoData.id;
					BlockRequest.level = MLToUnreal_MeshLOD(LevelOfDetail);

					switch (MeshInfoData.state)
					{
						case MLMeshingMeshState_New:
						case MLMeshingMeshState_Updated:
						{
							// Store the block request so we can update it
							Impl->MeshBlockRequests.Add(BlockRequest);
							break;
						}
						case MLMeshingMeshState_Deleted:
						{
							// Delete the brick and its cache entry
							if (Impl->MeshBrickCache.Contains(MeshInfoData.id))
							{
								const auto& BrickId = Impl->MeshBrickCache[MeshInfoData.id];

								if (MeshType != EMeshType::PointCloud)
								{
									const static TArray<FVector> EmptyVertices;
									const static TArray<FVector2D> EmptyUVs;
									const static TArray<FPackedNormal> EmptyTangents;
									const static TArray<FColor> EmptyVertexColors;
									const static TArray<uint32> EmptyTriangles;
									static_cast<IMRMesh*>(MRMesh)->SendBrickData(IMRMesh::FSendBrickDataArgs
										{
											nullptr,
											BrickId,
											EmptyVertices,
											EmptyUVs,
											EmptyTangents,
											EmptyVertexColors,
											EmptyTriangles
										}
									);
								}

								if (OnMeshTrackerUpdated.IsBound())
								{
									OnMeshTrackerUpdated.Broadcast(BrickId, 
										TArray<FVector>(), TArray<int32>(), TArray<FVector>(), TArray<float>());
								}

								Impl->MeshBrickCache.Remove(MeshInfoData.id);
							}
							break;
						}
						default:
							break;
					}
				}

				// Free up the ML meshing resources
				MLMeshingFreeResource(Impl->MeshTracker, &Impl->CurrentMeshInfoRequest);
				Impl->CurrentMeshInfoRequest = ML_INVALID_HANDLE;
			}
		}

		// Request mesh data
		if (Impl->CurrentMeshRequest != ML_INVALID_HANDLE)
		{
			MLMeshingMesh Mesh = {};

			auto Result = MLMeshingGetMeshResult(Impl->MeshTracker, Impl->CurrentMeshRequest, &Mesh);

			if (MLResult_Ok != Result)
			{
				// Just silently wait for pending result
				if (MLResult_Pending != Result)
				{
					UE_LOG(LogMagicLeap, Error, TEXT("MLMeshingGetMeshResult failed: %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
				}
			}
			else
			{
				//UE_LOG(LogMagicLeap, Log, TEXT("Mesh tracker received %d blocks for mesh request %d"), 
					//Mesh.data_count, static_cast<int>(Impl->CurrentMeshRequest));

				FVector VertexOffset = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this).Inverse().GetLocation();
				for (uint32_t MeshIndex = 0; MeshIndex < Mesh.data_count; ++ MeshIndex)
				{
					const auto &MeshData = Mesh.data[MeshIndex];

					// Create a brick index for any new mesh block
					if (!Impl->MeshBrickCache.Contains(MeshData.id))
					{
						Impl->MeshBrickCache.Add(MeshData.id, Impl->MeshBrickIndex ++);
					}

					auto BrickId = Impl->MeshBrickCache[MeshData.id];

					// Acquire mesh data cache and mark its brick ID
					FMeshTrackerImpl::FMLCachedMeshData::SharedPtr CurrentMeshDataCache = Impl->AquireMeshDataCache();
					CurrentMeshDataCache->BrickId = BrickId;

					// Pull vertices
					CurrentMeshDataCache->OffsetVertices.Reserve(MeshData.vertex_count);
					CurrentMeshDataCache->WorldVertices.Reserve(MeshData.vertex_count);
					for (uint32_t v = 0; v < MeshData.vertex_count; ++ v)
					{
							CurrentMeshDataCache->OffsetVertices.Add(MagicLeap::ToFVector(MeshData.vertex[v], WorldToMetersScale) - VertexOffset);
							CurrentMeshDataCache->WorldVertices.Add(MagicLeap::ToFVector(MeshData.vertex[v], WorldToMetersScale));
					}

					// Pull indices
					CurrentMeshDataCache->Triangles.Reserve(MeshData.index_count);
					for (uint16_t i = 0; i < MeshData.index_count; ++ i)
					{
						CurrentMeshDataCache->Triangles.Add(static_cast<uint32>(MeshData.index[i]));
					}

					// Pull normals
					CurrentMeshDataCache->Normals.Reserve(MeshData.vertex_count);
					if (nullptr != MeshData.normal)
					{
						for (uint32_t n = 0; n < MeshData.vertex_count; ++ n)
						{
							CurrentMeshDataCache->Normals.Add(MagicLeap::
								ToFVector(MeshData.normal[n], 1.0f));
						}
					}
					// If no normals were provided we need to pack fake ones for Vulkan
					else
					{
						for (uint32_t n = 0; n < MeshData.vertex_count; ++ n)
						{
							FVector FakeNormal = CurrentMeshDataCache->OffsetVertices[n];
							FakeNormal.Normalize();
							CurrentMeshDataCache->Normals.Add(FakeNormal);
						}
					}

					// Calculate and pack tangents
					CurrentMeshDataCache->Tangents.Reserve(MeshData.vertex_count * 2);
					for (uint32_t t = 0; t < MeshData.vertex_count; ++ t)
					{
						const FVector& Norm = CurrentMeshDataCache->Normals[t];

						// Calculate tangent
						auto Perp = Norm.X < Norm.Z ? 
							FVector(1.0f, 0.0f, 0.0f) : FVector(0.0f, 1.0f, 0.0f);
						auto Tang = FVector::CrossProduct(Norm, Perp);

						CurrentMeshDataCache->Tangents.Add(Tang);
						CurrentMeshDataCache->Tangents.Add(Norm);
					}

					// Pull confidence
					if (nullptr != MeshData.confidence)
					{
						CurrentMeshDataCache->Confidence.Append(MeshData.confidence, MeshData.vertex_count);
					}

					// Apply chosen vertex color mode
					switch (VertexColorMode)
					{
						case EMLMeshVertexColorMode::Confidence:
						{
							if (nullptr != MeshData.confidence)
							{
								CurrentMeshDataCache->VertexColors.Reserve(MeshData.vertex_count);
								for (uint32 v = 0; v < MeshData.vertex_count; ++ v)
								{
									const FLinearColor VertexColor = FMath::Lerp(VertexColorFromConfidenceZero, 
										VertexColorFromConfidenceOne, CurrentMeshDataCache->Confidence[v]);
									CurrentMeshDataCache->VertexColors.Add(VertexColor.ToFColor(false));
								}
							}
							else
							{
								UE_LOG(LogMagicLeap, Warning, TEXT("MeshTracker vertex color mode is Confidence "
									"but no confidence values available. Using white for all blocks."));
							}
							break;
						}
						case EMLMeshVertexColorMode::Block:
						{
							if (BlockVertexColors.Num() > 0)
							{
								const FColor& VertexColor = BlockVertexColors[BrickId % BlockVertexColors.Num()];

								CurrentMeshDataCache->VertexColors.Reserve(MeshData.vertex_count);
								for (uint32 v = 0; v < MeshData.vertex_count; ++ v)
								{
									CurrentMeshDataCache->VertexColors.Add(VertexColor);
								}
							}
							else
							{
								UE_LOG(LogMagicLeap, Warning, TEXT("MeshTracker vertex color mode is Block but "
									"no BlockVertexColors set. Using white for all blocks."));
							}
							break;
						}
						case EMLMeshVertexColorMode::None:
						{
							break;
						}
						default:
							check(false);
							break;
					}

					// To work in all rendering paths we always set a vertex color
					if (CurrentMeshDataCache->VertexColors.Num() == 0)
					{
						for (uint32 v = 0; v < MeshData.vertex_count; ++ v)
						{
							CurrentMeshDataCache->VertexColors.Add(FColor::White);
						}
					}

					// Write UVs
					CurrentMeshDataCache->UV0.Reserve(MeshData.vertex_count);
					for (uint32 v = 0; v < MeshData.vertex_count; ++ v)
					{
						const float FakeCoord = static_cast<float>(v) / static_cast<float>(MeshData.vertex_count);
						CurrentMeshDataCache->UV0.Add(FVector2D(FakeCoord, FakeCoord));
					}

					// Create/update brick
					if (MeshType != EMeshType::PointCloud)
					{
						static_cast<IMRMesh*>(MRMesh)->SendBrickData(IMRMesh::FSendBrickDataArgs
							{
								TSharedPtr<IMRMesh::FBrickDataReceipt, ESPMode::ThreadSafe>
									(new FMeshTrackerImpl::FMeshTrackerComponentBrickDataReceipt(CurrentMeshDataCache)),
								CurrentMeshDataCache->BrickId,
								CurrentMeshDataCache->WorldVertices,
								CurrentMeshDataCache->UV0,
								CurrentMeshDataCache->Tangents,
								CurrentMeshDataCache->VertexColors,
								CurrentMeshDataCache->Triangles
							}
						);
					}

					// Broadcast that a mesh was updated
					if (OnMeshTrackerUpdated.IsBound())
					{
						// Hack because blueprints don't support uint32.
						TArray<int32> Triangles(reinterpret_cast<const int32*>(CurrentMeshDataCache->
							Triangles.GetData()), CurrentMeshDataCache->Triangles.Num());
						OnMeshTrackerUpdated.Broadcast(CurrentMeshDataCache->BrickId, CurrentMeshDataCache->OffsetVertices, 
							Triangles, CurrentMeshDataCache->Normals, CurrentMeshDataCache->Confidence);
					}
				}

				// All meshes pulled and/or updated; free the ML resource
				MLMeshingFreeResource(Impl->MeshTracker, &Impl->CurrentMeshRequest);
				Impl->CurrentMeshRequest = ML_INVALID_HANDLE;
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
