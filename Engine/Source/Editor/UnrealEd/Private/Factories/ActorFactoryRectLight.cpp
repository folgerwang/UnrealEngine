// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ActorFactoryRectLight.cpp: Factory for RectLight
=============================================================================*/

#include "ActorFactories/ActorFactoryRectLight.h"
#include "GameFramework/Actor.h"
#include "Components/RectLightComponent.h"

void UActorFactoryRectLight::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	// Make all spawned actors use the candela units.
	TArray<URectLightComponent*> RectLightComponents;
	NewActor->GetComponents<URectLightComponent>(RectLightComponents);
	for (URectLightComponent* Component : RectLightComponents)
	{
		if (Component && Component->CreationMethod == EComponentCreationMethod::Native)
		{
			static const auto CVarDefaultSpotLightUnits = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultFeature.SpotLightUnits"));
			ELightUnits DefaultUnits = (ELightUnits)CVarDefaultSpotLightUnits->GetValueOnAnyThread();

			Component->Intensity *= URectLightComponent::GetUnitsConversionFactor(Component->IntensityUnits, DefaultUnits);
			Component->IntensityUnits = DefaultUnits;
		}
	}
}

