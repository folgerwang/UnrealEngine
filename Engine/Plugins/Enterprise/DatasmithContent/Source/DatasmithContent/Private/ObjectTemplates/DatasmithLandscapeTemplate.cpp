// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ObjectTemplates/DatasmithLandscapeTemplate.h"

#include "Landscape.h"
#include "ObjectTemplates/DatasmithActorTemplate.h"

void UDatasmithLandscapeTemplate::Apply( UObject* Destination, bool bForce )
{
#if WITH_EDITORONLY_DATA
	ALandscape* Landscape = UDatasmithActorTemplate::GetActor< ALandscape >( Destination );

	if( !Landscape )
	{
		return;
	}

	UDatasmithLandscapeTemplate* PreviousTemplate = !bForce ? FDatasmithObjectTemplateUtils::GetObjectTemplate< UDatasmithLandscapeTemplate >( Destination ) : nullptr;

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET(LandscapeMaterial, Landscape, PreviousTemplate);
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET(StaticLightingLOD, Landscape, PreviousTemplate);

	FDatasmithObjectTemplateUtils::SetObjectTemplate( Landscape->GetRootComponent(), this );
#endif // #if WITH_EDITORONLY_DATA
}

void UDatasmithLandscapeTemplate::Load( const UObject* Source )
{
#if WITH_EDITORONLY_DATA
	const ALandscape* Landscape = UDatasmithActorTemplate::GetActor< ALandscape >( Source );

	if( !Landscape )
	{
		return;
	}

	LandscapeMaterial = Landscape->LandscapeMaterial;
	StaticLightingLOD = Landscape->StaticLightingLOD;
#endif // #if WITH_EDITORONLY_DATA
}

bool UDatasmithLandscapeTemplate::Equals( const UDatasmithObjectTemplate* Other ) const
{
	const UDatasmithLandscapeTemplate* TypedOther = Cast< UDatasmithLandscapeTemplate >( Other );

	if ( !TypedOther )
	{
		return false;
	}

	bool bEquals = ( LandscapeMaterial == TypedOther->LandscapeMaterial );
	bEquals = bEquals && ( StaticLightingLOD == TypedOther->StaticLightingLOD );

	return bEquals;
}
