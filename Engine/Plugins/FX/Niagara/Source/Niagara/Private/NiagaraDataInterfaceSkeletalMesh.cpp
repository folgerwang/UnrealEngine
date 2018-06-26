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

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceSkeletalMesh"

DECLARE_CYCLE_STAT(TEXT("PreSkin"), STAT_NiagaraSkel_PreSkin, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Skel Mesh Sampling"), STAT_NiagaraSkel_Sample, STATGROUP_Niagara);

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

//Instance Data END
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Helper classes for reducing duplicate code when accessing vertex positions. 

struct FSkeletalMeshAccessorHelper
{
	FSkeletalMeshAccessorHelper()
		: Comp(nullptr)
		, Mesh(nullptr)
		, LODData(nullptr)
		, SkinWeightBuffer(nullptr)
		, IndexBuffer(nullptr)
		, SamplingRegion(nullptr)
		, SamplingRegionBuiltData(nullptr)
		, SkinningData(nullptr)
	{

	}

	template<typename FilterMode, typename AreaWeightingMode>
	FORCEINLINE void Init(FNDISkeletalMesh_InstanceData* InstData);

	USkeletalMeshComponent* Comp;
	USkeletalMesh* Mesh;
	TWeakObjectPtr<USkeletalMesh> MeshSafe;
	FSkeletalMeshLODRenderData* LODData;
	FSkinWeightVertexBuffer* SkinWeightBuffer;
	FRawStaticIndexBuffer16or32Interface* IndexBuffer;
	const FSkeletalMeshSamplingRegion* SamplingRegion;
	const FSkeletalMeshSamplingRegionBuiltData* SamplingRegionBuiltData;
	FSkeletalMeshSkinningData* SkinningData;
	FSkeletalMeshSkinningDataUsage Usage;
};

template<typename FilterMode, typename AreaWeightingMode>
void FSkeletalMeshAccessorHelper::Init(FNDISkeletalMesh_InstanceData* InstData)
{
	Comp = Cast<USkeletalMeshComponent>(InstData->Component.Get());
	Mesh = InstData->Mesh;
	LODData = &InstData->GetLODRenderDataAndSkinWeights(SkinWeightBuffer);
	IndexBuffer = LODData->MultiSizeIndexContainer.GetIndexBuffer();
	SkinningData = InstData->SkinningData.SkinningData.Get();
	Usage = InstData->SkinningData.Usage;
}

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

	const FSkeletalMeshSamplingInfo& SamplingInfo = InstData->Mesh->GetSamplingInfo();
	SamplingRegion = &SamplingInfo.GetRegion(InstData->SamplingRegionIndices[0]);
	SamplingRegionBuiltData = &SamplingInfo.GetRegionBuiltData(InstData->SamplingRegionIndices[0]);
}

//////////////////////////////////////////////////////////////////////////

template<typename SkinningMode>
struct FSkinnedPositionAccessorHelper
{
	FORCEINLINE void GetSkinnedTrianglePositions(FSkeletalMeshAccessorHelper& Accessor,
		int32 Tri, FVector& OutPos0, FVector& OutPos1, FVector& OutPos2, FVector& OutPrev0, FVector& OutPrev1, FVector& OutPrev2, int32& Idx0, int32& Idx1, int32& Idx2)
	{
		checkf(false, TEXT("Must provide a specialization for this template type"));
	}

	FORCEINLINE void GetSkinnedTrianglePositions(FSkeletalMeshAccessorHelper& Accessor,
		int32 Tri, FVector& OutPos0, FVector& OutPos1, FVector& OutPos2, int32& Idx0, int32& Idx1, int32& Idx2)
	{
		checkf(false, TEXT("Must provide a specialization for this template type"));
	}
};

template<>
struct FSkinnedPositionAccessorHelper<TIntegralConstant<ENDISkeletalMesh_SkinningMode, ENDISkeletalMesh_SkinningMode::None>>
{
	FORCEINLINE void GetSkinnedTrianglePositions(FSkeletalMeshAccessorHelper& Accessor,
		int32 Tri, FVector& OutPos0, FVector& OutPos1, FVector& OutPos2, int32& Idx0, int32& Idx1, int32& Idx2)
	{
		checkSlow(Tri + 2 < Accessor.IndexBuffer->Num());
		Idx0 = Accessor.IndexBuffer->Get(Tri);
		Idx1 = Accessor.IndexBuffer->Get(Tri + 1);
		Idx2 = Accessor.IndexBuffer->Get(Tri + 2);
		
		OutPos0 = GetSkeletalMeshRefVertLocation(Accessor.Mesh, *Accessor.LODData, *Accessor.SkinWeightBuffer, Idx0);
		OutPos1 = GetSkeletalMeshRefVertLocation(Accessor.Mesh, *Accessor.LODData, *Accessor.SkinWeightBuffer, Idx1);
		OutPos2 = GetSkeletalMeshRefVertLocation(Accessor.Mesh, *Accessor.LODData, *Accessor.SkinWeightBuffer, Idx2);
	}

	FORCEINLINE void GetSkinnedTrianglePositions(FSkeletalMeshAccessorHelper& Accessor,
		int32 Tri, FVector& OutPos0, FVector& OutPos1, FVector& OutPos2, FVector& OutPrev0, FVector& OutPrev1, FVector& OutPrev2, int32& Idx0, int32& Idx1, int32& Idx2)
	{
		checkSlow(Tri + 2 < Accessor.IndexBuffer->Num());
		Idx0 = Accessor.IndexBuffer->Get(Tri);
		Idx1 = Accessor.IndexBuffer->Get(Tri + 1);
		Idx2 = Accessor.IndexBuffer->Get(Tri + 2);
		OutPos0 = GetSkeletalMeshRefVertLocation(Accessor.Mesh, *Accessor.LODData, *Accessor.SkinWeightBuffer, Idx0);
		OutPos1 = GetSkeletalMeshRefVertLocation(Accessor.Mesh, *Accessor.LODData, *Accessor.SkinWeightBuffer, Idx1);
		OutPos2 = GetSkeletalMeshRefVertLocation(Accessor.Mesh, *Accessor.LODData, *Accessor.SkinWeightBuffer, Idx2);

		OutPrev0 = OutPos0;
		OutPrev1 = OutPos1;
		OutPrev2 = OutPos2;
	}
};

template<>
struct FSkinnedPositionAccessorHelper<TIntegralConstant<ENDISkeletalMesh_SkinningMode, ENDISkeletalMesh_SkinningMode::SkinOnTheFly>>
{
	FORCEINLINE void GetSkinnedTrianglePositions(FSkeletalMeshAccessorHelper& Accessor,
		int32 Tri, FVector& OutPos0, FVector& OutPos1, FVector& OutPos2, int32& Idx0, int32& Idx1, int32& Idx2)
	{
		checkSlow(Tri + 2 < Accessor.IndexBuffer->Num());
		Idx0 = Accessor.IndexBuffer->Get(Tri);
		Idx1 = Accessor.IndexBuffer->Get(Tri + 1);
		Idx2 = Accessor.IndexBuffer->Get(Tri + 2);
		OutPos0 = USkeletalMeshComponent::GetSkinnedVertexPosition(Accessor.Comp, Idx0, *Accessor.LODData, *Accessor.SkinWeightBuffer, Accessor.SkinningData->CurrBoneRefToLocals());
		OutPos1 = USkeletalMeshComponent::GetSkinnedVertexPosition(Accessor.Comp, Idx1, *Accessor.LODData, *Accessor.SkinWeightBuffer, Accessor.SkinningData->CurrBoneRefToLocals());
		OutPos2 = USkeletalMeshComponent::GetSkinnedVertexPosition(Accessor.Comp, Idx2, *Accessor.LODData, *Accessor.SkinWeightBuffer, Accessor.SkinningData->CurrBoneRefToLocals());
	}

	FORCEINLINE void GetSkinnedTrianglePositions(FSkeletalMeshAccessorHelper& Accessor,
		int32 Tri, FVector& OutPos0, FVector& OutPos1, FVector& OutPos2, FVector& OutPrev0, FVector& OutPrev1, FVector& OutPrev2, int32& Idx0, int32& Idx1, int32& Idx2)
	{
		checkSlow(Tri + 2 < Accessor.IndexBuffer->Num());
		Idx0 = Accessor.IndexBuffer->Get(Tri);
		Idx1 = Accessor.IndexBuffer->Get(Tri + 1);
		Idx2 = Accessor.IndexBuffer->Get(Tri + 2);
		OutPos0 = USkeletalMeshComponent::GetSkinnedVertexPosition(Accessor.Comp, Idx0, *Accessor.LODData, *Accessor.SkinWeightBuffer, Accessor.SkinningData->CurrBoneRefToLocals());
		OutPos1 = USkeletalMeshComponent::GetSkinnedVertexPosition(Accessor.Comp, Idx1, *Accessor.LODData, *Accessor.SkinWeightBuffer, Accessor.SkinningData->CurrBoneRefToLocals());
		OutPos2 = USkeletalMeshComponent::GetSkinnedVertexPosition(Accessor.Comp, Idx2, *Accessor.LODData, *Accessor.SkinWeightBuffer, Accessor.SkinningData->CurrBoneRefToLocals());
		OutPrev0 = USkeletalMeshComponent::GetSkinnedVertexPosition(Accessor.Comp, Idx0, *Accessor.LODData, *Accessor.SkinWeightBuffer, Accessor.SkinningData->PrevBoneRefToLocals());
		OutPrev1 = USkeletalMeshComponent::GetSkinnedVertexPosition(Accessor.Comp, Idx1, *Accessor.LODData, *Accessor.SkinWeightBuffer, Accessor.SkinningData->PrevBoneRefToLocals());
		OutPrev2 = USkeletalMeshComponent::GetSkinnedVertexPosition(Accessor.Comp, Idx2, *Accessor.LODData, *Accessor.SkinWeightBuffer, Accessor.SkinningData->PrevBoneRefToLocals());
	}
};

template<>
struct FSkinnedPositionAccessorHelper<TIntegralConstant<ENDISkeletalMesh_SkinningMode, ENDISkeletalMesh_SkinningMode::PreSkin>>
{
	FORCEINLINE void GetSkinnedTrianglePositions(FSkeletalMeshAccessorHelper& Accessor,
		int32 Tri, FVector& OutPos0, FVector& OutPos1, FVector& OutPos2, int32& Idx0, int32& Idx1, int32& Idx2)
	{
		checkSlow(Tri + 2 < Accessor.IndexBuffer->Num());
		Idx0 = Accessor.IndexBuffer->Get(Tri);
		Idx1 = Accessor.IndexBuffer->Get(Tri + 1);
		Idx2 = Accessor.IndexBuffer->Get(Tri + 2);
		OutPos0 = Accessor.SkinningData->GetPosition(Accessor.Usage.GetLODIndex(), Idx0);
		OutPos1 = Accessor.SkinningData->GetPosition(Accessor.Usage.GetLODIndex(), Idx1);
		OutPos2 = Accessor.SkinningData->GetPosition(Accessor.Usage.GetLODIndex(), Idx2);
	}

	FORCEINLINE void GetSkinnedTrianglePositions(FSkeletalMeshAccessorHelper& Accessor,
		int32 Tri, FVector& OutPos0, FVector& OutPos1, FVector& OutPos2, FVector& OutPrev0, FVector& OutPrev1, FVector& OutPrev2, int32& Idx0, int32& Idx1, int32& Idx2)
	{
		checkSlow(Tri + 2 < Accessor.IndexBuffer->Num());
		Idx0 = Accessor.IndexBuffer->Get(Tri);
		Idx1 = Accessor.IndexBuffer->Get(Tri + 1);
		Idx2 = Accessor.IndexBuffer->Get(Tri + 2);
		OutPos0 = Accessor.SkinningData->GetPosition(Accessor.Usage.GetLODIndex(), Idx0);
		OutPos1 = Accessor.SkinningData->GetPosition(Accessor.Usage.GetLODIndex(), Idx1);
		OutPos2 = Accessor.SkinningData->GetPosition(Accessor.Usage.GetLODIndex(), Idx2);
		OutPrev0 = Accessor.SkinningData->GetPreviousPosition(Accessor.Usage.GetLODIndex(), Idx0);
		OutPrev1 = Accessor.SkinningData->GetPreviousPosition(Accessor.Usage.GetLODIndex(), Idx1);
		OutPrev2 = Accessor.SkinningData->GetPreviousPosition(Accessor.Usage.GetLODIndex(), Idx2);
	}
};

//////////////////////////////////////////////////////////////////////////
// Helper for accessing misc vertex data
template<bool bUseFullPrecisionUVs>
struct FSkelMeshVertexAccessor
{
	FORCEINLINE FVector2D GetVertexUV(FSkeletalMeshLODRenderData& LODData, int32 VertexIdx, int32 UVChannel)const
	{
		if (bUseFullPrecisionUVs)
		{
			return LODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV_Typed<EStaticMeshVertexUVType::HighPrecision>(VertexIdx, UVChannel);
		}
		else
		{
			return LODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV_Typed<EStaticMeshVertexUVType::Default>(VertexIdx, UVChannel);
		}
	}

	FORCEINLINE FLinearColor GetVertexColor(FSkeletalMeshLODRenderData& LODData, int32 VertexIdx)const
	{
		return LODData.StaticVertexBuffers.ColorVertexBuffer.VertexColor(VertexIdx);
	}
};

//////////////////////////////////////////////////////////////////////////
//Function Binders.

//External function binder choosing between template specializations based on if we're area weighting or not.
template<typename NextBinder>
struct TAreaWeightingModeBinder
{
	template<typename... ParamTypes>
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
	{
		FNDISkeletalMesh_InstanceData* InstData = (FNDISkeletalMesh_InstanceData*)InstanceData;
		check(InstData);
		UNiagaraDataInterfaceSkeletalMesh* MeshInterface = CastChecked<UNiagaraDataInterfaceSkeletalMesh>(Interface);
		const FSkeletalMeshSamplingInfo& SamplingInfo = InstData->Mesh->GetSamplingInfo();

		bool bAreaWeighting = false;
		if (InstData->SamplingRegionIndices.Num() > 1)
		{
			bAreaWeighting = InstData->SamplingRegionAreaWeightedSampler.IsValid();
		}
		else if (InstData->SamplingRegionIndices.Num() == 1)
		{
			const FSkeletalMeshSamplingRegion& Region = SamplingInfo.GetRegion(InstData->SamplingRegionIndices[0]);
			bAreaWeighting = Region.bSupportUniformlyDistributedSampling;
		}
		else
		{
			int32 LODIndex = InstData->GetLODIndex();
			check(InstData->Mesh->GetLODInfo(LODIndex)->bAllowCPUAccess);
			bAreaWeighting = InstData->Mesh->GetLODInfo(LODIndex)->bSupportUniformlyDistributedSampling;
		}

		if (bAreaWeighting)
		{
			NextBinder::template Bind<ParamTypes..., TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::AreaWeighted>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
		else
		{
			NextBinder::template Bind<ParamTypes..., TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::None>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
	}
};

//External function binder choosing between template specializations based on filtering methods
template<typename NextBinder>
struct TFilterModeBinder
{
	template<typename... ParamTypes>
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
	{
		FNDISkeletalMesh_InstanceData* InstData = (FNDISkeletalMesh_InstanceData*)InstanceData;
		check(InstData);
		UNiagaraDataInterfaceSkeletalMesh* MeshInterface = CastChecked<UNiagaraDataInterfaceSkeletalMesh>(Interface);

		if (InstData->SamplingRegionIndices.Num() == 1)
		{
			NextBinder::template Bind<ParamTypes..., TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::SingleRegion>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
		else if (InstData->SamplingRegionIndices.Num() > 1)
		{
			NextBinder::template Bind<ParamTypes..., TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::MultiRegion>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
		else
		{
			NextBinder::template Bind<ParamTypes..., TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::None>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
	}
};

//External function binder choosing between template specializations based vetrex data format
template<typename NextBinder>
struct TVertexAccessorBinder
{
	template<typename... ParamTypes>
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
	{
		FNDISkeletalMesh_InstanceData* InstData = (FNDISkeletalMesh_InstanceData*)InstanceData;
		UNiagaraDataInterfaceSkeletalMesh* MeshInterface = CastChecked<UNiagaraDataInterfaceSkeletalMesh>(Interface);
		USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(InstData->Component.Get());
		FSkinWeightVertexBuffer* SkinWeightBuffer = nullptr;
		FSkeletalMeshLODRenderData& LODData = InstData->GetLODRenderDataAndSkinWeights(SkinWeightBuffer);

		if (LODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs())
		{
			NextBinder::template Bind<ParamTypes..., FSkelMeshVertexAccessor<true>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
		else
		{
			NextBinder::template Bind<ParamTypes..., FSkelMeshVertexAccessor<false>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
	}
};

//External function binder choosing between template specializations based on skinning mode
template<typename NextBinder>
struct TSkinningModeBinder
{
	template<typename... ParamTypes>
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
	{
		FNDISkeletalMesh_InstanceData* InstData = (FNDISkeletalMesh_InstanceData*)InstanceData;
		UNiagaraDataInterfaceSkeletalMesh* MeshInterface = CastChecked<UNiagaraDataInterfaceSkeletalMesh>(Interface);
		USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(InstData->Component.Get());
		if (MeshInterface->SkinningMode == ENDISkeletalMesh_SkinningMode::None || !Component)//Can't skin if we have no component.
		{
			NextBinder::template Bind<ParamTypes..., FSkinnedPositionAccessorHelper<TIntegralConstant<ENDISkeletalMesh_SkinningMode, ENDISkeletalMesh_SkinningMode::None>>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
		else if (MeshInterface->SkinningMode == ENDISkeletalMesh_SkinningMode::SkinOnTheFly)
		{
			check(Component);
			NextBinder::template Bind<ParamTypes..., FSkinnedPositionAccessorHelper<TIntegralConstant<ENDISkeletalMesh_SkinningMode, ENDISkeletalMesh_SkinningMode::SkinOnTheFly>>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
		else if (MeshInterface->SkinningMode == ENDISkeletalMesh_SkinningMode::PreSkin)
		{
			check(Component);
			NextBinder::template Bind<ParamTypes..., FSkinnedPositionAccessorHelper<TIntegralConstant<ENDISkeletalMesh_SkinningMode, ENDISkeletalMesh_SkinningMode::PreSkin>>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
		else
		{
			checkf(false, TEXT("Invalid skinning mode in %s"), *Interface->GetPathName());
		}
	}
};

//Final binders for all static mesh interface functions.
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, RandomTriCoord);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetTriCoordPosition);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetTriCoordPositionVelocityAndNormal);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetTriCoordPositionVelocityAndNormalBinormalTangent);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetTriCoordColor);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetTriCoordUV);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, IsValidTriCoord);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetFilteredTriangleCount);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetFilteredTriangleAt);

//Function Binders END
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// UNiagaraDataInterfaceSkeletalMesh

UNiagaraDataInterfaceSkeletalMesh::UNiagaraDataInterfaceSkeletalMesh(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, DefaultMesh(nullptr)
	, Source(nullptr)
	, SkinningMode(ENDISkeletalMesh_SkinningMode::PreSkin)
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

static const FName RandomTriCoordName("RandomTriCoord");

static const FName GetTriPositionName("GetTriPosition");
static const FName GetTriPositionWSName("GetTriPositionWS");

static const FName GetTriNormalName("GetTriNormal");
static const FName GetTriNormalWSName("GetTriNormalWS");
static const FName IsValidTriCoordName("IsValidTriCoord");

static const FName GetTriColorName("GetTriColor");
static const FName GetTriUVName("GetTriUV");
static const FName GetTriangleCountName("GetFilteredTriangleCount");
static const FName GetTriangleAtName("GetFilteredTriangle");

static const FName GetTriPositionVelocityAndNormalName("GetTriPositionVelocityAndNormal");
static const FName GetTriPositionVelocityAndNormalWSName("GetTriPositionVelocityAndNormalWS");

static const FName GetTriPositionVelocityAndNormalBinormalTangentName("GetTriPositionVelocityAndNormalBinormalTangent");
static const FName GetTriPositionVelocityAndNormalBinormalTangentWSName("GetTriPositionVelocityAndNormalBinormalTangentWS");


void UNiagaraDataInterfaceSkeletalMesh::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = RandomTriCoordName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = IsValidTriCoordName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("IsValid")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("IsValidDesc","Determine if this tri coordinate's triangle index is valid for this mesh. Note that this only checks the mesh index buffer size and does not include any filtering settings." );
#endif
		OutFunctions.Add(Sig);
	}
	
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetTriPositionName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetTriPositionWSName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetTriPositionVelocityAndNormalName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetTriPositionVelocityAndNormalWSName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetTriPositionVelocityAndNormalBinormalTangentName;
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
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetTriPositionVelocityAndNormalBinormalTangentWSName;
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
		OutFunctions.Add(Sig);
	}
		
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetTriColorName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetTriUVName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("UV Set")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("UV")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetTriangleCountName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetTriangleAtName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}
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

	bool bNeedsVertexColors = false;

	if (BindingInfo.Name == RandomTriCoordName)
	{
		check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4);
		TFilterModeBinder<TAreaWeightingModeBinder<NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, RandomTriCoord)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == IsValidTriCoordName)
	{
		check(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 1);
		TFilterModeBinder<TAreaWeightingModeBinder<TNDIParamBinder<0, int32, TNDIParamBinder<1, float, TNDIParamBinder<2, float, TNDIParamBinder<3, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, IsValidTriCoord)>>>>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetTriPositionName)
	{
		check(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 3);
		TSkinningModeBinder<TNDIExplicitBinder<FNDITransformHandlerNoop, TNDIParamBinder<0, int32, TNDIParamBinder<1, float, TNDIParamBinder<2, float, TNDIParamBinder<3, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetTriCoordPosition)>>>>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetTriPositionWSName)
	{
		check(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 3);
		TSkinningModeBinder<TNDIExplicitBinder<FNDITransformHandler, TNDIParamBinder<0, int32, TNDIParamBinder<1, float, TNDIParamBinder<2, float, TNDIParamBinder<3, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetTriCoordPosition)>>>>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetTriPositionVelocityAndNormalName)
	{
		check(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 9);
		TSkinningModeBinder<TNDIExplicitBinder<FNDITransformHandlerNoop, TNDIParamBinder<0, int32, TNDIParamBinder<1, float, TNDIParamBinder<2, float, TNDIParamBinder<3, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetTriCoordPositionVelocityAndNormal)>>>>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetTriPositionVelocityAndNormalWSName)
	{
		check(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 9);
		TSkinningModeBinder<TNDIExplicitBinder<FNDITransformHandler, TNDIParamBinder<0, int32, TNDIParamBinder<1, float, TNDIParamBinder<2, float, TNDIParamBinder<3, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetTriCoordPositionVelocityAndNormal)>>>>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetTriPositionVelocityAndNormalBinormalTangentName)
	{
		check(BindingInfo.GetNumInputs() == 6 && BindingInfo.GetNumOutputs() == 17);
		TSkinningModeBinder<TNDIExplicitBinder<FNDITransformHandlerNoop, TVertexAccessorBinder<TNDIParamBinder<0, int32, TNDIParamBinder<1, float, TNDIParamBinder<2, float, TNDIParamBinder<3, float, TNDIParamBinder<4, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetTriCoordPositionVelocityAndNormalBinormalTangent)>>>>>>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetTriPositionVelocityAndNormalBinormalTangentWSName)
	{
		check(BindingInfo.GetNumInputs() == 6 && BindingInfo.GetNumOutputs() == 17);
		TSkinningModeBinder<TNDIExplicitBinder<FNDITransformHandler, TVertexAccessorBinder<TNDIParamBinder<0, int32, TNDIParamBinder<1, float, TNDIParamBinder<2, float, TNDIParamBinder<3, float, TNDIParamBinder<4, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetTriCoordPositionVelocityAndNormalBinormalTangent)>>>>>>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetTriColorName)
	{
		check(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 4);
		bNeedsVertexColors = true;
		TNDIParamBinder<0, int32, TNDIParamBinder<1, float, TNDIParamBinder<2, float, TNDIParamBinder<3, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetTriCoordColor)>>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetTriUVName)
	{
		check(BindingInfo.GetNumInputs() == 6 && BindingInfo.GetNumOutputs() == 2);
		TVertexAccessorBinder<TNDIParamBinder<0, int32, TNDIParamBinder<1, float, TNDIParamBinder<2, float, TNDIParamBinder<3, float, TNDIParamBinder<4, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetTriCoordUV)>>>>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetTriangleCountName)
	{
		check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1);
		TFilterModeBinder<TAreaWeightingModeBinder<NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetFilteredTriangleCount)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetTriangleAtName)
	{
		check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 4);
		TFilterModeBinder<TAreaWeightingModeBinder<TNDIParamBinder<0, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetFilteredTriangleAt)>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	
	check(InstData->Mesh);
	FSkinWeightVertexBuffer* SkinWeightBuffer;
	FSkeletalMeshLODRenderData& LODData = InstData->GetLODRenderDataAndSkinWeights(SkinWeightBuffer);

	if (bNeedsVertexColors && LODData.StaticVertexBuffers.ColorVertexBuffer.GetNumVertices() == 0)
	{
		UE_LOG(LogNiagara, Log, TEXT("Skeletal Mesh data interface is cannot run as it's reading color data on a mesh that does not provide it. - Mesh:%s  "), *InstData->Mesh->GetFullName());
		OutFunc = FVMExternalFunction();
	}

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
		OtherTyped->WholeMeshLOD == WholeMeshLOD;
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
	bool bHasError= false;
	if (DefaultMesh != nullptr)
	{
		for (auto info : DefaultMesh->GetLODInfoArray())
		{
			if (!info.bAllowCPUAccess)
				bHasError = true;
		}
	}

	if (Source == nullptr && bHasError)
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
	return Errors;
}
#endif

//////////////////////////////////////////////////////////////////////////

template<typename FilterMode, typename AreaWeightingMode>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::RandomTriIndex(FRandomStream& RandStream, FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	checkf(false, TEXT("Invalid template call for RandomTriIndex. Bug in Filter binding or Area Weighting binding. Contact code team."));
	return 0;
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::RandomTriIndex<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::None>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::None>>
	(FRandomStream& RandStream, FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	int32 SecIdx = RandStream.RandRange(0, Accessor.LODData->RenderSections.Num() - 1);
	FSkelMeshRenderSection& Sec = Accessor.LODData->RenderSections[SecIdx];
	int32 Tri = RandStream.RandRange(0, Sec.NumTriangles - 1);
	return Sec.BaseIndex + Tri * 3;
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::RandomTriIndex<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::None>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::AreaWeighted>>
	(FRandomStream& RandStream, FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	const FSkeletalMeshSamplingInfo& SamplingInfo = InstData->Mesh->GetSamplingInfo();
	const FSkeletalMeshSamplingLODBuiltData& WholeMeshBuiltData = SamplingInfo.GetWholeMeshLODBuiltData(InstData->GetLODIndex());
	int32 TriIdx = WholeMeshBuiltData.AreaWeightedTriangleSampler.GetEntryIndex(RandStream.GetFraction(), RandStream.GetFraction());
	return TriIdx * 3;
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::RandomTriIndex<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::SingleRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::None>>
	(FRandomStream& RandStream, FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	int32 Idx = RandStream.RandRange(0, Accessor.SamplingRegionBuiltData->TriangleIndices.Num() - 1);
	return Accessor.SamplingRegionBuiltData->TriangleIndices[Idx];
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::RandomTriIndex<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::SingleRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::AreaWeighted>>
	(FRandomStream& RandStream, FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	int32 Idx = Accessor.SamplingRegionBuiltData->AreaWeightedSampler.GetEntryIndex(RandStream.GetFraction(), RandStream.GetFraction());
	return Accessor.SamplingRegionBuiltData->TriangleIndices[Idx];
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::RandomTriIndex<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::MultiRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::None>>
	(FRandomStream& RandStream, FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	int32 RegionIdx = RandStream.RandRange(0, InstData->SamplingRegionIndices.Num() - 1);
	const FSkeletalMeshSamplingInfo& SamplingInfo = InstData->Mesh->GetSamplingInfo();
	const FSkeletalMeshSamplingRegion& Region = SamplingInfo.GetRegion(InstData->SamplingRegionIndices[RegionIdx]);
	const FSkeletalMeshSamplingRegionBuiltData& RegionBuiltData = SamplingInfo.GetRegionBuiltData(InstData->SamplingRegionIndices[RegionIdx]);
	int32 Idx = RandStream.RandRange(0, RegionBuiltData.TriangleIndices.Num() - 1);
	return RegionBuiltData.TriangleIndices[Idx];
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::RandomTriIndex<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::MultiRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::AreaWeighted>>
	(FRandomStream& RandStream, FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	int32 RegionIdx = InstData->SamplingRegionAreaWeightedSampler.GetEntryIndex(RandStream.GetFraction(), RandStream.GetFraction());
	const FSkeletalMeshSamplingInfo& SamplingInfo = InstData->Mesh->GetSamplingInfo();
	const FSkeletalMeshSamplingRegion& Region = SamplingInfo.GetRegion(InstData->SamplingRegionIndices[RegionIdx]);
	const FSkeletalMeshSamplingRegionBuiltData& RegionBuiltData = SamplingInfo.GetRegionBuiltData(InstData->SamplingRegionIndices[RegionIdx]);
	int32 Idx = RegionBuiltData.AreaWeightedSampler.GetEntryIndex(RandStream.GetFraction(), RandStream.GetFraction());
	return RegionBuiltData.TriangleIndices[Idx];
}

template<typename FilterMode, typename AreaWeightingMode>
void UNiagaraDataInterfaceSkeletalMesh::RandomTriCoord(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Sample);
	FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);
	checkfSlow(InstData.Get(), TEXT("Skeletal Mesh Interface has invalid instance data. %s"), *GetPathName());
	checkfSlow(InstData->Mesh, TEXT("Skeletal Mesh Interface has invalid mesh. %s"), *GetPathName());

	FRegisterHandler<int32> OutTri(Context);
	FRegisterHandler<float> OutBaryX(Context);	FRegisterHandler<float> OutBaryY(Context);	FRegisterHandler<float> OutBaryZ(Context);

	FSkeletalMeshAccessorHelper MeshAccessor;
	MeshAccessor.Init<FilterMode, AreaWeightingMode>(InstData);
	
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutTri.GetDest() = RandomTriIndex<FilterMode, AreaWeightingMode>(Context.RandStream, MeshAccessor, InstData);
		FVector Bary = RandomBarycentricCoord(Context.RandStream);
		*OutBaryX.GetDest() = Bary.X;		*OutBaryY.GetDest() = Bary.Y;		*OutBaryZ.GetDest() = Bary.Z;
		
		OutTri.Advance();
		OutBaryX.Advance();		OutBaryY.Advance();		OutBaryZ.Advance();
	}
}

template<typename FilterMode, typename AreaWeightingMode, typename TriType, typename BaryXType, typename BaryYType, typename BaryZType >
void UNiagaraDataInterfaceSkeletalMesh::IsValidTriCoord(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Sample);
	
	TriType TriParam(Context);
	BaryXType BaryXParam(Context);	BaryYType BaryYParam(Context);	BaryZType BaryZParam(Context);
	
	FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);
	checkfSlow(InstData.Get(), TEXT("Skeletal Mesh Interface has invalid instance data. %s"), *GetPathName());
	checkfSlow(InstData->Mesh, TEXT("Skeletal Mesh Interface has invalid mesh. %s"), *GetPathName());
	
	FRegisterHandler<FNiagaraBool> OutValid(Context);

	FSkeletalMeshAccessorHelper MeshAccessor;
	MeshAccessor.Init<FilterMode, AreaWeightingMode>(InstData);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 RequestedIndex = TriParam.Get() + 2; // Get the last triangle index of the set

		FNiagaraBool Value;
		Value.SetValue(MeshAccessor.IndexBuffer != nullptr && MeshAccessor.IndexBuffer->Num() > RequestedIndex);
		*OutValid.GetDest() = Value;

		OutValid.Advance();
		BaryXParam.Advance();		BaryYParam.Advance();		BaryZParam.Advance(); TriParam.Advance();
	}
}

//////////////////////////////////////////////////////////////////////////

template<typename FilterMode, typename AreaWeightingMode>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleCount(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	checkf(false, TEXT("Invalid template call for GetSpecificTriangleCount. Bug in Filter binding or Area Weighting binding. Contact code team."));
	return 0;
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleCount<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::None>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::None>>
	(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	int32 NumTris = 0;
	for (int32 i = 0; i < Accessor.LODData->RenderSections.Num(); i++)
	{
		NumTris += Accessor.LODData->RenderSections[i].NumTriangles;
	}
	return NumTris;
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleCount<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::None>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::AreaWeighted>>
	(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	const FSkeletalMeshSamplingInfo& SamplingInfo = InstData->Mesh->GetSamplingInfo();
	const FSkeletalMeshSamplingLODBuiltData& WholeMeshBuiltData = SamplingInfo.GetWholeMeshLODBuiltData(InstData->GetLODIndex());
	return WholeMeshBuiltData.AreaWeightedTriangleSampler.GetNumEntries();
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleCount<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::SingleRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::None>>
	(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	return Accessor.SamplingRegionBuiltData->TriangleIndices.Num();
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleCount<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::SingleRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::AreaWeighted>>
	(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	return Accessor.SamplingRegionBuiltData->AreaWeightedSampler.GetNumEntries();
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleCount<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::MultiRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::None>>
	(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	int32 NumTris = 0;

	for (int32 RegionIdx = 0; RegionIdx < InstData->SamplingRegionIndices.Num(); RegionIdx++)
	{
		const FSkeletalMeshSamplingInfo& SamplingInfo = InstData->Mesh->GetSamplingInfo();
		const FSkeletalMeshSamplingRegion& Region = SamplingInfo.GetRegion(InstData->SamplingRegionIndices[RegionIdx]);
		const FSkeletalMeshSamplingRegionBuiltData& RegionBuiltData = SamplingInfo.GetRegionBuiltData(InstData->SamplingRegionIndices[RegionIdx]);
		NumTris += RegionBuiltData.TriangleIndices.Num();
	}
	return NumTris;
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleCount<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::MultiRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::AreaWeighted>>
	(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData)
{
	int32 NumTris = 0;

	for (int32 RegionIdx = 0; RegionIdx < InstData->SamplingRegionIndices.Num(); RegionIdx++)
	{
		const FSkeletalMeshSamplingInfo& SamplingInfo = InstData->Mesh->GetSamplingInfo();
		const FSkeletalMeshSamplingRegion& Region = SamplingInfo.GetRegion(InstData->SamplingRegionIndices[RegionIdx]);
		const FSkeletalMeshSamplingRegionBuiltData& RegionBuiltData = SamplingInfo.GetRegionBuiltData(InstData->SamplingRegionIndices[RegionIdx]);
		NumTris += RegionBuiltData.TriangleIndices.Num();
	}
	return NumTris;
}

template<typename FilterMode, typename AreaWeightingMode>
void UNiagaraDataInterfaceSkeletalMesh::GetFilteredTriangleCount(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Sample);
	FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);
	checkfSlow(InstData.Get(), TEXT("Skeletal Mesh Interface has invalid instance data. %s"), *GetPathName());
	checkfSlow(InstData->Mesh, TEXT("Skeletal Mesh Interface has invalid mesh. %s"), *GetPathName());

	FRegisterHandler<int32> OutTri(Context);

	FSkeletalMeshAccessorHelper MeshAccessor;
	MeshAccessor.Init<FilterMode, AreaWeightingMode>(InstData);

	int32 Count = GetSpecificTriangleCount<FilterMode, AreaWeightingMode>(MeshAccessor, InstData);
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutTri.GetDest() = Count;
		OutTri.Advance();
	}
}


//////////////////////////////////////////////////////////////////////////

template<typename FilterMode, typename AreaWeightingMode>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleAt(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData, int32 FilteredIndex)
{
	checkf(false, TEXT("Invalid template call for GetSpecificTriangleAt. Bug in Filter binding or Area Weighting binding. Contact code team."));
	return 0;
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleAt<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::None>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::None>>
	(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData, int32 FilteredIndex)
{
	for (int32 i = 0; i < Accessor.LODData->RenderSections.Num(); i++)
	{
		if (Accessor.LODData->RenderSections[i].NumTriangles > (uint32)FilteredIndex)
		{
			FSkelMeshRenderSection& Sec = Accessor.LODData->RenderSections[i];
			return Sec.BaseIndex + FilteredIndex * 3;
		}
		FilteredIndex -= Accessor.LODData->RenderSections[i].NumTriangles;
	}
	return 0;
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleAt<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::None>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::AreaWeighted>>
	(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData, int32 FilteredIndex)
{
	int32 TriIdx = FilteredIndex;
	return TriIdx * 3;
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleAt<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::SingleRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::None>>
	(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData, int32 FilteredIndex)
{
	int32 MaxIdx = Accessor.SamplingRegionBuiltData->TriangleIndices.Num() - 1;
	FilteredIndex = FMath::Min(FilteredIndex, MaxIdx);
	return Accessor.SamplingRegionBuiltData->TriangleIndices[FilteredIndex];
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleAt<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::SingleRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::AreaWeighted>>
	(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData, int32 FilteredIndex)
{
	int32 Idx = FilteredIndex;
	int32 MaxIdx = Accessor.SamplingRegionBuiltData->TriangleIndices.Num() - 1;
	Idx = FMath::Min(Idx, MaxIdx);

	return Accessor.SamplingRegionBuiltData->TriangleIndices[Idx];
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleAt<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::MultiRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::None>>
	(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData, int32 FilteredIndex)
{
	for (int32 RegionIdx = 0; RegionIdx < InstData->SamplingRegionIndices.Num(); RegionIdx++)
	{
		const FSkeletalMeshSamplingInfo& SamplingInfo = InstData->Mesh->GetSamplingInfo();
		const FSkeletalMeshSamplingRegion& Region = SamplingInfo.GetRegion(InstData->SamplingRegionIndices[RegionIdx]);
		const FSkeletalMeshSamplingRegionBuiltData& RegionBuiltData = SamplingInfo.GetRegionBuiltData(InstData->SamplingRegionIndices[RegionIdx]);
		if (FilteredIndex < RegionBuiltData.TriangleIndices.Num())
		{
			return RegionBuiltData.TriangleIndices[FilteredIndex];
		}

		FilteredIndex -= RegionBuiltData.TriangleIndices.Num();
	}
	return 0;
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceSkeletalMesh::GetSpecificTriangleAt<
	TIntegralConstant<ENDISkeletalMesh_FilterMode, ENDISkeletalMesh_FilterMode::MultiRegion>,
	TIntegralConstant<ENDISkelMesh_AreaWeightingMode, ENDISkelMesh_AreaWeightingMode::AreaWeighted>>
	(FSkeletalMeshAccessorHelper& Accessor, FNDISkeletalMesh_InstanceData* InstData, int32 FilteredIndex)
{
	for (int32 RegionIdx = 0; RegionIdx < InstData->SamplingRegionIndices.Num(); RegionIdx++)
	{
		const FSkeletalMeshSamplingInfo& SamplingInfo = InstData->Mesh->GetSamplingInfo();
		const FSkeletalMeshSamplingRegion& Region = SamplingInfo.GetRegion(InstData->SamplingRegionIndices[RegionIdx]);
		const FSkeletalMeshSamplingRegionBuiltData& RegionBuiltData = SamplingInfo.GetRegionBuiltData(InstData->SamplingRegionIndices[RegionIdx]);
		if (FilteredIndex < RegionBuiltData.TriangleIndices.Num())
		{
			return RegionBuiltData.TriangleIndices[FilteredIndex];
		}
		FilteredIndex -= RegionBuiltData.TriangleIndices.Num();
	}
	return 0;
}

template<typename FilterMode, typename AreaWeightingMode, typename TriType>
void UNiagaraDataInterfaceSkeletalMesh::GetFilteredTriangleAt(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Sample);

	TriType TriParam(Context);	
	FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);
	checkfSlow(InstData.Get(), TEXT("Skeletal Mesh Interface has invalid instance data. %s"), *GetPathName());
	checkfSlow(InstData->Mesh, TEXT("Skeletal Mesh Interface has invalid mesh. %s"), *GetPathName());
	FRegisterHandler<int32> OutTri(Context);
	FRegisterHandler<float> OutBaryX(Context);	FRegisterHandler<float> OutBaryY(Context);	FRegisterHandler<float> OutBaryZ(Context);

	FSkeletalMeshAccessorHelper Accessor;
	Accessor.Init<FilterMode, AreaWeightingMode>(InstData);
	
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 Tri = TriParam.Get();
		int32 RealIdx = 0;
		RealIdx = GetSpecificTriangleAt<FilterMode, AreaWeightingMode>(Accessor, InstData, Tri);

		int32 TriMax = Accessor.IndexBuffer->Num() - 3;
		RealIdx = FMath::Min(RealIdx, TriMax);
		
		*OutTri.GetDest() = RealIdx;
		float Coord = 1.0f / 3.0f;
		*OutBaryX.GetDest() = Coord;		*OutBaryY.GetDest() = Coord;		*OutBaryZ.GetDest() = Coord;

		TriParam.Advance();
		OutTri.Advance();
		OutBaryX.Advance();		OutBaryY.Advance();		OutBaryZ.Advance();
	}
}

template<typename SkinningHandlerType, typename TransformHandlerType, typename TriType, typename BaryXType, typename BaryYType, typename BaryZType>
void UNiagaraDataInterfaceSkeletalMesh::GetTriCoordPosition(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Sample);
	SkinningHandlerType SkinningHandler;
	TransformHandlerType TransformHandler;
	TriType TriParam(Context);
	BaryXType BaryXParam(Context);	BaryYType BaryYParam(Context);	BaryZType BaryZParam(Context);
	FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);

	checkfSlow(InstData.Get(), TEXT("Skeletal Mesh Interface has invalid instance data. %s"), *GetPathName());
	checkfSlow(InstData->Mesh, TEXT("Skeletal Mesh Interface has invalid mesh. %s"), *GetPathName());

	FRegisterHandler<float> OutPosX(Context);	FRegisterHandler<float> OutPosY(Context);	FRegisterHandler<float> OutPosZ(Context);

	FSkeletalMeshAccessorHelper Accessor;
	Accessor.Init<TIntegralConstant<int32, 0>,TIntegralConstant<int32, 0>>(InstData);
	int32 TriMax = Accessor.IndexBuffer->Num() - 3;
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 Tri = TriParam.Get();
		FVector V0;		FVector V1;		FVector V2;
		int32 Idx0; int32 Idx1; int32 Idx2;

		Tri = FMath::Min(Tri, TriMax);
		
		SkinningHandler.GetSkinnedTrianglePositions(Accessor, Tri, V0, V1, V2, Idx0, Idx1, Idx2);
		
		FVector Pos = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(), V0, V1, V2);
		TransformHandler.TransformPosition(Pos, InstData->Transform);

		*OutPosX.GetDest() = Pos.X;		*OutPosY.GetDest() = Pos.Y;		*OutPosZ.GetDest() = Pos.Z;

		TriParam.Advance();
		BaryXParam.Advance();	BaryYParam.Advance();		BaryZParam.Advance();
		OutPosX.Advance();		OutPosY.Advance();			OutPosZ.Advance();
	}
}

template<typename SkinningHandlerType, typename TransformHandlerType, typename TriType, typename BaryXType, typename BaryYType, typename BaryZType>
void UNiagaraDataInterfaceSkeletalMesh::GetTriCoordPositionVelocityAndNormal(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Sample);
	SkinningHandlerType SkinningHandler;
	TransformHandlerType TransformHandler;
	TriType TriParam(Context);
	BaryXType BaryXParam(Context);	BaryYType BaryYParam(Context);	BaryZType BaryZParam(Context);
	FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);

	checkfSlow(InstData.Get(), TEXT("Skeletal Mesh Interface has invalid instance data. %s"), *GetPathName());
	checkfSlow(InstData->Mesh, TEXT("Skeletal Mesh Interface has invalid mesh. %s"), *GetPathName());

	FRegisterHandler<float> OutPosX(Context);	FRegisterHandler<float> OutPosY(Context);	FRegisterHandler<float> OutPosZ(Context);
	FRegisterHandler<float> OutVelX(Context);	FRegisterHandler<float> OutVelY(Context);	FRegisterHandler<float> OutVelZ(Context);
	FRegisterHandler<float> OutNormX(Context);	FRegisterHandler<float> OutNormY(Context);	FRegisterHandler<float> OutNormZ(Context);

	FSkeletalMeshAccessorHelper Accessor;
	Accessor.Init<TIntegralConstant<int32, 0>, TIntegralConstant<int32, 0>>(InstData);
	float InvDt = 1.0f / InstData->DeltaSeconds;
	int32 TriMax = Accessor.IndexBuffer->Num() - 3;
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 Tri = TriParam.Get();
		FVector Pos0;		FVector Pos1;		FVector Pos2;
		FVector Prev0;		FVector Prev1;		FVector Prev2;		
		int32 Idx0; int32 Idx1; int32 Idx2;
		Tri = FMath::Min(Tri, TriMax);
		SkinningHandler.GetSkinnedTrianglePositions(Accessor, Tri, Pos0, Pos1, Pos2, Prev0, Prev1, Prev2, Idx0, Idx1, Idx2);

		FVector Pos = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(), Pos0, Pos1, Pos2);
		FVector Prev = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(), Prev0, Prev1, Prev2);
		TransformHandler.TransformPosition(Pos, InstData->Transform);
		TransformHandler.TransformPosition(Prev, InstData->PrevTransform);

		FVector Vel = (Pos - Prev) * InvDt;

		// Normal calculation code based on tools code found at:
		// \Engine\Source\Developer\MeshUtilities\Private\MeshUtilities.cpp
		FVector Normal = ((Pos1 - Pos2) ^ (Pos0 - Pos2)).GetUnsafeNormal();//Temporarily having to get a dirty normal here until the newer pre skinning goodness comes online.
		TransformHandler.TransformVector(Normal, InstData->Transform);

		*OutPosX.GetDest() = Pos.X;		*OutPosY.GetDest() = Pos.Y;		*OutPosZ.GetDest() = Pos.Z;
		*OutVelX.GetDest() = Vel.X;		*OutVelY.GetDest() = Vel.Y;		*OutVelZ.GetDest() = Vel.Z;
		*OutNormX.GetDest() = Normal.X;	*OutNormY.GetDest() = Normal.Y;	*OutNormZ.GetDest() = Normal.Z;
// 		//TODO: Use the velocity temp buffer as storage for the prev position so I can SIMD the velocity calculation.
// 		*OutVelX.GetDest() = Prev.X;		*OutVelY.GetDest() = Prev.Y;		*OutVelZ.GetDest() = Prev.Z;

		TriParam.Advance();
		BaryXParam.Advance();	BaryYParam.Advance();		BaryZParam.Advance();
		OutPosX.Advance();		OutPosY.Advance();			OutPosZ.Advance();
		OutVelX.Advance();		OutVelY.Advance();			OutVelZ.Advance();
		OutNormX.Advance();		OutNormY.Advance();			OutNormZ.Advance();
	}
}

template<typename SkinningHandlerType, typename TransformHandlerType, typename VertexAccessorType, typename TriType, typename BaryXType, typename BaryYType, typename BaryZType, typename UVSetType>
void UNiagaraDataInterfaceSkeletalMesh::GetTriCoordPositionVelocityAndNormalBinormalTangent(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Sample);
	SkinningHandlerType SkinningHandler;
	TransformHandlerType TransformHandler;
	VertexAccessorType VertAccessor;
	TriType TriParam(Context);
	BaryXType BaryXParam(Context);	BaryYType BaryYParam(Context);	BaryZType BaryZParam(Context);
	UVSetType UVSetParam(Context);

	FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);

	checkfSlow(InstData.Get(), TEXT("Skeletal Mesh Interface has invalid instance data. %s"), *GetPathName());
	checkfSlow(InstData->Mesh, TEXT("Skeletal Mesh Interface has invalid mesh. %s"), *GetPathName());

	FRegisterHandler<float> OutPosX(Context);	FRegisterHandler<float> OutPosY(Context);	FRegisterHandler<float> OutPosZ(Context);
	FRegisterHandler<float> OutVelX(Context);	FRegisterHandler<float> OutVelY(Context);	FRegisterHandler<float> OutVelZ(Context);
	FRegisterHandler<float> OutNormX(Context);	FRegisterHandler<float> OutNormY(Context);	FRegisterHandler<float> OutNormZ(Context);
	FRegisterHandler<float> OutBiNormX(Context);	FRegisterHandler<float> OutBiNormY(Context);	FRegisterHandler<float> OutBiNormZ(Context);
	FRegisterHandler<float> OutTangentX(Context);	FRegisterHandler<float> OutTangentY(Context);	FRegisterHandler<float> OutTangentZ(Context);
	FRegisterHandler<float> OutUVX(Context);	FRegisterHandler<float> OutUVY(Context);

	FSkinWeightVertexBuffer* SkinWeightBuffer;
	FSkeletalMeshLODRenderData& LODData = InstData->GetLODRenderDataAndSkinWeights(SkinWeightBuffer);

	FSkeletalMeshAccessorHelper Accessor;
	Accessor.Init<TIntegralConstant<int32, 0>, TIntegralConstant<int32, 0>>(InstData);
	int32 TriMax = Accessor.IndexBuffer->Num() - 3;
	float InvDt = 1.0f / InstData->DeltaSeconds;
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 Tri = FMath::Min(TriParam.Get(), TriMax);
		FVector Pos0;		FVector Pos1;		FVector Pos2;
		FVector Prev0;		FVector Prev1;		FVector Prev2;
		FVector Normal;
		FVector Binormal;
		FVector Tangent;
		int32 Idx0; int32 Idx1; int32 Idx2;
		SkinningHandler.GetSkinnedTrianglePositions(Accessor, Tri, Pos0, Pos1, Pos2, Prev0, Prev1, Prev2, Idx0, Idx1, Idx2);
		int32 UVSet = UVSetParam.Get();

		FVector Pos = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(), Pos0, Pos1, Pos2);
		FVector Prev = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(), Prev0, Prev1, Prev2);

		FMatrix Transform = InstData->Transform;
		FMatrix PrevTransform = InstData->PrevTransform;
		TransformHandler.TransformPosition(Pos, Transform);
		TransformHandler.TransformPosition(Prev, PrevTransform);

		FVector2D UV0 = VertAccessor.GetVertexUV(LODData, Idx0, UVSet);
		FVector2D UV1 = VertAccessor.GetVertexUV(LODData, Idx1, UVSet);
		FVector2D UV2 = VertAccessor.GetVertexUV(LODData, Idx2, UVSet);

		Normal = ((Pos1 - Pos2) ^ (Pos0 - Pos2)).GetSafeNormal();
		
		// Normal binormal tangent calculation code based on tools code found at:
		// \Engine\Source\Developer\MeshUtilities\Private\MeshUtilities.cpp
		// Skeletal_ComputeTriangleTangents
		FMatrix	ParameterToLocal(
			FPlane(Pos1.X - Pos0.X, Pos1.Y - Pos0.Y, Pos1.Z - Pos0.Z, 0),
			FPlane(Pos2.X - Pos0.X, Pos2.Y - Pos0.Y, Pos2.Z - Pos0.Z, 0),
			FPlane(Pos0.X, Pos0.Y, Pos0.Z, 0),
			FPlane(0, 0, 0, 1)
		);

		FMatrix ParameterToTexture(
			FPlane(UV1.X - UV0.X, UV1.Y - UV0.Y, 0, 0),
			FPlane(UV2.X - UV0.X, UV2.Y - UV0.Y, 0, 0),
			FPlane(UV0.X, UV0.Y, 1, 0),
			FPlane(0, 0, 0, 1)
		);

		// Use InverseSlow to catch singular matrices.  Inverse can miss this sometimes.
		const FMatrix TextureToLocal = ParameterToTexture.Inverse() * ParameterToLocal;

		Binormal = (TextureToLocal.TransformVector(FVector(1, 0, 0)).GetSafeNormal());
		Tangent = (TextureToLocal.TransformVector(FVector(0, 1, 0)).GetSafeNormal());

		TransformHandler.TransformVector(Normal, Transform);
		TransformHandler.TransformVector(Binormal, Transform);
		TransformHandler.TransformVector(Tangent, Transform);
		
		FVector Vel = (Pos - Prev) * InvDt;

		*OutPosX.GetDest() = Pos.X;		*OutPosY.GetDest() = Pos.Y;		*OutPosZ.GetDest() = Pos.Z;
		*OutVelX.GetDest() = Vel.X;		*OutVelY.GetDest() = Vel.Y;		*OutVelZ.GetDest() = Vel.Z;
		*OutNormX.GetDest() = Normal.X;	*OutNormY.GetDest() = Normal.Y;	*OutNormZ.GetDest() = Normal.Z;
		*OutBiNormX.GetDest() = Binormal.X;	*OutBiNormY.GetDest() = Binormal.Y;	*OutBiNormZ.GetDest() = Binormal.Z;
		*OutTangentX.GetDest() = Tangent.X;	*OutTangentY.GetDest() = Tangent.Y;	*OutTangentZ.GetDest() = Tangent.Z;
		
		// We already got it to compute NBT's, let's just compute the UV to save time later.
		FVector2D UV = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(), UV0, UV1, UV2);
		*OutUVX.GetDest() = UV.X;
		*OutUVY.GetDest() = UV.Y;

		// 		//TODO: Use the velocity temp buffer as storage for the prev position so I can SIMD the velocity calculation.
		// 		*OutVelX.GetDest() = Prev.X;		*OutVelY.GetDest() = Prev.Y;		*OutVelZ.GetDest() = Prev.Z;

		UVSetParam.Advance();
		TriParam.Advance();
		BaryXParam.Advance();	BaryYParam.Advance();		BaryZParam.Advance();
		OutPosX.Advance();		OutPosY.Advance();			OutPosZ.Advance();
		OutVelX.Advance();		OutVelY.Advance();			OutVelZ.Advance();
		OutNormX.Advance();		OutNormY.Advance();			OutNormZ.Advance();
		OutBiNormX.Advance();		OutBiNormY.Advance();			OutBiNormZ.Advance();
		OutTangentX.Advance();		OutTangentY.Advance();			OutTangentZ.Advance();
		OutUVX.Advance();		OutUVY.Advance();
	}
}

template<typename TriType, typename BaryXType, typename BaryYType, typename BaryZType>
void UNiagaraDataInterfaceSkeletalMesh::GetTriCoordColor(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Sample);
	TriType TriParam(Context);
	BaryXType BaryXParam(Context);
	BaryYType BaryYParam(Context);
	BaryZType BaryZParam(Context);
	FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);

	FRegisterHandler<float> OutColorR(Context);
	FRegisterHandler<float> OutColorG(Context);
	FRegisterHandler<float> OutColorB(Context);
	FRegisterHandler<float> OutColorA(Context);

	USkeletalMeshComponent* Comp = Cast<USkeletalMeshComponent>(InstData->Component.Get());
	FSkinWeightVertexBuffer* SkinWeightBuffer;
	FSkeletalMeshLODRenderData& LODData = InstData->GetLODRenderDataAndSkinWeights(SkinWeightBuffer);
	const FColorVertexBuffer& Colors = LODData.StaticVertexBuffers.ColorVertexBuffer;
	checkfSlow(Colors.GetNumVertices() != 0, TEXT("Trying to access vertex colors from mesh without any."));

	FMultiSizeIndexContainer& Indices = LODData.MultiSizeIndexContainer;
	const FRawStaticIndexBuffer16or32Interface* IndexBuffer = Indices.GetIndexBuffer();
	int32 TriMax = IndexBuffer->Num() - 3;
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 Tri = TriParam.Get();
		Tri = FMath::Min(Tri, TriMax);

		int32 Idx0 = IndexBuffer->Get(Tri);
		int32 Idx1 = IndexBuffer->Get(Tri + 1);
		int32 Idx2 = IndexBuffer->Get(Tri + 2);

		FLinearColor Color = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(),
			Colors.VertexColor(Idx0).ReinterpretAsLinear(), Colors.VertexColor(Idx1).ReinterpretAsLinear(), Colors.VertexColor(Idx2).ReinterpretAsLinear());

		*OutColorR.GetDest() = Color.R;
		*OutColorG.GetDest() = Color.G;
		*OutColorB.GetDest() = Color.B;
		*OutColorA.GetDest() = Color.A;
		TriParam.Advance();
		BaryXParam.Advance();
		BaryYParam.Advance();
		BaryZParam.Advance();
		OutColorR.Advance();
		OutColorG.Advance();
		OutColorB.Advance();
		OutColorA.Advance();
	}
}

template<typename VertexAccessorType, typename TriType, typename BaryXType, typename BaryYType, typename BaryZType, typename UVSetType>
void UNiagaraDataInterfaceSkeletalMesh::GetTriCoordUV(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Sample);
	VertexAccessorType VertAccessor;
	TriType TriParam(Context);
	BaryXType BaryXParam(Context);	BaryYType BaryYParam(Context);	BaryZType BaryZParam(Context);
	UVSetType UVSetParam(Context);
	FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);

	checkfSlow(InstData.Get(), TEXT("Skeletal Mesh Interface has invalid instance data. %s"), *GetPathName());
	checkfSlow(InstData->Mesh, TEXT("Skeletal Mesh Interface has invalid mesh. %s"), *GetPathName());

	FRegisterHandler<float> OutUVX(Context);	FRegisterHandler<float> OutUVY(Context);

	USkeletalMeshComponent* Comp = Cast<USkeletalMeshComponent>(InstData->Component.Get());
	FSkinWeightVertexBuffer* SkinWeightBuffer;
	FSkeletalMeshLODRenderData& LODData = InstData->GetLODRenderDataAndSkinWeights(SkinWeightBuffer);

	FMultiSizeIndexContainer& Indices = LODData.MultiSizeIndexContainer;
	FRawStaticIndexBuffer16or32Interface* IndexBuffer = Indices.GetIndexBuffer();
	int32 TriMax = IndexBuffer->Num() - 3;
	float InvDt = 1.0f / InstData->DeltaSeconds;
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 Tri = TriParam.Get();
		Tri = FMath::Min(Tri, TriMax);

		int32 Idx0 = IndexBuffer->Get(Tri);
		int32 Idx1 = IndexBuffer->Get(Tri + 1);
		int32 Idx2 = IndexBuffer->Get(Tri + 2);
		FVector2D UV0;		FVector2D UV1;		FVector2D UV2;
		int32 UVSet = UVSetParam.Get();
		UV0 = VertAccessor.GetVertexUV(LODData, Idx0, UVSet);
		UV1 = VertAccessor.GetVertexUV(LODData, Idx1, UVSet);
		UV2 = VertAccessor.GetVertexUV(LODData, Idx2, UVSet);

		FVector2D UV = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(), UV0, UV1, UV2);

		*OutUVX.GetDest() = UV.X;
		*OutUVY.GetDest() = UV.Y;

		TriParam.Advance();
		BaryXParam.Advance(); BaryYParam.Advance(); BaryZParam.Advance();
		UVSetParam.Advance();
		OutUVX.Advance();
		OutUVY.Advance();
	}
}

//UNiagaraDataInterfaceSkeletalMesh END
//////////////////////////////////////////////////////////////////////////
#undef LOCTEXT_NAMESPACE
