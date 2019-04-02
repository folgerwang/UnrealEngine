// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ObjectTemplates/DatasmithPostProcessVolumeTemplate.h"

#include "ObjectTemplates/DatasmithActorTemplate.h"

#include "Engine/PostProcessVolume.h"

void UDatasmithPostProcessVolumeTemplate::Apply( UObject* Destination, bool bForce )
{
#if WITH_EDITORONLY_DATA

	APostProcessVolume* PostProcessVolume = UDatasmithActorTemplate::GetActor< APostProcessVolume >( Destination );
	if ( !PostProcessVolume )
	{
		return;
	}

	UDatasmithPostProcessVolumeTemplate* PreviousTemplate = !bForce ? FDatasmithObjectTemplateUtils::GetObjectTemplate< UDatasmithPostProcessVolumeTemplate >( Destination ) : nullptr;

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bEnabled, PostProcessVolume, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bUnbound, PostProcessVolume, PreviousTemplate );

	Settings.Apply( &PostProcessVolume->Settings, PreviousTemplate ? &PreviousTemplate->Settings : nullptr );

	FDatasmithObjectTemplateUtils::SetObjectTemplate( PostProcessVolume->GetRootComponent(), this );
#endif // #if WITH_EDITORONLY_DATA
}

void UDatasmithPostProcessVolumeTemplate::Load( const UObject* Source )
{
#if WITH_EDITORONLY_DATA
	const APostProcessVolume* PostProcessVolume = UDatasmithActorTemplate::GetActor< const APostProcessVolume >( Source );
	if ( !PostProcessVolume )
	{
		return;
	}

	bEnabled = PostProcessVolume->bEnabled;
	bUnbound = PostProcessVolume->bUnbound;

	Settings.Load( PostProcessVolume->Settings );
#endif // #if WITH_EDITORONLY_DATA
}

bool UDatasmithPostProcessVolumeTemplate::Equals( const UDatasmithObjectTemplate* Other ) const
{
	const UDatasmithPostProcessVolumeTemplate* TypedOther = Cast< UDatasmithPostProcessVolumeTemplate >( Other );

	if ( !TypedOther )
	{
		return false;
	}

	bool bEquals = ( bEnabled == TypedOther->bEnabled );
	bEquals = bEquals && ( bUnbound == TypedOther->bUnbound );
	bEquals = bEquals && Settings.Equals( TypedOther->Settings );

	return bEquals;
}
