// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MRMeshComponent.h"
#include "PrimitiveSceneProxy.h"
#include "DynamicMeshBuilder.h"
#include "LocalVertexFactory.h"
#include "Containers/ResourceArray.h"
#include "SceneManagement.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "RenderingThread.h"
#include "BaseMeshReconstructorModule.h"
#include "MeshReconstructorBase.h"
#include "AI/NavigationSystemHelpers.h"
#include "PackedNormal.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsPublic.h"
#include "Physics/IPhysXCooking.h"
#include "PhysXCookHelper.h"
#include "Misc/RuntimeErrors.h"
#include "Engine/Engine.h"
#include "UObject/UObjectThreadContext.h"
#include "Stats/Stats.h"

DECLARE_CYCLE_STAT(TEXT("MrMesh SetCollisionProfileName"), STAT_MrMesh_SetCollisionProfileName, STATGROUP_Physics);

class FMRMeshVertexResourceArray : public FResourceArrayInterface
{
public:
	FMRMeshVertexResourceArray(const void* InData, uint32 InSize)
		: Data(InData)
		, Size(InSize)
	{
	}

	virtual const void* GetResourceData() const override { return Data; }
	virtual uint32 GetResourceDataSize() const override { return Size; }
	virtual void Discard() override { }
	virtual bool IsStatic() const override { return false; }
	virtual bool GetAllowCPUAccess() const override { return false; }
	virtual void SetAllowCPUAccess(bool bInNeedsCPUAccess) override { }

private:
	const void* Data;
	uint32 Size;
};

/** Support for non-interleaved data streams. */
template<typename DataType>
class FMRMeshVertexBuffer : public FVertexBuffer
{
public:
	int32 NumVerts = 0;
	void InitRHIWith( const TArray<DataType>& PerVertexData )
	{
		NumVerts = PerVertexData.Num();

		const uint32 SizeInBytes = PerVertexData.Num() * sizeof(DataType);

		FMRMeshVertexResourceArray ResourceArray(PerVertexData.GetData(), SizeInBytes);
		FRHIResourceCreateInfo CreateInfo(&ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer(SizeInBytes, BUF_Static | BUF_ShaderResource, CreateInfo);
	}

};

class FMRMeshIndexBuffer : public FIndexBuffer
{
public:
	int32 NumIndices = 0;
	void InitRHIWith( const TArray<uint32>& Indices )
	{
		NumIndices = Indices.Num();

		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(int32), Indices.Num() * sizeof(int32), BUF_Static, CreateInfo, Buffer);

		// Write the indices to the index buffer.
		FMemory::Memcpy(Buffer, Indices.GetData(), Indices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};


struct FMRMeshProxySection;

struct FMRMeshProxySection
{
	/** Which brick this section represents */
	IMRMesh::FBrickId BrickId;
	/** Position buffer */
	FMRMeshVertexBuffer<FVector> PositionBuffer;
	/** Texture coordinates buffer */
	FMRMeshVertexBuffer<FVector2D> UVBuffer;
	/** Tangent space buffer */
	FMRMeshVertexBuffer<FPackedNormal> TangentXZBuffer;
	/** We don't need color */
	FMRMeshVertexBuffer<FColor> ColorBuffer;
	/** Index buffer for this section */
	FMRMeshIndexBuffer IndexBuffer;
	/** Vertex factory for this section */
	FLocalVertexFactory VertexFactory;

	FShaderResourceViewRHIRef PositionBufferSRV;
	FShaderResourceViewRHIRef UVBufferSRV;
	FShaderResourceViewRHIRef TangentXZBufferSRV;
	FShaderResourceViewRHIRef ColorBufferSRV;

	FMRMeshProxySection(IMRMesh::FBrickId InBrickId, ERHIFeatureLevel::Type InFeatureLevel)
		: BrickId(InBrickId)
		, VertexFactory(InFeatureLevel, "FMRMeshProxySection")
	{
	}

	void ReleaseResources()
	{
		PositionBuffer.ReleaseResource();
		UVBuffer.ReleaseResource();
		TangentXZBuffer.ReleaseResource();
		ColorBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	FMRMeshProxySection(const FLocalVertexFactory&) = delete;
	void operator==(const FLocalVertexFactory&) = delete;
};

static void InitVertexFactory(FLocalVertexFactory* VertexFactory, const FMRMeshProxySection& MRMeshSection)
{
	ENQUEUE_RENDER_COMMAND(InitMrMeshVertexFactory)(
		[VertexFactory, &MRMeshSection](FRHICommandListImmediate& RHICmdList)
	{
		check(IsInRenderingThread());

		// Initialize the vertex factory's stream components.
		FLocalVertexFactory::FDataType NewData;

		{
			NewData.PositionComponentSRV = MRMeshSection.PositionBufferSRV;
			NewData.PositionComponent = FVertexStreamComponent(&MRMeshSection.PositionBuffer, 0, sizeof(FVector), VET_Float3, EVertexStreamUsage::Default);
		}

		if (MRMeshSection.UVBuffer.NumVerts != 0)
		{
			NewData.TextureCoordinatesSRV = MRMeshSection.UVBufferSRV;
			NewData.TextureCoordinates.Add(FVertexStreamComponent(&MRMeshSection.UVBuffer, 0, sizeof(FVector2D), VET_Float2, EVertexStreamUsage::ManualFetch));
			NewData.NumTexCoords = 1;
		}

		if (MRMeshSection.TangentXZBuffer.NumVerts != 0)
		{
			NewData.TangentsSRV = MRMeshSection.TangentXZBufferSRV;
			NewData.TangentBasisComponents[0] = FVertexStreamComponent(&MRMeshSection.TangentXZBuffer, 0, 2 * sizeof(FPackedNormal), VET_PackedNormal, EVertexStreamUsage::ManualFetch);
			NewData.TangentBasisComponents[1] = FVertexStreamComponent(&MRMeshSection.TangentXZBuffer, sizeof(FPackedNormal), 2 * sizeof(FPackedNormal), VET_PackedNormal, EVertexStreamUsage::ManualFetch);
		}

		if (MRMeshSection.ColorBuffer.NumVerts != 0)
		{
			NewData.ColorComponentsSRV = MRMeshSection.ColorBufferSRV;
			NewData.ColorComponent = FVertexStreamComponent(&MRMeshSection.ColorBuffer, 0, sizeof(FColor), VET_Color, EVertexStreamUsage::ManualFetch);
		}

		VertexFactory->SetData(NewData);
		VertexFactory->InitResource();
	});
}


class FMRMeshProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FMRMeshProxy(const UMRMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, InComponent->GetFName())
	, MaterialToUse((InComponent->Material!=nullptr) ? InComponent->Material : UMaterial::GetDefaultMaterial(MD_Surface) )
	, FeatureLevel(GetScene().GetFeatureLevel())
	{
	}

	virtual ~FMRMeshProxy()
	{
		for (FMRMeshProxySection* Section : ProxySections)
		{
			if (Section != nullptr)
			{
				Section->ReleaseResources();
				delete Section;
			}
		}
	}

	void RenderThread_UploadNewSection(IMRMesh::FSendBrickDataArgs Args)
	{
		check(IsInRenderingThread() || IsInRHIThread());

		FMRMeshProxySection* NewSection = new FMRMeshProxySection(Args.BrickId, FeatureLevel);
		ProxySections.Add(NewSection);

		// Vulkan requires that all the buffers be full.
		const int32 NumVerts = Args.PositionData.Num();
		check((NumVerts == Args.ColorData.Num()));
		check((NumVerts == Args.UVData.Num()));
		check((NumVerts * 2) == Args.TangentXZData.Num());

		// POSITION BUFFER
		{
			NewSection->PositionBuffer.InitResource();
			NewSection->PositionBuffer.InitRHIWith(Args.PositionData);
			NewSection->PositionBufferSRV = RHICreateShaderResourceView(NewSection->PositionBuffer.VertexBufferRHI, sizeof(float), PF_R32_FLOAT);
		}

		// TEXTURE COORDS BUFFER
		{
			NewSection->UVBuffer.InitResource();
			if (Args.UVData.Num())
			{
				NewSection->UVBuffer.InitRHIWith(Args.UVData);
				NewSection->UVBufferSRV = RHICreateShaderResourceView(NewSection->UVBuffer.VertexBufferRHI, 8, PF_G32R32F);
			}
		}

		// TANGENTS BUFFER
		{
			NewSection->TangentXZBuffer.InitResource();
			if (Args.TangentXZData.Num())
			{
				NewSection->TangentXZBuffer.InitRHIWith(Args.TangentXZData);
			}

			if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
			{
				NewSection->TangentXZBufferSRV = RHICreateShaderResourceView(NewSection->TangentXZBuffer.VertexBufferRHI, 4, PF_R8G8B8A8_SNORM);
			}
		}

		// COLOR
		{
			NewSection->ColorBuffer.InitResource();
			if (Args.ColorData.Num())
			{
				NewSection->ColorBuffer.InitRHIWith(Args.ColorData);
				NewSection->ColorBufferSRV = RHICreateShaderResourceView(NewSection->ColorBuffer.VertexBufferRHI, 4, PF_R8G8B8A8);
			}
		}

		// INDEX BUFFER
		{
			NewSection->IndexBuffer.InitResource();
			NewSection->IndexBuffer.InitRHIWith(Args.Indices);
		}

		// VERTEX FACTORY
		{
			InitVertexFactory(&NewSection->VertexFactory, *NewSection);
		}
	}

	bool RenderThread_RemoveSection(IMRMesh::FBrickId BrickId)
	{
		check(IsInRenderingThread() || IsInRHIThread());
		for (int32 i = 0; i < ProxySections.Num(); ++i)
		{
			if (ProxySections[i]->BrickId == BrickId)
			{
				ProxySections[i]->ReleaseResources();
				delete ProxySections[i];
				ProxySections.RemoveAtSwap(i);
				return true;
			}
		}
		return false;
	}

	void RenderThread_RemoveAllSections()
	{
		check(IsInRenderingThread() || IsInRHIThread());
		for (int32 i = ProxySections.Num()-1; i >=0; i--)
		{
			ProxySections[i]->ReleaseResources();
			delete ProxySections[i];
			ProxySections.RemoveAtSwap(i);
		}
	}

	void RenderThread_SetMaterial(UMaterialInterface* Material)
	{
		MaterialToUse = Material;
	}

private:
	//~ FPrimitiveSceneProxy

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const override
	{
		static const FBoxSphereBounds InfiniteBounds(FSphere(FVector::ZeroVector, HALF_WORLD_MAX));

		// Iterate over sections
		for (const FMRMeshProxySection* Section : ProxySections)
		{
			if (Section != nullptr)
			{
				const bool bIsSelected = false;
				FMaterialRenderProxy* MaterialProxy = MaterialToUse->GetRenderProxy();

				// For each view..
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (VisibilityMap & (1 << ViewIndex))
					{
						const FSceneView* View = Views[ViewIndex];
						// Draw the mesh.
						FMeshBatch& Mesh = Collector.AllocateMesh();
						FMeshBatchElement& BatchElement = Mesh.Elements[0];
						BatchElement.IndexBuffer = &Section->IndexBuffer;
						Mesh.bWireframe = false;
						Mesh.VertexFactory = &Section->VertexFactory;
						Mesh.MaterialRenderProxy = MaterialProxy;

						FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
						DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), GetLocalToWorld(), InfiniteBounds, InfiniteBounds, true, false, UseEditorDepthTest());
						BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

						BatchElement.FirstIndex = 0;
						BatchElement.NumPrimitives = Section->IndexBuffer.NumIndices / 3;
						BatchElement.MinVertexIndex = 0;
						BatchElement.MaxVertexIndex = Section->PositionBuffer.NumVerts - 1;
						Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
						Mesh.Type = PT_TriangleList;
						Mesh.DepthPriorityGroup = SDPG_World;
						Mesh.bCanApplyViewModeOverrides = false;
						Collector.AddMesh(ViewIndex, Mesh);
					}
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		//MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}
	virtual uint32 GetMemoryFootprint(void) const
	{
		return(sizeof(*this) + GetAllocatedSize());
	}

	uint32 GetAllocatedSize(void) const
	{
		return(FPrimitiveSceneProxy::GetAllocatedSize());
	}

private:
	TArray<FMRMeshProxySection*> ProxySections;
	UMaterialInterface* MaterialToUse;
	ERHIFeatureLevel::Type FeatureLevel;
};


UMRMeshComponent::UMRMeshComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UMRMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	SetCustomNavigableGeometry(bCanEverAffectNavigation ? EHasCustomNavigableGeometry::Yes : EHasCustomNavigableGeometry::No);
}

void UMRMeshComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAllBrickData();

	Super::EndPlay(EndPlayReason);
}

void UMRMeshComponent::OnActorEnableCollisionChanged()
{
	for (auto& BodyInstanceElement : BodyInstances)
	{
		BodyInstanceElement->UpdatePhysicsFilterData();
	}

	Super::OnActorEnableCollisionChanged();
}

bool UMRMeshComponent::ShouldCreatePhysicsState() const
{
	// This component does not use the default physics state creation.  It creates in response to meshing data delivered via SendBrickData.
	return false;
}

void UMRMeshComponent::SetCollisionEnabled(ECollisionEnabled::Type NewType)
{
	if (BodyInstance.GetCollisionEnabled() != NewType)
	{
		for (auto& BodyInstanceElement : BodyInstances)
		{
			BodyInstanceElement->SetCollisionEnabled(NewType);
		}

		if (IsRegistered() && BodyInstance.bSimulatePhysics && !IsWelded())
		{
			for (auto& BodyInstanceElement : BodyInstances)
			{
				BodyInstanceElement->ApplyWeldOnChildren();
			}
		}
	}

	Super::SetCollisionEnabled(NewType);
}

void UMRMeshComponent::SetCollisionProfileName(FName InCollisionProfileName)
{
	SCOPE_CYCLE_COUNTER(STAT_MrMesh_SetCollisionProfileName);

	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	if (ThreadContext.ConstructedObject == this)
	{
		// If we are in our constructor, defer setup until PostInitProperties as derived classes
		for (auto& BodyInstanceElement : BodyInstances)
		{
			BodyInstanceElement->SetCollisionProfileNameDeferred(InCollisionProfileName);
		}
	}
	else
	{
		for (auto& BodyInstanceElement : BodyInstances)
		{
			BodyInstanceElement->SetCollisionProfileName(InCollisionProfileName);
		}
	}

	Super::SetCollisionProfileName(InCollisionProfileName);
}

void UMRMeshComponent::SetCollisionObjectType(ECollisionChannel Channel)
{
	for (auto& BodyInstanceElement : BodyInstances)
	{
		BodyInstanceElement->SetObjectType(Channel);
	}

	Super::SetCollisionObjectType(Channel);
}

void UMRMeshComponent::SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse)
{
	for (auto& BodyInstanceElement : BodyInstances)
	{
		BodyInstanceElement->SetResponseToChannel(Channel, NewResponse);
	}

	Super::SetCollisionResponseToChannel(Channel, NewResponse);
}

void UMRMeshComponent::SetCollisionResponseToAllChannels(enum ECollisionResponse NewResponse)
{
	for (auto& BodyInstanceElement : BodyInstances)
	{
		BodyInstanceElement->SetResponseToAllChannels(NewResponse);
	}

	Super::SetCollisionResponseToAllChannels(NewResponse);
}

void UMRMeshComponent::SetCollisionResponseToChannels(const FCollisionResponseContainer& NewResponses)
{
	for (auto& BodyInstanceElement : BodyInstances)
	{
		BodyInstanceElement->SetResponseToChannels(NewResponses);
	}

	Super::SetCollisionResponseToChannels(NewResponses);
}

void UMRMeshComponent::UpdatePhysicsToRBChannels()
{
	for (auto& BodyInstanceElement : BodyInstances)
	{
		if (BodyInstanceElement->IsValidBodyInstance())
		{
			BodyInstanceElement->UpdatePhysicsFilterData();
		}
	}

	Super::UpdatePhysicsToRBChannels();
}

void UMRMeshComponent::SetWalkableSlopeOverride(const FWalkableSlopeOverride& NewOverride)
{
	for (auto& BodyInstanceElement : BodyInstances)
	{
		if (BodyInstanceElement->IsValidBodyInstance())
		{
			BodyInstanceElement->SetWalkableSlopeOverride(NewOverride);
		}
	}

	Super::SetWalkableSlopeOverride(NewOverride);
}

FPrimitiveSceneProxy* UMRMeshComponent::CreateSceneProxy()
{
	// The render thread owns the memory, so if this function is
	// being called, it's safe to just re-allocate.
	return new FMRMeshProxy(this);
}

void UMRMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials /*= false*/) const
{
	if (Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}

FBoxSphereBounds UMRMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(FSphere(FVector::ZeroVector, HALF_WORLD_MAX));
}

void UMRMeshComponent::SendBrickData(IMRMesh::FSendBrickDataArgs Args)
{
	auto BrickDataTask = FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UMRMeshComponent::SendBrickData_Internal, Args);

	DECLARE_CYCLE_STAT(TEXT("UMRMeshComponent.SendBrickData"),
		STAT_UMRMeshComponent_SendBrickData,
		STATGROUP_MRMESH);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(BrickDataTask, GET_STATID(STAT_UMRMeshComponent_SendBrickData), nullptr, ENamedThreads::GameThread);
}

void UMRMeshComponent::ClearAllBrickData()
{
	auto ClearBrickDataTask = FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UMRMeshComponent::ClearAllBrickData_Internal);

	DECLARE_CYCLE_STAT(TEXT("UMRMeshComponent.ClearAllBrickData"),
	STAT_UMRMeshComponent_ClearAllBrickData,
		STATGROUP_MRMESH);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(ClearBrickDataTask, GET_STATID(STAT_UMRMeshComponent_ClearAllBrickData), nullptr, ENamedThreads::GameThread);
}

void UMRMeshComponent::CacheBodySetupHelper()
{
	CachedBodySetup = NewObject<UBodySetup>(this, NAME_None);
	CachedBodySetup->BodySetupGuid = FGuid::NewGuid();
	CachedBodySetup->bGenerateMirroredCollision = false;
	CachedBodySetup->bHasCookedCollisionData = true;
}

UBodySetup* UMRMeshComponent::CreateBodySetupHelper()
{
	// The body setup in a template needs to be public since the property is Instanced and thus is the archetype of the instance meaning there is a direct reference
	UBodySetup* NewBS = NewObject<UBodySetup>(this, NAME_None);
	NewBS->BodySetupGuid = FGuid::NewGuid();
	NewBS->bGenerateMirroredCollision = false;
	NewBS->bHasCookedCollisionData = true;

	// Copy the cached body setup (unless we are creating it)
	if (CachedBodySetup == nullptr)
	{
		CacheBodySetupHelper();
	}
	NewBS->CopyBodyPropertiesFrom(CachedBodySetup);

	return NewBS;
}

UBodySetup* UMRMeshComponent::GetBodySetup()
{
	if (CachedBodySetup == nullptr)
	{
		CacheBodySetupHelper();
	}
	return CachedBodySetup;
}

void UMRMeshComponent::SendBrickData_Internal(IMRMesh::FSendBrickDataArgs Args)
{
#if WITH_PHYSX
	check(IsInGameThread());

	UE_LOG(LogMrMesh, Log, TEXT("SendBrickData_Internal() processing brick %llu with %i triangles"), Args.BrickId, Args.Indices.Num() / 3);

	if (!IsPendingKill() && !bNeverCreateCollisionMesh)
	{
		// Physics update
		UWorld* MyWorld = GetWorld();
		if ( MyWorld && MyWorld->GetPhysicsScene() )
		{
			int32 BodyIndex = BodyIds.Find(Args.BrickId);

			if (const bool bBrickHasData = Args.Indices.Num() > 0)
			{
				bPhysicsStateCreated = true;

				if (BodyIndex == INDEX_NONE)
				{
					BodyIds.Add(Args.BrickId);
					BodySetups.Add(CreateBodySetupHelper());
					BodyInstances.Add(new FBodyInstance());
					BodyIndex = BodyIds.Num() - 1;
				}

				UBodySetup* MyBS = BodySetups[BodyIndex];
				MyBS->bHasCookedCollisionData = true;
				MyBS->CollisionTraceFlag = CTF_UseComplexAsSimple;
				MyBS->ClearPhysicsMeshes();
				MyBS->InvalidatePhysicsData();

				FCookBodySetupInfo CookInfo;
				// Disable mesh cleaning by passing in EPhysXMeshCookFlags::DeformableMesh
				static const EPhysXMeshCookFlags CookFlags = EPhysXMeshCookFlags::FastCook | EPhysXMeshCookFlags::DeformableMesh;
				MyBS->GetCookInfo(CookInfo, CookFlags);
				CookInfo.bCookTriMesh = true;
				CookInfo.TriMeshCookFlags = CookInfo.ConvexCookFlags = CookFlags;
				CookInfo.TriangleMeshDesc.bFlipNormals = true;
				CookInfo.TriangleMeshDesc.Vertices = Args.PositionData;
				const int NumFaces = Args.Indices.Num() / 3;
				CookInfo.TriangleMeshDesc.Indices.Reserve(Args.Indices.Num() / 3);
				for (int i = 0; i < NumFaces; ++i)
				{
					CookInfo.TriangleMeshDesc.Indices.AddUninitialized(1);
					CookInfo.TriangleMeshDesc.Indices[i].v0 = Args.Indices[3 * i + 0];
					CookInfo.TriangleMeshDesc.Indices[i].v1 = Args.Indices[3 * i + 1];
					CookInfo.TriangleMeshDesc.Indices[i].v2 = Args.Indices[3 * i + 2];
				}

				FPhysXCookHelper CookHelper(GetPhysXCookingModule());
				CookHelper.CookInfo = CookInfo;
				CookHelper.CreatePhysicsMeshes_Concurrent();

				MyBS->FinishCreatingPhysicsMeshes(CookHelper.OutNonMirroredConvexMeshes, CookHelper.OutMirroredConvexMeshes, CookHelper.OutTriangleMeshes);

				FBodyInstance* MyBI = BodyInstances[BodyIndex];
				MyBI->TermBody();
				MyBI->InitBody(MyBS, GetComponentTransform(), this, MyWorld->GetPhysicsScene());
				MyBI->CopyRuntimeBodyInstancePropertiesFrom(&BodyInstance);
			}
			else
			{
				if (BodyIndex != INDEX_NONE)
				{
					RemoveBodyInstance(BodyIndex);
				}
				else
				{
					// This brick already doesn't exist, so no work to be done.
				}
			}
		}
		if (bUpdateNavMeshOnMeshUpdate && bHasCustomNavigableGeometry)
		{
			UpdateNavigationData();
		}
	}
#endif // WITH_PHYSX

	if (bCreateMeshProxySections)
	{
		if (SceneProxy != nullptr && GRenderingThread != nullptr)
		{
			check(GRenderingThread != nullptr);
			check(SceneProxy != nullptr);

			// Graphics update
			UMRMeshComponent* This = this;
			ENQUEUE_RENDER_COMMAND(FSendBrickDataLambda)(
				[This, Args](FRHICommandListImmediate& RHICmdList)
				{
					FMRMeshProxy* MRMeshProxy = static_cast<FMRMeshProxy*>(This->SceneProxy);
					if (MRMeshProxy)
					{
						MRMeshProxy->RenderThread_RemoveSection(Args.BrickId);

						if (const bool bBrickHasData = Args.Indices.Num() > 0)
						{
							MRMeshProxy->RenderThread_UploadNewSection(Args);
						}
					}
				}
			);
		}
	}
}

void UMRMeshComponent::RemoveBodyInstance(int32 BodyIndex)
{
	BodyInstances[BodyIndex]->TermBody();
	delete BodyInstances[BodyIndex];
	BodyInstances.RemoveAtSwap(BodyIndex);
	BodySetups.RemoveAtSwap(BodyIndex);
	BodyIds.RemoveAtSwap(BodyIndex);
}

void UMRMeshComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	for (FBodyInstance* BI : BodyInstances)
	{
		BI->SetBodyTransform(GetComponentTransform(), Teleport);
		BI->UpdateBodyScale(GetComponentTransform().GetScale3D());
	}
}

void UMRMeshComponent::ClearAllBrickData_Internal()
{
	check(IsInGameThread());

	for (int32 i = BodyIds.Num()-1; i >= 0; i--)
	{
		RemoveBodyInstance(i);
	}

	// Graphics update
	UMRMeshComponent* This = this;
	ENQUEUE_RENDER_COMMAND(FClearAllBricksLambda)(
		[This](FRHICommandListImmediate& RHICmdList)
		{
			FMRMeshProxy* MRMeshProxy = static_cast<FMRMeshProxy*>(This->SceneProxy);
			if (MRMeshProxy)
			{
				MRMeshProxy->RenderThread_RemoveAllSections();
			}
		}
	);
}

void UMRMeshComponent::SetMaterial(int32 ElementIndex, class UMaterialInterface* InMaterial)
{
	if (Material != InMaterial)
	{
		Material = InMaterial;
		MarkRenderDynamicDataDirty();
	}
}

void UMRMeshComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();

	if (SceneProxy)
	{
		// Enqueue command to send to render thread
		UMRMeshComponent* This = this;
		UMaterialInterface* InMaterial = Material;
		ENQUEUE_RENDER_COMMAND(FSetMaterialLambda)(
			[This, InMaterial](FRHICommandListImmediate& RHICmdList)
			{
				FMRMeshProxy* MRMeshProxy = static_cast<FMRMeshProxy*>(This->SceneProxy);
				if (MRMeshProxy)
				{
					MRMeshProxy->RenderThread_SetMaterial(InMaterial);
				}
			});
	}
}

bool UMRMeshComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	check(bHasCustomNavigableGeometry);
	
	for (UBodySetup* BodySetup : BodySetups)
	{
		check(BodySetup);
		GeomExport.ExportRigidBodySetup(*BodySetup, GetComponentTransform());
	}

	return false;
}

void UMRMeshComponent::ForceNavMeshUpdate()
{
	if (bHasCustomNavigableGeometry)
	{
		UpdateNavigationData();
	}
	else
	{
		UE_LOG(LogMrMesh, Log, TEXT("ForceNavMeshUpdate() called, but this MRMesh component has bCanEverAffectNavigation==false.  Ignoring forced update."));
	}
}

void UMRMeshComponent::Clear()
{
	ClearAllBrickData();
	UE_LOG(LogMrMesh, Log, TEXT("Clearing all brick data"));
}
