// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionExampleProximity.h"

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/TransformCollection.h"
#include "GeometryCollection/GeometryCollectionProximityUtility.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "../Resource/FracturedGeometry.h"

namespace GeometryCollectionExample
{
	template<class T>
	bool BuildProximity(ExampleResponse&& R)
	{
		TSharedPtr<FGeometryCollection> Collection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.f, 0.f, 0.f)), FVector(1.0));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.5f, 0.f, 1.f)),FVector(1.0)));

		//  0
		//  ...1
		//  ......2
		(*Collection->BoneHierarchy)[0].Parent = -1;
		(*Collection->BoneHierarchy)[0].Children.Add(1);
		(*Collection->BoneHierarchy)[1].Parent = 0;
		(*Collection->BoneHierarchy)[1].Children.Add(2);
		(*Collection->BoneHierarchy)[2].Parent = 1;
//		GeometryCollectionAlgo::ParentTransform(Collection, 0, 1);

		FGeometryCollection* Coll = Collection.Get();

		TArray<FTransform> GlobalTransform;
		GeometryCollectionAlgo::GlobalMatrices(Coll, GlobalTransform);

		FGeometryCollectionProximityUtility::UpdateProximity(Coll);

		R.ExpectTrue((*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue((*Coll->Proximity)[2].Contains(1));

		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(0));

		return !R.HasError();
	}
	template bool BuildProximity<float>(ExampleResponse&& R);

	template<class T>
	bool GeometryDeleteFromStart(ExampleResponse&& R)
	{
		TSharedPtr<FGeometryCollection> Collection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.f, 0.f, 0.f)), FVector(1.0));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(2.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(-0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.5f, 0.f, 1.f)),FVector(1.0)));

		//  0
		//  ...1
		//  ......2
		(*Collection->BoneHierarchy)[0].Parent = -1;
		(*Collection->BoneHierarchy)[0].Children.Add(1);

		(*Collection->BoneHierarchy)[1].Parent = 0;
		(*Collection->BoneHierarchy)[1].Children.Add(2);

		(*Collection->BoneHierarchy)[2].Parent = 0;
		(*Collection->BoneHierarchy)[2].Children.Add(3);

		(*Collection->BoneHierarchy)[3].Parent = 0;
		(*Collection->BoneHierarchy)[3].Children.Add(4);

		(*Collection->BoneHierarchy)[4].Parent = 0;
		(*Collection->BoneHierarchy)[4].Children.Add(5);

		(*Collection->BoneHierarchy)[5].Parent = 0;
		//		GeometryCollectionAlgo::ParentTransform(Collection, 0, 1);

		FGeometryCollection* Coll = Collection.Get();

		TArray<FTransform> GlobalTransform;
		GeometryCollectionAlgo::GlobalMatrices(Coll, GlobalTransform);

		FGeometryCollectionProximityUtility::UpdateProximity(Coll);

		// Proximity = [(3,4,1), (0,4,5,2), (1,5), (0,4), (0,1,3,5), (1,2,4)]

		R.ExpectTrue((*Coll->Proximity)[0].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(5));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(3));

		R.ExpectTrue((*Coll->Proximity)[2].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[2].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[3].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[3].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[4].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[5].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(2));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(5));

		TArray<int32> DelList = { 0 };
		Coll->RemoveElements(FGeometryCollection::GeometryGroup, DelList);

		// Proximity = [(3,4,1), (0,4), (3), (0,2,4), (0,1,3)]

		R.ExpectTrue((*Coll->Proximity)[0].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[2].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[3].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[3].Contains(2));
		R.ExpectTrue((*Coll->Proximity)[3].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[4].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(5));

		R.ExpectTrue(Collection->NumElements(FGeometryCollection::GeometryGroup) == 5);

		return !R.HasError();
	}
	template bool GeometryDeleteFromStart<float>(ExampleResponse&& R);



	template<class T>
	bool GeometryDeleteFromEnd(ExampleResponse&& R)
	{
		TSharedPtr<FGeometryCollection> Collection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.f, 0.f, 0.f)), FVector(1.0));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(2.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(-0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.5f, 0.f, 1.f)),FVector(1.0)));

		//  0
		//  ...1
		//  ......2
		(*Collection->BoneHierarchy)[0].Parent = -1;
		(*Collection->BoneHierarchy)[0].Children.Add(1);

		(*Collection->BoneHierarchy)[1].Parent = 0;
		(*Collection->BoneHierarchy)[1].Children.Add(2);

		(*Collection->BoneHierarchy)[2].Parent = 0;
		(*Collection->BoneHierarchy)[2].Children.Add(3);

		(*Collection->BoneHierarchy)[3].Parent = 0;
		(*Collection->BoneHierarchy)[3].Children.Add(4);

		(*Collection->BoneHierarchy)[4].Parent = 0;
		(*Collection->BoneHierarchy)[4].Children.Add(5);

		(*Collection->BoneHierarchy)[5].Parent = 0;
		//		GeometryCollectionAlgo::ParentTransform(Collection, 0, 1);

		FGeometryCollection* Coll = Collection.Get();

		TArray<FTransform> GlobalTransform;
		GeometryCollectionAlgo::GlobalMatrices(Coll, GlobalTransform);

		FGeometryCollectionProximityUtility::UpdateProximity(Coll);

		// Proximity = [(3,4,1), (0,4,5,2), (1,5), (0,4), (0,1,3,5), (1,2,4)]

		R.ExpectTrue((*Coll->Proximity)[0].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(5));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(3));

		R.ExpectTrue((*Coll->Proximity)[2].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[2].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[3].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[3].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[4].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[5].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(2));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(5));

		TArray<int32> DelList = { 5 };
		Coll->RemoveElements(FGeometryCollection::GeometryGroup, DelList);

		// Proximity = [(3,4,1), (0,4,2), (1), (0,4), (0,1,3)]

		R.ExpectTrue((*Coll->Proximity)[0].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[2].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[3].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[3].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[4].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(5));

		R.ExpectTrue(Collection->NumElements(FGeometryCollection::GeometryGroup) == 5);

		return !R.HasError();
	}
	template bool GeometryDeleteFromEnd<float>(ExampleResponse&& R);

	template<class T>
	bool GeometryDeleteFromMiddle(ExampleResponse&& R)
	{
		TSharedPtr<FGeometryCollection> Collection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.f, 0.f, 0.f)), FVector(1.0));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(2.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(-0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.5f, 0.f, 1.f)),FVector(1.0)));

		//  0
		//  ...1
		//  ......2
		(*Collection->BoneHierarchy)[0].Parent = -1;
		(*Collection->BoneHierarchy)[0].Children.Add(1);

		(*Collection->BoneHierarchy)[1].Parent = 0;
		(*Collection->BoneHierarchy)[1].Children.Add(2);

		(*Collection->BoneHierarchy)[2].Parent = 0;
		(*Collection->BoneHierarchy)[2].Children.Add(3);

		(*Collection->BoneHierarchy)[3].Parent = 0;
		(*Collection->BoneHierarchy)[3].Children.Add(4);

		(*Collection->BoneHierarchy)[4].Parent = 0;
		(*Collection->BoneHierarchy)[4].Children.Add(5);

		(*Collection->BoneHierarchy)[5].Parent = 0;
		//		GeometryCollectionAlgo::ParentTransform(Collection, 0, 1);

		FGeometryCollection* Coll = Collection.Get();

		TArray<FTransform> GlobalTransform;
		GeometryCollectionAlgo::GlobalMatrices(Coll, GlobalTransform);

		FGeometryCollectionProximityUtility::UpdateProximity(Coll);

		// Proximity = [(3,4,1), (0,4,5,2), (1,5), (0,4), (0,1,3,5), (1,2,4)]

		R.ExpectTrue((*Coll->Proximity)[0].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(5));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(3));

		R.ExpectTrue((*Coll->Proximity)[2].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[2].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[3].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[3].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[4].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[5].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(2));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(5));

		TArray<int32> DelList = { 3 };
		Coll->RemoveElements(FGeometryCollection::GeometryGroup, DelList);

		// Proximity = [(3,1), (0,3,4,2), (1,4), (0,1,4), (1,2,3)]

		R.ExpectTrue((*Coll->Proximity)[0].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[2].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[2].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[3].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[3].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[3].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[4].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(2));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(5));

		R.ExpectTrue(Collection->NumElements(FGeometryCollection::GeometryGroup) == 5);

		return !R.HasError();
	}
	template bool GeometryDeleteFromMiddle<float>(ExampleResponse&& R);

	template<class T>
	bool GeometryDeleteMultipleFromMiddle(ExampleResponse&& R)
	{
		TSharedPtr<FGeometryCollection> Collection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.f, 0.f, 0.f)), FVector(1.0));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(2.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(-0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.5f, 0.f, 1.f)),FVector(1.0)));

		//  0
		//  ...1
		//  ......2
		(*Collection->BoneHierarchy)[0].Parent = -1;
		(*Collection->BoneHierarchy)[0].Children.Add(1);

		(*Collection->BoneHierarchy)[1].Parent = 0;
		(*Collection->BoneHierarchy)[1].Children.Add(2);

		(*Collection->BoneHierarchy)[2].Parent = 0;
		(*Collection->BoneHierarchy)[2].Children.Add(3);

		(*Collection->BoneHierarchy)[3].Parent = 0;
		(*Collection->BoneHierarchy)[3].Children.Add(4);

		(*Collection->BoneHierarchy)[4].Parent = 0;
		(*Collection->BoneHierarchy)[4].Children.Add(5);

		(*Collection->BoneHierarchy)[5].Parent = 0;
		//		GeometryCollectionAlgo::ParentTransform(Collection, 0, 1);

		FGeometryCollection* Coll = Collection.Get();

		TArray<FTransform> GlobalTransform;
		GeometryCollectionAlgo::GlobalMatrices(Coll, GlobalTransform);

		FGeometryCollectionProximityUtility::UpdateProximity(Coll);

		// Proximity = [(3,4,1), (0,4,5,2), (1,5), (0,4), (0,1,3,5), (1,2,4)]

		R.ExpectTrue((*Coll->Proximity)[0].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(5));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(3));

		R.ExpectTrue((*Coll->Proximity)[2].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[2].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[3].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[3].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[4].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[5].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(2));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(5));

		TArray<int32> DelList = { 2,3,4 };
		Coll->RemoveElements(FGeometryCollection::GeometryGroup, DelList);

		// Proximity = [(1), (0,2), (1)]

		R.ExpectTrue((*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[2].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(5));

		R.ExpectTrue(Collection->NumElements(FGeometryCollection::GeometryGroup) == 3);

		return !R.HasError();
	}
	template bool GeometryDeleteMultipleFromMiddle<float>(ExampleResponse&& R);

	template<class T>
	bool GeometryDeleteRandom(ExampleResponse&& R)
	{
		TSharedPtr<FGeometryCollection> Collection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.f, 0.f, 0.f)), FVector(1.0));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(2.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(-0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.5f, 0.f, 1.f)),FVector(1.0)));

		//  0
		//  ...1
		//  ......2
		(*Collection->BoneHierarchy)[0].Parent = -1;
		(*Collection->BoneHierarchy)[0].Children.Add(1);

		(*Collection->BoneHierarchy)[1].Parent = 0;
		(*Collection->BoneHierarchy)[1].Children.Add(2);

		(*Collection->BoneHierarchy)[2].Parent = 0;
		(*Collection->BoneHierarchy)[2].Children.Add(3);

		(*Collection->BoneHierarchy)[3].Parent = 0;
		(*Collection->BoneHierarchy)[3].Children.Add(4);

		(*Collection->BoneHierarchy)[4].Parent = 0;
		(*Collection->BoneHierarchy)[4].Children.Add(5);

		(*Collection->BoneHierarchy)[5].Parent = 0;
		//		GeometryCollectionAlgo::ParentTransform(Collection, 0, 1);

		FGeometryCollection* Coll = Collection.Get();

		TArray<FTransform> GlobalTransform;
		GeometryCollectionAlgo::GlobalMatrices(Coll, GlobalTransform);

		FGeometryCollectionProximityUtility::UpdateProximity(Coll);

		// Proximity = [(3,4,1), (0,4,5,2), (1,5), (0,4), (0,1,3,5), (1,2,4)]

		R.ExpectTrue((*Coll->Proximity)[0].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(5));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(3));

		R.ExpectTrue((*Coll->Proximity)[2].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[2].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[3].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[3].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[4].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[5].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(2));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(5));

		TArray<int32> DelList = { 1,3,5 };
		Coll->RemoveElements(FGeometryCollection::GeometryGroup, DelList);

		// Proximity = [(2), (), (0)]

		R.ExpectTrue((*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(5));

		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[2].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(5));

		R.ExpectTrue(Collection->NumElements(FGeometryCollection::GeometryGroup) == 3);

		return !R.HasError();
	}
	template bool GeometryDeleteRandom<float>(ExampleResponse&& R); 

	template<class T>
	bool GeometryDeleteRandom2(ExampleResponse&& R)
	{
		TSharedPtr<FGeometryCollection> Collection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.f, 0.f, 0.f)), FVector(1.0));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(2.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(-0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.5f, 0.f, 1.f)),FVector(1.0)));

		//  0
		//  ...1
		//  ......2
		(*Collection->BoneHierarchy)[0].Parent = -1;
		(*Collection->BoneHierarchy)[0].Children.Add(1);

		(*Collection->BoneHierarchy)[1].Parent = 0;
		(*Collection->BoneHierarchy)[1].Children.Add(2);

		(*Collection->BoneHierarchy)[2].Parent = 0;
		(*Collection->BoneHierarchy)[2].Children.Add(3);

		(*Collection->BoneHierarchy)[3].Parent = 0;
		(*Collection->BoneHierarchy)[3].Children.Add(4);

		(*Collection->BoneHierarchy)[4].Parent = 0;
		(*Collection->BoneHierarchy)[4].Children.Add(5);

		(*Collection->BoneHierarchy)[5].Parent = 0;
		//		GeometryCollectionAlgo::ParentTransform(Collection, 0, 1);

		FGeometryCollection* Coll = Collection.Get();

		TArray<FTransform> GlobalTransform;
		GeometryCollectionAlgo::GlobalMatrices(Coll, GlobalTransform);

		FGeometryCollectionProximityUtility::UpdateProximity(Coll);

		// Proximity = [(3,4,1), (0,4,5,2), (1,5), (0,4), (0,1,3,5), (1,2,4)]

		R.ExpectTrue((*Coll->Proximity)[0].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(5));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(3));

		R.ExpectTrue((*Coll->Proximity)[2].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[2].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[3].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[3].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[4].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[5].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(2));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(5));

		TArray<int32> DelList = { 0,1,4,5 };
		Coll->RemoveElements(FGeometryCollection::GeometryGroup, DelList);

		// Proximity = [(), ()]

		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(5));

		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(5));

		R.ExpectTrue(Collection->NumElements(FGeometryCollection::GeometryGroup) == 2);

		return !R.HasError();
	}
	template bool GeometryDeleteRandom2<float>(ExampleResponse&& R);

	template<class T>
	bool GeometryDeleteAll(ExampleResponse&& R)
	{
		TSharedPtr<FGeometryCollection> Collection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.f, 0.f, 0.f)), FVector(1.0));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(2.f, 0.f, 0.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(-0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.5f, 0.f, 1.f)),FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.5f, 0.f, 1.f)),FVector(1.0)));

		//  0
		//  ...1
		//  ......2
		(*Collection->BoneHierarchy)[0].Parent = -1;
		(*Collection->BoneHierarchy)[0].Children.Add(1);

		(*Collection->BoneHierarchy)[1].Parent = 0;
		(*Collection->BoneHierarchy)[1].Children.Add(2);

		(*Collection->BoneHierarchy)[2].Parent = 0;
		(*Collection->BoneHierarchy)[2].Children.Add(3);

		(*Collection->BoneHierarchy)[3].Parent = 0;
		(*Collection->BoneHierarchy)[3].Children.Add(4);

		(*Collection->BoneHierarchy)[4].Parent = 0;
		(*Collection->BoneHierarchy)[4].Children.Add(5);

		(*Collection->BoneHierarchy)[5].Parent = 0;
		//		GeometryCollectionAlgo::ParentTransform(Collection, 0, 1);

		FGeometryCollection* Coll = Collection.Get();

		TArray<FTransform> GlobalTransform;
		GeometryCollectionAlgo::GlobalMatrices(Coll, GlobalTransform);

		FGeometryCollectionProximityUtility::UpdateProximity(Coll);

		// Proximity = [(3,4,1), (0,4,5,2), (1,5), (0,4), (0,1,3,5), (1,2,4)]

		R.ExpectTrue((*Coll->Proximity)[0].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[0].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[0].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[1].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(4));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(5));
		R.ExpectTrue((*Coll->Proximity)[1].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[1].Contains(3));

		R.ExpectTrue((*Coll->Proximity)[2].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[2].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[2].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[3].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[3].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(1));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[3].Contains(5));

		R.ExpectTrue((*Coll->Proximity)[4].Contains(0));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(3));
		R.ExpectTrue((*Coll->Proximity)[4].Contains(5));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(2));
		R.ExpectTrue(!(*Coll->Proximity)[4].Contains(4));

		R.ExpectTrue((*Coll->Proximity)[5].Contains(1));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(2));
		R.ExpectTrue((*Coll->Proximity)[5].Contains(4));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(0));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(3));
		R.ExpectTrue(!(*Coll->Proximity)[5].Contains(5));

		TArray<int32> DelList = { 0,1,2,3,4,5 };
		Coll->RemoveElements(FGeometryCollection::GeometryGroup, DelList);

		// Proximity = []

		R.ExpectTrue(Collection->NumElements(FGeometryCollection::GeometryGroup) == 0);

		return !R.HasError();
	}
	template bool GeometryDeleteAll<float>(ExampleResponse&& R);

	
	template<class T>
	bool TestFracturedGeometry(ExampleResponse&& R)
	{
		FGeometryCollection* TestCollection = FGeometryCollection::NewGeometryCollection(FracturedGeometry::RawVertexArray,
																						 FracturedGeometry::RawIndicesArray,
																						 FracturedGeometry::RawBoneMapArray,
																						 FracturedGeometry::RawTransformArray,
																						 FracturedGeometry::RawBoneHierarchyArray);

		R.ExpectTrue(TestCollection->NumElements(FGeometryCollection::GeometryGroup) == 11);

		return !R.HasError();
	}
	template bool TestFracturedGeometry<float>(ExampleResponse&& R);


}

