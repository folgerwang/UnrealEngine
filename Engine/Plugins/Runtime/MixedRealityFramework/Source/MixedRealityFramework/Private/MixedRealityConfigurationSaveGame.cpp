// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MixedRealityConfigurationSaveGame.h"
#include "Materials/MaterialInstanceDynamic.h"

UMixedRealityCalibrationData::UMixedRealityCalibrationData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

UMixedRealityConfigurationSaveGame::UMixedRealityConfigurationSaveGame(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SaveSlotName = TEXT("MixedRealityConfigurationSaveSlot");
	UserIndex = 0;
	ConfigurationSaveVersion = 0;
}

/* FChromaKeyParams
 *****************************************************************************/

//------------------------------------------------------------------------------
void FChromaKeyParams::ApplyToMaterial(UMaterialInstanceDynamic* Material) const
{
	if (Material)
	{
		static FName ColorName("ChromaColor");
		Material->SetVectorParameterValue(ColorName, ChromaColor);

		static FName ClipThresholdName("ChromaClipThreshold");
		Material->SetScalarParameterValue(ClipThresholdName, ChromaClipThreshold);

		static FName ToleranceCapName("ChromaToleranceCap");
		Material->SetScalarParameterValue(ToleranceCapName, ChromaToleranceCap);

		static FName EdgeSoftnessName("EdgeSoftness");
		Material->SetScalarParameterValue(EdgeSoftnessName, EdgeSoftness);
	}
}