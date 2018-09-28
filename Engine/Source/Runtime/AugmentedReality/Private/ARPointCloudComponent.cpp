// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ARPointCloudComponent.h"
#include "AugmentedRealityModule.h"
#include "ARBlueprintLibrary.h"
#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "PrimitiveSceneProxy.h"
#include "Containers/ResourceArray.h"
#include "EngineGlobals.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "Engine/Engine.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "StaticMeshResources.h"

DECLARE_CYCLE_STAT(TEXT("Create Point Cloud Proxy"), STAT_PointCloud_CreateSceneProxy, STATGROUP_AR);
DECLARE_CYCLE_STAT(TEXT("Update Point Cloud GT"), STAT_PointCloud_UpdatePointCloud, STATGROUP_AR);
DECLARE_CYCLE_STAT(TEXT("Point Cloud Get Mesh Elements"), STAT_PointCloud_GetMeshElements, STATGROUP_AR);

/** Procedural mesh scene proxy */
class FARPointCloudSceneProxy final :
	public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FARPointCloudSceneProxy(UARPointCloudComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, Points(Component->PointCloud)
		, Color(Component->PointColor)
		, Size(Component->PointSize)
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
	}

	virtual ~FARPointCloudSceneProxy()
	{
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		SCOPE_CYCLE_COUNTER(STAT_PointCloud_GetMeshElements);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				// Loop through manually drawing the points since PT_PointList isn't really a supported primitive type
				for (const FVector& Point : Points)
				{
					PDI->DrawPoint(Point, Color, Size, SDPG_World);
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = false;
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = true;
		Result.bUsesLightingChannels = false;
		Result.bRenderCustomDepth = false;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual bool CanBeOccluded() const override
	{
		return false;
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
	TArray<FVector> Points;
	FLinearColor Color;
	float Size;
	FMaterialRelevance MaterialRelevance;
};

UARPointCloudComponent::UARPointCloudComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PointColor(1.f, 1.f, 1.f, 1.f)
	, PointSize(4.f)
{
	PrimaryComponentTick.bCanEverTick = true;
	// Tick late in the frame to make sure the ARKit has had a chance to update
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UARPointCloudComponent::InitializeComponent()
{
	Super::InitializeComponent();
	
	PrimaryComponentTick.SetTickFunctionEnable(bAutoBindToARSystem);
}

void UARPointCloudComponent::ClearPointCloud()
{
	PointCloud.Empty();
	LocalBounds = FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0);

	MarkRenderStateDirty();
}

FPrimitiveSceneProxy* UARPointCloudComponent::CreateSceneProxy()
{
	SCOPE_CYCLE_COUNTER(STAT_PointCloud_CreateSceneProxy);
	return new FARPointCloudSceneProxy(this);
}

FBoxSphereBounds UARPointCloudComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return LocalBounds;
}

FMatrix UARPointCloudComponent::GetRenderMatrix() const
{
	return FMatrix::Identity;
}

void UARPointCloudComponent::SetPointCloud(const TArray<FVector>& Points)
{
	// Zero and rebuild our bounds from the points
	FBox PointBounds(Points);
	LocalBounds = FBoxSphereBounds(PointBounds);
	PointCloud = Points;

	MarkRenderStateDirty();
}

void UARPointCloudComponent::SetPointColor(const FLinearColor& Color)
{
	PointColor = Color;
	
	MarkRenderStateDirty();
}

void UARPointCloudComponent::SetPointSize(float Size)
{
	PointSize = Size;
	
	MarkRenderStateDirty();
}

void UARPointCloudComponent::TickComponent(float DeltaTime, ELevelTick, FActorComponentTickFunction*)
{
	SCOPE_CYCLE_COUNTER(STAT_PointCloud_UpdatePointCloud);
	
	if (!bAutoBindToARSystem)
	{
		return;
	}
	// Ask the ar system for updated point cloud
	SetPointCloud(UARBlueprintLibrary::GetPointCloud());
}
