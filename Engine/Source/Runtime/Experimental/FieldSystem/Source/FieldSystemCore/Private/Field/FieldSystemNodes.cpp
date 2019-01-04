// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Field/FieldSystemNodes.h"
#include "Async/ParallelFor.h"
DEFINE_LOG_CATEGORY_STATIC(FSN_Log, NoLogging, All);


/**
* RadialMaskField
*/

void FRadialIntMask::Evaluate(const FFieldContext & Context, TArrayView<int32> & Results) const
{
	ensure(Context.SampleIndices.Num() == Results.Num());

	float RadiusVal = Context.Radius?*Context.Radius:Radius;
	FVector PositionVal = Context.Position?*Context.Position:Position;

	int32 NumSamples = Context.Samples.Num();
	float Radius2 = RadiusVal * RadiusVal;

	for (int32 Index = 0; Index < Results.Num(); Index++)
	{
		if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
		{
			float Result;
			float Delta2 = (PositionVal - Context.Samples[Context.SampleIndices[Index]]).SizeSquared();
			if (Delta2 < Radius2)
				Result = InteriorValue;
			else
				Result = ExteriorValue;

			switch (SetMaskCondition) {
			case ESetMaskConditionType::Field_Set_Always:
				Results[Index] = Result;
				break;
			case ESetMaskConditionType::Field_Set_IFF_NOT_Interior:
				if (Results[Index] != InteriorValue) {
					Results[Index] = Result;
				}
				break;
			case ESetMaskConditionType::Field_Set_IFF_NOT_Exterior:
				if (Results[Index] != ExteriorValue) {
					Results[Index] = Result;
				}
				break;
			}
		}
	}

}

/**
* FUniformVector
*/

void FRadialFalloff::Evaluate(const FFieldContext & Context, TArrayView<float> & Results) const
{
	ensure(Context.SampleIndices.Num() == Results.Num());

	float RadiusVal = Context.Radius ? *Context.Radius : Radius;
	FVector PositionVal = Context.Position ? *Context.Position : Position;
	float MagnitudeVal = Context.Magnitude? *Context.Magnitude: Magnitude;

	int32 NumSamples = Context.Samples.Num();
	float Radius2 = RadiusVal * RadiusVal;

	if (Radius2 > 0.f)
	{
		for (int32 Index = 0; Index < Results.Num(); Index++)
		{
			if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
			{
				Results[Index] = 0;
				float Delta2 = (PositionVal - Context.Samples[Context.SampleIndices[Index]]).SizeSquared();
				if (Delta2 < Radius2)
				{
					Results[Index] = MagnitudeVal * (Radius2 - Delta2) / Radius2;
				}
			}
		}
	}
}


/**
* FUniformVector
*/

void FUniformVector::Evaluate(const FFieldContext & Context, TArrayView<FVector> & Results) const
{
	ensure(Context.SampleIndices.Num() == Results.Num());

	FVector DirectionVal = Context.Direction ? *Context.Direction : Direction;
	float MagnitudeVal = Context.Magnitude ? *Context.Magnitude : Magnitude;

	int32 NumSamples = Context.Samples.Num();

	for (int32 Index = 0; Index < Results.Num(); Index++)
	{
		Results[Index] = MagnitudeVal * DirectionVal;
	}
}


/**
* FRadialVector
*/

void FRadialVector::Evaluate(const FFieldContext & Context, TArrayView<FVector> & Results) const
{
	ensure(Context.SampleIndices.Num() == Results.Num());

	FVector PositionVal = Context.Position ? *Context.Position : Position;
	float MagnitudeVal = Context.Magnitude ? *Context.Magnitude : Magnitude;

	int32 NumSamples = Context.Samples.Num();

	for (int32 Index = 0; Index < Results.Num(); Index++)
	{
		if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
		{
			
			Results[Index] = MagnitudeVal * (Context.Samples[Context.SampleIndices[Index]] - PositionVal).GetSafeNormal();
		}
	}
}


/**
* FSumVector
*/

void FSumVector::Evaluate(const FFieldContext & Context, TArrayView<FVector> & Results) const
{
	int32 NumSamples = Context.Samples.Num();
	ensure(Context.SampleIndices.Num() == Results.Num());
	
	if(!ensure(FieldSystem != nullptr))
	{
		return;
	}

	float MagnitudeVal = Magnitude;
	if (VectorLeft != Invalid && VectorRight != Invalid)
	{
		TArray<FVector> Buffer;
		Buffer.SetNumUninitialized(2 * NumSamples);
		TArrayView<FVector> Buffers[2] = {
			TArrayView<FVector>(&Buffer[0],NumSamples),
			TArrayView<FVector>(&Buffer[NumSamples],NumSamples),
		};
		TArray<int32> NodeID = { VectorLeft,VectorRight };

		for (int32 i = 0; i < 2; i++)
		//ParallelFor(2, [&](int8 i)
		{
			FFieldContext EvalContext(NodeID[i], Context);
			FieldSystem->Evaluate<FVector>(EvalContext, Buffers[i]);
		}//);

		switch (Operation)
		{
		case EFieldOperationType::Field_Multiply:
			for (int32 Index = 0; Index < Results.Num(); Index++)
			{
				if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
				{
					Results[Index] = Buffers[1][Index] * Buffers[0][Index];
				}
			}
			break;
		case EFieldOperationType::Field_Divide:
			for (int32 Index = 0; Index < Results.Num(); Index++)
			{
				if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
				{
					Results[Index] = Buffers[0][Index] / Buffers[1][Index];
				}
			}
			break;
		case EFieldOperationType::Field_Add:
			for (int32 Index = 0; Index < Results.Num(); Index++)
			{
				if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
				{
					Results[Index] = Buffers[1][Index] + Buffers[0][Index];
				}
			}
			break;
		case EFieldOperationType::Field_Substract:
			for (int32 Index = 0; Index < Results.Num(); Index++)
			{
				if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
				{
					Results[Index] = Buffers[0][Index] - Buffers[1][Index];
				}
			}
			break;
		}
	}
	else if (VectorLeft != Invalid)
	{
		FFieldContext EvalContext(VectorLeft, Context);
		FieldSystem->Evaluate<FVector>(EvalContext, Results);
	}
	else if (VectorRight != Invalid)
	{
		FFieldContext EvalContext(VectorRight, Context);
		FieldSystem->Evaluate<FVector>(EvalContext, Results);
	}

	if (Scalar != Invalid)
	{
		TArray<float> Buffer;
		Buffer.SetNumUninitialized(NumSamples);
		TArrayView<float> BufferView(&Buffer[0], NumSamples);

		FFieldContext EvalContext(Scalar , Context);
		FieldSystem->Evaluate<float>(EvalContext, BufferView);

		for (int32 Index = 0; Index < Results.Num(); Index++)
		{
			if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
			{
				Results[Index] *= Buffer[Index];
			}
		}
	}

	if (MagnitudeVal != 1.0)
	{
		for (int32 Index = 0; Index < Results.Num(); Index++)
		{
			if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
			{
				Results[Index] *= MagnitudeVal;
			}
		}
	}
}

/**
* FSumScalar
*/

void FSumScalar::Evaluate(const FFieldContext & Context, TArrayView<float> & Results) const
{
	int32 NumSamples = Context.Samples.Num();
	ensure(Context.SampleIndices.Num() == Results.Num());
	
	if(!ensure(FieldSystem != nullptr))
	{
		return;
	}

	float MagnitudeVal = Magnitude;
	if (ScalarLeft != Invalid && ScalarRight != Invalid)
	{
		TArray<float> Buffer;
		Buffer.SetNumUninitialized(2 * NumSamples);
		TArrayView<float> Buffers[2] = {
			TArrayView<float>(&Buffer[0],NumSamples),
			TArrayView<float>(&Buffer[NumSamples],NumSamples),
		};
		TArray<int32> NodeID = { ScalarLeft,ScalarRight };

		//for (int32 i = 0; i < 2; i++)
		ParallelFor(2, [&](int8 i)
		{
			FFieldContext EvalContext(NodeID[i], Context);
			if (FieldSystem->GetNode(NodeID[i])->Type() == FFieldNode<int32>::StaticType())
			{
				TArray<int32> IntBuffer;
				IntBuffer.SetNumUninitialized(Context.SampleIndices.Num());
				TArrayView<int32> IntBufferView(&IntBuffer[0], IntBuffer.Num());
				FieldSystem->Evaluate<int32>(EvalContext, IntBufferView);
				for (int32 Index = 0; Index < IntBufferView.Num(); Index++)
				{
					Buffers[i][Context.SampleIndices[Index]] = (float)IntBufferView[Index];
				}
			}
			else if (FieldSystem->GetNode(NodeID[i])->Type() == FFieldNode<float>::StaticType())
			{
				FieldSystem->Evaluate<float>(EvalContext, Buffers[i]);
			}
			else
			{
				ensureMsgf(false, TEXT("Unsupported field evaluation in SumScalar Field."));
			}
		});

		switch (Operation)
		{
		case EFieldOperationType::Field_Multiply:
			for (int32 Index = 0; Index < Results.Num(); Index++)
			{
				if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
				{
					Results[Index] = Buffers[1][Index] * Buffers[0][Index];
				}
			}
			break;
		case EFieldOperationType::Field_Divide:
			for (int32 Index = 0; Index < Results.Num(); Index++)
			{
				if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
				{
					Results[Index] = Buffers[0][Index] / Buffers[1][Index];
				}
			}
			break;
		case EFieldOperationType::Field_Add:
			for (int32 Index = 0; Index < Results.Num(); Index++)
			{
				if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
				{
					Results[Index] = Buffers[1][Index] + Buffers[0][Index];
				}
			}
			break;
		case EFieldOperationType::Field_Substract:
			for (int32 Index = 0; Index < Results.Num(); Index++)
			{
				if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
				{
					Results[Index] = Buffers[0][Index] - Buffers[1][Index];
				}
			}
			break;
		}
	}
	else if (ScalarLeft != Invalid)
	{
		FFieldContext EvalContext(ScalarLeft, Context);
		if (FieldSystem->GetNode(ScalarLeft)->Type() == FFieldNode<int32>::StaticType())
		{
			TArray<int32> IntBuffer;
			IntBuffer.SetNumUninitialized(Context.SampleIndices.Num());
			TArrayView<int32> IntBufferView(&IntBuffer[0], IntBuffer.Num());
			FieldSystem->Evaluate<int32>(EvalContext, IntBufferView);
			for (int32 Index = 0; Index < IntBufferView.Num(); Index++)
			{
				Results[Context.SampleIndices[Index]] = (float)IntBufferView[Index];
			}
		}
		else if (FieldSystem->GetNode(ScalarLeft)->Type() == FFieldNode<float>::StaticType())
		{
			FieldSystem->Evaluate<float>(EvalContext, Results);
		}
		else
		{
			ensureMsgf(false, TEXT("Unsupported field evaluation in SumScalar Field."));
		}
	}
	else if (ScalarRight != Invalid)
	{
		FFieldContext EvalContext(ScalarRight, Context);
		if (FieldSystem->GetNode(ScalarRight)->Type() == FFieldNode<int32>::StaticType())
		{
			TArray<int32> IntBuffer;
			IntBuffer.SetNumUninitialized(Context.SampleIndices.Num());
			TArrayView<int32> IntBufferView(&IntBuffer[0], IntBuffer.Num());
			FieldSystem->Evaluate<int32>(EvalContext, IntBufferView);
			for (int32 Index = 0; Index < IntBufferView.Num(); Index++)
			{
				Results[Context.SampleIndices[Index]] = (float)IntBufferView[Index];
			}
		}
		else if (FieldSystem->GetNode(ScalarLeft)->Type() == FFieldNode<float>::StaticType())
		{
			FieldSystem->Evaluate<float>(EvalContext, Results);
		}
		else
		{
			ensureMsgf(false, TEXT("Unsupported field evaluation in SumScalar Field."));
		}
	}

	if (MagnitudeVal != 1.0)
	{
		for (int32 Index = 0; Index < Results.Num(); Index++)
		{
			if (0 <= Context.SampleIndices[Index] && Context.SampleIndices[Index] < NumSamples)
			{
				Results[Index] *= MagnitudeVal;
			}
		}
	}
}

template FIELDSYSTEMCORE_API FRadialIntMask& UFieldSystem::NewNode<FRadialIntMask>(const FName &);
template FIELDSYSTEMCORE_API FRadialFalloff& UFieldSystem::NewNode<FRadialFalloff>(const FName &);
template FIELDSYSTEMCORE_API FUniformVector& UFieldSystem::NewNode<FUniformVector>(const FName &);
template FIELDSYSTEMCORE_API FRadialVector& UFieldSystem::NewNode<FRadialVector>(const FName &);
template FIELDSYSTEMCORE_API FSumVector& UFieldSystem::NewNode<FSumVector>(const FName &);
template FIELDSYSTEMCORE_API FSumScalar& UFieldSystem::NewNode<FSumScalar>(const FName &);
