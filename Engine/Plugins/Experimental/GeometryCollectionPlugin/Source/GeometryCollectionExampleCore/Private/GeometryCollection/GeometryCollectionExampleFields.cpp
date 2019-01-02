// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionExampleFields.h"

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"

#include "GeometryCollection/TransformCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include "Field/FieldSystem.h"
#include "Field/FieldSystemNodes.h"
#include "Field/FieldSystemCoreAlgo.h"

DEFINE_LOG_CATEGORY_STATIC(GCTF_Log, Verbose, All);


namespace GeometryCollectionExample
{
	template<class T>
	bool Fields_RadialIntMask(ExampleResponse&& R)
	{

		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(10);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), 10);
		for (int32 Index = 0; Index < 10; Index++)
		{
			SamplesArray[Index] = FVector(Index);
		}
 		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FRadialIntMask & RadialMask = System->NewNode<FRadialIntMask>("FieldName");
		RadialMask.Position = FVector(0.0, 0.0, 0.0);
		RadialMask.Radius = 5.0;

		FFieldContext Context{
			RadialMask.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData()
		};
	
		TArray<int32> ResultsArray;
		ResultsArray.Init(false, 10.0);
		TArrayView<int32> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		for (int32 Index = 0; Index < 10; Index++)
		{
			if (Index <= 2)
			{
				R.ExpectTrue(!!ResultsView[Index]);
			}
			else
			{
				R.ExpectTrue(!ResultsView[Index]);
			}
			//UE_LOG(GCTF_Log, Error, TEXT("[%d] %d"), Index, ResultsView[Index]);
		}

		return !R.HasError();
	}
	template bool Fields_RadialIntMask<float>(ExampleResponse&& R);

	template<class T>
	bool Fields_RadialFalloff(ExampleResponse&& R)
	{

		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(10);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), 10);
		for (int32 Index = 0; Index < 10; Index++)
		{
			SamplesArray[Index] = FVector(Index,0,0);
		}
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FRadialFalloff & RadialFalloff = System->NewNode<FRadialFalloff>("FieldName");
		RadialFalloff.Position = FVector(0.0, 0.0, 0.0);
		RadialFalloff.Radius = 5.0;
		RadialFalloff.Magnitude = 3.0;

		FFieldContext Context{
			RadialFalloff.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData()
		};

		TArray<float> ResultsArray;
		ResultsArray.Init(false, 10);
		TArrayView<float> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		for (int32 Index = 0; Index < 10; Index++)
		{			
			float ExpectedVal = RadialFalloff.Magnitude*float((RadialFalloff.Radius * RadialFalloff.Radius) - (Index*Index)) / (RadialFalloff.Radius*RadialFalloff.Radius);

			if (Index <=5 )
			{
				R.ExpectTrue(FMath::Abs(ResultsView[Index])-ExpectedVal<KINDA_SMALL_NUMBER);
			}
			else
			{
				R.ExpectTrue(!ResultsView[Index]);
			}
			//UE_LOG(GCTF_Log, Error, TEXT("[%d] sample:%3.5f (%3.5f,%3.5f,%3.5f) %3.5f"), Index,
			//	ExpectedVal ,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ResultsView[Index]);
		}

		return !R.HasError();
	}
	template bool Fields_RadialFalloff<float>(ExampleResponse&& R);



	template<class T>
	bool Fields_UniformVector(ExampleResponse&& R)
	{

		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(10);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), 10);
		for (int32 Index = 0; Index < 10; Index++)
		{
			SamplesArray[Index] = FVector(Index, 0, 0);
		}
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FUniformVector & UniformVector = System->NewNode<FUniformVector>("FieldName");
		UniformVector.Direction = FVector(3,5,7);
		UniformVector.Magnitude = 10.0;

		FFieldContext Context{
			UniformVector.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData()
		};

		TArray<FVector> ResultsArray;
		ResultsArray.Init(FVector(0), 10);
		TArrayView<FVector> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		for (int32 Index = 0; Index < 10; Index++)
		{
			FVector ExpectedVal = 10.0*FVector(3, 5, 7);
			R.ExpectTrue((ResultsView[Index]- ExpectedVal).Size() < KINDA_SMALL_NUMBER);
			//UE_LOG(GCTF_Log, Error, TEXT("[%d] sample:(%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f)"), Index,
			//	ExpectedVal.X, ExpectedVal.Y, ExpectedVal.Z,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ResultsView[Index].X, ResultsView[Index].Y, ResultsView[Index].Z);
		}

		return !R.HasError();
	}
	template bool Fields_UniformVector<float>(ExampleResponse&& R);


	template<class T>
	bool Fields_RaidalVector(ExampleResponse&& R)
	{
		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(10);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), 10);
		for (int32 Index = 0; Index < 10; Index++)
		{
			SamplesArray[Index] = FVector(100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5));
		}
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FRadialVector & RadialVector = System->NewNode<FRadialVector>("FieldName");
		RadialVector.Position = FVector(3,4,5);
		RadialVector.Magnitude = 10.0;

		FFieldContext Context{
			RadialVector.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData()
		};

		TArray<FVector> ResultsArray;
		ResultsArray.Init(FVector(0), 10);
		TArrayView<FVector> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		for (int32 Index = 0; Index < 10; Index++)
		{
			FVector ExpectedVal = RadialVector.Magnitude * (SamplesArray[Index] - RadialVector.Position).GetSafeNormal();
			R.ExpectTrue((ResultsView[Index] - ExpectedVal).Size() < KINDA_SMALL_NUMBER);
			//UE_LOG(GCTF_Log, Error, TEXT("[%d] sample:(%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f)"), Index,
			//	ExpectedVal.X, ExpectedVal.Y, ExpectedVal.Z,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ResultsView[Index].X, ResultsView[Index].Y, ResultsView[Index].Z);
		}

		return !R.HasError();
	}
	template bool Fields_RaidalVector<float>(ExampleResponse&& R);


	template<class T>
	bool Fields_SumVectorFullMult(ExampleResponse&& R)
	{
		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(10);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		float AverageSampleLength = 0.0;
		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), 10);
		for (int32 Index = 0; Index < 10; Index++)
		{
			SamplesArray[Index] = FVector(0);
			if (Index > 0)
			{
				SamplesArray[Index] = FVector(100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5));
			}
			AverageSampleLength += SamplesArray[Index].Size();
		}
		AverageSampleLength /= 10.0;
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FRadialFalloff & RadialFalloff = System->NewNode<FRadialFalloff>("FieldName");
		RadialFalloff.Position = FVector(0.0, 0.0, 0.0);
		RadialFalloff.Radius = AverageSampleLength;
		RadialFalloff.Magnitude = 3.0;

		FRadialVector & RadialVector = System->NewNode<FRadialVector>("FieldName");
		RadialVector.Position = FVector(0);
		RadialVector.Magnitude = 10.0;

		FUniformVector & UniformVector = System->NewNode<FUniformVector>("FieldName");
		UniformVector.Direction = FVector(3, 5, 7);
		UniformVector.Magnitude = 10.0;

		FSumVector & SumVector = System->NewNode<FSumVector>("FieldName");
		SumVector.Scalar = RadialFalloff.GetTerminalID();
		SumVector.VectorLeft = RadialVector.GetTerminalID();
		SumVector.VectorRight = UniformVector.GetTerminalID();
		SumVector.Operation = EFieldOperationType::Field_Multiply;


		FFieldContext Context{
			SumVector.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData()
		};

		TArray<FVector> ResultsArray;
		ResultsArray.Init(FVector(0), 10);
		TArrayView<FVector> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		float RadialFalloffSize2 = RadialFalloff.Radius * RadialFalloff.Radius;
		for (int32 Index = 0; Index < 10; Index++)
		{
			FVector RightResult = UniformVector.Magnitude * UniformVector.Direction;
			FVector LeftResult = RadialVector.Magnitude  * (SamplesArray[Index]- RadialVector.Position).GetSafeNormal();
			float RadialFalloffDelta2 = (SamplesArray[Index] - RadialFalloff.Position).SizeSquared();
			float ScalarResult = RadialFalloff.Magnitude* (RadialFalloffSize2 - RadialFalloffDelta2) / RadialFalloffSize2;
			if (RadialFalloffDelta2 >= RadialFalloffSize2)
				ScalarResult = 0.f;

			FVector ExpectedVal = ScalarResult * (LeftResult * RightResult);

			//UE_LOG(GCTF_Log, Error, TEXT("[%d:%3.5f] sample:(%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f)"), Index,
			//	(ResultsView[Index] - ExpectedVal).Size(),
			//	ExpectedVal.X, ExpectedVal.Y, ExpectedVal.Z,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ResultsView[Index].X, ResultsView[Index].Y, ResultsView[Index].Z);

			R.ExpectTrue((ResultsView[Index] - ExpectedVal).Size() < KINDA_SMALL_NUMBER);
		}

		return !R.HasError();
	}
	template bool Fields_SumVectorFullMult<float>(ExampleResponse&& R);


	template<class T>
	bool Fields_SumVectorFullDiv(ExampleResponse&& R)
	{
		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(10);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		float AverageSampleLength = 0.0;
		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), 10);
		for (int32 Index = 0; Index < 10; Index++)
		{
			SamplesArray[Index] = FVector(0);
			if (Index > 0)
			{
				SamplesArray[Index] = FVector(100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5));
			}
			AverageSampleLength += SamplesArray[Index].Size();
		}
		AverageSampleLength /= 10.0;
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FRadialFalloff & RadialFalloff = System->NewNode<FRadialFalloff>("FieldName");
		RadialFalloff.Position = FVector(0.0, 0.0, 0.0);
		RadialFalloff.Radius = AverageSampleLength;
		RadialFalloff.Magnitude = 3.0;

		FRadialVector & RadialVector = System->NewNode<FRadialVector>("FieldName");
		RadialVector.Position = FVector(0);
		RadialVector.Magnitude = 10.0;

		FUniformVector & UniformVector = System->NewNode<FUniformVector>("FieldName");
		UniformVector.Direction = FVector(3, 5, 7);
		UniformVector.Magnitude = 10.0;

		FSumVector & SumVector = System->NewNode<FSumVector>("FieldName");
		SumVector.Scalar = RadialFalloff.GetTerminalID();
		SumVector.VectorLeft = RadialVector.GetTerminalID();
		SumVector.VectorRight = UniformVector.GetTerminalID();
		SumVector.Operation = EFieldOperationType::Field_Divide;


		FFieldContext Context{
			SumVector.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData()
		};

		TArray<FVector> ResultsArray;
		ResultsArray.Init(FVector(0), 10);
		TArrayView<FVector> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		float RadialFalloffSize2 = RadialFalloff.Radius * RadialFalloff.Radius;
		for (int32 Index = 0; Index < 10; Index++)
		{
			FVector RightResult = UniformVector.Magnitude * UniformVector.Direction;
			FVector LeftResult = RadialVector.Magnitude  * (SamplesArray[Index] - RadialVector.Position).GetSafeNormal();
			float RadialFalloffDelta2 = (SamplesArray[Index] - RadialFalloff.Position).SizeSquared();
			float ScalarResult = RadialFalloff.Magnitude* (RadialFalloffSize2 - RadialFalloffDelta2) / RadialFalloffSize2;
			if (RadialFalloffDelta2 >= RadialFalloffSize2)
				ScalarResult = 0.f;

			FVector ExpectedVal = ScalarResult * (LeftResult / RightResult);

			//UE_LOG(GCTF_Log, Error, TEXT("[%d:%3.5f] sample:(%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f)"), Index,
			//	(ResultsView[Index] - ExpectedVal).Size(),
			//	ExpectedVal.X, ExpectedVal.Y, ExpectedVal.Z,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ResultsView[Index].X, ResultsView[Index].Y, ResultsView[Index].Z);

			R.ExpectTrue((ResultsView[Index] - ExpectedVal).Size() < KINDA_SMALL_NUMBER);
		}

		return !R.HasError();
	}
	template bool Fields_SumVectorFullDiv<float>(ExampleResponse&& R);


	template<class T>
	bool Fields_SumVectorFullAdd(ExampleResponse&& R)
	{
		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(10);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		float AverageSampleLength = 0.0;
		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), 10);
		for (int32 Index = 0; Index < 10; Index++)
		{
			SamplesArray[Index] = FVector(0);
			if (Index > 0)
			{
				SamplesArray[Index] = FVector(100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5));
			}
			AverageSampleLength += SamplesArray[Index].Size();
		}
		AverageSampleLength /= 10.0;
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FRadialFalloff & RadialFalloff = System->NewNode<FRadialFalloff>("FieldName");
		RadialFalloff.Position = FVector(0.0, 0.0, 0.0);
		RadialFalloff.Radius = AverageSampleLength;
		RadialFalloff.Magnitude = 3.0;

		FRadialVector & RadialVector = System->NewNode<FRadialVector>("FieldName");
		RadialVector.Position = FVector(0);
		RadialVector.Magnitude = 10.0;

		FUniformVector & UniformVector = System->NewNode<FUniformVector>("FieldName");
		UniformVector.Direction = FVector(3, 5, 7);
		UniformVector.Magnitude = 10.0;

		FSumVector & SumVector = System->NewNode<FSumVector>("FieldName");
		SumVector.Scalar = RadialFalloff.GetTerminalID();
		SumVector.VectorLeft = RadialVector.GetTerminalID();
		SumVector.VectorRight = UniformVector.GetTerminalID();
		SumVector.Operation = EFieldOperationType::Field_Add;


		FFieldContext Context{
			SumVector.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData()
		};

		TArray<FVector> ResultsArray;
		ResultsArray.Init(FVector(0), 10);
		TArrayView<FVector> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		float RadialFalloffSize2 = RadialFalloff.Radius * RadialFalloff.Radius;
		for (int32 Index = 0; Index < 10; Index++)
		{
			FVector RightResult = UniformVector.Magnitude * UniformVector.Direction;
			FVector LeftResult = RadialVector.Magnitude  * (SamplesArray[Index] - RadialVector.Position).GetSafeNormal();
			float RadialFalloffDelta2 = (SamplesArray[Index] - RadialFalloff.Position).SizeSquared();
			float ScalarResult = RadialFalloff.Magnitude* (RadialFalloffSize2 - RadialFalloffDelta2) / RadialFalloffSize2;
			if (RadialFalloffDelta2 >= RadialFalloffSize2)
				ScalarResult = 0.f;

			FVector ExpectedVal = ScalarResult * (LeftResult + RightResult);

			//UE_LOG(GCTF_Log, Error, TEXT("[%d:%3.5f] sample:(%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f)"), Index,
			//	(ResultsView[Index] - ExpectedVal).Size(),
			//	ExpectedVal.X, ExpectedVal.Y, ExpectedVal.Z,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ResultsView[Index].X, ResultsView[Index].Y, ResultsView[Index].Z);

			R.ExpectTrue((ResultsView[Index] - ExpectedVal).Size() < KINDA_SMALL_NUMBER);
		}

		return !R.HasError();
	}
	template bool Fields_SumVectorFullAdd<float>(ExampleResponse&& R);

	template<class T>
	bool Fields_SumVectorFullSub(ExampleResponse&& R)
	{
		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(10);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		float AverageSampleLength = 0.0;
		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), 10);
		for (int32 Index = 0; Index < 10; Index++)
		{
			SamplesArray[Index] = FVector(0);
			if (Index > 0)
			{
				SamplesArray[Index] = FVector(100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5));
			}
			AverageSampleLength += SamplesArray[Index].Size();
		}
		AverageSampleLength /= 10.0;
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FRadialFalloff & RadialFalloff = System->NewNode<FRadialFalloff>("FieldName");
		RadialFalloff.Position = FVector(0.0, 0.0, 0.0);
		RadialFalloff.Radius = AverageSampleLength;
		RadialFalloff.Magnitude = 3.0;

		FRadialVector & RadialVector = System->NewNode<FRadialVector>("FieldName");
		RadialVector.Position = FVector(0);
		RadialVector.Magnitude = 10.0;

		FUniformVector & UniformVector = System->NewNode<FUniformVector>("FieldName");
		UniformVector.Direction = FVector(3, 5, 7);
		UniformVector.Magnitude = 10.0;

		FSumVector & SumVector = System->NewNode<FSumVector>("FieldName");
		SumVector.Scalar = RadialFalloff.GetTerminalID();
		SumVector.VectorLeft = RadialVector.GetTerminalID();
		SumVector.VectorRight = UniformVector.GetTerminalID();
		SumVector.Operation = EFieldOperationType::Field_Substract;


		FFieldContext Context{
			SumVector.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData()
		};

		TArray<FVector> ResultsArray;
		ResultsArray.Init(FVector(0), 10);
		TArrayView<FVector> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		float RadialFalloffSize2 = RadialFalloff.Radius * RadialFalloff.Radius;
		for (int32 Index = 0; Index < 10; Index++)
		{
			FVector RightResult = UniformVector.Magnitude * UniformVector.Direction;
			FVector LeftResult = RadialVector.Magnitude  * (SamplesArray[Index] - RadialVector.Position).GetSafeNormal();
			float RadialFalloffDelta2 = (SamplesArray[Index] - RadialFalloff.Position).SizeSquared();
			float ScalarResult = RadialFalloff.Magnitude* (RadialFalloffSize2 - RadialFalloffDelta2) / RadialFalloffSize2;
			if (RadialFalloffDelta2 >= RadialFalloffSize2)
				ScalarResult = 0.f;

			FVector ExpectedVal = ScalarResult * (LeftResult - RightResult);

			//UE_LOG(GCTF_Log, Error, TEXT("[%d:%3.5f] sample:(%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f)"), Index,
			//	(ResultsView[Index] - ExpectedVal).Size(),
			//	ExpectedVal.X, ExpectedVal.Y, ExpectedVal.Z,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ResultsView[Index].X, ResultsView[Index].Y, ResultsView[Index].Z);

			R.ExpectTrue((ResultsView[Index] - ExpectedVal).Size() < KINDA_SMALL_NUMBER);
		}

		return !R.HasError();
	}
	template bool Fields_SumVectorFullSub<float>(ExampleResponse&& R);

	template<class T>
	bool Fields_SumVectorLeftSide(ExampleResponse&& R)
	{
		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(10);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		float AverageSampleLength = 0.0;
		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), 10);
		for (int32 Index = 0; Index < 10; Index++)
		{
			SamplesArray[Index] = FVector(0);
			if (Index > 0)
			{
				SamplesArray[Index] = FVector(100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5));
			}
			AverageSampleLength += SamplesArray[Index].Size();
		}
		AverageSampleLength /= 10.0;
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FRadialFalloff & RadialFalloff = System->NewNode<FRadialFalloff>("FieldName");
		RadialFalloff.Position = FVector(0.0, 0.0, 0.0);
		RadialFalloff.Radius = AverageSampleLength;
		RadialFalloff.Magnitude = 3.0;

		FRadialVector & RadialVector = System->NewNode<FRadialVector>("FieldName");
		RadialVector.Position = FVector(0);
		RadialVector.Magnitude = 10.0;

		FUniformVector & UniformVector = System->NewNode<FUniformVector>("FieldName");
		UniformVector.Direction = FVector(3, 5, 7);
		UniformVector.Magnitude = 10.0;

		FSumVector & SumVector = System->NewNode<FSumVector>("FieldName");
		SumVector.Scalar = RadialFalloff.GetTerminalID();
		SumVector.VectorLeft = RadialVector.GetTerminalID();
		SumVector.VectorRight = FFieldNodeBase::Invalid;
		SumVector.Operation = EFieldOperationType::Field_Multiply;


		FFieldContext Context{
			SumVector.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData()
		};

		TArray<FVector> ResultsArray;
		ResultsArray.Init(FVector(0), 10);
		TArrayView<FVector> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		float RadialFalloffSize2 = RadialFalloff.Radius * RadialFalloff.Radius;
		for (int32 Index = 0; Index < 10; Index++)
		{
			FVector RightResult = UniformVector.Magnitude * UniformVector.Direction;
			FVector LeftResult = RadialVector.Magnitude  * (SamplesArray[Index] - RadialVector.Position).GetSafeNormal();
			float RadialFalloffDelta2 = (SamplesArray[Index] - RadialFalloff.Position).SizeSquared();
			float ScalarResult = RadialFalloff.Magnitude* (RadialFalloffSize2 - RadialFalloffDelta2) / RadialFalloffSize2;
			if (RadialFalloffDelta2 >= RadialFalloffSize2)
				ScalarResult = 0.f;

			FVector ExpectedVal = ScalarResult * (LeftResult);

			//UE_LOG(GCTF_Log, Error, TEXT("[%d:%3.5f] sample:(%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f)"), Index,
			//	(ResultsView[Index] - ExpectedVal).Size(),
			//	ExpectedVal.X, ExpectedVal.Y, ExpectedVal.Z,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ResultsView[Index].X, ResultsView[Index].Y, ResultsView[Index].Z);

			R.ExpectTrue((ResultsView[Index] - ExpectedVal).Size() < KINDA_SMALL_NUMBER);
		}

		return !R.HasError();
	}
	template bool Fields_SumVectorLeftSide<float>(ExampleResponse&& R);


	template<class T>
	bool Fields_SumVectorRightSide(ExampleResponse&& R)
	{
		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(10);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		float AverageSampleLength = 0.0;
		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), 10);
		for (int32 Index = 0; Index < 10; Index++)
		{
			SamplesArray[Index] = FVector(0);
			if (Index > 0)
			{
				SamplesArray[Index] = FVector(100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5));
			}
			AverageSampleLength += SamplesArray[Index].Size();
		}
		AverageSampleLength /= 10.0;
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FRadialFalloff & RadialFalloff = System->NewNode<FRadialFalloff>("FieldName");
		RadialFalloff.Position = FVector(0.0, 0.0, 0.0);
		RadialFalloff.Radius = AverageSampleLength;
		RadialFalloff.Magnitude = 3.0;

		FRadialVector & RadialVector = System->NewNode<FRadialVector>("FieldName");
		RadialVector.Position = FVector(0);
		RadialVector.Magnitude = 10.0;

		FUniformVector & UniformVector = System->NewNode<FUniformVector>("FieldName");
		UniformVector.Direction = FVector(3, 5, 7);
		UniformVector.Magnitude = 10.0;

		FSumVector & SumVector = System->NewNode<FSumVector>("FieldName");
		SumVector.Scalar = RadialFalloff.GetTerminalID();
		SumVector.VectorLeft = FFieldNodeBase::Invalid;
		SumVector.VectorRight = UniformVector.GetTerminalID();
		SumVector.Operation = EFieldOperationType::Field_Multiply;


		FFieldContext Context{
			SumVector.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData()
		};

		TArray<FVector> ResultsArray;
		ResultsArray.Init(FVector(0), 10);
		TArrayView<FVector> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		float RadialFalloffSize2 = RadialFalloff.Radius * RadialFalloff.Radius;
		for (int32 Index = 0; Index < 10; Index++)
		{
			FVector RightResult = UniformVector.Magnitude * UniformVector.Direction;
			FVector LeftResult = RadialVector.Magnitude  * (SamplesArray[Index] - RadialVector.Position).GetSafeNormal();
			float RadialFalloffDelta2 = (SamplesArray[Index] - RadialFalloff.Position).SizeSquared();
			float ScalarResult = RadialFalloff.Magnitude* (RadialFalloffSize2 - RadialFalloffDelta2) / RadialFalloffSize2;
			if (RadialFalloffDelta2 >= RadialFalloffSize2)
				ScalarResult = 0.f;

			FVector ExpectedVal = ScalarResult * (RightResult);

			//UE_LOG(GCTF_Log, Error, TEXT("[%d:%3.5f] sample:(%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f)"), Index,
			//	(ResultsView[Index] - ExpectedVal).Size(),
			//	ExpectedVal.X, ExpectedVal.Y, ExpectedVal.Z,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ResultsView[Index].X, ResultsView[Index].Y, ResultsView[Index].Z);

			R.ExpectTrue((ResultsView[Index] - ExpectedVal).Size() < KINDA_SMALL_NUMBER);
		}

		return !R.HasError();
	}
	template bool Fields_SumVectorRightSide<float>(ExampleResponse&& R);


	template<class T>
	bool Fields_SumScalar(ExampleResponse&& R)
	{
		int32 NumPoints = 20;
		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(NumPoints);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), NumPoints);
		for (int32 Index = -10; Index < 10; Index++)
		{
			SamplesArray[Index+10] = FVector(Index, 0, 0);
		}
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FRadialFalloff & RadialFalloff = System->NewNode<FRadialFalloff>("FieldName");
		RadialFalloff.Position = FVector(5.0, 0.0, 0.0);
		RadialFalloff.Radius = 10;
		RadialFalloff.Magnitude = 3.0;
		float RadialFalloffRadius2 = RadialFalloff.Radius * RadialFalloff.Radius;

		FRadialIntMask & RadialMask = System->NewNode<FRadialIntMask>("FieldName");
		RadialMask.Position = FVector(-5.0, 0.0, 0.0);
		RadialMask.Radius = 5.0;
		RadialMask.InteriorValue = 1.0;
		RadialMask.ExteriorValue = 0.0;
		float RadialMaskRadius2 = RadialMask.Radius * RadialMask.Radius;

		FSumScalar & SumScalar = System->NewNode<FSumScalar>("FieldName");
		SumScalar.ScalarLeft = RadialFalloff.GetTerminalID();
		SumScalar.ScalarRight = RadialMask.GetTerminalID();
		SumScalar.Operation = EFieldOperationType::Field_Multiply;

		FFieldContext Context{
			SumScalar.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData()
		};

		TArray<float> ResultsArray;
		ResultsArray.Init(0.f, SamplesArray.Num());
		TArrayView<float> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		for (int32 Index = 0; Index < NumPoints; Index++)
		{ 
			float ScalarLeft = 0.f;
			{ //  FRadialFalloff::Evaluate
				float  RadialFalloffDelta2 = (RadialFalloff.Position - SamplesArray[Index]).SizeSquared();
				if (RadialFalloffDelta2 < RadialFalloffRadius2)
				{
					ScalarLeft = RadialFalloff.Magnitude * (RadialFalloffRadius2 - RadialFalloffDelta2) / RadialFalloffRadius2;
				}
			}

			float ScalarRight = 0.f;
			{// FRadialIntMask::Evaluate
				float  RadialMaskDelta2 = (RadialMask.Position - SamplesArray[Index]).SizeSquared();
				if (RadialMaskDelta2 < RadialMaskRadius2)
					ScalarRight = RadialMask.InteriorValue;
				else
					ScalarRight = RadialMask.ExteriorValue;
			}

			float ExpectedVal = ScalarLeft*ScalarRight;

			//UE_LOG(GCTF_Log, Error, TEXT("[%d:%3.5f] sample:(%3.5f,%3.5f,%3.5f) %3.5f -> %3.5f"), Index,
			//	ResultsView[Index] - ExpectedVal,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ExpectedVal, ResultsView[Index]);

			R.ExpectTrue((ResultsView[Index] - ExpectedVal) < KINDA_SMALL_NUMBER);
		}

		return !R.HasError();
	}
	template bool Fields_SumScalar<float>(ExampleResponse&& R);


	template<class T>
	bool Fields_SumScalarRightSide(ExampleResponse&& R)
	{
		int32 NumPoints = 20;
		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(NumPoints);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), NumPoints);
		for (int32 Index = -10; Index < 10; Index++)
		{
			SamplesArray[Index + 10] = FVector(Index, 0, 0);
		}
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FRadialFalloff & RadialFalloff = System->NewNode<FRadialFalloff>("FieldName");
		RadialFalloff.Position = FVector(5.0, 0.0, 0.0);
		RadialFalloff.Radius = 10;
		RadialFalloff.Magnitude = 3.0;
		float RadialFalloffRadius2 = RadialFalloff.Radius * RadialFalloff.Radius;

		FRadialIntMask & RadialMask = System->NewNode<FRadialIntMask>("FieldName");
		RadialMask.Position = FVector(-5.0, 0.0, 0.0);
		RadialMask.Radius = 5.0;
		RadialMask.InteriorValue = 1.0;
		RadialMask.ExteriorValue = 0.0;
		float RadialMaskRadius2 = RadialMask.Radius * RadialMask.Radius;

		FSumScalar & SumScalar = System->NewNode<FSumScalar>("FieldName");
		SumScalar.ScalarLeft = FFieldNodeBase::Invalid;
		SumScalar.ScalarRight = RadialMask.GetTerminalID();
		SumScalar.Operation = EFieldOperationType::Field_Multiply;

		FFieldContext Context{
			SumScalar.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData()
		};

		TArray<float> ResultsArray;
		ResultsArray.Init(0.f, SamplesArray.Num());
		TArrayView<float> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		for (int32 Index = 0; Index < NumPoints; Index++)
		{
			float ScalarLeft = 1.f;

			float ScalarRight = 0.f;
			{// FRadialIntMask::Evaluate
				float  RadialMaskDelta2 = (RadialMask.Position - SamplesArray[Index]).SizeSquared();
				if (RadialMaskDelta2 < RadialMaskRadius2)
					ScalarRight = RadialMask.InteriorValue;
				else
					ScalarRight = RadialMask.ExteriorValue;
			}

			float ExpectedVal = ScalarLeft * ScalarRight;

			//UE_LOG(GCTF_Log, Error, TEXT("[%d:%3.5f] sample:(%3.5f,%3.5f,%3.5f) %3.5f -> %3.5f"), Index,
			//	ResultsView[Index] - ExpectedVal,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ExpectedVal, ResultsView[Index]);

			R.ExpectTrue((ResultsView[Index] - ExpectedVal) < KINDA_SMALL_NUMBER);
		}

		return !R.HasError();
	}
	template bool Fields_SumScalarRightSide<float>(ExampleResponse&& R);

	template<class T>
	bool Fields_SumScalarLeftSide(ExampleResponse&& R)
	{
		int32 NumPoints = 20;
		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(NumPoints);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), NumPoints);
		for (int32 Index = -10; Index < 10; Index++)
		{
			SamplesArray[Index + 10] = FVector(Index, 0, 0);
		}
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FRadialFalloff & RadialFalloff = System->NewNode<FRadialFalloff>("FieldName");
		RadialFalloff.Position = FVector(5.0, 0.0, 0.0);
		RadialFalloff.Radius = 10;
		RadialFalloff.Magnitude = 3.0;
		float RadialFalloffRadius2 = RadialFalloff.Radius * RadialFalloff.Radius;

		FRadialIntMask & RadialMask = System->NewNode<FRadialIntMask>("FieldName");
		RadialMask.Position = FVector(-5.0, 0.0, 0.0);
		RadialMask.Radius = 5.0;
		RadialMask.InteriorValue = 1.0;
		RadialMask.ExteriorValue = 0.0;
		float RadialMaskRadius2 = RadialMask.Radius * RadialMask.Radius;

		FSumScalar & SumScalar = System->NewNode<FSumScalar>("FieldName");
		SumScalar.ScalarLeft = RadialFalloff.GetTerminalID();
		SumScalar.ScalarRight = FFieldNodeBase::Invalid;
		SumScalar.Operation = EFieldOperationType::Field_Multiply;

		FFieldContext Context{
			SumScalar.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData()
		};

		TArray<float> ResultsArray;
		ResultsArray.Init(0.f, SamplesArray.Num());
		TArrayView<float> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		for (int32 Index = 0; Index < NumPoints; Index++)
		{
			float ScalarLeft = 0.f;
			{ //  FRadialFalloff::Evaluate
				float  RadialFalloffDelta2 = (RadialFalloff.Position - SamplesArray[Index]).SizeSquared();
				if (RadialFalloffDelta2 < RadialFalloffRadius2)
				{
					ScalarLeft = RadialFalloff.Magnitude * (RadialFalloffRadius2 - RadialFalloffDelta2) / RadialFalloffRadius2;
				}
			}

			float ScalarRight = 1.f;

			float ExpectedVal = ScalarLeft * ScalarRight;

			//UE_LOG(GCTF_Log, Error, TEXT("[%d:%3.5f] sample:(%3.5f,%3.5f,%3.5f) %3.5f -> %3.5f"), Index,
			//	ResultsView[Index] - ExpectedVal,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ExpectedVal, ResultsView[Index]);

			R.ExpectTrue((ResultsView[Index] - ExpectedVal) < KINDA_SMALL_NUMBER);
		}

		return !R.HasError();
	}
	template bool Fields_SumScalarLeftSide<float>(ExampleResponse&& R);


	template<class T>
	bool Fields_ContextOverrides(ExampleResponse&& R)
	{
		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(10);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		float AverageSampleLength = 0.0;
		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), 10);
		for (int32 Index = 0; Index < 10; Index++)
		{
			SamplesArray[Index] = FVector(0);
			if (Index > 0)
			{
				SamplesArray[Index] = FVector(100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5), 100.*(FMath::SRand() - 0.5));
			}
			AverageSampleLength += SamplesArray[Index].Size();
		}
		AverageSampleLength /= 10.0;
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());

		FRadialFalloff & RadialFalloff = System->NewNode<FRadialFalloff>("FieldName");
		RadialFalloff.Position = FVector(0.0, 0.0, 0.0);
		RadialFalloff.Radius = AverageSampleLength;
		RadialFalloff.Magnitude = 3.0;

		FRadialVector & RadialVector = System->NewNode<FRadialVector>("FieldName");
		RadialVector.Position = FVector(0);
		RadialVector.Magnitude = 10.0;

		FUniformVector & UniformVector = System->NewNode<FUniformVector>("FieldName");
		UniformVector.Direction = FVector(3, 5, 7);
		UniformVector.Magnitude = 10.0;

		FSumVector & SumVector = System->NewNode<FSumVector>("FieldName");
		SumVector.Scalar = RadialFalloff.GetTerminalID();
		SumVector.VectorLeft = RadialVector.GetTerminalID();
		SumVector.VectorRight = UniformVector.GetTerminalID();
		SumVector.Operation = EFieldOperationType::Field_Multiply;

		FVector Position(100, 33, 55);
		FVector Direction(-2, 5, 22);
		float Magnitude = 0.2f;
		float Radius = 1000.f;
		FFieldContext Context{
			SumVector.GetTerminalID(),
			IndexView,
			SamplesView,
			&System->GetFieldData(),
			&Position,
			&Direction,
			&Radius,
			&Magnitude
		};

		TArray<FVector> ResultsArray;
		ResultsArray.Init(FVector(0), 10);
		TArrayView<FVector> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		float Radial2 = Radius * Radius;
		for (int32 Index = 0; Index < 10; Index++)
		{
			FVector RightResult = Magnitude * Direction;
			FVector LeftResult = Magnitude  * (SamplesArray[Index] - Position).GetSafeNormal();
			float RadialFalloffDelta2 = (SamplesArray[Index] - Position).SizeSquared();
			float ScalarResult = Magnitude* (Radial2 - RadialFalloffDelta2) / Radial2;
			if (RadialFalloffDelta2 >= Radial2)
				ScalarResult = 0.f;

			FVector ExpectedVal = ScalarResult * (LeftResult * RightResult);

			//UE_LOG(GCTF_Log, Error, TEXT("[%d:%3.5f] sample:(%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f)"), Index,
			//	(ResultsView[Index] - ExpectedVal).Size(),
			//	ExpectedVal.X, ExpectedVal.Y, ExpectedVal.Z,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ResultsView[Index].X, ResultsView[Index].Y, ResultsView[Index].Z);

			R.ExpectTrue((ResultsView[Index] - ExpectedVal).Size() < KINDA_SMALL_NUMBER);
		}

		return !R.HasError();
	}
	template bool Fields_ContextOverrides<float>(ExampleResponse&& R);

	template<class T>
	bool Fields_DefaultRadialFalloff(ExampleResponse&& R)
	{
	
		TSharedPtr< TArray<int32> > Indices = GeometryCollectionAlgo::ContiguousArray(10);
		TArrayView<int32> IndexView(&(Indices->operator[](0)), Indices->Num());

		TArray<FVector> SamplesArray;
		SamplesArray.Init(FVector(0.f), 10);
		for (int32 Index = 0; Index < 10; Index++)
		{
			SamplesArray[Index] = FVector(Index, 0, 0);
		}
		TArrayView<FVector> SamplesView(&(SamplesArray.operator[](0)), SamplesArray.Num());


		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());
		FieldSystemAlgo::InitDefaultFieldData(System->GetFieldData());

		FVector Position = FVector(0.0, 0.0, 0.0);
		FVector Direction = FVector(0.0, 0.0, 0.0);
		float Radius = 5.0;
		float Magnitude = 3.0;

		R.ExpectTrue(System->TerminalIndex("RadialVectorFalloff") != -1);

		FFieldContext Context{
			System->TerminalIndex("RadialVectorFalloff"),
			IndexView,
			SamplesView,
			&System->GetFieldData(),
			&Position,
			&Direction,
			&Radius,
			&Magnitude
		};

		TArray<FVector> ResultsArray;
		ResultsArray.Init(FVector(0), 10);
		TArrayView<FVector> ResultsView(&(ResultsArray.operator[](0)), ResultsArray.Num());
		System->Evaluate(Context, ResultsView);

		float RadialFalloffSize2 = Radius * Radius;

		for (int32 Index = 0; Index < 10; Index++)
		{
			FVector LeftResult = Magnitude  * (SamplesArray[Index] - Position).GetSafeNormal();
			float RadialFalloffDelta2 = (SamplesArray[Index] - Position).SizeSquared();
			float ScalarResult = Magnitude* (RadialFalloffSize2 - RadialFalloffDelta2) / RadialFalloffSize2;
			if (RadialFalloffDelta2 >= RadialFalloffSize2)
				ScalarResult = 0.f;
			FVector ExpectedVal = ScalarResult * (LeftResult);
			R.ExpectTrue((ResultsView[Index]- ExpectedVal).Size() < KINDA_SMALL_NUMBER);
			
			//UE_LOG(GCTF_Log, Error, TEXT("[%d:%3.5f] sample:(%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f) (%3.5f,%3.5f,%3.5f)"), Index,
			//	(ResultsView[Index] - ExpectedVal).Size(),
			//	ExpectedVal.X, ExpectedVal.Y, ExpectedVal.Z,
			//	SamplesArray[Index].X, SamplesArray[Index].Y, SamplesArray[Index].Z,
			//	ResultsView[Index].X, ResultsView[Index].Y, ResultsView[Index].Z);
		}

		return !R.HasError();
	}
	template bool Fields_DefaultRadialFalloff<float>(ExampleResponse&& R);

}
