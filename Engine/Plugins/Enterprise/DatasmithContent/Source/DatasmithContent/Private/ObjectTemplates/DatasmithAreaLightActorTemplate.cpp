// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ObjectTemplates/DatasmithAreaLightActorTemplate.h"

void UDatasmithAreaLightActorTemplate::Apply( UObject* Destination, bool bForce )
{
#if WITH_EDITORONLY_DATA
	ADatasmithAreaLightActor* AreaLightActor = Cast< ADatasmithAreaLightActor >( Destination );

	if ( !AreaLightActor )
	{
		return;
	}

	UDatasmithAreaLightActorTemplate* PreviousTemplate = !bForce ? FDatasmithObjectTemplateUtils::GetObjectTemplate< UDatasmithAreaLightActorTemplate >( Destination ) : nullptr;

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( LightShape, AreaLightActor, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( Dimensions, AreaLightActor, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( Color, AreaLightActor, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( Intensity, AreaLightActor, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bHidden, AreaLightActor, PreviousTemplate );

	FDatasmithObjectTemplateUtils::SetObjectTemplate( AreaLightActor->GetRootComponent(), this );
#endif // #if WITH_EDITORONLY_DATA
}

void UDatasmithAreaLightActorTemplate::Load( const UObject* Source )
{
#if WITH_EDITORONLY_DATA
	const ADatasmithAreaLightActor* AreaLightActor = Cast< ADatasmithAreaLightActor >( Source );

	if ( !AreaLightActor )
	{
		const UActorComponent* ActorComponent = Cast< UActorComponent >( Source );

		if ( !ActorComponent )
		{
			return;
		}
		else
		{
			AreaLightActor = Cast< ADatasmithAreaLightActor >( ActorComponent->GetOwner() );

			if ( !AreaLightActor )
			{
				return;
			}
		}
	}

	LightShape = AreaLightActor->LightShape;
	Dimensions = AreaLightActor->Dimensions;
	Color = AreaLightActor->Color;
	Intensity = AreaLightActor->Intensity;
	bHidden = AreaLightActor->bHidden;
#endif // #if WITH_EDITORONLY_DATA
}

bool UDatasmithAreaLightActorTemplate::Equals( const UDatasmithObjectTemplate* Other ) const
{
	const UDatasmithAreaLightActorTemplate* TypedOther = Cast< UDatasmithAreaLightActorTemplate >( Other );

	if ( !TypedOther )
	{
		return false;
	}

	bool bEquals = ( LightShape == TypedOther->LightShape );
	bEquals = bEquals && Dimensions.Equals( TypedOther->Dimensions );
	bEquals = bEquals && Color.Equals( TypedOther->Color );
	bEquals = bEquals && FMath::IsNearlyEqual( Intensity, TypedOther->Intensity );
	bEquals = bEquals && ( bHidden == TypedOther->bHidden );

	return bEquals;
}
