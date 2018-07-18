// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RigUnit_BlendTransform.h"
#include "Units/RigUnitContext.h"
#include "AnimationRuntime.h"

void FRigUnit_BlendTransform::Execute(const FRigUnitContext& InContext)
{
	if (Targets.Num() > 0)
	{
		float TotalSum = 0.f;
		TArray<FTransform> BlendTransform;
		TArray<float> BlendWeights;
		for (int32 Index = 0; Index < Targets.Num(); ++Index)
		{
			if (Targets[Index].Weight > ZERO_ANIMWEIGHT_THRESH)
			{
				BlendTransform.Add(Targets[Index].Transform);
				BlendWeights.Add(Targets[Index].Weight);
				TotalSum += Targets[Index].Weight;
			}
		}

		if (BlendTransform.Num() > 0)
		{
			if (TotalSum > 1.f )
			{
				for (int32 Index = 0; Index < BlendWeights.Num(); ++Index)
				{
					BlendWeights[Index] /= TotalSum;
				}
			}

			const float SourceWeight = FMath::Clamp(1.f - (TotalSum), 0.f, 1.f);
			if (SourceWeight > ZERO_ANIMWEIGHT_THRESH)
			{
				BlendTransform.Add(Source);
				BlendWeights.Add(SourceWeight);
			}

			FAnimationRuntime::BlendTransformsByWeight(Result, BlendTransform, BlendWeights);
		}
	}

	// if failed on any of the above, it will just use source as target pose
	Result = Source;
}
