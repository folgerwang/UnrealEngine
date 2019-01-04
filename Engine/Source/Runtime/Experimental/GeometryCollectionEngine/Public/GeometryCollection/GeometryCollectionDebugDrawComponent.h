// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef GEOMETRYCOLLECTION_DEBUG_DRAW
#define GEOMETRYCOLLECTION_DEBUG_DRAW !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#endif

#include "Components/MeshComponent.h"
#include "GeometryCollection/ManagedArray.h"

#include "GeometryCollectionDebugDrawComponent.generated.h"

class AGeometryCollectionRenderLevelSetActor;
class UGeometryCollectionComponent;
class AGeometryCollectionDebugDrawActor;

// class responsible for debug drawing functionality for GeometryCollectionComponents
// @todo: formalize the idea of a "debug draw mode" in some class hierarchy to make it easy 
// to implement new types of visualizations
UCLASS(meta = (BlueprintSpawnableComponent))
class GEOMETRYCOLLECTIONENGINE_API UGeometryCollectionDebugDrawComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

public:

	/**
	* Singleton actor, containing the debug draw properties. Automatically populated at play time.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Settings")
	AGeometryCollectionDebugDrawActor* GeometryCollectionDebugDrawActor;

	/**
	* Level Set singleton actor, containing the Render properties. Automatically populated at play time.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Settings")
	AGeometryCollectionRenderLevelSetActor* GeometryCollectionRenderLevelSet;

	/**
	* Enable Level Set visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Level Set")
	bool bDebugDrawLevelSet;

	/**
	* Enable to visualize the selected level sets at the world origin.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Level Set")
	bool bRenderLevelSetAtOrigin;

	/**
	* Transform index of the level set to visualize.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Level Set", meta = (ClampMin="0"))
	int LevelSetIndex;

	/**
	* Enable transform visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	bool bDebugDrawTransform;

	/**
	* Enable transform indices visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	bool bDebugDrawTransformIndex;

	/**
	* Enable bounding boxes visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	bool bDebugDrawBoundingBox;

	/**
	* Color tint used for visualizing all geometry elements.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	FLinearColor GeometryColor;

	/**
	* Enable proximity visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Breaking")
	bool bDebugDrawProximity;

	/**
	* Enable breaking faces visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Breaking")
	bool bDebugDrawBreakingFace;

	/**
	* Enable breaking regions visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Breaking")
	bool bDebugDrawBreakingRegionData;

	/**
	* Color tint for the breaking visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Breaking")
	FLinearColor BreakingColor;

	/**
	* Enable face visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Face")
	bool bDebugDrawFace;

	/**
	* Enable face indices visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Face")
	bool bDebugDrawFaceIndex;

	/**
	* Enable face normals visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Face")
	bool bDebugDrawFaceNormal;

	/**
	* Enable single face visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Face")
	bool bDebugDrawSingleFace;

	/**
	* Index of the single face to visualize.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Face", meta = (ClampMin="0"))
	int32 SingleFaceIdx;

	/**
	* Color tint used for visualizing all faces elements.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Face")
	FLinearColor FaceColor;

	/**
	* Enable vertex visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Vertex")
	bool bDebugDrawVertex;

	/**
	* Enable vertex indices visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Vertex")
	bool bDebugDrawVertexIndex;

	/**
	* Enable vertex normals visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Vertex")
	bool bDebugDrawVertexNormal;

	/**
	* Color tint used for visualizing all vertex elements.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Vertex")
	FLinearColor VertexColor;

	UGeometryCollectionComponent* GeometryCollectionComponent;  // the component we are debug rendering for, set by the GeometryCollectionActor after creation

	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type ReasonEnd) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	
#if WITH_EDITOR
	/**
	* Property changed callback. Used to clamp the level set and single face index properties.
	*/
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void DebugDrawLevelSetBeginPlay();
	void DebugDrawLevelSetEndPlay();
	void DebugDrawLevelSetResetVisiblity();
	void DebugDrawLevelSetTick();

	void DebugDrawBeginPlay();
	void DebugDrawTick();

private:
	bool bLevelSetTextureDirty;
	int LevelSetTextureTransformIndex;
	TSharedPtr<TManagedArray<bool>> BaseVisibilityArray;
};
