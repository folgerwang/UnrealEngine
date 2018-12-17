// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionExampleMatrices.h"

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/TransformCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace GeometryCollectionExample
{
	DEFINE_LOG_CATEGORY_STATIC(GCTM_Log, Verbose, All);

	template<class T>
	bool BasicGlobalMatrices(ExampleResponse&& R)
	{ 
		TSharedPtr<FGeometryCollection> Collection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 90.)), FVector(0, 10, 0)), FVector(1.0));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 90.)), FVector(0, 10, 0)), FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 90.)), FVector(0, 10, 0)), FVector(1.0)));

		//  0
		//  ...1
		//  ......2
		(*Collection->BoneHierarchy)[0].Parent = -1;
		(*Collection->BoneHierarchy)[0].Children.Add(1);
		(*Collection->BoneHierarchy)[1].Parent = 0;
		(*Collection->BoneHierarchy)[1].Children.Add(2);
		(*Collection->BoneHierarchy)[2].Parent = 1;

		TArray<FTransform> GlobalTransform;
		GeometryCollectionAlgo::GlobalMatrices(Collection.Get(), GlobalTransform);

		FVector Rot0 = GlobalTransform[0].GetRotation().Euler();
		R.ExpectTrue(Rot0.Equals(FVector(0, 0, 90), 0.0001));

		FVector Rot1 = GlobalTransform[1].GetRotation().Euler();
		R.ExpectTrue(Rot1.Equals(FVector(0, 0, -180), 0.0001));

		FVector Rot2 = GlobalTransform[2].GetRotation().Euler();
		R.ExpectTrue(Rot2.Equals(FVector(0, 0, -90), 0.0001));

		FVector Pos0 = GlobalTransform[0].GetTranslation();
		R.ExpectTrue(Pos0.Equals(FVector(0, 10, 0), 0.0001));

		FVector Pos1 = GlobalTransform[1].GetTranslation();
		R.ExpectTrue(Pos1.Equals(FVector(-10, 10, 0), 0.0001));

		FVector Pos2 = GlobalTransform[2].GetTranslation();
		R.ExpectTrue(Pos2.Equals(FVector(-10, 0, 0), 0.0001));

		FTransform Frame = GeometryCollectionAlgo::GlobalMatrix(Collection.Get(), 2);
		R.ExpectTrue(Frame.GetRotation().Euler().Equals(FVector(0, 0, -90), 0.0001));
		R.ExpectTrue(Frame.GetTranslation().Equals(FVector(-10, 0, 0), 0.0001));

		Frame = GeometryCollectionAlgo::GlobalMatrix(Collection.Get(), 1);
		R.ExpectTrue(Frame.GetRotation().Euler().Equals(FVector(0, 0, -180), 0.0001));
		R.ExpectTrue(Frame.GetTranslation().Equals(FVector(-10, 10, 0), 0.0001));

		return !R.HasError();
	}
	template bool BasicGlobalMatrices<float>(ExampleResponse&& R);

	template<class T>
	bool ReparentingMatrices(ExampleResponse&& R)
	{
		TSharedPtr<FGeometryCollection> Collection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 90.)), FVector(0, 10, 0)), FVector(1.0));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 90.)), FVector(0, 10, 0)), FVector(1.0)));
		Collection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, -90.)), FVector(-10, 0, 0)), FVector(1.0)));

		//  0
		//  ...1
		//  2
		(*Collection->BoneHierarchy)[0].Parent = -1;
		(*Collection->BoneHierarchy)[0].Children.Add(1);
		(*Collection->BoneHierarchy)[1].Parent = 0;
		(*Collection->BoneHierarchy)[2].Parent = -1;

		//  0
		//  ...1
		//  ......2
		TArray<int32> Bones = { 2 };
		GeometryCollectionAlgo::ParentTransforms(Collection.Get(), 1, Bones);
		R.ExpectTrue((*Collection->Transform)[2].GetTranslation().Equals(FVector(0, 10, 0), 0.0001));
		R.ExpectTrue((*Collection->Transform)[2].GetRotation().Euler().Equals(FVector(0, 0, 90.), 0.0001));

		TArray<FTransform> GlobalTransform;
		GeometryCollectionAlgo::GlobalMatrices(Collection.Get(), GlobalTransform);

		FVector Rot0 = GlobalTransform[0].GetRotation().Euler();
		R.ExpectTrue(Rot0.Equals(FVector(0, 0, 90), 0.0001));

		FVector Rot1 = GlobalTransform[1].GetRotation().Euler();
		R.ExpectTrue(Rot1.Equals(FVector(0, 0, -180), 0.0001));

		FVector Rot2 = GlobalTransform[2].GetRotation().Euler();
		R.ExpectTrue(Rot2.Equals(FVector(0, 0, -90), 0.0001));

		FVector Pos0 = GlobalTransform[0].GetTranslation();
		R.ExpectTrue(Pos0.Equals(FVector(0, 10, 0), 0.0001));

		FVector Pos1 = GlobalTransform[1].GetTranslation();
		R.ExpectTrue(Pos1.Equals(FVector(-10, 10, 0), 0.0001));

		FVector Pos2 = GlobalTransform[2].GetTranslation();
		R.ExpectTrue(Pos2.Equals(FVector(-10, 0, 0), 0.0001));

		return !R.HasError();
	}
	template bool ReparentingMatrices<float>(ExampleResponse&& R);


	template<class T>
	bool TransformMatrixElement(ExampleResponse&& R)
	{
		FTransformCollection Collection;

		int index=0;
		for (int i=0; i < 4; i++)
		{
			index = Collection.AddElements(1, FGeometryCollection::TransformGroup);
			(*Collection.BoneHierarchy)[index].Parent = index - 1;
			(*Collection.BoneHierarchy)[index].Children.Add(index+1);
		}
		(*Collection.BoneHierarchy)[index].Children.Empty();

		(*Collection.Transform)[0] = FTransform(FQuat::MakeFromEuler(FVector(0, 0, 0)), FVector(0, 0, 0));
		(*Collection.Transform)[1] = FTransform(FQuat::MakeFromEuler(FVector(0, 0, 90.)), FVector(1, 0, 0));
		(*Collection.Transform)[2] = FTransform(FQuat::MakeFromEuler(FVector(0, 90.,0)), FVector(1, 0, 0));
		(*Collection.Transform)[3] = FTransform(FQuat::MakeFromEuler(FVector(0, 0, 0)), FVector(1, 0, 0));

		TArray<FTransform> GlobalMatrices0;
		GeometryCollectionAlgo::GlobalMatrices(&Collection, GlobalMatrices0);

		Collection.RelativeTransformation(1, FTransform(FQuat::MakeFromEuler(FVector(22, 90, 55)), FVector(17,11,13)));
		Collection.RelativeTransformation(2, FTransform(FQuat::MakeFromEuler(FVector(22, 90, 55)), FVector(17, 11, 13)));

		TArray<FTransform> GlobalMatrices1;
		GeometryCollectionAlgo::GlobalMatrices(&Collection, GlobalMatrices1);

		
		//for (int rdx = 0; rdx < GlobalMatrices0.Num(); rdx++)
		//{
		//	UE_LOG(GCTM_Log, Verbose, TEXT("... Position[%d] : (%3.5f,%3.5f,%3.5f)"), rdx, (*Collection.Transform)[rdx].GetTranslation().X, (*Collection.Transform)[rdx].GetTranslation().Y, (*Collection.Transform)[rdx].GetTranslation().Z);
		//	UE_LOG(GCTM_Log, Verbose, TEXT("... GlobalM0[%d] : (%3.5f,%3.5f,%3.5f)"), rdx, GlobalMatrices0[rdx].GetTranslation().X, GlobalMatrices0[rdx].GetTranslation().Y, GlobalMatrices0[rdx].GetTranslation().Z);
		//	UE_LOG(GCTM_Log, Verbose, TEXT("... GlobalM1[%d] : (%3.5f,%3.5f,%3.5f)"), rdx, GlobalMatrices1[rdx].GetTranslation().X, GlobalMatrices1[rdx].GetTranslation().Y, GlobalMatrices1[rdx].GetTranslation().Z);
		//}
		R.ExpectTrue(FMath::Abs((GlobalMatrices1[0].GetTranslation() - GlobalMatrices0[0].GetTranslation()).Size()) < 1e-3);
		R.ExpectTrue(FMath::Abs((GlobalMatrices1[1].GetTranslation() - GlobalMatrices0[1].GetTranslation()).Size()) > 1.0);
		R.ExpectTrue(FMath::Abs((GlobalMatrices1[2].GetTranslation() - GlobalMatrices0[2].GetTranslation()).Size()) > 1.0);
		R.ExpectTrue(FMath::Abs((GlobalMatrices1[3].GetTranslation() - GlobalMatrices0[3].GetTranslation()).Size()) < 1e-3);

		return !R.HasError();
	}
	template bool TransformMatrixElement<float>(ExampleResponse&& R);

}

