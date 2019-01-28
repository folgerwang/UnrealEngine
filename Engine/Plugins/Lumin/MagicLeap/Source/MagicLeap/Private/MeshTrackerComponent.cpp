// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
// TODO: Don't rely on the size being same.
static_assert(sizeof(FGuid) == sizeof(MLCoordinateFrameUID), "Size of FGuid should be same as MLCoordinateFrameUID. TODO: Don't rely on the size being same.");

// Map an Unreal meshing LOD to the corresponding ML meshing LOD
FORCEINLINE MLMeshingLOD UnrealToML_MeshLOD(EMeshLOD UnrealMeshLOD)
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

EMeshState MLToUEMeshState(MLMeshingMeshState MLMeshState)
{
	switch (MLMeshState)
	{
		case MLMeshingMeshState_New:
			return EMeshState::New;
		case MLMeshingMeshState_Updated:
			return EMeshState::Updated;
		case MLMeshingMeshState_Deleted:
			return EMeshState::Deleted;
		case MLMeshingMeshState_Unchanged:
			return EMeshState::Unchanged;
	}
	check(false);
	return EMeshState::Unchanged;
}

void MLToUnrealBlockInfo(const MLMeshingBlockInfo& MLBlockInfo, const FTransform& TrackingToWorld, float WorldToMetersScale, FMeshBlockInfo& UEBlockInfo)
{
	FMemory::Memcpy(&UEBlockInfo.BlockID, &MLBlockInfo.id, sizeof(MLCoordinateFrameUID));

	FTransform BlockTransform = FTransform(MagicLeap::ToFQuat(MLBlockInfo.extents.rotation), MagicLeap::ToFVector(MLBlockInfo.extents.center, WorldToMetersScale), FVector(1.0f, 1.0f, 1.0f));
	if (!BlockTransform.GetRotation().IsNormalized())
	{
		FQuat rotation = BlockTransform.GetRotation();
		rotation.Normalize();
		BlockTransform.SetRotation(rotation);
	}

	BlockTransform.AddToTranslation(TrackingToWorld.GetLocation());
	BlockTransform.ConcatenateRotation(TrackingToWorld.Rotator().Quaternion());
	UEBlockInfo.BlockPosition = BlockTransform.GetLocation();
	UEBlockInfo.BlockOrientation = BlockTransform.Rotator();
	UEBlockInfo.BlockDimensions = MagicLeap::ToFVectorExtents(MLBlockInfo.extents.extents, WorldToMetersScale);

	UEBlockInfo.Timestamp = FTimespan::FromMicroseconds(MLBlockInfo.timestamp / 1000.0);
	UEBlockInfo.BlockState = MLToUEMeshState(MLBlockInfo.state);
}

void UnrealToMLBlockRequest(const FMeshBlockRequest& UEBlockRequest, MLMeshingBlockRequest& MLBlockRequest)
{
	FMemory::Memcpy(&MLBlockRequest.id, &UEBlockRequest.BlockID, sizeof(MLCoordinateFrameUID));
	MLBlockRequest.level = UnrealToML_MeshLOD(UEBlockRequest.LevelOfDetail);
}
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

	// The set of meshing blocks that have been processed or removed between queries
	TSet<FGuid> MeshBrickCache;

#endif //WITH_MLSDK

	FMLTrackingMeshInfo LastestMeshInfo;
	TArray<FMeshBlockRequest> UEMeshBlockRequests;
	TScriptInterface<IMeshBlockSelectorInterface> BlockSelector;

	// Keep a copy of the mesh data here.  MRMeshComponent will use it from the game and render thread.
	struct FMLCachedMeshData
	{
		typedef TSharedPtr<FMLCachedMeshData, ESPMode::ThreadSafe> SharedPtr;
		
		FMeshTrackerImpl* Owner = nullptr;
		
		FGuid BlockID;
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
	
			BlockID.Invalidate();
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

	bool Update(const UMeshTrackerComponent& MeshTrackerComponent)
	{
#if WITH_MLSDK
		MLMeshingSettings MeshSettings = CreateSettings(MeshTrackerComponent);

		if (0 != memcmp(&CurrentMeshSettings, &MeshSettings, sizeof(MeshSettings)))
		{
			auto Result = MLMeshingUpdateSettings(MeshTracker, &MeshSettings);

			if (MLResult_Ok == Result)
			{
				// For some parameter changes we will want to clear already-generated data
				if (MeshTrackerComponent.MRMesh != nullptr)
				{
					if ((MLMeshingFlags_PointCloud & MeshSettings.flags) !=
						(MLMeshingFlags_PointCloud & CurrentMeshSettings.flags))
					{
						UE_LOG(LogMagicLeap, Log,
							TEXT("MLMeshingSettings change caused a clear"));
						MeshTrackerComponent.MRMesh->Clear();
					}
				}

				CurrentMeshSettings = MeshSettings;
				return true;
			}

			UE_LOG(LogMagicLeap, Error,
				TEXT("MLMeshingUpdateSettings failed: %s"), UTF8_TO_TCHAR(MLGetResultString(Result)));
		}
#endif //WITH_MLSDK
		return false;
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

UMeshTrackerComponent::UMeshTrackerComponent()
	: VertexColorFromConfidenceZero(FLinearColor::Red)
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
		if (Impl->Update(*this))
		{
			UE_LOG(LogMagicLeap, Log, 
				TEXT("PostEditChangeProperty is changing MLMeshingSettings"));
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

	// Dont use the bool() operator from TScriptInterface class since it only checks for the InterfacePointer. 
	// Since the InterfacePointer is null for it's blueprint implementors, bool() operator gives us the wrong result for checking if interface is valid.
	if (Impl->BlockSelector.GetObject() == nullptr)
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("No block selector is connected, using default implementation."));
		Impl->BlockSelector = this;
	}

	// Update the bounding box.
	FTransform PoseInverse = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this).Inverse();
	Impl->BoundsCenter = PoseInverse.TransformPosition(BoundingVolume->GetComponentLocation());
	Impl->BoundsRotation = PoseInverse.TransformRotation(BoundingVolume->GetComponentQuat());

	// Potentially update for changed component parameters
	if (Impl->Update(*this))
	{
		UE_LOG(LogMagicLeap, Log, TEXT("MLMeshingSettings changed on the fly"));
	}

	const float WorldToMetersScale = AppFramework.GetWorldToMetersScale();

	// Make sure MR Mesh is at 0,0,0 (verts received from ML meshing are in tracking space)
	MRMesh->SendRelativeTransform(FTransform::Identity);

	if (ScanWorld)
	{
		if (GetMeshResult())
		{
			RequestMeshInfo();
			if (GetMeshInfoResult())
			{
				RequestMesh();
			}
		}
	}
#endif //WITH_MLSDK
}

void UMeshTrackerComponent::SelectMeshBlocks_Implementation(const FMLTrackingMeshInfo& NewMeshInfo, TArray<FMeshBlockRequest>& RequestedMesh)
{
	for (const FMeshBlockInfo& BlockInfo : NewMeshInfo.BlockData)
	{
		if (BlockInfo.BlockState == EMeshState::New || BlockInfo.BlockState == EMeshState::Unchanged)
		{
			FMeshBlockRequest BlockRequest;
			BlockRequest.BlockID = BlockInfo.BlockID;
			BlockRequest.LevelOfDetail = LevelOfDetail;
			RequestedMesh.Add(BlockRequest);
		}
	}
}

void UMeshTrackerComponent::ConnectBlockSelector(TScriptInterface<IMeshBlockSelectorInterface> Selector)
{
	if (Impl != nullptr)
	{
		// Dont use the bool() operator from TScriptInterface class since it only checks for the InterfacePointer. 
		// Since the InterfacePointer is null for it's blueprint implementors, bool() operator gives us the wrong result for checking if interface is valid.
		if (Selector.GetObject() != nullptr)
		{
			// If called via C++, Selector might have been created manually and not implement IMeshBlockSelectorInterface.
			if (Selector.GetObject()->GetClass()->ImplementsInterface(UMeshBlockSelectorInterface::StaticClass()))
			{
				Impl->BlockSelector = Selector;
			}
			else
			{
				UE_LOG(LogMagicLeap, Warning, TEXT("Selector %s does not implement IMeshBlockSelectorInterface. Using default block selector from MeshTrackerComponent."), *(Selector.GetObject()->GetFName().ToString()));
				Impl->BlockSelector = this;	
			}
		}
		else
		{
			UE_LOG(LogMagicLeap, Warning, TEXT("Invalid selector passed to UMeshTrackerComponent::ConnectBlockSelector(). Using default block selector from MeshTrackerComponent."));
			Impl->BlockSelector = this;	
		}
	}
}

void UMeshTrackerComponent::DisconnectBlockSelector()
{
	if (Impl != nullptr)
	{
		Impl->BlockSelector = this;
	}
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

void UMeshTrackerComponent::RequestMeshInfo()
{
#if WITH_MLSDK
	const FAppFramework& AppFramework = static_cast<FMagicLeapHMD *>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
	const float WorldToMetersScale = AppFramework.GetWorldToMetersScale();

	// Request mesh info as frequently as possible.
	// Actual request for mesh will be submitted based on the latest available info at the time of triggering the mesh request.
	if (Impl->CurrentMeshInfoRequest == ML_INVALID_HANDLE)
	{
		MLMeshingExtents Extents = {};
		Extents.center = MagicLeap::ToMLVector(Impl->BoundsCenter, WorldToMetersScale);
		Extents.rotation = MagicLeap::ToMLQuat(Impl->BoundsRotation);
		Extents.extents = MagicLeap::ToMLVectorExtents(BoundingVolume->GetScaledBoxExtent(), WorldToMetersScale);

		auto Result = MLMeshingRequestMeshInfo(Impl->MeshTracker, &Extents, &Impl->CurrentMeshInfoRequest);
		if (MLResult_Ok != Result)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLMeshingRequestMeshInfo failed: %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
			Impl->CurrentMeshInfoRequest = ML_INVALID_HANDLE;
		}
	}
#endif
}

bool UMeshTrackerComponent::GetMeshInfoResult()
{
#if WITH_MLSDK
	const FAppFramework& AppFramework = static_cast<FMagicLeapHMD *>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
	const float WorldToMetersScale = AppFramework.GetWorldToMetersScale();

	// Check for updated mesh info and cache the result.
	// The cached result will be used by app to choose which blocks it wants to actually request the mesh for.
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
				Impl->CurrentMeshInfoRequest = ML_INVALID_HANDLE;
				return true;
			}
			return false;
		}
		else
		{
			// Clear our stored block requests
			Impl->LastestMeshInfo.BlockData.Empty(MeshInfo.data_count);
			Impl->LastestMeshInfo.BlockData.AddUninitialized(MeshInfo.data_count);
			Impl->LastestMeshInfo.Timestamp = FTimespan::FromMicroseconds(MeshInfo.timestamp / 1000.0);

			for (uint32_t MeshInfoIndex = 0; MeshInfoIndex < MeshInfo.data_count; ++ MeshInfoIndex)
			{
				const auto &MeshInfoData = MeshInfo.data[MeshInfoIndex];

				// TODO: right now we are adding even the deleted and unchanged blocks here. we probably only need to add new and unchanged blocks.
				MLToUnrealBlockInfo(MeshInfoData, UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this), WorldToMetersScale, Impl->LastestMeshInfo.BlockData[MeshInfoIndex]);

				switch (MeshInfoData.state)
				{
					// TODO: maybe do this only when the rest of the mesh is updated i.e. success from MLMeshingGetMeshResult()
					case MLMeshingMeshState_Deleted:
					{
						const auto& BlockID = FGuid(MeshInfoData.id.data[0], MeshInfoData.id.data[0] >> 32, MeshInfoData.id.data[1], MeshInfoData.id.data[1] >> 32);

						// Delete the brick and its cache entry
						if (Impl->MeshBrickCache.Contains(BlockID))
						{

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
										(static_cast<uint64>(BlockID.B) << 32 | BlockID.A),
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
								OnMeshTrackerUpdated.Broadcast(BlockID,
									TArray<FVector>(), TArray<int32>(), TArray<FVector>(), TArray<float>());
							}

							Impl->MeshBrickCache.Remove(BlockID);
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
			return true;
		}
	}
#endif

	// if it reaches here, something has gone wrong
	return false;
}

void UMeshTrackerComponent::RequestMesh()
{
#if WITH_MLSDK
	// Request block meshes for current mesh info and block list
	if (Impl->CurrentMeshRequest == ML_INVALID_HANDLE)
	{
		Impl->UEMeshBlockRequests.Empty(Impl->LastestMeshInfo.BlockData.Num());
		// Allow applications to choose which blocks to mesh.
		IMeshBlockSelectorInterface::Execute_SelectMeshBlocks(Impl->BlockSelector.GetObject(), Impl->LastestMeshInfo, Impl->UEMeshBlockRequests);

		if (Impl->UEMeshBlockRequests.Num() > 0)
		{
			Impl->MeshBlockRequests.Empty(Impl->UEMeshBlockRequests.Num());
			Impl->MeshBlockRequests.AddUninitialized(Impl->UEMeshBlockRequests.Num());
			for (int32 i = 0; i < Impl->UEMeshBlockRequests.Num(); ++i)
			{
				UnrealToMLBlockRequest(Impl->UEMeshBlockRequests[i], Impl->MeshBlockRequests[i]);
			}

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
	}
#endif
}

bool UMeshTrackerComponent::GetMeshResult()
{
#if WITH_MLSDK
	const FAppFramework& AppFramework = static_cast<FMagicLeapHMD *>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
	const float WorldToMetersScale = AppFramework.GetWorldToMetersScale();

	// Get mesh result
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
				// Mesh request failed, lets queue another one.
				Impl->CurrentMeshRequest = ML_INVALID_HANDLE;
				return true;
			}
			// Mesh request pending...
			return false;
		}
		else
		{
			FVector VertexOffset = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this).Inverse().GetLocation();
			for (uint32_t MeshIndex = 0; MeshIndex < Mesh.data_count; ++ MeshIndex)
			{
				const auto &MeshData = Mesh.data[MeshIndex];

				auto BlockID = FGuid(MeshData.id.data[0], MeshData.id.data[0] >> 32, MeshData.id.data[1], MeshData.id.data[1] >> 32);

				// Create a brick index for any new mesh block
				if (!Impl->MeshBrickCache.Contains(BlockID))
				{
					Impl->MeshBrickCache.Add(BlockID);
				}

				// Acquire mesh data cache and mark its brick ID
				FMeshTrackerImpl::FMLCachedMeshData::SharedPtr CurrentMeshDataCache = Impl->AquireMeshDataCache();
				CurrentMeshDataCache->BlockID = BlockID;

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
						CurrentMeshDataCache->Normals.Add(MagicLeap::ToFVectorNoScale(MeshData.normal[n]));
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
							const FColor& VertexColor = BlockVertexColors[(static_cast<uint64>(BlockID.B) << 32 | BlockID.A) % BlockVertexColors.Num()];

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
					case EMLMeshVertexColorMode::LOD:
					{
						if (BlockVertexColors.Num() >= MLMeshingLOD_Maximum)
						{
							const FColor& VertexColor = BlockVertexColors[MeshData.level];

							CurrentMeshDataCache->VertexColors.Reserve(MeshData.vertex_count);
							for (uint32 v = 0; v < MeshData.vertex_count; ++ v)
							{
								CurrentMeshDataCache->VertexColors.Add(VertexColor);
							}
						}
						else
						{
							UE_LOG(LogMagicLeap, Warning, TEXT("MeshTracker vertex color mode is LOD but "
								"BlockVertexColors are less then the number of LODs. Using white for all blocks."));
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
							(static_cast<uint64>(CurrentMeshDataCache->BlockID.B) << 32 | CurrentMeshDataCache->BlockID.A),
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
					OnMeshTrackerUpdated.Broadcast(CurrentMeshDataCache->BlockID, CurrentMeshDataCache->OffsetVertices,
						Triangles, CurrentMeshDataCache->Normals, CurrentMeshDataCache->Confidence);
				}
			}

			// All meshes pulled and/or updated; free the ML resource
			MLMeshingFreeResource(Impl->MeshTracker, &Impl->CurrentMeshRequest);
			Impl->CurrentMeshRequest = ML_INVALID_HANDLE;

			return true;
		}
	}
#endif

	return true;
}

#if WITH_EDITOR
void UMeshTrackerComponent::PrePIEEnded(bool bWasSimulatingInEditor)
{
	Impl->Destroy();
}
#endif
