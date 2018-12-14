// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Misc/Crc.h"

/**
* TransformCollection (ManagedArrayCollection)
*
* Stores the TArray<T> groups necessary to process transform hierarchies. 
*
* @see FTransformCollectionComponent
*/
class GEOMETRYCOLLECTIONCORE_API FTransformCollection : public FManagedArrayCollection
{
public:
	typedef FManagedArrayCollection Super;

	FTransformCollection();
	~FTransformCollection() {};
	FTransformCollection( FTransformCollection & );

		/***
		*  Attribute Groups
		*
		*   These attribute groups are predefined data member of the FTransformCollection.
		*
		*   TransformGroup ("Transform")
		*		Default Attributes :
		*
		*          FTransformArray Transform =  GetAttribute<FTransform>("Transform", TransformGroup)
		*		   FBoneNodeArray BoneHierarchy = GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", TransformGroup)
		*
		*       The TransformGroup defines transform information for each Vertex. All positional
		*       information stored within Vertices and Geometry groups should be relative to its
		*       TransformGroup Transform.
		*       The bone hierarchy describes the parent child relationship tree of the bone nodes as well as the level,
		*       which is the distance from the root node at level 0. Leaf nodes will have the highest level number.
		*/
	static const FName TransformGroup;

	/** Append a single geometric object to a FTransformCollection */
	int32 AppendTransform(const FTransformCollection & GeometryCollection);


	/*
	*
	*/
	void RelativeTransformation(const int32& Index, const FTransform& LocalOffset);


	/**
	* Remove elements from the transform collection
	*/
	virtual void RemoveElements(const FName & Group, const TArray<int32> & SortedDeletionList) override;


	/**  Connect the Geometry Collection to the users arrays.*/
	virtual void BindSharedArrays() override;
		
	/**
	* Setup collection based on input collection, resulting arrays are shared.
	* @param CollectionIn : Input collection to share
	*/
	virtual void Initialize(FManagedArrayCollection & CollectionIn) override;

	// Transform Group
	TSharedPtr< TManagedArray<FTransform> >   Transform;
	TSharedPtr< TManagedArray<FString> >      BoneName;
	TSharedPtr< TManagedArray<FGeometryCollectionBoneNode> >  BoneHierarchy;
	TSharedPtr< TManagedArray<FLinearColor> > BoneColor;


protected:

	/** Construct */
	void Construct();
};


