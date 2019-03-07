// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "ObjectTemplates/DatasmithObjectTemplate.h"

#include "DatasmithActorTemplate.generated.h"

// hold template informations common to all AActors.
UCLASS()
class DATASMITHCONTENT_API UDatasmithActorTemplate : public UDatasmithObjectTemplate
{
	GENERATED_BODY()

public:

	/** Layers this actor belongs to. (see AActor::Layers) */
	UPROPERTY()
	TSet<FName> Layers;

	/** Tags on this actor. (see AActor::Tags) */
	UPROPERTY()
	TSet<FName> Tags;

	virtual void Apply( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const override;

	/** Helper function to get the typed Actor from either a component or an actor */
	template< typename T >
	static T* GetActor( UObject* Object )
	{
		const UActorComponent* ActorComponent = Cast< UActorComponent >( Object );
		return Cast< T >( ActorComponent ? ActorComponent->GetOwner() : Object );
	}

	template< typename T >
	static const T* GetActor( const UObject* Object )
	{
		const UActorComponent* ActorComponent = Cast< UActorComponent >( Object );
		return Cast< T >( ActorComponent ? static_cast< const AActor* >( ActorComponent->GetOwner() ) : Object );
	}
};