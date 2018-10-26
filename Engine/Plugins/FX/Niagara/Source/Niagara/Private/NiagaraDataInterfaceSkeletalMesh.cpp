// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceSkeletalMesh.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstance.h"
#include "Internationalization/Internationalization.h"
#include "NiagaraScript.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "SkeletalMeshTypes.h"
#include "NiagaraWorldManager.h"
#include "Async/ParallelFor.h"
#include "NiagaraStats.h"
#include "Templates/AlignmentTemplates.h"
#include "NDISkeletalMeshCommon.h"
#include "Engine/SkeletalMeshSocket.h"

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceSkeletalMesh"

DECLARE_CYCLE_STAT(TEXT("PreSkin"), STAT_NiagaraSkel_PreSkin, STATGROUP_Niagara);

//////////////////////////////////////////////////////////////////////////

FSkeletalMeshSamplingRegionAreaWeightedSampler::FSkeletalMeshSamplingRegionAreaWeightedSampler()
	: Owner(nullptr)
{
}

void FSkeletalMeshSamplingRegionAreaWeightedSampler::Init(FNDISkeletalMesh_InstanceData* InOwner)
{
	Owner = InOwner;
	Initialize();
}

float FSkeletalMeshSamplingRegionAreaWeightedSampler::GetWeights(TArray<float>& OutWeights)
{
	check(Owner && Owner->Mesh);
	check(Owner->Mesh->IsValidLODIndex(Owner->GetLODIndex()));

	float Total = 0.0f;
	int32 NumUsedRegions = Owner->SamplingRegionIndices.Num();
	if (NumUsedRegions <= 1)
	{
		//Use 0 or 1 Sampling region. Only need additional area weighting between regions if we're sampling from multiple.
		return 0.0f;
	}
	
	const FSkeletalMeshSamplingInfo& SamplingInfo = Owner->Mesh->GetSamplingInfo();
	OutWeights.Empty(NumUsedRegions);
	for (int32 i = 0; i < NumUsedRegions; ++i)
	{
		int32 RegionIdx = Owner->SamplingRegionIndices[i];
		const FSkeletalMeshSamplingRegion& Region = SamplingInfo.GetRegion(RegionIdx);
		float T = SamplingInfo.GetRegionBuiltData(RegionIdx).AreaWeightedSampler.GetTotalWeight();
		OutWeights.Add(T);
		Total += T;
	}
	return Total;
}

//////////////////////////////////////////////////////////////////////////
FSkeletalMeshSkinningDataHandle::FSkeletalMeshSkinningDataHandle()
	: SkinningData(nullptr)
{
}

FSkeletalMeshSkinningDataHandle::FSkeletalMeshSkinningDataHandle(FSkeletalMeshSkinningDataUsage InUsage, TSharedPtr<FSkeletalMeshSkinningData> InSkinningData)
	: Usage(InUsage)
	, SkinningData(InSkinningData)
{
	if (FSkeletalMeshSkinningData* SkinData = SkinningData.Get())
	{
		SkinData->RegisterUser(Usage);
	}
}

FSkeletalMeshSkinningDataHandle::~FSkeletalMeshSkinningDataHandle()
{
	if (FSkeletalMeshSkinningData* SkinData = SkinningData.Get())
	{
		SkinData->UnregisterUser(Usage);
	}
}

//////////////////////////////////////////////////////////////////////////
void FSkeletalMeshSkinningData::ForceDataRefresh()
{
	FScopeLock Lock(&CriticalSection);
	bForceDataRefresh = true;
}

void FSkeletalMeshSkinningData::RegisterUser(FSkeletalMeshSkinningDataUsage Usage)
{
	FScopeLock Lock(&CriticalSection);
	USkeletalMeshComponent* SkelComp = MeshComp.Get();

	int32 LODIndex = Usage.GetLODIndex();
	check(LODIndex != INDEX_NONE);
	check(SkelComp);

	LODData.SetNum(SkelComp->SkeletalMesh->GetLODInfoArray().Num());

	if (Usage.NeedBoneMatrices())
	{
		++BoneMatrixUsers;
	}

	FLODData& LOD = LODData[LODIndex];
	if (Usage.NeedPreSkinnedVerts())
	{
		++LOD.PreSkinnedVertsUsers;
	}

	if (Usage.NeedsDataImmediately())
	{
		check(IsInGameThread());
		if (CurrBoneRefToLocals().Num() == 0)
		{
			SkelComp->CacheRefToLocalMatrices(CurrBoneRefToLocals());
		}
		
		//Prime the prev matrices if they're missing.
		if (PrevBoneRefToLocals().Num() != CurrBoneRefToLocals().Num())
		{
			PrevBoneRefToLocals() = CurrBoneRefToLocals();
		}

		if (Usage.NeedPreSkinnedVerts() && CurrSkinnedPositions(LODIndex).Num() == 0)
		{
			FSkeletalMeshLODRenderData& SkelMeshLODData = SkelComp->SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex];
			FSkinWeightVertexBuffer* SkinWeightBuffer = SkelComp->GetSkinWeightBuffer(LODIndex);
			USkeletalMeshComponent::ComputeSkinnedPositions(SkelComp, CurrSkinnedPositions(LODIndex), CurrBoneRefToLocals(), SkelMeshLODData, *SkinWeightBuffer);

			//Prime the previous positions if they're missing
			if (PrevSkinnedPositions(LODIndex).Num() != CurrSkinnedPositions(LODIndex).Num())
			{
				PrevSkinnedPositions(LODIndex) = CurrSkinnedPositions(LODIndex);
			}
		}
	}
}

void FSkeletalMeshSkinningData::UnregisterUser(FSkeletalMeshSkinningDataUsage Usage)
{
	FScopeLock Lock(&CriticalSection);
	check(LODData.IsValidIndex(Usage.GetLODIndex()));

	if (Usage.NeedBoneMatrices())
	{
		--BoneMatrixUsers;
	}

	FLODData& LOD = LODData[Usage.GetLODIndex()];
	if (Usage.NeedPreSkinnedVerts())
	{
		--LOD.PreSkinnedVertsUsers;
	}
}

bool FSkeletalMeshSkinningData::IsUsed()const
{
	if (BoneMatrixUsers > 0)
	{
		return true;
	}

	for (const FLODData& LOD : LODData)
	{
		if (LOD.PreSkinnedVertsUsers > 0)
		{
			return true;
		}
	}

	return false;
}

bool FSkeletalMeshSkinningData::Tick(float InDeltaSeconds)
{
	USkeletalMeshComponent* SkelComp = MeshComp.Get();
	check(SkelComp);
	DeltaSeconds = InDeltaSeconds;
	CurrIndex ^= 1;

	if (BoneMatrixUsers > 0)
	{
		SkelComp->CacheRefToLocalMatrices(CurrBoneRefToLocals());
	}

	//Prime the prev matrices if they're missing.
	if (PrevBoneRefToLocals().Num() != CurrBoneRefToLocals().Num() || bForceDataRefresh)
	{
		PrevBoneRefToLocals() = CurrBoneRefToLocals();
	}

	for (int32 LODIndex = 0; LODIndex < LODData.Num(); ++LODIndex)
	{
		FLODData& LOD = LODData[LODIndex];
		if (LOD.PreSkinnedVertsUsers > 0)
		{
			//TODO: If we pass the sections in the usage too, we can probably skin a minimal set of verts just for the used regions.
			FSkeletalMeshLODRenderData& SkelMeshLODData = SkelComp->SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex];
			FSkinWeightVertexBuffer* SkinWeightBuffer = SkelComp->GetSkinWeightBuffer(LODIndex);
			USkeletalMeshComponent::ComputeSkinnedPositions(SkelComp, CurrSkinnedPositions(LODIndex), CurrBoneRefToLocals(), SkelMeshLODData, *SkinWeightBuffer);
			//check(CurrSkinnedPositions(LODIndex).Num() == SkelMeshLODData.NumVertices);
			//Prime the previous positions if they're missing
			if (PrevSkinnedPositions(LODIndex).Num() != CurrSkinnedPositions(LODIndex).Num())
			{
				PrevSkinnedPositions(LODIndex) = CurrSkinnedPositions(LODIndex);
			}
		}
	}
	
	bForceDataRefresh = false;
	return true;
}

//////////////////////////////////////////////////////////////////////////

FSkeletalMeshSkinningDataHandle FNDI_SkeletalMesh_GeneratedData::GetCachedSkinningData(TWeakObjectPtr<USkeletalMeshComponent>& InComponent, FSkeletalMeshSkinningDataUsage Usage)
{
	FScopeLock Lock(&CriticalSection);
	
	USkeletalMeshComponent* Component = InComponent.Get();
	check(Component);
	TSharedPtr<FSkeletalMeshSkinningData> SkinningData = nullptr;

	if (TSharedPtr<FSkeletalMeshSkinningData>* Existing = CachedSkinningData.Find(Component))
	{
		check(Existing->IsValid());//We shouldn't be able to have an invalid ptr here.
		SkinningData = *Existing;
	}
	else
	{
		SkinningData = MakeShared<FSkeletalMeshSkinningData>(InComponent);
		CachedSkinningData.Add(Component) = SkinningData;
	}

	return FSkeletalMeshSkinningDataHandle(Usage, SkinningData);
}

void FNDI_SkeletalMesh_GeneratedData::TickGeneratedData(float DeltaSeconds)
{
	check(IsInGameThread());
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_PreSkin);

	//Tick skinning data.
	{
		TArray<TWeakObjectPtr<USkeletalMeshComponent>, TInlineAllocator<64>> ToRemove;
		TArray<FSkeletalMeshSkinningData*> ToTick;
		ToTick.Reserve(CachedSkinningData.Num());
		for (TPair<TWeakObjectPtr<USkeletalMeshComponent>, TSharedPtr<FSkeletalMeshSkinningData>>& Pair : CachedSkinningData)
		{
			TSharedPtr<FSkeletalMeshSkinningData>& Ptr = Pair.Value;
			FSkeletalMeshSkinningData* SkinData = Ptr.Get();
			USkeletalMeshComponent* Component = Pair.Key.Get();
			check(SkinData);
			if (Ptr.IsUnique() || !Component || !Ptr->IsUsed())
			{
				ToRemove.Add(Pair.Key);//Remove unused skin data or for those with GCd components as we go.
			}
			else
			{
				ToTick.Add(SkinData);
			}
		}

		for (TWeakObjectPtr<USkeletalMeshComponent> Key : ToRemove)
		{
			CachedSkinningData.Remove(Key);
		}

		ParallelFor(ToTick.Num(), [&](int32 Index)
		{
			ToTick[Index]->Tick(DeltaSeconds);
		});
	}
}

//////////////////////////////////////////////////////////////////////////
//FNDISkeletalMesh_InstanceData

USkeletalMesh* UNiagaraDataInterfaceSkeletalMesh::GetSkeletalMeshHelper(UNiagaraDataInterfaceSkeletalMesh* Interface, UNiagaraComponent* OwningComponent, TWeakObjectPtr<USceneComponent>& SceneComponent, USkeletalMeshComponent*& FoundSkelComp)
{
	USkeletalMesh* Mesh = nullptr;
	if (Interface->Source)
	{
		ASkeletalMeshActor* MeshActor = Cast<ASkeletalMeshActor>(Interface->Source);
		USkeletalMeshComponent* SourceComp = nullptr;
		if (MeshActor != nullptr)
		{
			SourceComp = MeshActor->GetSkeletalMeshComponent();
		}
		else
		{
			SourceComp = Interface->Source->FindComponentByClass<USkeletalMeshComponent>();
		}

		if (SourceComp)
		{
			Mesh = SourceComp->SkeletalMesh;
			FoundSkelComp = SourceComp;
		}
		else
		{
			SceneComponent = Interface->Source->GetRootComponent();
		}
	}
	else
	{
		if (UNiagaraComponent* SimComp = OwningComponent)
		{
			if (USkeletalMeshComponent* ParentComp = Cast<USkeletalMeshComponent>(SimComp->GetAttachParent()))
			{
				FoundSkelComp = ParentComp;
				Mesh = ParentComp->SkeletalMesh;
			}
			else if (USkeletalMeshComponent* OuterComp = SimComp->GetTypedOuter<USkeletalMeshComponent>())
			{
				FoundSkelComp = OuterComp;
				Mesh = OuterComp->SkeletalMesh;
			}
			else if (AActor* Owner = SimComp->GetAttachmentRootActor())
			{
				TArray<UActorComponent*> SourceComps = Owner->GetComponentsByClass(USkeletalMeshComponent::StaticClass());
				for (UActorComponent* ActorComp : SourceComps)
				{
					USkeletalMeshComponent* SourceComp = Cast<USkeletalMeshComponent>(ActorComp);
					if (SourceComp)
					{
						USkeletalMesh* PossibleMesh = SourceComp->SkeletalMesh;
						if (PossibleMesh != nullptr/* && PossibleMesh->bAllowCPUAccess*/)
						{
							Mesh = PossibleMesh;
							FoundSkelComp = SourceComp;
							break;
						}
					}
				}
			}

			if (!SceneComponent.IsValid())
			{
				SceneComponent = SimComp;
			}
		}
	}

	if (FoundSkelComp)
	{
		SceneComponent = FoundSkelComp;
	}
	
	if (!Mesh && Interface->DefaultMesh)
	{
		Mesh = Interface->DefaultMesh;
	}

	return Mesh;
}


bool FNDISkeletalMesh_InstanceData::Init(UNiagaraDataInterfaceSkeletalMesh* Interface, FNiagaraSystemInstance* SystemInstance)
{
	check(SystemInstance);
	ChangeId = Interface->ChangeId;
	USkeletalMesh* PrevMesh = Mesh;
	Component = nullptr;
	Mesh = nullptr;
	Transform = FMatrix::Identity;
	TransformInverseTransposed = FMatrix::Identity;
	PrevTransform = FMatrix::Identity;
	PrevTransformInverseTransposed = FMatrix::Identity;
	DeltaSeconds = 0.0f;

	USkeletalMeshComponent* NewSkelComp = nullptr;
	Mesh = UNiagaraDataInterfaceSkeletalMesh::GetSkeletalMeshHelper(Interface, SystemInstance->GetComponent(), Component, NewSkelComp);
	
	MeshSafe = Mesh;

	if (Component.IsValid() && Mesh)
	{
		PrevTransform = Transform;
		PrevTransformInverseTransposed = TransformInverseTransposed;
		Transform = Component->GetComponentToWorld().ToMatrixWithScale();
		TransformInverseTransposed = Transform.InverseFast().GetTransposed();
	}

	if (!Mesh)
	{
		/*USceneComponent* Comp = Component.Get();
		UE_LOG(LogNiagara, Log, TEXT("SkeletalMesh data interface has no valid mesh. Failed InitPerInstanceData!\nInterface: %s\nComponent: %s\nActor: %s\n")
			, *Interface->GetFullName()
			, Comp ? *Component->GetFullName() : TEXT("Null Component!")
			, Comp ? *Comp->GetOwner()->GetFullName() : TEXT("NA"));*/
		return false;
	}

#if WITH_EDITOR
	MeshSafe->GetOnMeshChanged().AddUObject(SystemInstance->GetComponent(), &UNiagaraComponent::ReinitializeSystem);
#endif


// 	if (!Mesh->bAllowCPUAccess)
// 	{
// 		UE_LOG(LogNiagara, Log, TEXT("SkeletalMesh data interface using a mesh that does not allow CPU access. Failed InitPerInstanceData - Mesh: %s"), *Mesh->GetFullName());
// 		return false;
// 	}

	if (!Component.IsValid())
	{
		UE_LOG(LogNiagara, Log, TEXT("SkeletalMesh data interface has no valid component. Failed InitPerInstanceData - %s"), *Interface->GetFullName());
		return false;
	}

	//Setup where to spawn from
	SamplingRegionIndices.Empty();
	bool bAllRegionsAreAreaWeighting = true;
	const FSkeletalMeshSamplingInfo& SamplingInfo = Mesh->GetSamplingInfo();
	int32 LODIndex = INDEX_NONE;
	if (Interface->SamplingRegions.Num() == 0)
	{
		LODIndex = Interface->WholeMeshLOD;
		//If we have no regions, sample the whole mesh at the specified LOD.
		if (LODIndex == INDEX_NONE)
		{
			LODIndex = Mesh->GetLODNum() - 1;
		}
		else
		{
			LODIndex = FMath::Clamp(Interface->WholeMeshLOD, 0, Mesh->GetLODNum() - 1);
		}

		if (!Mesh->GetLODInfo(LODIndex)->bAllowCPUAccess)
		{
			UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface is trying to spawn from a whole mesh that does not allow CPU Access.\nInterface: %s\nMesh: %s\nLOD: %d"),
				*Interface->GetFullName(),
				*Mesh->GetFullName(),
				LODIndex);

			return false;
		}
	}
	else
	{
		//Sampling from regions. Gather the indices of the regions we'll sample from.
		for (FName RegionName : Interface->SamplingRegions)
		{
			int32 RegionIdx = SamplingInfo.IndexOfRegion(RegionName);
			if (RegionIdx != INDEX_NONE)
			{
				const FSkeletalMeshSamplingRegion& Region = SamplingInfo.GetRegion(RegionIdx);
				const FSkeletalMeshSamplingRegionBuiltData& RegionBuiltData = SamplingInfo.GetRegionBuiltData(RegionIdx);
				int32 RegionLODIndex = Region.LODIndex;
				if (RegionLODIndex == INDEX_NONE)
				{
					RegionLODIndex = Mesh->GetLODInfoArray().Num() - 1;
				}
				else
				{
					RegionLODIndex = FMath::Clamp(RegionLODIndex, 0, Mesh->GetLODInfoArray().Num() - 1);
				}

				if (LODIndex == INDEX_NONE)
				{
					LODIndex = RegionLODIndex;
				}

				//ensure we don't try to use two regions from different LODs.
				if (LODIndex != RegionLODIndex)
				{
					UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface is trying to use regions on different LODs of the mesh. This is currently unsupported.\nInterface: %s\nMesh: %s\nRegion: %s"),
						*Interface->GetFullName(),
						*Mesh->GetFullName(),
						*RegionName.ToString());

					return false;
				}

				if (RegionBuiltData.TriangleIndices.Num() > 0)
				{
					SamplingRegionIndices.Add(RegionIdx);
					bAllRegionsAreAreaWeighting &= Region.bSupportUniformlyDistributedSampling;
				}
				else
				{
					UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface is trying to use a region with no associated triangles.\nLOD: %d\nInterface: %s\nMesh: %s\nRegion: %s"),
						LODIndex,
						*Interface->GetFullName(),
						*Mesh->GetFullName(),
						*RegionName.ToString());

					return false;
				}
			}
			else
			{
				UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface is trying to use a region on a mesh that does not provide this region.\nInterface: %s\nMesh: %s\nRegion: %s"),
					*Interface->GetFullName(),
					*Mesh->GetFullName(),
					*RegionName.ToString());

				return false;
			}
		}
	}

	// TODO: This change is temporary to work around a crash that happens when you change the
	// source mesh on a system which is running in the level from the details panel.
	// bool bNeedDataImmediately = SystemInstance->IsSolo();
	bool bNeedDataImmediately = true;
		
	//Grab a handle to the skinning data if we have a component to skin.
	ENDISkeletalMesh_SkinningMode SkinningMode = Interface->SkinningMode;
	FSkeletalMeshSkinningDataUsage Usage(
		LODIndex,
		SkinningMode == ENDISkeletalMesh_SkinningMode::SkinOnTheFly || SkinningMode == ENDISkeletalMesh_SkinningMode::PreSkin,
		SkinningMode == ENDISkeletalMesh_SkinningMode::PreSkin,
		bNeedDataImmediately);

	if (NewSkelComp)
	{
		SkinningMode = Interface->SkinningMode;
		TWeakObjectPtr<USkeletalMeshComponent> SkelWeakCompPtr = NewSkelComp;
		FNDI_SkeletalMesh_GeneratedData& GeneratedData = SystemInstance->GetWorldManager()->GetSkeletalMeshGeneratedData();
		SkinningData = GeneratedData.GetCachedSkinningData(SkelWeakCompPtr, Usage);
	}
	else
	{
		SkinningData = FSkeletalMeshSkinningDataHandle(Usage, nullptr);
	}

	//Init area weighting sampler for Sampling regions.
	if (SamplingRegionIndices.Num() > 1 && bAllRegionsAreAreaWeighting)
	{
		//We are sampling from multiple area weighted regions so setup the inter-region weighting sampler.
		SamplingRegionAreaWeightedSampler.Init(this);
	}

	FSkinWeightVertexBuffer* SkinWeightBuffer = nullptr;
	FSkeletalMeshLODRenderData& LODData = GetLODRenderDataAndSkinWeights(SkinWeightBuffer);

	//Check for the validity of the Mesh's cpu data.

	bool LODDataNumVerticesCorrect = LODData.GetNumVertices() > 0;
	bool LODDataPositonNumVerticesCorrect = LODData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices() > 0;
	bool bSkinWeightBuffer = SkinWeightBuffer != nullptr;
	bool SkinWeightBufferNumVerticesCorrect = bSkinWeightBuffer && (SkinWeightBuffer->GetNumVertices() > 0);
	bool bIndexBufferValid = LODData.MultiSizeIndexContainer.IsIndexBufferValid();
	bool bIndexBufferFound = bIndexBufferValid && (LODData.MultiSizeIndexContainer.GetIndexBuffer() != nullptr);
	bool bIndexBufferNumCorrect = bIndexBufferFound && (LODData.MultiSizeIndexContainer.GetIndexBuffer()->Num() > 0);

	bool bMeshCPUDataValid = LODDataNumVerticesCorrect &&
		LODDataPositonNumVerticesCorrect &&
		bSkinWeightBuffer &&
		SkinWeightBufferNumVerticesCorrect && 
		bIndexBufferValid &&
		bIndexBufferFound &&
		bIndexBufferNumCorrect;

	if (!bMeshCPUDataValid)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface is trying to sample from a mesh with missing CPU vertex or index data.\nInterface: %s\nMesh: %s\nLOD: %d\n"
			"LODDataNumVerticesCorrect: %d  LODDataPositonNumVerticesCorrect : %d  bSkinWeightBuffer : %d  SkinWeightBufferNumVerticesCorrect : %d bIndexBufferValid : %d  bIndexBufferFound : %d  bIndexBufferNumCorrect : %d"),
			*Interface->GetFullName(),
			*Mesh->GetFullName(),
			LODIndex,
			LODDataNumVerticesCorrect ? 1 : 0,
			LODDataPositonNumVerticesCorrect ? 1 : 0,
			bSkinWeightBuffer ? 1 : 0,
			SkinWeightBufferNumVerticesCorrect ? 1 : 0,
			bIndexBufferValid ? 1 : 0,
			bIndexBufferFound ? 1 : 0,
			bIndexBufferNumCorrect ? 1 : 0
			);

		return false;
	}

	FReferenceSkeleton& RefSkel = Mesh->RefSkeleton;
	SpecificBones.SetNumUninitialized(Interface->SpecificBones.Num());
	TArray<FName, TInlineAllocator<16>> MissingBones;
	for (int32 BoneIdx = 0; BoneIdx < SpecificBones.Num(); ++BoneIdx)
	{
		FName BoneName = Interface->SpecificBones[BoneIdx];
		int32 Bone = RefSkel.FindBoneIndex(BoneName);
		if (Bone == INDEX_NONE)
		{
			MissingBones.Add(BoneName);
			SpecificBones[BoneIdx] = 0;
		}
		else
		{
			SpecificBones[BoneIdx] = Bone;
		}
	}

	if (MissingBones.Num() > 0)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface is trying to sample from bones that don't exist in it's skeleton.\nMesh: %s\nBones: "), *Mesh->GetName());
		for (FName BoneName : MissingBones)
		{
			UE_LOG(LogNiagara, Warning, TEXT("%s\n"), *BoneName.ToString());
		}
	}

	SpecificSockets.SetNumUninitialized(Interface->SpecificSockets.Num());
	SpecificSocketBones.SetNumUninitialized(Interface->SpecificSockets.Num());
	TArray<FName, TInlineAllocator<16>> MissingSockets;
	for (int32 SocketIdx = 0; SocketIdx < SpecificSockets.Num(); ++SocketIdx)
	{
		FName SocketName = Interface->SpecificSockets[SocketIdx];
		int32 SocketIndex = INDEX_NONE;
		USkeletalMeshSocket* Socket = Mesh->FindSocketAndIndex(SocketName, SocketIndex);
		if (SocketIndex == INDEX_NONE)
		{
			MissingSockets.Add(SocketName);
			SpecificSockets[SocketIdx] = 0;
			SpecificSocketBones[SocketIdx] = 0;
		}
		else
		{
			check(Socket);
			SpecificSockets[SocketIdx] = SocketIndex;
			SpecificSocketBones[SocketIdx] = RefSkel.FindBoneIndex(Socket->BoneName);
		}
	}

	if (MissingSockets.Num() > 0)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface is trying to sample from sockets that don't exist in it's skeleton.\nMesh: %s\nSockets: "), *Mesh->GetName());
		for (FName SocketName : MissingSockets)
		{
			UE_LOG(LogNiagara, Warning, TEXT("%s\n"), *SocketName.ToString());
		}
	}

	return true;
}

bool FNDISkeletalMesh_InstanceData::ResetRequired(UNiagaraDataInterfaceSkeletalMesh* Interface)const
{
	USceneComponent* Comp = Component.Get();
	if (!Comp)
	{
		//The component we were bound to is no longer valid so we have to trigger a reset.
		return true;
	}
		
	if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Comp))
	{
		if (!SkelComp->SkeletalMesh)
		{
			return true;
		}
		
		// Handle the case where they've procedurally swapped out the skeletal mesh from
		// the one we previously cached data for.
		if (SkelComp->SkeletalMesh != Mesh && Mesh != nullptr && SkelComp->SkeletalMesh != nullptr)
		{
			if (SkinningData.SkinningData.IsValid())
			{
				SkinningData.SkinningData.Get()->ForceDataRefresh();
			}
			return true;
		}
	}
	else
	{
		if (!Interface->DefaultMesh)
		{
			return true;
		}
	}

	if (Interface->ChangeId != ChangeId)
	{
		return true;
	}

	
	return false;
}

bool FNDISkeletalMesh_InstanceData::Tick(UNiagaraDataInterfaceSkeletalMesh* Interface, FNiagaraSystemInstance* SystemInstance, float InDeltaSeconds)
{
	if (ResetRequired(Interface))
	{
		return true;
	}
	else
	{
		DeltaSeconds = InDeltaSeconds;
		if (Component.IsValid() && Mesh)
		{
			PrevTransform = Transform;
			PrevTransformInverseTransposed = TransformInverseTransposed;
			Transform = Component->GetComponentToWorld().ToMatrixWithScale();
			TransformInverseTransposed = Transform.InverseFast().GetTransposed();
		}
		else
		{
			PrevTransform = FMatrix::Identity;
			PrevTransformInverseTransposed = FMatrix::Identity;
			Transform = FMatrix::Identity;
			TransformInverseTransposed = FMatrix::Identity;
		}
		return false;
	}
}

bool FNDISkeletalMesh_InstanceData::HasColorData()
{
	check(Mesh);
	FSkinWeightVertexBuffer* SkinWeightBuffer;
	FSkeletalMeshLODRenderData& LODData = GetLODRenderDataAndSkinWeights(SkinWeightBuffer);

	return LODData.StaticVertexBuffers.ColorVertexBuffer.GetNumVertices() != 0;
}

//Instance Data END
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// UNiagaraDataInterfaceSkeletalMesh

UNiagaraDataInterfaceSkeletalMesh::UNiagaraDataInterfaceSkeletalMesh(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, DefaultMesh(nullptr)
	, Source(nullptr)
	, SkinningMode(ENDISkeletalMesh_SkinningMode::SkinOnTheFly)
	, WholeMeshLOD(INDEX_NONE)
	, ChangeId(0)
{

}

#if WITH_EDITOR

void UNiagaraDataInterfaceSkeletalMesh::PostInitProperties()
{
	Super::PostInitProperties();

	//Can we register data interfaces as regular types and fold them into the FNiagaraVariable framework for UI and function calls etc?
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);

		//Still some issues with using custom structs. Convert node for example throws a wobbler. TODO after GDC.
		FNiagaraTypeRegistry::Register(FMeshTriCoordinate::StaticStruct(), true, true, false);
	}
}
void UNiagaraDataInterfaceSkeletalMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ChangeId++;
}

#endif //WITH_EDITOR


void UNiagaraDataInterfaceSkeletalMesh::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	GetTriangleSamplingFunctions(OutFunctions);
	GetVertexSamplingFunctions(OutFunctions);
	GetSkeletonSamplingFunctions(OutFunctions);
}

void UNiagaraDataInterfaceSkeletalMesh::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	FNDISkeletalMesh_InstanceData* InstData = (FNDISkeletalMesh_InstanceData*)InstanceData;
	USkeletalMeshComponent* SkelComp = InstData != nullptr ? Cast<USkeletalMeshComponent>(InstData->Component.Get()) : nullptr;
	
	if (!InstData || !InstData->Mesh)
	{
		OutFunc = FVMExternalFunction();
		return;
	}

	BindTriangleSamplingFunction(BindingInfo, InstData, OutFunc);

	if (OutFunc.IsBound())
	{
		return;
	}

	BindVertexSamplingFunction(BindingInfo, InstData, OutFunc);

	if (OutFunc.IsBound())
	{
		return;
	}

	BindSkeletonSamplingFunction(BindingInfo, InstData, OutFunc);
}


bool UNiagaraDataInterfaceSkeletalMesh::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceSkeletalMesh* OtherTyped = CastChecked<UNiagaraDataInterfaceSkeletalMesh>(Destination);
	OtherTyped->Source = Source;
	OtherTyped->DefaultMesh = DefaultMesh;
	OtherTyped->SkinningMode = SkinningMode;
	OtherTyped->SamplingRegions = SamplingRegions;
	OtherTyped->WholeMeshLOD = WholeMeshLOD;
	OtherTyped->SpecificBones = SpecificBones;
	OtherTyped->SpecificSockets = SpecificSockets;
	return true;
}

bool UNiagaraDataInterfaceSkeletalMesh::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceSkeletalMesh* OtherTyped = CastChecked<const UNiagaraDataInterfaceSkeletalMesh>(Other);
	return OtherTyped->Source == Source &&
		OtherTyped->DefaultMesh == DefaultMesh &&
		OtherTyped->SkinningMode == SkinningMode &&
		OtherTyped->SamplingRegions == SamplingRegions &&
		OtherTyped->WholeMeshLOD == WholeMeshLOD &&
		OtherTyped->SpecificBones == SpecificBones &&
		OtherTyped->SpecificSockets == SpecificSockets;
}

bool UNiagaraDataInterfaceSkeletalMesh::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDISkeletalMesh_InstanceData* Inst = new (PerInstanceData) FNDISkeletalMesh_InstanceData();
	check(IsAligned(PerInstanceData, 16));
	return Inst->Init(this, SystemInstance);
}

void UNiagaraDataInterfaceSkeletalMesh::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDISkeletalMesh_InstanceData* Inst = (FNDISkeletalMesh_InstanceData*)PerInstanceData;

#if WITH_EDITOR
	if(Inst->MeshSafe.IsValid())
	{
		Inst->MeshSafe.Get()->GetOnMeshChanged().RemoveAll(SystemInstance->GetComponent());
	}
#endif

	Inst->~FNDISkeletalMesh_InstanceData();
}

bool UNiagaraDataInterfaceSkeletalMesh::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float InDeltaSeconds)
{
	FNDISkeletalMesh_InstanceData* Inst = (FNDISkeletalMesh_InstanceData*)PerInstanceData;
	return Inst->Tick(this, SystemInstance, InDeltaSeconds);
}

#if WITH_EDITOR	
TArray<FNiagaraDataInterfaceError> UNiagaraDataInterfaceSkeletalMesh::GetErrors()
{
	TArray<FNiagaraDataInterfaceError> Errors;
	bool bHasCPUAccessError= false;
	bool bHasNoMeshAssignedError = false;
	
	// Collect Errors
	if (DefaultMesh != nullptr)
	{
		for (auto info : DefaultMesh->GetLODInfoArray())
		{
			if (!info.bAllowCPUAccess)
				bHasCPUAccessError = true;
		}
	}
	else
	{
		bHasNoMeshAssignedError = true;
	}

	// Report Errors
	if (Source == nullptr && bHasCPUAccessError)
	{
		FNiagaraDataInterfaceError CPUAccessNotAllowedError(FText::Format(LOCTEXT("CPUAccessNotAllowedError", "This mesh needs CPU access in order to be used properly.({0})"), FText::FromString(DefaultMesh->GetName())),
			LOCTEXT("CPUAccessNotAllowedErrorSummary", "CPU access error"),
			FNiagaraDataInterfaceFix::CreateLambda([=]()
		{
			DefaultMesh->Modify();
			for (int i = 0; i < DefaultMesh->GetLODInfoArray().Num(); i++)
			{
				FSkeletalMeshLODInfo* info = &DefaultMesh->GetLODInfoArray()[i];
				DefaultMesh->Modify();
				info->bAllowCPUAccess = true;
			}
			return true;
		}));

		Errors.Add(CPUAccessNotAllowedError);
	}

	if (Source == nullptr && bHasNoMeshAssignedError)
	{
		FNiagaraDataInterfaceError NoMeshAssignedError(LOCTEXT("NoMeshAssignedError", "This Data Interface must be assigned a skeletal mesh to operate."),
			LOCTEXT("NoMeshAssignedErrorSummary", "No mesh assigned error"),
			FNiagaraDataInterfaceFix());

		Errors.Add(NoMeshAssignedError);
	}

	return Errors;
}

//Deprecated functions we check for and advise on updates in ValidateFunction
static const FName GetTriPositionName_DEPRECATED("GetTriPosition");
static const FName GetTriPositionWSName_DEPRECATED("GetTriPositionWS");
static const FName GetTriNormalName_DEPRECATED("GetTriNormal");
static const FName GetTriNormalWSName_DEPRECATED("GetTriNormalWS");
static const FName GetTriPositionVelocityAndNormalName_DEPRECATED("GetTriPositionVelocityAndNormal");
static const FName GetTriPositionVelocityAndNormalWSName_DEPRECATED("GetTriPositionVelocityAndNormalWS");
static const FName GetTriPositionVelocityAndNormalBinormalTangentName_DEPRECATED("GetTriPositionVelocityAndNormalBinormalTangent");
static const FName GetTriPositionVelocityAndNormalBinormalTangentWSName_DEPRECATED("GetTriPositionVelocityAndNormalBinormalTangentWS");

void UNiagaraDataInterfaceSkeletalMesh::ValidateFunction(const FNiagaraFunctionSignature& Function, TArray<FText>& OutValidationErrors)
{
	TArray<FNiagaraFunctionSignature> DIFuncs;
	GetFunctions(DIFuncs);

	if (!DIFuncs.Contains(Function))
	{
		TArray<FNiagaraFunctionSignature> SkinnedDataDeprecatedFunctions;

		{
			FNiagaraFunctionSignature Sig;
			Sig.Name = GetTriPositionName_DEPRECATED;
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
			Sig.bMemberFunction = true;
			Sig.bRequiresContext = false;
			SkinnedDataDeprecatedFunctions.Add(Sig);
		}

		{
			FNiagaraFunctionSignature Sig;
			Sig.Name = GetTriPositionWSName_DEPRECATED;
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
			Sig.bMemberFunction = true;
			Sig.bRequiresContext = false;
			SkinnedDataDeprecatedFunctions.Add(Sig);
		}

		{
			FNiagaraFunctionSignature Sig;
			Sig.Name = GetTriPositionVelocityAndNormalName_DEPRECATED;
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
			Sig.bMemberFunction = true;
			Sig.bRequiresContext = false;
			SkinnedDataDeprecatedFunctions.Add(Sig);
		}

		{
			FNiagaraFunctionSignature Sig;
			Sig.Name = GetTriPositionVelocityAndNormalWSName_DEPRECATED;
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
			Sig.bMemberFunction = true;
			Sig.bRequiresContext = false;
			SkinnedDataDeprecatedFunctions.Add(Sig);
		}

		{
			FNiagaraFunctionSignature Sig;
			Sig.Name = GetTriPositionVelocityAndNormalBinormalTangentName_DEPRECATED;
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("UV Set")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Binormal")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Tangent")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("UV")));
			Sig.bMemberFunction = true;
			Sig.bRequiresContext = false;
			SkinnedDataDeprecatedFunctions.Add(Sig);
		}

		{
			FNiagaraFunctionSignature Sig;
			Sig.Name = GetTriPositionVelocityAndNormalBinormalTangentWSName_DEPRECATED;
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("UV Set")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Binormal")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Tangent")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("UV")));
			Sig.bMemberFunction = true;
			Sig.bRequiresContext = false;
			SkinnedDataDeprecatedFunctions.Add(Sig);
		}

		if (SkinnedDataDeprecatedFunctions.Contains(Function))
		{
			OutValidationErrors.Add(FText::Format(LOCTEXT("SkinnedDataFunctionDeprecationMsgFmt", "Skeletal Mesh DI Function {0} has been deprecated. Use GetSinnedTriangleData or GetSkinnedTriangleDataWS instead.\n"), FText::FromString(Function.GetName())));
		}
		else
		{
			Super::ValidateFunction(Function, OutValidationErrors);
		}
	}
}

#endif

//UNiagaraDataInterfaceSkeletalMesh END
//////////////////////////////////////////////////////////////////////////

template<>
void FSkeletalMeshAccessorHelper::Init<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::SingleRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::None>>
	(FNDISkeletalMesh_InstanceData* InstData)
{
	Comp = Cast<USkeletalMeshComponent>(InstData->Component.Get());
	Mesh = InstData->Mesh;
	LODData = &InstData->GetLODRenderDataAndSkinWeights(SkinWeightBuffer);
	IndexBuffer = LODData->MultiSizeIndexContainer.GetIndexBuffer();
	SkinningData = InstData->SkinningData.SkinningData.Get();
	Usage = InstData->SkinningData.Usage;

	if (Comp)
	{
		const USkinnedMeshComponent* BaseComp = Comp->GetBaseComponent();
		BoneComponentSpaceTransforms = &BaseComp->GetComponentSpaceTransforms();
		PrevBoneComponentSpaceTransforms = &BaseComp->GetPreviousComponentTransformsArray();
	}

	const FSkeletalMeshSamplingInfo& SamplingInfo = InstData->Mesh->GetSamplingInfo();
	SamplingRegion = &SamplingInfo.GetRegion(InstData->SamplingRegionIndices[0]);
	SamplingRegionBuiltData = &SamplingInfo.GetRegionBuiltData(InstData->SamplingRegionIndices[0]);
}

template<>
void FSkeletalMeshAccessorHelper::Init<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::SingleRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::AreaWeighted>>
	(FNDISkeletalMesh_InstanceData* InstData)
{
	Comp = Cast<USkeletalMeshComponent>(InstData->Component.Get());
	Mesh = InstData->Mesh;
	LODData = &InstData->GetLODRenderDataAndSkinWeights(SkinWeightBuffer);
	IndexBuffer = LODData->MultiSizeIndexContainer.GetIndexBuffer();
	SkinningData = InstData->SkinningData.SkinningData.Get();
	Usage = InstData->SkinningData.Usage;

	if (Comp)
	{
		const USkinnedMeshComponent* BaseComp = Comp->GetBaseComponent();
		BoneComponentSpaceTransforms = &BaseComp->GetComponentSpaceTransforms();
		PrevBoneComponentSpaceTransforms = &BaseComp->GetPreviousComponentTransformsArray();
	}

	const FSkeletalMeshSamplingInfo& SamplingInfo = InstData->Mesh->GetSamplingInfo();
	SamplingRegion = &SamplingInfo.GetRegion(InstData->SamplingRegionIndices[0]);
	SamplingRegionBuiltData = &SamplingInfo.GetRegionBuiltData(InstData->SamplingRegionIndices[0]);
}


#undef LOCTEXT_NAMESPACE
