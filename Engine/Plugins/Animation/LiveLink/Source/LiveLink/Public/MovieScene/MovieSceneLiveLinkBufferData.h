// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneChannelProxy.h"


/** Structure used to buffer up individual curve keys.*/
struct FLiveLinkCurveKeys
{
	void Add(float Val, FFrameNumber FrameNumber)
	{
		FMovieSceneFloatValue NewValue(Val);
		NewValue.InterpMode = RCIM_Cubic;
		Curve.Add(NewValue);
		Times.Add(FrameNumber);
	}

	void Reserve(int32 Num)
	{
		Curve.Reserve(Num);
		Times.Reserve(Num);
	}

	void AddToFloatChannels(int32 StartIndex, TArray<FMovieSceneFloatChannel> &FloatChannels)
	{
		FloatChannels[StartIndex].Set(Times, MoveTemp(Curve));
		FloatChannels[StartIndex].AutoSetTangents();
	}

	void AppendToFloatChannelsAndReset(int32 StartIndex, TArray<FMovieSceneFloatChannel> &FloatChannels)
	{
		FloatChannels[StartIndex].AddKeys(Times, Curve);
		Times.Reset();
		Curve.Reset();
	}

	void AutoSetTangents(int32 StartIndex, TArray<FMovieSceneFloatChannel> &FloatChannels)
	{
		FloatChannels[StartIndex].AutoSetTangents();
	}

	TArray<FMovieSceneFloatValue> Curve;
	/*Unlike Transforms that will always have a key per frame, curves are optional and so miss frames, so we need to keep track of each curve. */
	TArray<FFrameNumber> Times;

};

/** Structure used to buffer up transform keys.  */
struct FLiveLinkTransformKeys
{

	TArray<FMovieSceneFloatValue> LocationX, LocationY, LocationZ;
	TArray<FMovieSceneFloatValue> RotationX, RotationY, RotationZ;
	TArray<FMovieSceneFloatValue> ScaleX, ScaleY, ScaleZ;



	void Add(const FTransform& InTransform)
	{
		FMovieSceneFloatValue NewValue(InTransform.GetTranslation().X);
		NewValue.InterpMode = RCIM_Cubic;
		LocationX.Add(NewValue);
		NewValue = FMovieSceneFloatValue(InTransform.GetTranslation().Y);
		NewValue.InterpMode = RCIM_Cubic;
		LocationY.Add(NewValue);
		NewValue = FMovieSceneFloatValue(InTransform.GetTranslation().Z);
		NewValue.InterpMode = RCIM_Cubic;
		LocationZ.Add(NewValue);

		FRotator WoundRotation = InTransform.Rotator();
		NewValue = FMovieSceneFloatValue(WoundRotation.Roll);
		NewValue.InterpMode = RCIM_Cubic;
		RotationX.Add(NewValue);

		NewValue = FMovieSceneFloatValue(WoundRotation.Pitch);
		NewValue.InterpMode = RCIM_Cubic;
		RotationY.Add(NewValue);

		NewValue = FMovieSceneFloatValue(WoundRotation.Yaw);
		NewValue.InterpMode = RCIM_Cubic;
		RotationZ.Add(NewValue);

		NewValue = FMovieSceneFloatValue(InTransform.GetScale3D().X);
		NewValue.InterpMode = RCIM_Cubic;
		ScaleX.Add(NewValue);

		NewValue = FMovieSceneFloatValue(InTransform.GetScale3D().Y);
		NewValue.InterpMode = RCIM_Cubic;
		ScaleY.Add(NewValue);

		NewValue = FMovieSceneFloatValue(InTransform.GetScale3D().Z);
		NewValue.InterpMode = RCIM_Cubic;
		ScaleZ.Add(NewValue);

	}

	void Reserve(int32 Num)
	{
		LocationX.Reserve(Num);
		LocationY.Reserve(Num);
		LocationZ.Reserve(Num);

		RotationX.Reserve(Num);
		RotationY.Reserve(Num);
		RotationZ.Reserve(Num);

		ScaleX.Reserve(Num);
		ScaleY.Reserve(Num);
		ScaleZ.Reserve(Num);
	}

	void AddToFloatChannels(int32 StartIndex, TArray<FMovieSceneFloatChannel> &FloatChannels, const TArray<FFrameNumber> &Times)
	{
		FloatChannels[StartIndex].Set(Times, MoveTemp(LocationX));
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex].Set(Times, MoveTemp(LocationY));
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex].Set(Times, MoveTemp(LocationZ));
		FloatChannels[StartIndex++].AutoSetTangents();

		FloatChannels[StartIndex].Set(Times, MoveTemp(RotationX));
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex].Set(Times, MoveTemp(RotationY));
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex].Set(Times, MoveTemp(RotationZ));
		FloatChannels[StartIndex++].AutoSetTangents();

		FloatChannels[StartIndex].Set(Times, MoveTemp(ScaleX));
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex].Set(Times, MoveTemp(ScaleY));
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex].Set(Times, MoveTemp(ScaleZ));
		FloatChannels[StartIndex++].AutoSetTangents();
	}

	//This function is the one that's called when recording live link incrementally. We move the values over from our saved 
	//Location, Rotation and Scale buffers into the specified float channels and then reset our buffers, re-using it's memory 
	//for the next iteration. We also fix any euler flips during this process, avoiding iterating over the data once again during Finalize.
	void AppendToFloatChannelsAndReset(int32 StartIndex, TArray<FMovieSceneFloatChannel>& FloatChannels, const TArray<FFrameNumber>& Times, TOptional<FVector>& LastRotationValues)
	{
		if (Times.Num() > 0)
		{
			FloatChannels[StartIndex++].AddKeys(Times, LocationX);
			LocationX.Reset();
			FloatChannels[StartIndex++].AddKeys(Times, LocationY);
			LocationY.Reset();
			FloatChannels[StartIndex++].AddKeys(Times, LocationZ);
			LocationZ.Reset();

			//fix euler flips
			if (LastRotationValues.IsSet())
			{
				FVector Val = LastRotationValues.GetValue();
				FMath::WindRelativeAnglesDegrees(Val.X, RotationX[0].Value);
				FMath::WindRelativeAnglesDegrees(Val.Y, RotationY[0].Value);
				FMath::WindRelativeAnglesDegrees(Val.Z, RotationZ[0].Value);
			}
			int32 TotalCount = Times.Num();
			
			for (int32 RotIndex = 0; RotIndex < TotalCount - 1; RotIndex++)
			{
				FMath::WindRelativeAnglesDegrees(RotationX[RotIndex].Value, RotationX[RotIndex + 1].Value);
				FMath::WindRelativeAnglesDegrees(RotationY[RotIndex].Value, RotationY[RotIndex + 1].Value);
				FMath::WindRelativeAnglesDegrees(RotationZ[RotIndex].Value, RotationZ[RotIndex + 1].Value);
			}
			FVector Vec(RotationX[TotalCount - 1].Value, RotationY[TotalCount - 1].Value, RotationZ[TotalCount - 1].Value); 
			LastRotationValues = Vec;

			FloatChannels[StartIndex++].AddKeys(Times, RotationX);
			RotationX.Reset();
			FloatChannels[StartIndex++].AddKeys(Times, RotationY);
			RotationY.Reset();
			FloatChannels[StartIndex++].AddKeys(Times, RotationZ);
			RotationZ.Reset();

			FloatChannels[StartIndex++].AddKeys(Times, ScaleX);
			ScaleX.Reset();
			FloatChannels[StartIndex++].AddKeys(Times, ScaleY);
			ScaleY.Reset();
			FloatChannels[StartIndex++].AddKeys(Times, ScaleZ);
			ScaleZ.Reset();
		}
	}

	void AutoSetTangents(int32 StartIndex, TArray<FMovieSceneFloatChannel> &FloatChannels)
	{
		//translate
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex++].AutoSetTangents();
		//rotate
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex++].AutoSetTangents();
		//scale
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex++].AutoSetTangents();

	}
};
