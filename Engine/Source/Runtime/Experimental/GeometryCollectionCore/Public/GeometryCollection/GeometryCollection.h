// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TransformCollection.h"
#include "Misc/Crc.h"

/**
* FGeometryCollection (FTransformCollection)
*
* Stores the TArray<T> groups necessary to process simulation geometry. 
* For example:
*
	int32 NumVertices = 100;
	int32 NumParticles = 200;
	FGeometryCollection GeometryCollection;

	// Build Geometry Vertex Information
	GeometryCollection.AddAttribute<FVector>("Vertex", FGeometryCollection::VerticesGroup);
	int VerticesIndex = GeometryCollection.AddElements(NumVertices, FGeometryCollection::VerticesGroup);
	check(NumVertices == GeometryCollection.NumElements(FGeometryCollection::VerticesGroup));

	TSharedRef< TManagedArray<FVector> > VerticesArray = GeometryCollection.GetAttribute<FVector>("Vertex", FGeometryCollection::VerticesGroup);
	TManagedArray<FVector>& Vertices = *VerticesArray;
	for (int32 Index = VerticesIndex; Index < NumVertices; Index++)
	{
		//Vertices[Index] = Something ...
	}

	// Build a generic particle array
	GeometryCollection.AddAttribute<FTransform>("Transform", FGeometryCollection::TransformGroup);
	GeometryCollection.AddAttribute<FVector>("Velocity", FGeometryCollection::TransformGroup);
	int ParticleIndex = GeometryCollection.AddElements(NumParticles, FGeometryCollection::TransformGroup);
	check(NumParticles == GeometryCollection.NumElements(FGeometryCollection::TransformGroup));

	TSharedRef< TManagedArray<FTransform> > TransformArray = GeometryCollection.GetAttribute<FTransform>("Transform", FGeometryCollection::TransformGroup);
	TSharedRef< TManagedArray<FTransform> > VelocityArray = GeometryCollection.GetAttribute<FTransform>("Velocity", FGeometryCollection::TransformGroup);

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
* @see FGeometryCollectionComponent
*/
class GEOMETRYCOLLECTIONCORE_API FGeometryCollection : public FTransformCollection
{

public:
	FGeometryCollection();
	~FGeometryCollection() {};
	FGeometryCollection( FGeometryCollection & );

	typedef FTransformCollection Super;
		/***
		*  Attribute Groups
		*
		*   These attribute groups are predefined data member of the FGeometryCollection.
		*
		*   VerticesGroup ("Vertices")
		*
		*			FVectorArray      Vertex         = GetAttribute<FVector>("Vertex", VerticesGroup)
		*			FInt32Array       BoneMap        = GetAttribute<Int32>("BoneMap", VerticesGroup, {"Transform"})
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
		*	FacesGroup ("Faces")
		*		Default Attributes :
		*
		*            FIntVectorArray   Indices       = GetAttribute<FIntVector>("Indices", FacesGroup, {"Faces"})
		*            FBoolArray        Visible       = GetAttribute<bool>("Visible", FacesGroup)
		*            FInt32Array       MaterialIndex = GetAttribute<Int32>("MaterialIndex", FacesGroup)
		*            FInt32Array       MaterialID    = GetAttribute<Int32>("MaterialID", FacesGroup)
		*
		*       The FacesGroup will store the triangulated face data, and any other information
		*       that is associated with the faces of the geometry. The "Triangle" attribute is
		*       stored as Vector<int,3>, and represents the vertices of a individual triangle.
		*
		*	GeometryGroup ("Geometry")
		*		Default Attributes :
		*
		*			FInt32Array       TransformIndex = GetAttribute<Int32>("TransformIndex", GeometryGroup, {"Transform"})
		*			FBoxArray		  BoundingBox = GetAttribute<FBox>("BoundingBox", GeometryGroup)
		*			FIntArray		  FaceStart = GetAttribute<int32>("FaceStart", GeometryGroup)
		*			FIntArray		  FaceCount = GetAttribute<int32>("FaceCount", GeometryGroup)
		*			FIntArray		  VertexStart = GetAttribute<int32>("VertexStart", GeometryGroup)
		*			FIntArray		  VertexCount = GetAttribute<int32>("VertexCount", GeometryGroup)
		*
		*       The GeometryGroup will store the transform indices, bounding boxes and any other information
		*       that is associated with the geometry.
		*
		*	MaterialGroup ("Material")
		*		Default Attributes	:
		*
		*			FGeometryCollectionSection	Sections = GetAttribute<FGeometryCollectionSection>("Sections", MaterialGroup)
		*
		*		 The set of triangles which are rendered with the same material
		*/
	static const FName VerticesGroup; // Vertices
	static const FName FacesGroup;	  // Faces
	static const FName GeometryGroup; // Geometry
	static const FName BreakingGroup; // Breaking
	static const FName MaterialGroup; // Materials

	/** Append a single geometric object to a FGeometryCollection */
	int32 AppendGeometry(const FGeometryCollection & GeometryCollection);


	/**
	* Remove Geometry and update dependent elements
	*/
	virtual void RemoveElements(const FName & Group, const TArray<int32>& SortedDeletionList) override;

	/**
	* Remove Geometry elements i.e. verts, faces, etc, leaving the transform nodes intact
	*/
	void RemoveGeometryElements(const TArray<int32>& SortedDeletionList);

	/**
	* Reindex sections to keep polys with same materials together to reduce the number of draw calls
	*/
	void ReindexMaterials();

	/**
	* Connect the Geometry Collection to the users arrays.
	*/
	virtual void BindSharedArrays() override;
		
	/** Returns true if there is anything to render */
	bool HasVisibleGeometry();

	/** Returns true if the vertices are contiguous*/
	bool HasContiguousVertices() const;

	/** Returns true if the faces are contiguous*/
	bool HasContiguousFaces() const;

	/** Returns true if the render faces are contiguous*/
	bool HasContiguousRenderFaces() const;

	/**
	* Setup collection based on input collection, resulting arrays are shared.
	* @param CollectionIn : Input collection to share
	*/
	virtual void Initialize(FManagedArrayCollection & CollectionIn) override;

	/**/
	void UpdateBoundingBox();

	/* Builds the connectivity data in the GeometryGroup (Proximity array) */
//	void UpdateProximity();

	/** Serialize */
	void Serialize(FArchive& Ar);

	/*
	*  
	*/
	void WriteDataToHeaderFile(const FString &Name, const FString &Path);

	/*
	*  
	*/
	void WriteDataToOBJFile(const FString &Name, const FString &Path, const bool WriteTopology=true, const bool WriteAuxStructures=true);

	/**
	 * Create a GeometryCollection from Vertex and Indices arrays
	 */
	static FGeometryCollection* NewGeometryCollection(const TArray<float>& RawVertexArray, const TArray<int32>& RawIndicesArray, bool ReverseVertexOrder = true);

	/**
	* Create a GeometryCollection from Vertex, Indices, BoneMap, Transform, BoneHierarchy arrays
	*/
	static FGeometryCollection* NewGeometryCollection(const TArray<float>& RawVertexArray,
													  const TArray<int32>& RawIndicesArray,
													  const TArray<int32>& RawBoneMapArray,
													  const TArray<FTransform>& RawTransformArray,
													  const TManagedArray<FGeometryCollectionBoneNode>& RawBoneHierarchyArray);

	// Vertices Group
	TSharedPtr< TManagedArray<FVector> >      Vertex;
	TSharedPtr< TManagedArray<FVector2D> >    UV;
	TSharedPtr< TManagedArray<FLinearColor> > Color;
	TSharedPtr< TManagedArray<FVector> >      TangentU;
	TSharedPtr< TManagedArray<FVector> >      TangentV;
	TSharedPtr< TManagedArray<FVector> >      Normal;
	TSharedPtr< TManagedArray<int32> >        BoneMap;

	// Faces Group
	TSharedPtr< TManagedArray<FIntVector> >   Indices;
	TSharedPtr< TManagedArray<bool> >         Visible;
	TSharedPtr< TManagedArray<int32> >        MaterialIndex;
	TSharedPtr< TManagedArray<int32> >        MaterialID;

	// Geometry Group
	TSharedPtr< TManagedArray<int32> >        TransformIndex;
	TSharedPtr< TManagedArray<FBox> >		  BoundingBox;
	TSharedPtr< TManagedArray<float> >		  InnerRadius;
	TSharedPtr< TManagedArray<float> >		  OuterRadius;
	TSharedPtr< TManagedArray<int32> >		  VertexStart;
	TSharedPtr< TManagedArray<int32> >		  VertexCount;
	TSharedPtr< TManagedArray<int32> >		  FaceStart;
	TSharedPtr< TManagedArray<int32> >		  FaceCount;
	TSharedPtr< TManagedArray<TSet<int32>> >  Proximity;

	// Breaking Group
	TSharedPtr< TManagedArray<int32> >		  BreakingFaceIndex;
	TSharedPtr< TManagedArray<int32> >		  BreakingSourceTransformIndex;
	TSharedPtr< TManagedArray<int32> >		  BreakingTargetTransformIndex;
	TSharedPtr< TManagedArray<FVector> >	  BreakingRegionCentroid;
	TSharedPtr< TManagedArray<FVector> >	  BreakingRegionNormal;
	TSharedPtr< TManagedArray<float> >		  BreakingRegionRadius;

	// Material Group
	TSharedPtr< TManagedArray<FGeometryCollectionSection>> Sections;
	
protected:
	void Construct();
};


