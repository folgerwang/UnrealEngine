// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionDebugDrawActor.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(GeometryCollectionDrawLog, Log, All);

namespace GeomCollectionDebugDrawActorConstants
{
	// Constants
	static const bool bPersistent = true;  // Debug draw needs persistency to work well within the editor.
	static const float LifeTime = -1.0f;  // Lifetime is infinite.
	static const uint8 DepthPriority = 0;
	static const uint32 CircleSegments = 32;
	static const bool bDrawCircleAxis = true;

	// Defaults
	static const float PointThicknessDefault = 6.0f;
	static const float LineThicknessDefault = 0.5f;
	static const int32 TextShadowDefault = 0;  // Draw shadows under debug text, easier to read but slower to render.
	static const float TextScaleDefault = 1.0f;
	static const float NormalScaleDefault = 10.0f;
	static const float TransformScaleDefault = 20.0f;
	static const float ArrowScaleDefault = 2.5f;
}

// Console variables, also exposed as settings in this actor
static TAutoConsoleVariable<float> CVarPointThickness(TEXT("p.gc.PointThickness"), GeomCollectionDebugDrawActorConstants::PointThicknessDefault, TEXT("Geometry Collection debug draw, point thickness.\nDefault = 6."), ECVF_Cheat);
static TAutoConsoleVariable<float> CVarLineThickness (TEXT("p.gc.LineThickness" ), GeomCollectionDebugDrawActorConstants::LineThicknessDefault , TEXT("Geometry Collection debug draw, line thickness.\nDefault = 0.5."), ECVF_Cheat);
static TAutoConsoleVariable<int32> CVarTextShadow    (TEXT("p.gc.TextShadow"    ), GeomCollectionDebugDrawActorConstants::TextShadowDefault    , TEXT("Geometry Collection debug draw, text shadow under indices for better readability.\nDefault = 0."), ECVF_Cheat);
static TAutoConsoleVariable<float> CVarTextScale     (TEXT("p.gc.TextScale"     ), GeomCollectionDebugDrawActorConstants::TextScaleDefault     , TEXT("Geometry Collection debug draw, text scale.\nDefault = 1."), ECVF_Cheat);
static TAutoConsoleVariable<float> CVarNormalScale   (TEXT("p.gc.NormalScale"   ), GeomCollectionDebugDrawActorConstants::NormalScaleDefault   , TEXT("Geometry Collection debug draw, normal size.\nDefault = 10."), ECVF_Cheat);
static TAutoConsoleVariable<float> CVarTransformScale(TEXT("p.gc.TransformScale"), GeomCollectionDebugDrawActorConstants::TransformScaleDefault, TEXT("Geometry Collection debug draw, normal size.\nDefault = 10."), ECVF_Cheat);
static TAutoConsoleVariable<float> CVarArrowScale    (TEXT("p.gc.ArrowScale"    ), GeomCollectionDebugDrawActorConstants::ArrowScaleDefault    , TEXT("Geometry Collection debug draw, arrow size for normals.\nDefault = 2.5."), ECVF_Cheat);

AGeometryCollectionDebugDrawActor::AGeometryCollectionDebugDrawActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PointThickness()
	, LineThickness()
	, bTextShadow()
	, TextScale()
	, NormalScale()
	, TransformScale()
	, ArrowScale()
{
	// Set the console variables' callback
	const FConsoleVariableDelegate FloatConsoleVariableDelegate = FConsoleVariableDelegate::CreateUObject(this, &AGeometryCollectionDebugDrawActor::OnFloatPropertyChange);
	const FConsoleVariableDelegate BoolConsoleVariableDelegate = FConsoleVariableDelegate::CreateUObject(this, &AGeometryCollectionDebugDrawActor::OnBoolPropertyChange);
	CVarPointThickness->SetOnChangedCallback(FloatConsoleVariableDelegate);
	CVarLineThickness->SetOnChangedCallback(FloatConsoleVariableDelegate);
	CVarTextShadow->SetOnChangedCallback(BoolConsoleVariableDelegate);
	CVarTextScale->SetOnChangedCallback(FloatConsoleVariableDelegate);
	CVarNormalScale->SetOnChangedCallback(FloatConsoleVariableDelegate);
	CVarTransformScale->SetOnChangedCallback(FloatConsoleVariableDelegate);
	CVarArrowScale->SetOnChangedCallback(FloatConsoleVariableDelegate);

	// Initialize properties from console variables
	OnFloatPropertyChange(CVarPointThickness.AsVariable());
	OnFloatPropertyChange(CVarLineThickness.AsVariable());
	OnBoolPropertyChange(CVarTextShadow.AsVariable());
	OnFloatPropertyChange(CVarTextScale.AsVariable());
	OnFloatPropertyChange(CVarNormalScale.AsVariable());
	OnFloatPropertyChange(CVarTransformScale.AsVariable());
	OnFloatPropertyChange(CVarArrowScale.AsVariable());

	// Enable game tick calls
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(true);
}

void AGeometryCollectionDebugDrawActor::BeginDestroy()
{
	CVarPointThickness->SetOnChangedCallback(FConsoleVariableDelegate());
	Super::BeginDestroy();
}

void AGeometryCollectionDebugDrawActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Clear all persistent strings and debug lines.
	Flush();
}

#if WITH_EDITOR
void AGeometryCollectionDebugDrawActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Synchronize the command variables to this Actor's properties if the property name matches.
	static const EConsoleVariableFlags SetBy = ECVF_SetByConsole;  // Can't use the default ECVF_SetByCode as otherwise changing the UI won't update the global console variable.
	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName(): NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AGeometryCollectionDebugDrawActor, PointThickness)) { CVarPointThickness->Set(PointThickness, SetBy); }
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AGeometryCollectionDebugDrawActor, LineThickness)) { CVarLineThickness->Set(LineThickness, SetBy); }
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AGeometryCollectionDebugDrawActor, bTextShadow)) { CVarTextShadow->Set(int32(bTextShadow), SetBy); }
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AGeometryCollectionDebugDrawActor, TextScale)) { CVarTextScale->Set(TextScale, SetBy); }
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AGeometryCollectionDebugDrawActor, NormalScale)) { CVarNormalScale->Set(NormalScale, SetBy); }
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AGeometryCollectionDebugDrawActor, TransformScale)) { CVarTransformScale->Set(TransformScale, SetBy); }
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AGeometryCollectionDebugDrawActor, ArrowScale)) { CVarArrowScale->Set(ArrowScale, SetBy); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AGeometryCollectionDebugDrawActor::DrawVertices(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	const UWorld* const World = GetWorld();
	check(World);

	TArray<FTransform> Transforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

	const FTransform TransformActor = Actor->GetTransform();

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;
	const TManagedArray<FVector>& VertexArray = *Collection->Vertex;
	const TManagedArray<int32>& BoneMapArray = *Collection->BoneMap;

	const int32 NumVertices = Collection->NumElements(FGeometryCollection::VerticesGroup);

	for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
	{
		const int32 TransformIndex = BoneMapArray[IdxVertex];
		if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
		{
			const FTransform Transform = Transforms[TransformIndex] * TransformActor;

			const FVector Position = Transform.TransformPosition(VertexArray[IdxVertex]);

			DrawDebugPoint(World, Position, PointThickness, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority);
		}
	}
#endif
}

void AGeometryCollectionDebugDrawActor::DrawVertexIndices(FGeometryCollection* Collection, AActor* Actor, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	const UWorld* const World = GetWorld();
	check(World);

	TArray<FTransform> Transforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

	const FTransform TransformActor = Actor->GetTransform();
	const FVector ActorLocation = Actor->GetActorLocation();  // Actor->GetActorLocation() can return different location than Actor->GetTransform().GetLocation()

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;
	const TManagedArray<FVector>& VertexArray = *Collection->Vertex;
	const TManagedArray<int32>& BoneMapArray = *Collection->BoneMap;

	const int32 NumVertices = Collection->NumElements(FGeometryCollection::VerticesGroup);

	for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
	{
		const int32 TransformIndex = BoneMapArray[IdxVertex];
		if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
		{
			const FTransform Transform = Transforms[TransformIndex] * TransformActor;

			const FVector Position = Transform.TransformPosition(VertexArray[IdxVertex]);
			const FVector TextPosition = Position - ActorLocation;

			const FString Text = FString::Printf(TEXT("%d"), IdxVertex);

			DrawDebugString(World, TextPosition, Text, Actor, Color, GeomCollectionDebugDrawActorConstants::LifeTime, bTextShadow, TextScale);
		}
	}
#endif
}

void AGeometryCollectionDebugDrawActor::DrawVertexNormals(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	const UWorld* const World = GetWorld();
	check(World);

	TArray<FTransform> Transforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

	const FTransform TransformActor = Actor->GetTransform();

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;
	const TManagedArray<FVector>& VertexArray = *Collection->Vertex;
	const TManagedArray<int32>& BoneMapArray = *Collection->BoneMap;
	const TManagedArray<FVector>& NormalArray = *Collection->Normal;

	const int32 NumVertices = Collection->NumElements(FGeometryCollection::VerticesGroup);

	for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
	{
		const int32 TransformIndex = BoneMapArray[IdxVertex];
		if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
		{
			const FTransform Transform = Transforms[BoneMapArray[IdxVertex]] * TransformActor;

			const FVector LineStart = Transform.TransformPosition(VertexArray[IdxVertex]);
			const FVector VertexNormal = Transform.TransformVector(NormalArray[IdxVertex]).GetSafeNormal();
			const FVector LineEnd = LineStart + VertexNormal * NormalScale;

			DrawDebugDirectionalArrow(World, LineStart, LineEnd, ArrowScale, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
		}
	}
#endif
}

void AGeometryCollectionDebugDrawActor::DrawFaces(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	const UWorld* const World = GetWorld();
	check(World);

	TArray<FTransform> Transforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

	const FTransform TransformActor = Actor->GetTransform();

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;
	const TManagedArray<FVector>& VertexArray = *Collection->Vertex;
	const TManagedArray<int32>& BoneMapArray = *Collection->BoneMap;
	const TManagedArray<FIntVector>& IndicesArray = *Collection->Indices;

	const int32 NumFaces = Collection->NumElements(FGeometryCollection::FacesGroup);

	for (int32 IdxFace = 0; IdxFace < NumFaces; ++IdxFace)
	{
		const int32 TransformIndex = BoneMapArray[IndicesArray[IdxFace][0]];
		if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
		{
			const FTransform Transform = Transforms[BoneMapArray[IndicesArray[IdxFace][0]]] * TransformActor;

			const FVector Vertex0 = Transform.TransformPosition(VertexArray[IndicesArray[IdxFace][0]]);
			const FVector Vertex1 = Transform.TransformPosition(VertexArray[IndicesArray[IdxFace][1]]);
			const FVector Vertex2 = Transform.TransformPosition(VertexArray[IndicesArray[IdxFace][2]]);

			DrawDebugLine(World, Vertex0, Vertex1, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
			DrawDebugLine(World, Vertex0, Vertex2, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
			DrawDebugLine(World, Vertex1, Vertex2, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
		}
	}
#endif
}

void AGeometryCollectionDebugDrawActor::DrawFaceIndices(FGeometryCollection* Collection, AActor* Actor, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	const UWorld* const World = GetWorld();
	check(World);

	TArray<FTransform> Transforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

	const FTransform TransformActor = Actor->GetTransform();
	const FVector ActorLocation = Actor->GetActorLocation();  // Actor->GetActorLocation() can return different location than Actor->GetTransform().GetLocation()

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;
	const TManagedArray<FVector>& VertexArray = *Collection->Vertex;
	const TManagedArray<int32>& BoneMapArray = *Collection->BoneMap;
	const TManagedArray<FIntVector>& IndicesArray = *Collection->Indices;

	const int32 NumFaces = Collection->NumElements(FGeometryCollection::FacesGroup);

	for (int32 IdxFace = 0; IdxFace < NumFaces; ++IdxFace)
	{
		const int32 TransformIndex = BoneMapArray[IndicesArray[IdxFace][0]];
		if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
		{
			const FTransform Transform = Transforms[TransformIndex] * TransformActor;
			
			const FVector Vertex0 = VertexArray[IndicesArray[IdxFace][0]];
			const FVector Vertex1 = VertexArray[IndicesArray[IdxFace][1]];
			const FVector Vertex2 = VertexArray[IndicesArray[IdxFace][2]];

			const FVector FaceCenter = (Vertex0 + Vertex1 + Vertex2) / 3.f;

			const FVector Position = Transform.TransformPosition(FaceCenter);
			const FVector TextPosition = Position - ActorLocation;  // Actor location is added from within the DrawDebugString text position calculation, and need to be removed from the transform position if it is to be passed in world space

			const FString Text = FString::Printf(TEXT("%d"), IdxFace);

			DrawDebugString(World, TextPosition, Text, Actor, Color, GeomCollectionDebugDrawActorConstants::LifeTime, bTextShadow, TextScale);
		}
	}
#endif
}

void AGeometryCollectionDebugDrawActor::DrawFaceNormals(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	const UWorld* const World = GetWorld();
	check(World);

	TArray<FTransform> Transforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

	const FTransform TransformActor = Actor->GetTransform();

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;
	const TManagedArray<FVector>& VertexArray = *Collection->Vertex;
	const TManagedArray<int32>& BoneMapArray = *Collection->BoneMap;
	const TManagedArray<FIntVector>& IndicesArray = *Collection->Indices;

	const int32 NumFaces = Collection->NumElements(FGeometryCollection::FacesGroup);

	for (int32 IdxFace = 0; IdxFace < NumFaces; ++IdxFace)
	{
		const int32 TransformIndex = BoneMapArray[IndicesArray[IdxFace][0]];
		if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
		{
			const FTransform Transform = Transforms[TransformIndex] * TransformActor;

			const FVector Vertex0 = VertexArray[IndicesArray[IdxFace][0]];
			const FVector Vertex1 = VertexArray[IndicesArray[IdxFace][1]];
			const FVector Vertex2 = VertexArray[IndicesArray[IdxFace][2]];

			const FVector FaceCenter = (Vertex0 + Vertex1 + Vertex2) / 3.f;

			const FVector Edge1 = Vertex1 - Vertex0;
			const FVector Edge2 = -(Vertex2 - Vertex1);

			const FVector FaceNormal = Transform.TransformVector(Edge1 ^ Edge2).GetSafeNormal();

			const FVector LineStart = Transform.TransformPosition(FaceCenter);
			const FVector LineEnd = LineStart + FaceNormal * NormalScale;

			DrawDebugDirectionalArrow(World, LineStart, LineEnd, ArrowScale, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
		}
	}
#endif
}

void AGeometryCollectionDebugDrawActor::DrawSingleFace(FGeometryCollection* Collection, const AActor* Actor, const int32 FaceIndex, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	const int32 NumFaces = Collection->NumElements(FGeometryCollection::FacesGroup);
	if (FaceIndex < 0 || FaceIndex >= NumFaces) { return; }

	const UWorld* const World = GetWorld();
	check(World);

	TArray<FTransform> Transforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

	const FTransform TransformActor = Actor->GetTransform();

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;
	const TManagedArray<FVector>& VertexArray = *Collection->Vertex;
	const TManagedArray<int32>& BoneMapArray = *Collection->BoneMap;
	const TManagedArray<FIntVector>& IndicesArray = *Collection->Indices;

	const int32 TransformIndex = BoneMapArray[IndicesArray[FaceIndex][0]];
	if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
	{
		const FTransform Transform = Transforms[BoneMapArray[IndicesArray[FaceIndex][0]]] * TransformActor;

		const FVector Vertex0 = Transform.TransformPosition(VertexArray[IndicesArray[FaceIndex][0]]);
		const FVector Vertex1 = Transform.TransformPosition(VertexArray[IndicesArray[FaceIndex][1]]);
		const FVector Vertex2 = Transform.TransformPosition(VertexArray[IndicesArray[FaceIndex][2]]);

		DrawDebugLine(World, Vertex0, Vertex1, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
		DrawDebugLine(World, Vertex0, Vertex2, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
		DrawDebugLine(World, Vertex1, Vertex2, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
	}
#endif
}

void AGeometryCollectionDebugDrawActor::DrawTransforms(FGeometryCollection* Collection, const AActor* Actor)
{
#if ENABLE_DRAW_DEBUG
	const UWorld* const World = GetWorld();
	check(World);

	TArray<FTransform> Transforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

	const FTransform TransformActor = Actor->GetTransform();

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;
	const TManagedArray<int32>& BoneMapArray = *Collection->BoneMap;
	const TManagedArray<int32>& TransfomIndexArray = *Collection->TransformIndex;

	const int32 NumGeometries = Collection->NumElements(FGeometryCollection::GeometryGroup);

	for (int32 IdxGeometry = 0; IdxGeometry < NumGeometries; ++IdxGeometry)
	{
		const int32 TransformIndex = TransfomIndexArray[IdxGeometry];
		if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
		{
			const FTransform Transform = Transforms[TransformIndex] * TransformActor;

			const FVector Position = Transform.GetLocation();
			const FRotator Rotator = Transform.Rotator();

			DrawDebugCoordinateSystem(World, Position, Rotator, TransformScale, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
		}
	}
#endif
}

void AGeometryCollectionDebugDrawActor::DrawTransformIndices(FGeometryCollection* Collection, AActor* Actor, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	const UWorld* const World = GetWorld();
	check(World);

	TArray<FTransform> Transforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

	const FTransform TransformActor = Actor->GetTransform();
	const FVector ActorLocation = Actor->GetActorLocation();  // Actor->GetActorLocation() can return different location than Actor->GetTransform().GetLocation()

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;
	const TManagedArray<int32>& BoneMapArray = *Collection->BoneMap;
	const TManagedArray<int32>& TransfomIndexArray = *Collection->TransformIndex;

	const int32 NumGeometries = Collection->NumElements(FGeometryCollection::GeometryGroup);

	for (int32 IdxGeometry = 0; IdxGeometry < NumGeometries; ++IdxGeometry)
	{
		const int32 TransformIndex = TransfomIndexArray[IdxGeometry];
		if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
		{
			const FTransform Transform = Transforms[TransformIndex] * TransformActor;

			const FVector Position = Transform.GetLocation();
			const FVector TextPosition = Position - ActorLocation;  // Actor location is added from within the DrawDebugString text position calculation, and need to be removed from the transform position if it is to be passed in World space

			const FString Text = FString::Printf(TEXT("%d"), TransformIndex);

			DrawDebugString(World, TextPosition, Text, Actor, Color, GeomCollectionDebugDrawActorConstants::LifeTime, bTextShadow, TextScale);
		}
	}
#endif
}

void AGeometryCollectionDebugDrawActor::DrawBoundingBoxes(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	const UWorld* const World = GetWorld();
	check(World);

	TArray<FTransform> Transforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

	const FTransform TransformActor = Actor->GetTransform();

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;
	const TManagedArray<FBox>& BoundingBoxArray = *Collection->BoundingBox;
	const TManagedArray<int32>& TransfomIndexArray = *Collection->TransformIndex;

	const int32 NumGeometries = Collection->NumElements(FGeometryCollection::GeometryGroup);

	for (int32 IdxGeometry = 0; IdxGeometry < NumGeometries; ++IdxGeometry)
	{
		const int32 TransformIndex = TransfomIndexArray[IdxGeometry];
		if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
		{
			const FTransform Transform = Transforms[TransformIndex] * TransformActor;

			const FBox BBox = BoundingBoxArray[IdxGeometry];
			const FVector VertexMin = BBox.Min;
			const FVector VertexMax = BBox.Max;

			const FVector Vertex0 = VertexMin;
			const FVector Vertex1(VertexMax.X, VertexMin.Y, VertexMin.Z);
			const FVector Vertex2(VertexMax.X, VertexMax.Y, VertexMin.Z);
			const FVector Vertex3(VertexMin.X, VertexMax.Y, VertexMin.Z);
			const FVector Vertex4(VertexMin.X, VertexMin.Y, VertexMax.Z);
			const FVector Vertex5(VertexMax.X, VertexMin.Y, VertexMax.Z);
			const FVector Vertex6 = VertexMax;
			const FVector Vertex7(VertexMin.X, VertexMax.Y, VertexMax.Z);

			struct FEdge {
				FVector Start;
				FVector End;
			};
			TArray<FEdge> EdgeArray;
			EdgeArray.Add({ Vertex0, Vertex1 });
			EdgeArray.Add({ Vertex1, Vertex2 });
			EdgeArray.Add({ Vertex2, Vertex3 });
			EdgeArray.Add({ Vertex3, Vertex0 });
			EdgeArray.Add({ Vertex4, Vertex5 });
			EdgeArray.Add({ Vertex5, Vertex6 });
			EdgeArray.Add({ Vertex6, Vertex7 });
			EdgeArray.Add({ Vertex7, Vertex4 });
			EdgeArray.Add({ Vertex0, Vertex4 });
			EdgeArray.Add({ Vertex1, Vertex5 });
			EdgeArray.Add({ Vertex2, Vertex6 });
			EdgeArray.Add({ Vertex3, Vertex7 });

			for (int32 IdxEdge = 0; IdxEdge < EdgeArray.Num(); ++IdxEdge)
			{
				const FVector LineStart = Transform.TransformPosition(EdgeArray[IdxEdge].Start);
				const FVector LineEnd = Transform.TransformPosition(EdgeArray[IdxEdge].End);
				DrawDebugLine(World, LineStart, LineEnd, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
			}
		}
	}
#endif
}

void AGeometryCollectionDebugDrawActor::DrawProximity(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	const UWorld* const World = GetWorld();
	check(World);

	TArray<FTransform> Transforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

	const FTransform TransformActor = Actor->GetTransform();

	const TManagedArray<int32>& TransfomIndexArray = *Collection->TransformIndex;
	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;
	const TManagedArray<FVector>& VertexArray = *Collection->Vertex;
	const TManagedArray<int32>& BoneMapArray = *Collection->BoneMap;
	const TManagedArray<TSet<int32>>& ProximityArray = *Collection->Proximity;

	const int32 NumVertices = Collection->NumElements(FGeometryCollection::VerticesGroup);
	const int32 NumGeometries = Collection->NumElements(FGeometryCollection::GeometryGroup);

	// Compute centers of geometries
	TArray<FVector> GeometryCenterArray;
	GeometryCenterArray.SetNum(NumGeometries);
	for (int32 IdxGeometry = 0; IdxGeometry < NumGeometries; ++IdxGeometry)
	{
		const FTransform Transform = Transforms[TransfomIndexArray[IdxGeometry]] * TransformActor;

		FVector Center = FVector(0);
		int32 NumVerticesAdded = 0;

		for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
		{
			if (BoneMapArray[IdxVertex] == TransfomIndexArray[IdxGeometry])
			{
				Center += Transform.TransformPosition(VertexArray[IdxVertex]);
				NumVerticesAdded++;
			}
		}
		GeometryCenterArray[IdxGeometry] = Center / (float)NumVerticesAdded;
	}

	// Build reverse map between TransformIdx and index in the GeometryGroup
	TMap<int32, int32> GeometryGroupIndexMap;
	for (int32 IdxGeometry = 0; IdxGeometry < NumGeometries; ++IdxGeometry)
	{
		GeometryGroupIndexMap.Add(TransfomIndexArray[IdxGeometry], IdxGeometry);
	}

	for (int32 IdxGeometry = 0; IdxGeometry < NumGeometries; ++IdxGeometry)
	{
		const int32 TransformIndex = TransfomIndexArray[IdxGeometry];
		if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
		{
			for (auto& OtherGeometryIndex : ProximityArray[IdxGeometry])
			{
				const FVector Line1Start = GeometryCenterArray[IdxGeometry];
				const FVector Line1End = GeometryCenterArray[OtherGeometryIndex];
				DrawDebugLine(World, Line1Start, Line1End, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
			}
		}
	}
#endif
}

void AGeometryCollectionDebugDrawActor::DrawBreakingFaces(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	const UWorld* const World = GetWorld();
	check(World);

	TArray<FTransform> Transforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

	const FTransform TransformActor = Actor->GetTransform();

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;
	const TManagedArray<FVector>& VertexArray = *Collection->Vertex;
	const TManagedArray<int32>& BoneMapArray = *Collection->BoneMap;
	const TManagedArray<FIntVector>& IndicesArray = *Collection->Indices;
	const TManagedArray<int32>& BreakingFaceIndexArray = *Collection->BreakingFaceIndex;

	const int32 NumBreakings = Collection->NumElements(FGeometryCollection::BreakingGroup);

	for (int32 IdxBreak = 0; IdxBreak < NumBreakings; ++IdxBreak)
	{
		const int32 TransformIndex = BoneMapArray[IndicesArray[BreakingFaceIndexArray[IdxBreak]][0]];
		if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
		{
			const FTransform Transform = Transforms[TransformIndex] * TransformActor;

			const FVector Vertex0 = Transform.TransformPosition(VertexArray[IndicesArray[BreakingFaceIndexArray[IdxBreak]][0]]);
			const FVector Vertex1 = Transform.TransformPosition(VertexArray[IndicesArray[BreakingFaceIndexArray[IdxBreak]][1]]);
			const FVector Vertex2 = Transform.TransformPosition(VertexArray[IndicesArray[BreakingFaceIndexArray[IdxBreak]][2]]);

			DrawDebugLine(World, Vertex0, Vertex1, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
			DrawDebugLine(World, Vertex0, Vertex2, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
			DrawDebugLine(World, Vertex1, Vertex2, Color, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);
		}
	}
#endif
}

void AGeometryCollectionDebugDrawActor::DrawBreakingRegionData(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	const UWorld* const World = GetWorld();
	check(World);

	TArray<FTransform> Transforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

	const FTransform TransformActor = Actor->GetTransform();

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;
	const TManagedArray<int32>& BoneMapArray = *Collection->BoneMap;
	const TManagedArray<FVector>& VertexArray = *Collection->Vertex;
	const TManagedArray<FIntVector>& IndicesArray = *Collection->Indices;
	const TManagedArray<int32>& BreakingFaceIndexArray = *Collection->BreakingFaceIndex;
	const TManagedArray<FVector>& BreakingRegionCentroidArray = *Collection->BreakingRegionCentroid;
	const TManagedArray<FVector>& BreakingRegionNormalArray = *Collection->BreakingRegionNormal;
	const TManagedArray<float>& BreakingRegionRadiusArray = *Collection->BreakingRegionRadius;

	const int32 NumVertices = Collection->NumElements(FGeometryCollection::VerticesGroup);
	const int32 NumGeometries = Collection->NumElements(FGeometryCollection::GeometryGroup);
	const int32 NumBreakings = Collection->NumElements(FGeometryCollection::BreakingGroup);

	for (int32 IdxBreak = 0; IdxBreak < NumBreakings; ++IdxBreak)
	{
		const int32 TransformIndex = BoneMapArray[IndicesArray[BreakingFaceIndexArray[IdxBreak]][0]];
		if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
		{
			const FTransform Transform = Transforms[TransformIndex] * TransformActor;

			const FVector Center = Transform.TransformPosition(BreakingRegionCentroidArray[IdxBreak]);
			const FVector Normal = Transform.TransformVector(BreakingRegionNormalArray[IdxBreak]).GetSafeNormal();
			const FVector LineEnd = Center + Normal * 10.f;

			DrawDebugDirectionalArrow(World, Center, LineEnd, ArrowScale, FColor::Green, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness);

			// Draw InnerCircle for the region
			const FVector Vertex0 = Transform.TransformPosition(VertexArray[IndicesArray[BreakingFaceIndexArray[IdxBreak]][0]]);
			const FVector Vertex1 = Transform.TransformPosition(VertexArray[IndicesArray[BreakingFaceIndexArray[IdxBreak]][1]]);

			const FVector YAxis = (Vertex0 - Vertex1).GetSafeNormal();
			const FVector ZAxis = (YAxis ^ Normal).GetSafeNormal();
			DrawDebugCircle(World, Center, BreakingRegionRadiusArray[IdxBreak], GeomCollectionDebugDrawActorConstants::CircleSegments, FColor::Green, GeomCollectionDebugDrawActorConstants::bPersistent, GeomCollectionDebugDrawActorConstants::LifeTime, GeomCollectionDebugDrawActorConstants::DepthPriority, LineThickness, YAxis, ZAxis, GeomCollectionDebugDrawActorConstants::bDrawCircleAxis);
		}
	}
#endif
}

void AGeometryCollectionDebugDrawActor::Flush()
{
	const UWorld* const World = GetWorld();
	check(World);
	FlushDebugStrings(World);
	FlushPersistentDebugLines(World);
}

void AGeometryCollectionDebugDrawActor::OnFloatPropertyChange(IConsoleVariable* CVar)
{
	const float NewFloat = CVar->GetFloat();
	float* Float = nullptr;
	// Identify float property from variable
	if (CVar == CVarPointThickness.AsVariable()) { Float = &PointThickness; }
	else if (CVar == CVarLineThickness.AsVariable()) { Float = &LineThickness; }
	else if (CVar == CVarTextScale.AsVariable()) { Float = &TextScale; }
	else if (CVar == CVarNormalScale.AsVariable()) { Float = &NormalScale; }
	else if (CVar == CVarTransformScale.AsVariable()) { Float = &TransformScale; }
	else if (CVar == CVarArrowScale.AsVariable()) { Float = &ArrowScale; }
	// Change property
	if (Float && NewFloat != *Float)
	{
		*Float = NewFloat;
	}
}

void AGeometryCollectionDebugDrawActor::OnBoolPropertyChange(IConsoleVariable* CVar)
{
	const bool NewBool = !!CVar->GetInt();
	bool* Bool = nullptr;
	// Identify int property from variable
	if (CVar == CVarTextShadow.AsVariable()) { Bool = &bTextShadow; }
	// Change property
	if (Bool && NewBool != *Bool)
	{
		*Bool = NewBool;
	}
}
