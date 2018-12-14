// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionDebugDrawComponent.h"

#if GEOMETRYCOLLECTION_DEBUG_DRAW
#include "GeometryCollection/GeometryCollectionRenderLevelSetActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionDebugDrawActor.h"
#include "GeometryCollection/GeometryCollectionPhysicsProxy.h"
#if INCLUDE_CHAOS
#include "PBDRigidsSolver.h"
#endif  // #if INCLUDE_CHAOS
#endif  // #if GEOMETRYCOLLECTION_DEBUG_DRAW
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(UGCCDD_LOG, All, All);

// Constants
static const FLinearColor DarkerColorFactor(1.0f, 1.0f, 0.7f);  // Darker HSV multiplier
static const FLinearColor LighterColorFactor(1.0f, 1.0f, 3.0f);  // Lighter HSV multiplier
static const FLinearColor VertexColorDefault(0.2f, 0.4f, 0.6f, 1.0f);  // Blue
static const FLinearColor FaceColorDefault(0.4f, 0.2f, 0.6f, 1.0f);  // Purple
static const FLinearColor GeometryColorDefault(0.6, 0.4f, 0.2f, 1.0f);  // Orange
static const FLinearColor BreakingColorDefault(0.4f, 0.6f, 0.2f, 1.0f);  // Green

UGeometryCollectionDebugDrawComponent::UGeometryCollectionDebugDrawComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, GeometryCollectionDebugDrawActor(nullptr)
	, GeometryCollectionRenderLevelSet(nullptr)
	, bDebugDrawLevelSet(false)
	, bRenderLevelSetAtOrigin(false)
	, LevelSetIndex(0)
	, bDebugDrawTransform(false)
	, bDebugDrawTransformIndex(false)
	, bDebugDrawBoundingBox(false)
	, GeometryColor(GeometryColorDefault)
	, bDebugDrawProximity(false)
	, bDebugDrawBreakingFace(false)
	, bDebugDrawBreakingRegionData(false)
	, BreakingColor(BreakingColorDefault)
	, bDebugDrawFace(false)
	, bDebugDrawFaceIndex(false)
	, bDebugDrawFaceNormal(false)
	, bDebugDrawSingleFace(false)
	, SingleFaceIdx(0)
	, FaceColor(FaceColorDefault)
	, bDebugDrawVertex(false)
	, bDebugDrawVertexIndex(false)
	, bDebugDrawVertexNormal(false)
	, VertexColor(VertexColorDefault)
	, GeometryCollectionComponent(nullptr)
	, bLevelSetTextureDirty(false)
	, LevelSetTextureTransformIndex(-1)
	, BaseVisibilityArray()
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = false;
}

void UGeometryCollectionDebugDrawComponent::BeginPlay()
{
	Super::BeginPlay();

	DebugDrawBeginPlay();
	DebugDrawLevelSetBeginPlay();
}

void UGeometryCollectionDebugDrawComponent::EndPlay(EEndPlayReason::Type ReasonEnd)
{	
	Super::EndPlay(ReasonEnd);

	DebugDrawLevelSetEndPlay();
}

void UGeometryCollectionDebugDrawComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{	
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	DebugDrawTick();
	DebugDrawLevelSetTick();
}

#if WITH_EDITOR
void UGeometryCollectionDebugDrawComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
#if GEOMETRYCOLLECTION_DEBUG_DRAW
	if (GeometryCollectionComponent)
	{
		const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName(): NAME_None;
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UGeometryCollectionDebugDrawComponent, SingleFaceIdx))
		{
			check(GeometryCollectionComponent->DynamicCollection != nullptr);
			check(GeometryCollectionComponent->DynamicCollection->GetGeometryCollection().IsValid());
			FGeometryCollection* const Collection = GeometryCollectionComponent->DynamicCollection->GetGeometryCollection().Get();
			if (Collection)
			{
				const int32 NumFaces = Collection->NumElements(FGeometryCollection::FacesGroup);
				SingleFaceIdx = FMath::Clamp(SingleFaceIdx, 0, NumFaces - 1);
			}
		}
	}
#endif  // #if GEOMETRYCOLLECTION_DEBUG_DRAW
}
#endif  // #if WITH_EDITOR

void UGeometryCollectionDebugDrawComponent::DebugDrawLevelSetBeginPlay()
{
#if INCLUDE_CHAOS && GEOMETRYCOLLECTION_DEBUG_DRAW
	// Look out for existing render level set actor, or create one when needed
	if (!GeometryCollectionRenderLevelSet)
	{
		UWorld* const world = GetWorld();
		check(world);
		const TActorIterator<AGeometryCollectionRenderLevelSetActor> ActorIterator(world);
		if (ActorIterator)
		{
			GeometryCollectionRenderLevelSet = *ActorIterator;
		}
		else
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GeometryCollectionRenderLevelSet = world->SpawnActor<AGeometryCollectionRenderLevelSetActor>(SpawnInfo);
			if (GeometryCollectionRenderLevelSet)
			{
				GeometryCollectionRenderLevelSet->SetActorEnableCollision(false);
			}
		}
	}
	if (GeometryCollectionComponent && GeometryCollectionComponent->DynamicCollection && GeometryCollectionRenderLevelSet)
	{
		// For now it makes sense to always initialize our visibility arrays since a user might start with vis debug off, then turn it on
		// This logic could change in the future
		TManagedArray<bool> &VisibleArray = *GeometryCollectionComponent->DynamicCollection->GetGeometryCollection()->Visible;
		BaseVisibilityArray = GeometryCollectionComponent->DynamicCollection->GetGeometryCollection()->AddAttribute<bool>("BaseVisibility", FGeometryCollection::FacesGroup);
		(*BaseVisibilityArray).Init(VisibleArray);

		bLevelSetTextureDirty = true;
		LevelSetTextureTransformIndex = -1;
	}
#endif  // #if INCLUDE_CHAOS && GEOMETRYCOLLECTION_DEBUG_DRAW
}

void UGeometryCollectionDebugDrawComponent::DebugDrawLevelSetEndPlay()
{
#if INCLUDE_CHAOS && GEOMETRYCOLLECTION_DEBUG_DRAW
	if (GeometryCollectionComponent && GeometryCollectionRenderLevelSet)
	{
		// @note: it might be the case that the user checks and unchecks render level set multiple times, and when they exit it is off
		// but we still want to reset the visibility.  One solution is to always reset visibility at run time when the check box changes
		if (bDebugDrawLevelSet) {
			DebugDrawLevelSetResetVisiblity();
		}

		// turn off rendering
		GeometryCollectionRenderLevelSet->SetEnabled(false);

		bLevelSetTextureDirty = true;
		LevelSetTextureTransformIndex = -1;
	}
#endif  // #if INCLUDE_CHAOS && GEOMETRYCOLLECTION_DEBUG_DRAW
}

void UGeometryCollectionDebugDrawComponent::DebugDrawLevelSetResetVisiblity()
{
#if INCLUDE_CHAOS && GEOMETRYCOLLECTION_DEBUG_DRAW
	check(GeometryCollectionComponent != nullptr);
	check(GeometryCollectionComponent->DynamicCollection != nullptr);
	check(GeometryCollectionComponent->DynamicCollection->GetGeometryCollection().IsValid());
	check(BaseVisibilityArray.IsValid());
	// reset visibility array
	TManagedArray<bool> &VisibleArray = *GeometryCollectionComponent->DynamicCollection->GetGeometryCollection()->Visible;
	VisibleArray.Init(*BaseVisibilityArray);

	// if we only have one piece, and all of the faces were hidden, then ensure we set visibility true
	if (!GeometryCollectionComponent->IsVisible()) {
		GeometryCollectionComponent->SetVisibility(true);
	}
	else
	{
		GeometryCollectionComponent->ForceInitRenderData();
	}
#endif  // #if INCLUDE_CHAOS && GEOMETRYCOLLECTION_DEBUG_DRAW
}

void UGeometryCollectionDebugDrawComponent::DebugDrawLevelSetTick()
{
#if INCLUDE_CHAOS && GEOMETRYCOLLECTION_DEBUG_DRAW
	if (!bDebugDrawLevelSet)
	{
		if (LevelSetTextureTransformIndex != -1 && GeometryCollectionRenderLevelSet && BaseVisibilityArray)
		{
			// in this case, the user switched from debug draw to not at run time, reset visibility
			DebugDrawLevelSetResetVisiblity();

			// turn off rendering
			GeometryCollectionRenderLevelSet->SetEnabled(false);

			bLevelSetTextureDirty = true;
			LevelSetTextureTransformIndex = -1;
		}
	}
	else
	{
		// if the level set index has changed at run time, then reload the volume
		// because someone wants to visualize another piece
		if (LevelSetTextureTransformIndex != -1 && LevelSetTextureTransformIndex != LevelSetIndex) {
			bLevelSetTextureDirty = true;
		}

		// Error case if the level set renderer, physics proxy, or solver are null
		if (!GeometryCollectionRenderLevelSet) {
			UE_LOG(UGCCDD_LOG, Warning, TEXT("level set renderer: %s"), *GetFullName());
			return;
		}

		if (!GeometryCollectionComponent) {
			UE_LOG(UGCCDD_LOG, Warning, TEXT("no geometry component: %s"), *GetFullName());
			return;
		}

		FGeometryCollectionPhysicsProxy *PhysicsProxy = GeometryCollectionComponent->GetPhysicsProxy();
		if (!GeometryCollectionRenderLevelSet || !PhysicsProxy || !PhysicsProxy->GetSolver()) {
			UE_LOG(UGCCDD_LOG, Warning, TEXT("No solver context: %s"), *GetFullName());
			return;
		}

		// We must have at least one body to continue
		const Chaos::PBDRigidsSolver::FParticlesType &Particles = PhysicsProxy->GetSolver()->GetRigidParticles();
		if (Particles.Size() == 0) {		
			UE_LOG(UGCCDD_LOG, Warning, TEXT("No rbds in solver context: %s"), *GetFullName());
			return;
		}

		// Map the piece index to the rbd index to extract the level set	
		if (GeometryCollectionComponent->RigidBodyIds.Num() == 0) {
			UE_LOG(UGCCDD_LOG, Warning, TEXT("No rbd ids synced: %s"), *GetFullName());
			return;
		}

		// Make sure we have a valid tranform index
		if (LevelSetIndex < 0 || LevelSetIndex > GeometryCollectionComponent->RigidBodyIds.Num() - 1) {
			UE_LOG(UGCCDD_LOG, Warning, TEXT("Invalid level set index: %s"), *GetFullName());
			return;
		}

		// Make sure we have a valid rbd index
		int32 RbdId = GeometryCollectionComponent->RigidBodyIds[LevelSetIndex];
		if (RbdId < 0) {
			UE_LOG(UGCCDD_LOG, Warning, TEXT("No rbd ids synced: %s"), *GetFullName());
			return;
		}

		Chaos::TImplicitObject<float, 3>* CollisionBase = Particles.Geometry(RbdId);

		// Make sure the actual implicit object isn't null
		if (CollisionBase == NULL) {
			UE_LOG(UGCCDD_LOG, Warning, TEXT("Collision is null for level set visualization: %s"), *GetFullName());
			return;
		}

		// Cast to level set, make sure the type is correct
		Chaos::TLevelSet<float, 3>* CollisionLevelSet = CollisionBase->GetObject< Chaos::TLevelSet<float, 3> >();

		if (CollisionLevelSet == NULL) {
			UE_LOG(UGCCDD_LOG, Warning, TEXT("Incorrect collision type for level set rendering.  It must be a level set: %s"), *GetFullName());
			return;
		}

		// Get the transform for the current piece
		FTransform CurrTransform = FTransform::Identity;

		UGeometryCollection *DynamicCollection = GeometryCollectionComponent->DynamicCollection;

		// Update the transform if we are rendering the level set aligned with the simulated geometry
		if (!bRenderLevelSetAtOrigin) {
			//TManagedArray<FTransform> &TransformArray = DynamicCollection->GetGeometryCollection()->GetAttribute<FTransform>("Transform", FGeometryCollection::TransformGroup).Get();
			//CurrTransform = TransformArray[LevelSetIndex];

			// @todo: this is slow to recompute the global matrices here.  Ideally we'd grab some cached ones from the geom collection component
			TArray<FTransform> GlobalMatrices;
			GeometryCollectionAlgo::GlobalMatrices(DynamicCollection->GetGeometryCollection().Get(), GlobalMatrices);
			CurrTransform = GlobalMatrices[LevelSetIndex];

			CurrTransform *= this->GetOwner()->GetTransform();
		}

		// If we are only updating the transform, or also loading the volume
		if (!bLevelSetTextureDirty) {
			GeometryCollectionRenderLevelSet->SyncLevelSetTransform(CurrTransform);
		}
		else {
			// Build the volume texture
			// @note: we only want to do this once, so we have a state variable on the component to ensure that
			bool success = GeometryCollectionRenderLevelSet->SetLevelSetToRender(*CollisionLevelSet, CurrTransform);

			// Error case if volume fill didn't work
			if (!success) {
				UE_LOG(UGCCDD_LOG, Warning, TEXT("Levelset generation failed: %s"), *GetFullName());
				return;
			}

			// hide the original piece
			TManagedArray<bool> &VisibleArray = *DynamicCollection->GetGeometryCollection()->Visible;

			// reset visibility to original state
			// @todo: there is some logic missing here, but also GeometryCollectionComponent doesn't like
			// debug rendering flags being set in simulate mode, so we don't switch piece often right now
			if (LevelSetTextureTransformIndex != -1) {
				VisibleArray.Init(*BaseVisibilityArray);
			}

			const TManagedArray<int32> &TransformIndexArray = *DynamicCollection->GetGeometryCollection()->TransformIndex;

			const TManagedArray<int32> &FaceStartArray = *DynamicCollection->GetGeometryCollection()->FaceStart;
			const TManagedArray<int32> &FaceCountArray = *DynamicCollection->GetGeometryCollection()->FaceCount;

			const TManagedArray<FIntVector> &Indices = *DynamicCollection->GetGeometryCollection()->Indices;
			const TManagedArray<int32>& MaterialIndex = *DynamicCollection->GetGeometryCollection()->MaterialIndex;

			// for each geom, check if it is the transform object
			// if it is, hide all the faces
			int NumHid = 0;
			for (int i = 0; i < TransformIndexArray.Num(); ++i) {
				const int32 currT = TransformIndexArray[i];

				if (currT == LevelSetIndex) {

					int32 FaceStart = FaceStartArray[i];
					int32 FaceCount = FaceCountArray[i];

					// set visibility on faces to false
					for (int j = FaceStart; j < FaceStart + FaceCount; ++j) {
						VisibleArray[j] = false;
					}
					NumHid = FaceCount;
				}
			}

			// if we have no visible faces, hide the geometry without changing the collection
			// #todo: right now we can't send zero vertices to force the vertex buffer to be empty,
			// so we just hide the component.
			if (NumHid == VisibleArray.Num())
			{						
				GeometryCollectionComponent->SetVisibility(false);
			} 
			else
			{
				// init all render data
				GeometryCollectionComponent->ForceInitRenderData();
			}

			// Make sure we know not to refill the texture on subsequent frames
			bLevelSetTextureDirty = false;
			LevelSetTextureTransformIndex = LevelSetIndex;
		
			// Turn on the volume rendering
			GeometryCollectionRenderLevelSet->SetEnabled(true);
		}
	}
#endif  // #if INCLUDE_CHAOS && GEOMETRYCOLLECTION_DEBUG_DRAW
}

void UGeometryCollectionDebugDrawComponent::DebugDrawBeginPlay()
{
#if GEOMETRYCOLLECTION_DEBUG_DRAW
	if (!GeometryCollectionDebugDrawActor)
	{
		// Look out for existing debug draw actor, or create one when needed
		UWorld* const world = GetWorld();
		check(world);
		const TActorIterator<AGeometryCollectionDebugDrawActor> ActorIterator(world);
		if (ActorIterator)
		{
			GeometryCollectionDebugDrawActor = *ActorIterator;
		}
		else
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GeometryCollectionDebugDrawActor = world->SpawnActor<AGeometryCollectionDebugDrawActor>(SpawnInfo);
			if (GeometryCollectionDebugDrawActor)
			{
				GeometryCollectionDebugDrawActor->SetActorEnableCollision(false);
			}
		}
	}
	// Make sure to tick the debug draw first, it is required to clear up the persistent lines before drawing a new frame
	if (GeometryCollectionDebugDrawActor)
	{
		AActor* const actor = GetOwner();
		check(actor);
		GeometryCollectionDebugDrawActor->AddTickPrerequisiteActor(actor);
	}
#endif  // #if GEOMETRYCOLLECTION_DEBUG_DRAW
}

void UGeometryCollectionDebugDrawComponent::DebugDrawTick()
{
#if GEOMETRYCOLLECTION_DEBUG_DRAW
	check(GeometryCollectionDebugDrawActor);
	// Only draw when a GeometryCollectionComponent is also attached to the actor (GeometryCollectionComponent is set by AGeometryCollectionActor::AGeometryCollectionActor())
	if (!GeometryCollectionComponent)
	{
		UE_LOG(UGCCDD_LOG, Warning, TEXT("Null geometry component pointer: %s"), *GetFullName());
		return;
	}
	if (!GeometryCollectionComponent->DynamicCollection)
	{
		UE_LOG(UGCCDD_LOG, Warning, TEXT("Null geometry dynamic collection pointer: %s"), *GetFullName());
		return;
	}
	if (!GeometryCollectionComponent->DynamicCollection->GetGeometryCollection().IsValid())
	{
		UE_LOG(UGCCDD_LOG, Warning, TEXT("No valid geometry collection: %s"), *GetFullName());
		return;
	}

	// Draw collection
	FGeometryCollection* const Collection = GeometryCollectionComponent->DynamicCollection->GetGeometryCollection().Get();
	check(Collection);
	AActor* const Actor = GetOwner();
	check(Actor);

	if (bDebugDrawVertex)
	{
		const FColor Color = VertexColor.ToFColor(false);
		GeometryCollectionDebugDrawActor->DrawVertices(Collection, Actor, Color);
	}
	if (bDebugDrawVertexIndex)
	{ 
		const FColor Color = (VertexColor.LinearRGBToHSV() * LighterColorFactor).HSVToLinearRGB().ToFColor(false);
		GeometryCollectionDebugDrawActor->DrawVertexIndices(Collection, Actor, Color);
	}
	if (bDebugDrawVertexNormal)
	{
		const FColor Color = (VertexColor.LinearRGBToHSV() * DarkerColorFactor).HSVToLinearRGB().ToFColor(false);
		GeometryCollectionDebugDrawActor->DrawVertexNormals(Collection, Actor, Color);
	}
	if (bDebugDrawFace)
	{
		const FColor Color = FaceColor.ToFColor(false);
		GeometryCollectionDebugDrawActor->DrawFaces(Collection, Actor, Color);
	}
	if (bDebugDrawFaceIndex)
	{
		const FColor Color = (FaceColor.LinearRGBToHSV() * LighterColorFactor).HSVToLinearRGB().ToFColor(false);
		GeometryCollectionDebugDrawActor->DrawFaceIndices(Collection, Actor, Color);
	}
	if (bDebugDrawSingleFace)
	{
		const FColor Color = (FaceColor.LinearRGBToHSV() * LighterColorFactor).HSVToLinearRGB().ToFColor(false);
		GeometryCollectionDebugDrawActor->DrawSingleFace(Collection, Actor, SingleFaceIdx, Color);
	}
	if (bDebugDrawFaceNormal)
	{
		const FColor Color = (FaceColor.LinearRGBToHSV() * DarkerColorFactor).HSVToLinearRGB().ToFColor(false);
		GeometryCollectionDebugDrawActor->DrawFaceNormals(Collection, Actor, Color);
	}
	if (bDebugDrawTransform)
	{
		GeometryCollectionDebugDrawActor->DrawTransforms(Collection, Actor);
	}
	if (bDebugDrawTransformIndex)
	{
		const FColor Color = (GeometryColor.LinearRGBToHSV() * LighterColorFactor).HSVToLinearRGB().ToFColor(false);
		GeometryCollectionDebugDrawActor->DrawTransformIndices(Collection, Actor, Color);
	}
	if (bDebugDrawBoundingBox)
	{
		const FColor Color = GeometryColor.ToFColor(false);
		GeometryCollectionDebugDrawActor->DrawBoundingBoxes(Collection, Actor, Color);
	}
	if (bDebugDrawProximity)
	{
		const FColor Color = BreakingColor.ToFColor(false);
		GeometryCollectionDebugDrawActor->DrawProximity(Collection, Actor, Color);
	}
	if (bDebugDrawBreakingFace)
	{
		const FColor Color = (BreakingColor.LinearRGBToHSV() * LighterColorFactor).HSVToLinearRGB().ToFColor(false);
		GeometryCollectionDebugDrawActor->DrawBreakingFaces(Collection, Actor, Color);
	}
	if (bDebugDrawBreakingRegionData)
	{
		const FColor Color = (BreakingColor.LinearRGBToHSV() * DarkerColorFactor).HSVToLinearRGB().ToFColor(false);
		GeometryCollectionDebugDrawActor->DrawBreakingRegionData(Collection, Actor, Color);
	}
#endif  // #if GEOMETRYCOLLECTION_DEBUG_DRAW
}

