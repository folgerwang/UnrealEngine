// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithAreaLightActor.h"
#include "ObjectTemplates/DatasmithObjectTemplate.h"

#include "DatasmithAreaLightActorTemplate.generated.h"

UCLASS()
class DATASMITHCONTENT_API UDatasmithAreaLightActorTemplate : public UDatasmithObjectTemplate
{
	GENERATED_BODY()

public:
	UPROPERTY()
	EDatasmithAreaLightActorShape LightShape;

	UPROPERTY()
	FVector2D Dimensions;

	UPROPERTY()
	FLinearColor Color;

	UPROPERTY()
	float Intensity;

	UPROPERTY()
	uint8 bHidden:1;

	virtual void Apply( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const override;
};