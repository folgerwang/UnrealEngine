// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ObjectTemplates/DatasmithCineCameraActorTemplate.h"

FDatasmithCameraLookatTrackingSettingsTemplate::FDatasmithCameraLookatTrackingSettingsTemplate()
	: bEnableLookAtTracking( 0 )
{
}

void FDatasmithCameraLookatTrackingSettingsTemplate::Apply( FCameraLookatTrackingSettings* Destination, FDatasmithCameraLookatTrackingSettingsTemplate* PreviousTemplate )
{
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bEnableLookAtTracking, Destination, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSETSOFTOBJECTPTR( ActorToTrack, Destination, PreviousTemplate );
}

void FDatasmithCameraLookatTrackingSettingsTemplate::Load( const FCameraLookatTrackingSettings& Source )
{
	bEnableLookAtTracking = Source.bEnableLookAtTracking;
	ActorToTrack = Source.ActorToTrack;
}

bool FDatasmithCameraLookatTrackingSettingsTemplate::Equals( const FDatasmithCameraLookatTrackingSettingsTemplate& Other ) const
{
	bool bEquals = ( bEnableLookAtTracking == Other.bEnableLookAtTracking );
	bEquals = bEquals && ( ActorToTrack == Other.ActorToTrack );

	return bEquals;
}

void UDatasmithCineCameraActorTemplate::Apply( UObject* Destination, bool bForce )
{
#if WITH_EDITORONLY_DATA
	ACineCameraActor* CineCameraActor = Cast< ACineCameraActor >( Destination );

	if ( !CineCameraActor )
	{
		return;
	}

	UDatasmithCineCameraActorTemplate* PreviousTemplate = !bForce ? FDatasmithObjectTemplateUtils::GetObjectTemplate< UDatasmithCineCameraActorTemplate >( Destination ) : nullptr;

	LookatTrackingSettings.Apply( &CineCameraActor->LookatTrackingSettings, PreviousTemplate ? &PreviousTemplate->LookatTrackingSettings : nullptr );

	FDatasmithObjectTemplateUtils::SetObjectTemplate( CineCameraActor->GetRootComponent(), this );
#endif // #if WITH_EDITORONLY_DATA
}

void UDatasmithCineCameraActorTemplate::Load( const UObject* Source )
{
#if WITH_EDITORONLY_DATA
	const ACineCameraActor* CineCameraActor = Cast< ACineCameraActor >( Source );

	if ( !CineCameraActor )
	{
		return;
	}

	LookatTrackingSettings.Load( CineCameraActor->LookatTrackingSettings );
#endif // #if WITH_EDITORONLY_DATA
}

bool UDatasmithCineCameraActorTemplate::Equals( const UDatasmithObjectTemplate* Other ) const
{
	const UDatasmithCineCameraActorTemplate* TypedOther = Cast< UDatasmithCineCameraActorTemplate >( Other );

	if ( !TypedOther )
	{
		return false;
	}

	bool bEquals = LookatTrackingSettings.Equals( TypedOther->LookatTrackingSettings );

	return bEquals;
}
