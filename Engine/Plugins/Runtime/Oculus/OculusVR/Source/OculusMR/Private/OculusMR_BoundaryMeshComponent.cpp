// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OculusMR_BoundaryMeshComponent.h"

#include "OculusMR_CastingCameraActor.h"
#include "OculusMRPrivate.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "Engine/CollisionProfile.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "OculusMRFunctionLibrary.h"
#include "OculusFunctionLibrary.h"

/** Scene proxy */
class FOculusMR_BoundaryMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

	FOculusMR_BoundaryMeshSceneProxy(UOculusMR_BoundaryMeshComponent* Component, UMaterial* InMaterial)
		: FPrimitiveSceneProxy(Component)
		, bIsValid(false)
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
		, BoundaryMeshComponent(Component)
	{
		const FColor VertexColor(255, 255, 255);

		// Grab material
		Material = InMaterial;
		if (Material == NULL)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		TArray<FVector> Geometry;
		if (Component->BoundaryType == EOculusMR_BoundaryType::BT_OuterBoundary)
		{
			Geometry = UOculusFunctionLibrary::GetGuardianPoints(EBoundaryType::Boundary_Outer, true);
		}
		else
		{
			Geometry = UOculusFunctionLibrary::GetGuardianPoints(EBoundaryType::Boundary_PlayArea, true);
		}

		if (Geometry.Num() == 0)
		{
			// Add degenerated triangle
			Vertices.AddDefaulted(1);
			Indices.AddUninitialized(3);
			Indices[0] = 0;
			Indices[1] = 0;
			Indices[2] = 0;
		}
		else
		{
			FVector FirstPoint = Geometry[0];
			Geometry.Add(FirstPoint);
			int NumPoints = Geometry.Num();

			Vertices.AddUninitialized(NumPoints * 2);
			for (int i = 0; i < NumPoints; ++i)
			{
				FVector V = Geometry[i];
				FDynamicMeshVertex VertBottom, VertTop;
				VertBottom.Position = FVector(V.X, V.Y, Component->BottomZ);
				VertTop.Position = FVector(V.X, V.Y, Component->TopZ);
				VertBottom.TextureCoordinate[0] = FVector2D((float)i / (NumPoints - 1), 0.0f);
				VertTop.TextureCoordinate[0] = FVector2D(VertBottom.TextureCoordinate[0].X, 1.0f);
				Vertices[i] = VertBottom;
				Vertices[i + NumPoints] = VertTop;
			}

			Indices.AddUninitialized((NumPoints - 1) * 2 * 3);
			for (int i = 0; i < NumPoints - 1; ++i)
			{
				Indices[i * 6 + 0] = i;
				Indices[i * 6 + 2] = i + NumPoints;
				Indices[i * 6 + 1] = i + 1 + NumPoints;

				Indices[i * 6 + 3] = i;
				Indices[i * 6 + 5] = i + 1 + NumPoints;
				Indices[i * 6 + 4] = i + 1;
			}

			bIsValid = true;
		}
	}

	virtual ~FOculusMR_BoundaryMeshSceneProxy()
	{
	}

	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}
	
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_OculusMR_BoundaryMeshSceneProxy_GetDynamicMeshElements);

		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial->GetRenderProxy(),
			FLinearColor(0, 0.5f, 1.f)
		);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

		FMaterialRenderProxy* MaterialProxy = NULL;
		if (bWireframe)
		{
			MaterialProxy = WireframeMaterialInstance;
		}
		else
		{
			MaterialProxy = Material->GetRenderProxy();
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				FTransform Transform = BoundaryMeshComponent->GetComponentTransform();

				const FSceneView* View = Views[ViewIndex];
				FDynamicMeshBuilder DynamicMeshBuilder(View->GetFeatureLevel());
				DynamicMeshBuilder.AddVertices(Vertices);
				DynamicMeshBuilder.AddTriangles(Indices);

				DynamicMeshBuilder.GetMesh(Transform.ToMatrixWithScale(), MaterialProxy, SDPG_World, false, false, ViewIndex, Collector);

				// -- Original draw code for reference --
				//FMeshBatch& Mesh = Collector.AllocateMesh();
				//FMeshBatchElement& BatchElement = Mesh.Elements[0];
				//BatchElement.IndexBuffer = &IndexBuffer;
				//Mesh.bWireframe = bWireframe;
				//Mesh.VertexFactory = &VertexFactory;
				//Mesh.MaterialRenderProxy = MaterialProxy;
				//BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(Transform.ToMatrixWithScale(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
				//BatchElement.FirstIndex = 0;
				//BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
				//BatchElement.MinVertexIndex = 0;
				//BatchElement.MaxVertexIndex = VertexBuffer.Vertices.Num() - 1;
				//Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				//Mesh.Type = PT_TriangleList;
				//Mesh.DepthPriorityGroup = SDPG_Foreground;
				//Mesh.bCanApplyViewModeOverrides = false;
				//Collector.AddMesh(ViewIndex, Mesh);
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		
		bool PrimitiveVisible = false;
		if (View->ShowOnlyPrimitives.IsSet())
		{
			if (View->ShowOnlyPrimitives->Contains(GetPrimitiveComponentId()))
			{
				PrimitiveVisible = true;
			}
		}

		// set it to true in debugging
		//PrimitiveVisible = true;

		Result.bDrawRelevance = IsValid() && PrimitiveVisible;
		Result.bShadowRelevance = false;
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = true;
		Result.bUsesLightingChannels = false;
		Result.bRenderCustomDepth = false;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		Result.bOpaqueRelevance = true;
		Result.bUsesSceneDepth = false;
		Result.bRenderCustomDepth = false;
		return Result;
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }

	uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

	bool IsValid() const { return bIsValid; }

private:
	bool bIsValid;
	UMaterialInterface* Material;
	TArray<FDynamicMeshVertex> Vertices;
	TArray<uint32> Indices;
	FMaterialRelevance MaterialRelevance;
	UOculusMR_BoundaryMeshComponent* BoundaryMeshComponent;
};

//////////////////////////////////////////////////////////////////////////

UOculusMR_BoundaryMeshComponent::UOculusMR_BoundaryMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, BoundaryType(EOculusMR_BoundaryType::BT_OuterBoundary)
	, BottomZ(-10.0f * 100)
	, TopZ(10.0f * 100)
	, WhiteMaterial(NULL)
	, bIsValid(false)
{
	PrimaryComponentTick.bCanEverTick = false;

	WhiteMaterial = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), NULL, TEXT("/OculusVR/Materials/OculusMR_WhiteMaterial")));
	if (!WhiteMaterial)
	{
		UE_LOG(LogMR, Warning, TEXT("Invalid WhiteMaterial"));
	}

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

FPrimitiveSceneProxy* UOculusMR_BoundaryMeshComponent::CreateSceneProxy()
{
	FOculusMR_BoundaryMeshSceneProxy* Proxy = NULL;
	Proxy = new FOculusMR_BoundaryMeshSceneProxy(this, WhiteMaterial);
	if (Proxy->IsValid())
	{
		if (bIsValid)
		{
			UE_LOG(LogMR, Log, TEXT("Boundary mesh updated"));
		}
		else
		{
			UE_LOG(LogMR, Log, TEXT("Boundary mesh generated"));
		}
	}
	else
	{
		UE_LOG(LogMR, Warning, TEXT("Boundary mesh is invalid"));
	}
	bIsValid = Proxy->IsValid();
	return Proxy;
}

void UOculusMR_BoundaryMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	OutMaterials.Add(WhiteMaterial);
}

UMaterialInterface* UOculusMR_BoundaryMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex == 0)
	{
		return WhiteMaterial;
	}
	else
	{
		return Super::GetMaterial(ElementIndex);
	}
}


int32 UOculusMR_BoundaryMeshComponent::GetNumMaterials() const
{
	return 1;
}

FBoxSphereBounds UOculusMR_BoundaryMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = FVector::ZeroVector;
	NewBounds.BoxExtent = FVector(HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX);
	NewBounds.SphereRadius = FMath::Sqrt(3.0f * FMath::Square(HALF_WORLD_MAX));
	return NewBounds;
}

