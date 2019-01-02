// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "GeometryCollectionDebugDrawActor.generated.h"

class FGeometryCollection;

/**
* AGeometryCollectionDebugDrawActor
*   An actor representing the collection of data necessary to 
*   visualize geometry collections' debug informations.
*   Only one actor is to be used in the world, and should be
*   automatically spawned by the GeometryDebugDrawComponent.
*/
UCLASS()
class GEOMETRYCOLLECTIONENGINE_API AGeometryCollectionDebugDrawActor : public AActor
{
	GENERATED_UCLASS_BODY()
public:

	/**
	* Thickness of points when visualizing vertices.
	*/
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin="0"))
	float PointThickness;

	/**
	* Thickness of lines when visualizing faces, normals, ...etc.
	*/
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin="0"))
	float LineThickness;

	/**
	* Draw text shadows when visualizing indices.
	*/
	UPROPERTY(EditAnywhere, Category = "Settings")
	bool bTextShadow;

	/**
	* Scale of font used in visualizing indices.
	*/
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin="0.0001"))
	float TextScale;

	/**
	* Scale factor used for visualizing normals.
	*/
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin="0.0001"))
	float NormalScale;

	/**
	* Scale factor used for visualizing transforms.
	*/
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin="0.0001"))
	float TransformScale;

	/**
	* Size of arrows used for visualizing normals, breaking information, ...etc.
	*/
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin="0.0001"))
	float ArrowScale;

	/**
	* Game tick callback. This tick function is required to clean up the persistent debug lines.
	*/
	virtual void Tick(float DeltaSeconds) override;

	/**
	* Actor destruction callback. Used here to clear up the command callbacks.
	*/
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	/**
	* Property changed callback. Required to synchronize the command variables to this Actor's properties.
	*/
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/**
	* Draw vertices.
	*/
	void DrawVertices(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color);

	/**
	* Draw vertex indices.
	*/
	void DrawVertexIndices(FGeometryCollection* Collection, AActor* Actor, const FColor& Color);

	/**
	* Draw vertex normals.
	*/
	void DrawVertexNormals(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color);

	/**
	* Draw faces.
	*/
	void DrawFaces(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color);

	/**
	* Draw face indices.
	*/
	void DrawFaceIndices(FGeometryCollection* Collection, AActor* Actor, const FColor& Color);

	/**
	* Draw face normals.
	*/
	void DrawFaceNormals(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color);

	/**
	* Draw single face.
	*/
	void DrawSingleFace(FGeometryCollection* Collection, const AActor* Actor, const int32 FaceIndex, const FColor& Color);

	/**
	* Draw transforms.
	*/
	void DrawTransforms(FGeometryCollection* Collection, const AActor* Actor);

	/**
	* Draw transform indices.
	*/
	void DrawTransformIndices(FGeometryCollection* Collection, AActor* Actor, const FColor& Color);

	/**
	* Draw bounding boxes.
	*/
	void DrawBoundingBoxes(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color);

	/**
	* Draw proximity.
	*/
	void DrawProximity(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color);

	/**
	* Draw breaking faces.
	*/
	void DrawBreakingFaces(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color);

	/**
	* Draw breaking region data.
	*/
	void DrawBreakingRegionData(FGeometryCollection* Collection, const AActor* Actor, const FColor& Color);

	/**
	* Clear all persistent strings and debug lines.
	*/
	void Flush();

private:
	/**
	* Console variable float callback. Allows float console variables to update this actor's float properties.
	*/
	void OnFloatPropertyChange(IConsoleVariable* CVar);

	/**
	* Console variable bool callback. Allows int console variables to update this actor's bool properties.
	*/
	void OnBoolPropertyChange(IConsoleVariable* CVar);
};
