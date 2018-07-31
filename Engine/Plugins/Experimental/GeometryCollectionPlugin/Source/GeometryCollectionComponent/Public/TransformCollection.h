// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "Engine/EngineTypes.h"
#include "ManagedArrayCollection.h"
#include "Misc/Crc.h"

#include "TransformCollection.generated.h"

/**
* TransformCollection (UObject)
*
* Stores the TArray<T> groups necessary to process transform hierarchies. 
*
* @see UTransformCollectionComponent
*/
UCLASS(customconstructor)
class GEOMETRYCOLLECTIONCOMPONENT_API UTransformCollection
	: public UManagedArrayCollection
{
	GENERATED_UCLASS_BODY()

public:

	UTransformCollection(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());


		/***
		*  Attribute Groups
		*
		*   These attribute groups are predefined data member of the UTransformCollection.
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

	/** Append a single geometric object to a UTransformCollection */
	int32 AppendTransform(const UTransformCollection & GeometryCollection);

	/**  Connect the Geometry Collection to the users arrays.*/
	virtual void BindSharedArrays() override;
		
	/**
	* Setup collection based on input collection, resulting arrays are shared.
	* @param CollectionIn : Input collection to share
	*/
	virtual void Initialize(UManagedArrayCollection & CollectionIn) override;

	// Transform Group
	TSharedPtr< TManagedArray<FTransform> >   Transform;
	TSharedPtr< TManagedArray<FString> >      BoneName;
	TSharedPtr< TManagedArray<FGeometryCollectionBoneNode> >  BoneHierarchy;

};


