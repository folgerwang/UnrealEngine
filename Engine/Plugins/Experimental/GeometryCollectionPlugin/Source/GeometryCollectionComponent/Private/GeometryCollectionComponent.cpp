// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved. 

#include "GeometryCollectionComponent.h"
#include "GeometryCollectionComponentPluginPrivate.h"
#include "GeometryCollectionSceneProxy.h"
#include "GeometryCollectionUtility.h"
#include "GeometryCollectionAlgo.h"
#include "Async/ParallelFor.h"

DEFINE_LOG_CATEGORY_STATIC(UGeometryCollectionComponentLogging, NoLogging, All);

UGeometryCollectionComponent::UGeometryCollectionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RestCollection(NewObject<UGeometryCollection>())
	, DynamicCollection(NewObject<UGeometryCollection>())
	, bRenderStateDirty(true)
{
	//UE_LOG(UGeometryCollectionComponentLogging, Log, TEXT("GeometryCollectionComponent[%p]::UGeometryCollectionComponent()"),this);

	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bAutoActivate = true;

}

FBoxSphereBounds UGeometryCollectionComponent::CalcBounds(const FTransform& LocalToWorldIn) const
{

	//UE_LOG(UGeometryCollectionComponentLogging, Log, TEXT("GeometryCollectionComponent[%p]::CalcBounds()[%p]"), this, DynamicCollection);

	if (DynamicCollection && DynamicCollection->HasVisibleGeometry())
	{
		FBox BoundingBox(ForceInit);

		int32 NumParticles = DynamicCollection->NumElements(UGeometryCollection::TransformGroup);
		int32 NumFaces = DynamicCollection->NumElements(UGeometryCollection::GeometryGroup);
		int32 NumVertices = DynamicCollection->NumElements(UGeometryCollection::VerticesGroup);
		TManagedArray<FVector>& Vertices = *DynamicCollection->Vertex;
		TManagedArray<int32>& BoneMap = *DynamicCollection->BoneMap;
		TManagedArray<FIntVector>& Indices = *DynamicCollection->Indices;
		TManagedArray<bool>& VisibleFaces = *DynamicCollection->Visible;
		checkSlow(BoneMap.Num() == Vertices.Num());

		TArray<FTransform> Transform;
		GeometryCollectionAlgo::GlobalMatrices(DynamicCollection, Transform);
		checkSlow(DynamicCollection->Transform->Num() == Transform.Num());

		// Pre-calculate the local mesh to world matrices.
		TArray<FTransform> LocalToWorld;
		LocalToWorld.AddUninitialized(NumParticles);
		ParallelFor(NumParticles, [&](int32 ParticleIdx)
		{
			LocalToWorld[ParticleIdx] = Transform[ParticleIdx] * LocalToWorldIn;
		});

		// Transform the visible vertices.
		for (int32 FaceIdx = 0; FaceIdx < NumFaces; FaceIdx++)
		{
			if (VisibleFaces[FaceIdx])
			{

				for (uint8 Idx = 0; Idx < 3; Idx++) {
					int PointIdx = Indices[FaceIdx][Idx];
					checkSlow(0 <= PointIdx && PointIdx < BoneMap.Num());
					BoundingBox += LocalToWorld[BoneMap[PointIdx]].TransformPosition(Vertices[PointIdx]);
				}
			}
		}
		return FBoxSphereBounds(BoundingBox);
	}
	return FBoxSphereBounds(ForceInitToZero);
}

void UGeometryCollectionComponent::CreateRenderState_Concurrent()
{
	//UE_LOG(UGeometryCollectionComponentLogging, Log, TEXT("GeometryCollectionComponent[%p]::CreateRenderState_Concurrent()"), this);

	Super::CreateRenderState_Concurrent();

	if (SceneProxy && DynamicCollection && DynamicCollection->HasVisibleGeometry())
	{
		FGeometryCollectionConstantData * ConstantData = ::new FGeometryCollectionConstantData;
		InitConstantData(ConstantData);

		FGeometryCollectionDynamicData * DynamicData = ::new FGeometryCollectionDynamicData;
		InitDynamicData(DynamicData);

		// Enqueue command to send to render thread
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(FSendGeometryCollectionData,
			FGeometryCollectionSceneProxy*, GeometryCollectionSceneProxy, (FGeometryCollectionSceneProxy*)SceneProxy,
			FGeometryCollectionConstantData*, ConstantData, ConstantData,
			FGeometryCollectionDynamicData*, DynamicData, DynamicData,
			{
				GeometryCollectionSceneProxy->SetConstantData_RenderThread(ConstantData);
		GeometryCollectionSceneProxy->SetDynamicData_RenderThread(DynamicData);
			});
	}
}


FPrimitiveSceneProxy* UGeometryCollectionComponent::CreateSceneProxy()
{
	//UE_LOG(UGeometryCollectionComponentLogging, Log, TEXT("GeometryCollectionComponent[%p]::CreateSceneProxy()"), this);

	if (DynamicCollection)
	{
		return new FGeometryCollectionSceneProxy(this);
	}
	return nullptr;
}

void UGeometryCollectionComponent::InitConstantData(FGeometryCollectionConstantData * ConstantData)
{
	//UE_LOG(UGeometryCollectionComponentLogging, Log, TEXT("GeometryCollectionComponent[%p]::InitConstantData()"), this);

	check(ConstantData);
	check(DynamicCollection);

	int32 NumPoints = DynamicCollection->NumElements(UGeometryCollection::VerticesGroup);
	TManagedArray<FVector>& Vertex = *DynamicCollection->Vertex;
	TManagedArray<int32>& BoneMap = *DynamicCollection->BoneMap;
	TManagedArray<FVector>& TangentU = *DynamicCollection->TangentU;
	TManagedArray<FVector>& TangentV = *DynamicCollection->TangentV;
	TManagedArray<FVector>& Normal = *DynamicCollection->Normal;
	TManagedArray<FVector2D>& UV = *DynamicCollection->UV;
	TManagedArray<FLinearColor>& Color = *DynamicCollection->Color;

	ConstantData->Vertices.AddUninitialized(NumPoints);
	ConstantData->BoneMap.AddUninitialized(NumPoints);
	ConstantData->TangentU.AddUninitialized(NumPoints);
	ConstantData->TangentV.AddUninitialized(NumPoints);
	ConstantData->Normals.AddUninitialized(NumPoints);
	ConstantData->UVs.AddUninitialized(NumPoints);
	ConstantData->Colors.AddUninitialized(NumPoints);

	ParallelFor(NumPoints, [&](int32 PointIdx)
	{
		ConstantData->Vertices[PointIdx] = Vertex[PointIdx];
		ConstantData->BoneMap[PointIdx] = BoneMap[PointIdx];
		ConstantData->TangentU[PointIdx] = TangentU[PointIdx];
		ConstantData->TangentV[PointIdx] = TangentV[PointIdx];
		ConstantData->Normals[PointIdx] = Normal[PointIdx];
		ConstantData->UVs[PointIdx] = UV[PointIdx];
		ConstantData->Colors[PointIdx] = Color[PointIdx];
	});

	int32 NumIndices = 0;
	TManagedArray<FIntVector>& Indices = *DynamicCollection->Indices;
	TManagedArray<bool>& Visible = *DynamicCollection->Visible;
	for (int vdx = 0; vdx < DynamicCollection->NumElements(UGeometryCollection::GeometryGroup); vdx++)
	{
		NumIndices += static_cast<int>(Visible[vdx]);
	}

	ConstantData->Indices.AddUninitialized(NumIndices);
	for (int IndexIdx = 0, cdx = 0; IndexIdx < DynamicCollection->NumElements(UGeometryCollection::GeometryGroup); IndexIdx++)
	{
		if (Visible[IndexIdx])
		{
			ConstantData->Indices[cdx++] = Indices[IndexIdx];
		}
	}
}

void UGeometryCollectionComponent::InitDynamicData(FGeometryCollectionDynamicData * DynamicData)
{
	//UE_LOG(UGeometryCollectionComponentLogging, Log, TEXT("GeometryCollectionComponent[%p]::InitDynamicData()"), this);

	check(DynamicData);
	check(DynamicCollection);

	TArray<FTransform> GlobalMatrices;
	GeometryCollectionAlgo::GlobalMatrices(DynamicCollection, GlobalMatrices);

	int32 NumTransforms = DynamicCollection->NumElements(UGeometryCollection::TransformGroup);
	DynamicData->Transforms.AddUninitialized(NumTransforms);

	check(GlobalMatrices.Num() == NumTransforms);
	ParallelFor(NumTransforms, [&](int32 MatrixIdx)
	{
		DynamicData->Transforms[MatrixIdx] = GlobalMatrices[MatrixIdx].ToMatrixWithScale();
	});
}


void UGeometryCollectionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	//UE_LOG(UGeometryCollectionComponentLogging, Log, TEXT("GeometryCollectionComponent[%p]::TickComponent()"), this);
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bRenderStateDirty && DynamicCollection && DynamicCollection->HasVisibleGeometry())
	{
		MarkRenderDynamicDataDirty();
		bRenderStateDirty = false;
	}
}

void UGeometryCollectionComponent::OnRegister()
{
	//UE_LOG(UGeometryCollectionComponentLogging, Log, TEXT("GeometryCollectionComponent[%p]::OnRegister()[%p]"), this,RestCollection );
	Super::OnRegister();
	ResetDynamicCollection();
}

void UGeometryCollectionComponent::ResetDynamicCollection()
{
	//UE_LOG(UGeometryCollectionComponentLogging, Log, TEXT("GeometryCollectionComponent[%p]::ResetDynamicCollection()"), static_cast<const void*>(this));
	if (RestCollection)
	{
		DynamicCollection = NewObject<UGeometryCollection>(this);
		DynamicCollection->Initialize(*RestCollection);
		DynamicCollection->LocalizeAttribute("Transform", UGeometryCollection::TransformGroup);
		SetRenderStateDirty();

		if( RestCollection ) UE_LOG(UGeometryCollectionComponentLogging, Log, TEXT("... RestCollection[%p]\n%s"),  RestCollection, *RestCollection->ToString() );
		if( DynamicCollection ) UE_LOG(UGeometryCollectionComponentLogging, Log, TEXT("... DynamicCollection[%p]\n%s"), DynamicCollection, *DynamicCollection->ToString());
	}
}

void UGeometryCollectionComponent::SendRenderDynamicData_Concurrent()
{
	//UE_LOG(UGeometryCollectionComponentLogging, Log, TEXT("GeometryCollectionComponent[%p]::SendRenderDynamicData_Concurrent()"), this);
	Super::SendRenderDynamicData_Concurrent();
	if (SceneProxy)
	{
		if (DynamicCollection)
		{
			FGeometryCollectionDynamicData * DynamicData = ::new FGeometryCollectionDynamicData;
			InitDynamicData(DynamicData);

			// Enqueue command to send to render thread
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(FSendGeometryCollectionData,
				FGeometryCollectionSceneProxy*, GeometryCollectionSceneProxy, (FGeometryCollectionSceneProxy*)SceneProxy,
				FGeometryCollectionDynamicData*, DynamicData, DynamicData,
				{
					GeometryCollectionSceneProxy->SetDynamicData_RenderThread(DynamicData);
				});
		}
	}
}

void UGeometryCollectionComponent::SetRestCollection(UGeometryCollection * RestCollectionIn)
{
	//UE_LOG(UGeometryCollectionComponentLogging, Log, TEXT("GeometryCollectionComponent[%p]::SetRestCollection()"), this);
	if (RestCollectionIn)
	{
		RestCollection = RestCollectionIn;
		// All rest states are shared across components and will have a AssetScope. 
		RestCollection->SetArrayScopes(UManagedArrayCollection::EArrayScope::FScopeShared);
		ResetDynamicCollection();
	}
}

FGeometryCollectionEdit::~FGeometryCollectionEdit()
{
	if (Update)
	{
		Component->ResetDynamicCollection();
	}
}

UGeometryCollection* FGeometryCollectionEdit::GetRestCollection()
{
	return Component->RestCollection;
}