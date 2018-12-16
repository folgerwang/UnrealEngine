// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionExampleClean.h"

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
	bool TestDeleteCoincidentVertices(ExampleResponse&& R)
	{
		TSharedPtr<FGeometryCollection> Collection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.f, 0.f, 0.f)), FVector(1.0));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(1.f, 0.f, 0.f)), FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f)), FVector(0.5f, 0.f, 1.f)), FVector(1.0)));

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

		GeometryCollectionAlgo::DeleteCoincidentVertices(Coll, 1e-2);

		
		R.ExpectTrue(Coll->NumElements(FGeometryCollection::VerticesGroup) == 24);
		R.ExpectTrue(Coll->NumElements(FGeometryCollection::FacesGroup) == 36);

		GeometryCollectionAlgo::DeleteZeroAreaFaces(Coll, 1e-4);

		R.ExpectTrue(Coll->NumElements(FGeometryCollection::VerticesGroup) == 24);
		R.ExpectTrue(Coll->NumElements(FGeometryCollection::FacesGroup) == 36);

		return !R.HasError();
	}
	template bool TestDeleteCoincidentVertices<float>(ExampleResponse&& R);

	

	template<class T>
	bool TestDeleteCoincidentVertices2(ExampleResponse&& R)
	{
		FGeometryCollection* Coll = FGeometryCollection::NewGeometryCollection(FracturedGeometry::RawVertexArray,
																   			   FracturedGeometry::RawIndicesArray,
																			   FracturedGeometry::RawBoneMapArray,
																			   FracturedGeometry::RawTransformArray,
																			   FracturedGeometry::RawBoneHierarchyArray);

		R.ExpectTrue(Coll->NumElements(FGeometryCollection::VerticesGroup) == 667);
		R.ExpectTrue(Coll->NumElements(FGeometryCollection::FacesGroup) == 493);

		GeometryCollectionAlgo::DeleteCoincidentVertices(Coll, 1e-2);

		R.ExpectTrue(Coll->NumElements(FGeometryCollection::VerticesGroup) == 270);
		R.ExpectTrue(Coll->NumElements(FGeometryCollection::FacesGroup) == 493);

		return !R.HasError();
	}
	template bool TestDeleteCoincidentVertices2<float>(ExampleResponse&& R);

	template<class T>
	bool TestDeleteZeroAreaFaces(ExampleResponse&& R)
	{
		FGeometryCollection* Coll = FGeometryCollection::NewGeometryCollection(FracturedGeometry::RawVertexArray,
																			   FracturedGeometry::RawIndicesArray,
																			   FracturedGeometry::RawBoneMapArray,
																			   FracturedGeometry::RawTransformArray,
																			   FracturedGeometry::RawBoneHierarchyArray);

		R.ExpectTrue(Coll->NumElements(FGeometryCollection::VerticesGroup) == 667);
		R.ExpectTrue(Coll->NumElements(FGeometryCollection::FacesGroup) == 493);

		GeometryCollectionAlgo::DeleteZeroAreaFaces(Coll, 1e-4);

		R.ExpectTrue(Coll->NumElements(FGeometryCollection::VerticesGroup) == 667);
		R.ExpectTrue(Coll->NumElements(FGeometryCollection::FacesGroup) == 493);

		return !R.HasError();
	}
	template bool TestDeleteZeroAreaFaces<float>(ExampleResponse&& R);

	template<class T>
	bool TestDeleteHiddenFaces(ExampleResponse&& R)
	{
		FGeometryCollection* Coll = FGeometryCollection::NewGeometryCollection(FracturedGeometry::RawVertexArray,
																			   FracturedGeometry::RawIndicesArray,
																			   FracturedGeometry::RawBoneMapArray,
																			   FracturedGeometry::RawTransformArray,
																			   FracturedGeometry::RawBoneHierarchyArray);

		R.ExpectTrue(Coll->NumElements(FGeometryCollection::VerticesGroup) == 667);
		R.ExpectTrue(Coll->NumElements(FGeometryCollection::FacesGroup) == 493);

		TManagedArray<bool>& VisibleArray = *Coll->Visible;

		int32 NumFaces = Coll->NumElements(FGeometryCollection::FacesGroup);
		for (int32 Idx = 0; Idx < NumFaces; ++Idx)
		{
			if (!(Idx % 5))
			{
				VisibleArray[Idx] = false;
			}
		}

		R.ExpectTrue(Coll->NumElements(FGeometryCollection::VerticesGroup) == 667);
		R.ExpectTrue(Coll->NumElements(FGeometryCollection::FacesGroup) == 493);

		GeometryCollectionAlgo::DeleteHiddenFaces(Coll);

		R.ExpectTrue( Coll->NumElements(FGeometryCollection::VerticesGroup) == 667);
		R.ExpectTrue(Coll->NumElements(FGeometryCollection::FacesGroup) == 404);

		return !R.HasError();
	}
	template bool TestDeleteHiddenFaces<float>(ExampleResponse&& R);
}

