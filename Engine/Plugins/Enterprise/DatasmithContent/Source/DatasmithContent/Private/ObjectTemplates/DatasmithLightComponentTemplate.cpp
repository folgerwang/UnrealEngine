// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ObjectTemplates/DatasmithLightComponentTemplate.h"

#include "Components/LightComponent.h"

UDatasmithLightComponentTemplate::UDatasmithLightComponentTemplate()
{
}

void UDatasmithLightComponentTemplate::Apply( UObject* Destination, bool bForce )
{
#if WITH_EDITORONLY_DATA
	ULightComponent* LightComponent = Cast< ULightComponent >( Destination );

	if ( !LightComponent )
	{
		return;
	}

	UDatasmithLightComponentTemplate* PreviousTemplate = !bForce ? FDatasmithObjectTemplateUtils::GetObjectTemplate< UDatasmithLightComponentTemplate >( Destination ) : nullptr;

	if ( !PreviousTemplate || LightComponent->IsVisible() == PreviousTemplate->bVisible )
	{
		LightComponent->SetVisibility( bVisible );
	}

	if ( !PreviousTemplate || LightComponent->Intensity == PreviousTemplate->Intensity )
	{
		LightComponent->SetIntensity( Intensity );
	}

	if ( !PreviousTemplate || LightComponent->CastShadows == PreviousTemplate->CastShadows )
	{
		LightComponent->SetCastShadows( CastShadows );
	}

	if ( !PreviousTemplate || LightComponent->LightColor == PreviousTemplate->LightColor.ToFColor( true ) )
	{
		LightComponent->SetLightColor( LightColor );
	}

	if ( !PreviousTemplate || LightComponent->LightFunctionMaterial == PreviousTemplate->LightFunctionMaterial )
	{
		LightComponent->SetLightFunctionMaterial( LightFunctionMaterial );
	}

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bUseTemperature, LightComponent, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( Temperature, LightComponent, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( IESTexture, LightComponent, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bUseIESBrightness, LightComponent, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( IESBrightnessScale, LightComponent, PreviousTemplate );

	FDatasmithObjectTemplateUtils::SetObjectTemplate( Destination, this );
#endif // #if WITH_EDITORONLY_DATA
}

void UDatasmithLightComponentTemplate::Load( const UObject* Source )
{
#if WITH_EDITORONLY_DATA
	const ULightComponent* LightComponent = Cast< ULightComponent >( Source );

	if ( !LightComponent )
	{
		return;
	}

	bVisible = LightComponent->IsVisible();
	Intensity = LightComponent->Intensity;
	CastShadows = LightComponent->CastShadows;
	LightColor = LightComponent->LightColor;
	LightFunctionMaterial = LightComponent->LightFunctionMaterial;

	bUseTemperature = LightComponent->bUseTemperature;
	Temperature = LightComponent->Temperature;

	bUseIESBrightness = LightComponent->bUseIESBrightness;
	IESBrightnessScale = LightComponent->IESBrightnessScale;
#endif // #if WITH_EDITORONLY_DATA
}