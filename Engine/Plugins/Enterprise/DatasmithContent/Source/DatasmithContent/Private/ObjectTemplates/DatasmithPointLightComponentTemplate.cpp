// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ObjectTemplates/DatasmithPointLightComponentTemplate.h"

#include "Components/PointLightComponent.h"

UDatasmithPointLightComponentTemplate::UDatasmithPointLightComponentTemplate()
{
	Load( UPointLightComponent::StaticClass()->GetDefaultObject() );
}

void UDatasmithPointLightComponentTemplate::Apply( UObject* Destination, bool bForce )
{
#if WITH_EDITORONLY_DATA
	UPointLightComponent* PointLightComponent = Cast< UPointLightComponent >( Destination );

	if ( !PointLightComponent )
	{
		return;
	}

	UDatasmithPointLightComponentTemplate* PreviousTemplate = !bForce ? FDatasmithObjectTemplateUtils::GetObjectTemplate< UDatasmithPointLightComponentTemplate >( Destination ) : nullptr;

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( IntensityUnits, PointLightComponent, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( SourceRadius, PointLightComponent, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( SourceLength, PointLightComponent, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( AttenuationRadius, PointLightComponent, PreviousTemplate );

	FDatasmithObjectTemplateUtils::SetObjectTemplate( Destination, this );
#endif // #if WITH_EDITORONLY_DATA
}

void UDatasmithPointLightComponentTemplate::Load( const UObject* Source )
{
#if WITH_EDITORONLY_DATA
	const UPointLightComponent* PointLightComponent = Cast< UPointLightComponent >( Source );

	if ( !PointLightComponent )
	{
		return;
	}

	IntensityUnits = PointLightComponent->IntensityUnits;
	SourceRadius = PointLightComponent->SourceRadius;
	SourceLength = PointLightComponent->SourceLength;
	AttenuationRadius = PointLightComponent->AttenuationRadius;
#endif // #if WITH_EDITORONLY_DATA
}

bool UDatasmithPointLightComponentTemplate::Equals( const UDatasmithObjectTemplate* Other ) const
{
	const UDatasmithPointLightComponentTemplate* TypedOther = Cast< UDatasmithPointLightComponentTemplate >( Other );

	if ( !TypedOther )
	{
		return false;
	}

	bool bEquals = IntensityUnits == TypedOther->IntensityUnits;
	bEquals = bEquals && FMath::IsNearlyEqual( SourceRadius, TypedOther->SourceRadius );
	bEquals = bEquals && FMath::IsNearlyEqual( SourceLength, TypedOther->SourceLength );
	bEquals = bEquals && FMath::IsNearlyEqual( AttenuationRadius, TypedOther->AttenuationRadius );

	return bEquals;
}
