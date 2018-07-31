// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: UTransformCollection methods.
=============================================================================*/

#include "TransformCollection.h"

#if WITH_EDITOR
#include "EditorFramework/AssetImportData.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(UTransformCollectionLogging, NoLogging, All);
const FName UTransformCollection::TransformGroup = "Transform";

UTransformCollection::UTransformCollection(const FObjectInitializer& ObjectInitializer)
	: UManagedArrayCollection(ObjectInitializer)
	, Transform(new TManagedArray<FTransform>())
	, BoneName(new TManagedArray<FString>())
	, BoneHierarchy(new TManagedArray<FGeometryCollectionBoneNode>())
{
	check(ObjectInitializer.GetClass() == GetClass());
	if (UTransformCollection * CollectionAsset = static_cast<UTransformCollection*>(ObjectInitializer.GetObj()))
	{
		Transform = CollectionAsset->Transform;
		BoneName = CollectionAsset->BoneName;
		BoneHierarchy = CollectionAsset->BoneHierarchy;
	}

	// Hierarchy Group
	AddAttribute<FTransform>("Transform", UTransformCollection::TransformGroup, Transform);
	AddAttribute<FString>("BoneName", UTransformCollection::TransformGroup, BoneName);
	AddAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", UTransformCollection::TransformGroup, BoneHierarchy);
}

int32 UTransformCollection::AppendTransform(const UTransformCollection & Element)
{
	check(Element.NumElements(UTransformCollection::TransformGroup) == 1);
	const TManagedArray<FTransform>& ElementTransform = *Element.Transform;
	const TManagedArray<FString>& ElementBoneName = *Element.BoneName;
	const TManagedArray<FGeometryCollectionBoneNode>& ElementBoneHierarchy = *Element.BoneHierarchy;


	// we are adding just one new piece of geometry, @todo - add general append support ?
	int ParticleIndex = AddElements(1, UTransformCollection::TransformGroup);
	TManagedArray<FTransform>& Transforms = *Transform;
	Transforms[ParticleIndex] = ElementTransform[0];
	TManagedArray<FString>& BoneNames = *BoneName;
	BoneNames[ParticleIndex] = ElementBoneName[0];
	TManagedArray<FGeometryCollectionBoneNode> & BoneHierarchys = *BoneHierarchy;
	BoneHierarchys[ParticleIndex] = ElementBoneHierarchy[0];
	return NumElements(UTransformCollection::TransformGroup) - 1;
}

void  UTransformCollection::Initialize(UManagedArrayCollection & CollectionIn)
{
	Super::Initialize(CollectionIn);
	BindSharedArrays();
}

void  UTransformCollection::BindSharedArrays()
{
	Super::BindSharedArrays();

	Transform = ShareAttribute<FTransform>("Transform", TransformGroup);
	BoneName = ShareAttribute<FString>("BoneName", TransformGroup);
	BoneHierarchy = ShareAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", TransformGroup);
}

