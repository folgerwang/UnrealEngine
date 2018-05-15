// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RectLightComponentVisualizer.h"
#include "SceneManagement.h"
#include "Components/RectLightComponent.h"


void FRectLightComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	if(View->Family->EngineShowFlags.LightRadius)
	{
		const URectLightComponent* RectLightComp = Cast<const URectLightComponent>(Component);
		if(RectLightComp != NULL)
		{
			FTransform LightTM = RectLightComp->GetComponentTransform();
			LightTM.RemoveScaling();

			// Draw light radius
			DrawWireSphereAutoSides(PDI, LightTM, FColor(200, 255, 255), RectLightComp->AttenuationRadius, SDPG_World);

			FBox Box(
				FVector( 0.0f, -0.5f * RectLightComp->SourceWidth, -0.5f * RectLightComp->SourceHeight ),
				FVector( 0.0f,  0.5f * RectLightComp->SourceWidth,  0.5f * RectLightComp->SourceHeight )
			);

			DrawWireBox(PDI, LightTM.ToMatrixNoScale(), Box, FColor(231, 239, 0, 255), SDPG_World);
		}
	}
}
