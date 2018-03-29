// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ObjectTemplates/DatasmithCineCameraComponentTemplate.h"

#include "CineCameraComponent.h"

void FDatasmithCameraFilmbackSettingsTemplate::Apply( FCameraFilmbackSettings* Destination, const FDatasmithCameraFilmbackSettingsTemplate* PreviousTemplate )
{
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( SensorWidth, Destination, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( SensorHeight, Destination, PreviousTemplate );
}

void FDatasmithCameraFilmbackSettingsTemplate::Load( const FCameraFilmbackSettings& Source )
{
	SensorWidth = Source.SensorWidth;
	SensorHeight = Source.SensorHeight;
}

FDatasmithPostProcessSettingsTemplate::FDatasmithPostProcessSettingsTemplate()
{
	Load( FPostProcessSettings() );
}

void FDatasmithPostProcessSettingsTemplate::Apply( FPostProcessSettings* Destination, const FDatasmithPostProcessSettingsTemplate* PreviousTemplate )
{
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bOverride_WhiteTemp, Destination, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( WhiteTemp, Destination, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bOverride_VignetteIntensity, Destination, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( VignetteIntensity, Destination, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bOverride_FilmWhitePoint, Destination, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( FilmWhitePoint, Destination, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bOverride_ColorSaturation, Destination, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( ColorSaturation, Destination, PreviousTemplate );
}

void FDatasmithPostProcessSettingsTemplate::Load( const FPostProcessSettings& Source )
{
	bOverride_WhiteTemp = Source.bOverride_WhiteTemp;
	WhiteTemp = Source.WhiteTemp;

	bOverride_VignetteIntensity = Source.bOverride_VignetteIntensity;
	VignetteIntensity = Source.VignetteIntensity;

	bOverride_FilmWhitePoint = Source.bOverride_FilmWhitePoint;
	FilmWhitePoint = Source.FilmWhitePoint;

	bOverride_ColorSaturation = Source.bOverride_ColorSaturation;
	ColorSaturation = Source.ColorSaturation;
}

void FDatasmithCameraLensSettingsTemplate::Apply( FCameraLensSettings* Destination, const FDatasmithCameraLensSettingsTemplate* PreviousTemplate )
{
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( MaxFStop, Destination, PreviousTemplate );
}

void FDatasmithCameraLensSettingsTemplate::Load( const FCameraLensSettings& Source )
{
	MaxFStop = Source.MaxFStop;
}

void FDatasmithCameraFocusSettingsTemplate::Apply( FCameraFocusSettings* Destination, const FDatasmithCameraFocusSettingsTemplate* PreviousTemplate )
{
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( ManualFocusDistance, Destination, PreviousTemplate );
}

void FDatasmithCameraFocusSettingsTemplate::Load( const FCameraFocusSettings& Source )
{
	ManualFocusDistance = Source.ManualFocusDistance;
}

void UDatasmithCineCameraComponentTemplate::Apply( UObject* Destination, bool bForce )
{
#if WITH_EDITORONLY_DATA
	UCineCameraComponent* CineCameraComponent = Cast< UCineCameraComponent >( Destination );

	if ( !CineCameraComponent )
	{
		return;
	}

	UDatasmithCineCameraComponentTemplate* PreviousTemplate = !bForce ? FDatasmithObjectTemplateUtils::GetObjectTemplate< UDatasmithCineCameraComponentTemplate >( Destination ) : nullptr;

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( CurrentFocalLength, CineCameraComponent, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( CurrentAperture, CineCameraComponent, PreviousTemplate );

	FilmbackSettings.Apply( &CineCameraComponent->FilmbackSettings, PreviousTemplate ? &PreviousTemplate->FilmbackSettings : nullptr );
	LensSettings.Apply( &CineCameraComponent->LensSettings, PreviousTemplate ? &PreviousTemplate->LensSettings : nullptr );
	FocusSettings.Apply( &CineCameraComponent->FocusSettings, PreviousTemplate ? &PreviousTemplate->FocusSettings : nullptr );

	PostProcessSettings.Apply( &CineCameraComponent->PostProcessSettings, PreviousTemplate ? &PreviousTemplate->PostProcessSettings : nullptr );

	FDatasmithObjectTemplateUtils::SetObjectTemplate( Destination, this );
#endif // #if WITH_EDITORONLY_DATA
}

void UDatasmithCineCameraComponentTemplate::Load( const UObject* Source )
{
#if WITH_EDITORONLY_DATA
	const UCineCameraComponent* CineCameraComponent = Cast< UCineCameraComponent >( Source );

	if ( !CineCameraComponent )
	{
		return;
	}

	CurrentFocalLength = CineCameraComponent->CurrentFocalLength;
	CurrentAperture = CineCameraComponent->CurrentAperture;

	FilmbackSettings.Load( CineCameraComponent->FilmbackSettings );
	LensSettings.Load( CineCameraComponent->LensSettings );
	FocusSettings.Load( CineCameraComponent->FocusSettings );

	PostProcessSettings.Load( CineCameraComponent->PostProcessSettings );
#endif // #if WITH_EDITORONLY_DATA
}