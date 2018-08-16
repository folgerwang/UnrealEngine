// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "Engine/EngineTypes.h"
#include "TransformCollection.h"
#include "Misc/Crc.h"

#include "GeometryCollection.generated.h"

/**
* GeometryCollection (UObject)
*
* Stores the TArray<T> groups necessary to process simulation geometry. 
* For example:
*
	int32 NumVertices = 100;
	int32 NumParticles = 200;
	UGeometryCollection GeometryCollection;

	// Build Geometry Vertex Information
	GeometryCollection.AddAttribute<FVector>("Vertex", UGeometryCollection::VerticesGroup);
	int VerticesIndex = GeometryCollection.AddElements(NumVertices, UGeometryCollection::VerticesGroup);
	check(NumVertices == GeometryCollection.NumElements(UGeometryCollection::VerticesGroup));

	TSharedRef< TManagedArray<FVector> > VerticesArray = GeometryCollection.GetAttribute<FVector>("Vertex", UGeometryCollection::VerticesGroup);
	TManagedArray<FVector>& Vertices = *VerticesArray;
	for (int32 Index = VerticesIndex; Index < NumVertices; Index++)
	{
		//Vertices[Index] = Something ...
	}

	// Build a generic particle array
	GeometryCollection.AddAttribute<FTransform>("Transform", UGeometryCollection::TransformGroup);
	GeometryCollection.AddAttribute<FVector>("Velocity", UGeometryCollection::TransformGroup);
	int ParticleIndex = GeometryCollection.AddElements(NumParticles, UGeometryCollection::TransformGroup);
	check(NumParticles == GeometryCollection.NumElements(UGeometryCollection::TransformGroup));

	TSharedRef< TManagedArray<FTransform> > TransformArray = GeometryCollection.GetAttribute<FTransform>("Transform", UGeometryCollection::TransformGroup);
	TSharedRef< TManagedArray<FTransform> > VelocityArray = GeometryCollection.GetAttribute<FTransform>("Velocity", UGeometryCollection::TransformGroup);

	TManagedArray<FTransform>& Transform = *TransformArray;
	TManagedArray<FTransform>& Velocity = *VelocityArray;
	for (int32 Index = ParticleIndex; Index < NumParticles; Index++)
	{
		//Transform[Index] = Something...
	}
*
*
*
*
*
* @see UGeometryCollectionComponent
*/
UCLASS(customconstructor)
class GEOMETRYCOLLECTIONCOMPONENT_API UGeometryCollection
	: public UTransformCollection
{
	GENERATED_UCLASS_BODY()

public:

	UGeometryCollection(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());


		/***
		*  Attribute Groups
		*
		*   These attribute groups are predefined data member of the UGeometryCollection.
		*
		*   VerticesGroup ("Vertices")
		*
		*			FVectorArray      Vertex         = GetAttribute<FVector>("Vertex", VerticesGroup)
		*			FInt32Array       BoneMap        = GetAttribute<Int32>("BoneMap", VerticesGroup)
		*           FVectorArray      Normal         = GetAttribute<FVector>("Normal", MaterialGroup)
		*           FVector2DArray    UV             = GetAttribute<FVector2D>("UV", MaterialGroup)
		*           FVectorArray      TangentU       = GetAttribute<FVector>("TangentU", MaterialGroup)
		*           FVectorArray      TangentV       = GetAttribute<FVector>("TangentV", MaterialGroup)
		*           FLinearColorArray Color          = GetAttribute<FLinearColor>("Color", MaterialGroup)
		*
		*		The VerticesGroup will store per-vertex information about the geometry. For
		*       example, the "Position" attribute stores a FVector array for the relative
		*       offset of a vertex from the geometries geometric center, and the "BoneMap"
		*       attribute stores an index in to the TransformGroups Transform array so that
		*       the local space vertices may be mapped in to world space positions.
		*
		*
		*	GeometryGroup ("Geometry")
		*		Default Attributes :
		*
		*            FIntVectorArray   Indices  = GetAttribute<FIntVector>("Indices", GeometryGroup)
		*            FBoolArray        Visible  = GetAttribute<bool>("Visible", GeometryGroup)
		*
		*       The GeometryGroup will store the triangulated face data, and any other information
		*       that is associated with the faces of the geometry. The "Triangle" attribute is
		*       stored as Vector<int,3>, and represents the vertices of a individual triangle.
		*
		*/
	static const FName VerticesGroup; // Vertices
	static const FName GeometryGroup; // Geometry

	/** Append a single geometric object to a UGeometryCollection */
	int32 AppendGeometry(const UGeometryCollection & GeometryCollection);

	/**  Connect the Geometry Collection to the users arrays.*/
	virtual void BindSharedArrays() override;
		
	/** Returns true if there is anything to render */
	bool HasVisibleGeometry();

	/**
	* Setup collection based on input collection, resulting arrays are shared.
	* @param CollectionIn : Input collection to share
	*/
	virtual void Initialize(UManagedArrayCollection & CollectionIn) override;

	/** Serialize */
	void Serialize(FArchive& Ar);

	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, Category = Thumbnail)
	class UThumbnailInfo* ThumbnailInfo;

	// Vertices Group
	TSharedPtr< TManagedArray<FVector> >      Vertex;
	TSharedPtr< TManagedArray<FVector2D> >    UV;
	TSharedPtr< TManagedArray<FLinearColor> > Color;
	TSharedPtr< TManagedArray<FVector> >      TangentU;
	TSharedPtr< TManagedArray<FVector> >      TangentV;
	TSharedPtr< TManagedArray<FVector> >      Normal;
	TSharedPtr< TManagedArray<int32> >        BoneMap;

	// Geometry Group
	TSharedPtr< TManagedArray<FIntVector> >   Indices;
	TSharedPtr< TManagedArray<bool> >         Visible;

	/** The editable mesh representation of this geometry collection */
	class UObject* EditableMesh;
};


