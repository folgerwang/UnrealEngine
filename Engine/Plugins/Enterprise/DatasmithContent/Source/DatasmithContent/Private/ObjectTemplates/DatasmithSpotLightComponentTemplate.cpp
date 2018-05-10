// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ObjectTemplates/DatasmithSpotLightComponentTemplate.h"

#include "Components/SpotLightComponent.h"

UDatasmithSpotLightComponentTemplate::UDatasmithSpotLightComponentTemplate()
{
	Load( USpotLightComponent::StaticClass()->GetDefaultObject() );
}

void UDatasmithSpotLightComponentTemplate::Apply( UObject* Destination, bool bForce )
{
#if WITH_EDITORONLY_DATA
	USpotLightComponent* SpotLightComponent = Cast< USpotLightComponent >( Destination );

	if ( !SpotLightComponent )
	{
		return;
	}

	UDatasmithSpotLightComponentTemplate* PreviousTemplate = !bForce ? FDatasmithObjectTemplateUtils::GetObjectTemplate< UDatasmithSpotLightComponentTemplate >( Destination ) : nullptr;

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( InnerConeAngle, SpotLightComponent, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( OuterConeAngle, SpotLightComponent, PreviousTemplate );

	FDatasmithObjectTemplateUtils::SetObjectTemplate( Destination, this );
#endif // #if WITH_EDITORONLY_DATA
}

void UDatasmithSpotLightComponentTemplate::Load( const UObject* Source )
{
#if WITH_EDITORONLY_DATA
	const USpotLightComponent* SpotLightComponent = Cast< USpotLightComponent >( Source );

	if ( !SpotLightComponent )
	{
		return;
	}
	
	InnerConeAngle = SpotLightComponent->InnerConeAngle;
	OuterConeAngle = SpotLightComponent->OuterConeAngle;
#endif // #if WITH_EDITORONLY_DATA
}

bool UDatasmithSpotLightComponentTemplate::Equals( const UDatasmithObjectTemplate* Other ) const
{
	const UDatasmithSpotLightComponentTemplate* TypedOther = Cast< UDatasmithSpotLightComponentTemplate >( Other );

	if ( !TypedOther )
	{
		return false;
	}

	bool bEquals = FMath::IsNearlyEqual( InnerConeAngle, TypedOther->InnerConeAngle );
	bEquals = bEquals && FMath::IsNearlyEqual( OuterConeAngle, TypedOther->OuterConeAngle );

	return bEquals;
}
