// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshReductionSettings.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#if WITH_EDITOR

#include "IMeshReductionInterfaces.h"
#include "Modules/ModuleManager.h"
#include "IMeshReductionManagerModule.h"

bool FSkeletalMeshOptimizationSettings::IsReductionSettingActive()
{
	auto UseNativeReductionTool = []()->bool
	{
		IMeshReduction* SkeletalReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface").GetSkeletalMeshReductionInterface();
		if (SkeletalReductionModule == nullptr)
		{
			return false;
		}
		FString ModuleVersionString = SkeletalReductionModule->GetVersionString();
		TArray<FString> SplitVersionString;
		ModuleVersionString.ParseIntoArray(SplitVersionString, TEXT("_"), true);
		return SplitVersionString[0].Equals("QuadricSkeletalMeshReduction");
	};

	float Threshold_One = (1.0f - KINDA_SMALL_NUMBER);
	float Threshold_Zero = (0.0f + KINDA_SMALL_NUMBER);
	if (UseNativeReductionTool())
	{
		switch (TerminationCriterion)
		{
		case SkeletalMeshTerminationCriterion::SMTC_NumOfTriangles:
		{
			return NumOfTrianglesPercentage < Threshold_One;
		}
		break;
		case SkeletalMeshTerminationCriterion::SMTC_NumOfVerts:
		{
			return NumOfVertPercentage < Threshold_One;
		}
		break;
		case SkeletalMeshTerminationCriterion::SMTC_TriangleOrVert:
		{
			return NumOfTrianglesPercentage < Threshold_One || NumOfVertPercentage < Threshold_One;
		}
		break;
		}
	}
	else
	{
		switch (ReductionMethod)
		{
		case SkeletalMeshOptimizationType::SMOT_NumOfTriangles:
		{
			return NumOfTrianglesPercentage < Threshold_One;
		}
		break;
		case SkeletalMeshOptimizationType::SMOT_MaxDeviation:
		{
			return MaxDeviationPercentage > Threshold_Zero;
		}
		break;
		case SkeletalMeshOptimizationType::SMOT_TriangleOrDeviation:
		{
			return NumOfTrianglesPercentage < Threshold_One || MaxDeviationPercentage > Threshold_Zero;
		}
		break;
		}
	}

	return false;
}
#endif //WITH_EDITOR