// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
//

#include "MagicLeapStereoLayers.h"
#include "MagicLeapHMD.h"
#include "SceneViewExtension.h"
#include "RHI.h"
#include "RHIDefinitions.h"

#include "CoreMinimal.h"

IStereoLayers* FMagicLeapHMD::GetStereoLayers()
{
#if PLATFORM_LUMIN
	if (!DefaultStereoLayers.IsValid())
	{
		TSharedPtr<FMagicLeapStereoLayers, ESPMode::ThreadSafe> NewLayersPtr = FSceneViewExtensions::NewExtension<FMagicLeapStereoLayers>(this);
		DefaultStereoLayers = StaticCastSharedPtr<FDefaultStereoLayers>(NewLayersPtr);
	}
#endif //PLATFORM_LUMIN
	return DefaultStereoLayers.Get();
}

IStereoLayers::FLayerDesc FMagicLeapStereoLayers::GetDebugCanvasLayerDesc(FTextureRHIRef Texture)
{
	IStereoLayers::FLayerDesc StereoLayerDesc;
	StereoLayerDesc.Transform = FTransform(FVector(110.f, 0.f, 0.f));
	if (IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		StereoLayerDesc.Transform.SetScale3D(FVector(1.f, 1.f, -1.f));
	}
	StereoLayerDesc.QuadSize = FVector2D(75.f, 40.f);
	StereoLayerDesc.PositionType = IStereoLayers::ELayerType::FaceLocked;
	StereoLayerDesc.ShapeType = IStereoLayers::ELayerShape::QuadLayer;
	StereoLayerDesc.Texture = Texture;
	StereoLayerDesc.Flags = IStereoLayers::ELayerFlags::LAYER_FLAG_TEX_CONTINUOUS_UPDATE;
	StereoLayerDesc.Flags |= IStereoLayers::ELayerFlags::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO;
	return StereoLayerDesc;
}
