// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
//

#include "MagicLeapStereoLayers.h"
#include "MagicLeapHMD.h"
#include "SceneViewExtension.h"

#include "CoreMinimal.h"

IStereoLayers* FMagicLeapHMD::GetStereoLayers()
{
	if (!DefaultStereoLayers.IsValid())
	{
		TSharedPtr<FMagicLeapStereoLayers, ESPMode::ThreadSafe> NewLayersPtr = FSceneViewExtensions::NewExtension<FMagicLeapStereoLayers>(this);
		DefaultStereoLayers = StaticCastSharedPtr<FDefaultStereoLayers>(NewLayersPtr);
	}
	return DefaultStereoLayers.Get();
}

