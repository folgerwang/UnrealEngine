// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ObjectTemplates/DatasmithSceneComponentTemplate.h"

#include "DatasmithAssetUserData.h"
#include "Components/SceneComponent.h"

void UDatasmithSceneComponentTemplate::Apply( UObject* Destination, bool bForce )
{
#if WITH_EDITORONLY_DATA
	USceneComponent* SceneComponent = Cast< USceneComponent >( Destination );

	if ( !SceneComponent )
	{
		return;
	}

	UDatasmithSceneComponentTemplate* PreviousTemplate = !bForce ? FDatasmithObjectTemplateUtils::GetObjectTemplate< UDatasmithSceneComponentTemplate >( Destination ) : nullptr;

	if ( !PreviousTemplate || PreviousTemplate->RelativeTransform.Equals( SceneComponent->GetRelativeTransform() ) )
	{
		SceneComponent->SetRelativeTransform( RelativeTransform );
	}

	if ( !PreviousTemplate || PreviousTemplate->Mobility == SceneComponent->Mobility )
	{
		SceneComponent->SetMobility( Mobility );
	}

	if ( !PreviousTemplate || PreviousTemplate->AttachParent == SceneComponent->GetAttachParent() )
	{
		SceneComponent->AttachToComponent( AttachParent, FAttachmentTransformRules::KeepRelativeTransform );
	}

	FDatasmithObjectTemplateUtils::SetObjectTemplate( Destination, this );
#endif // #if WITH_EDITORONLY_DATA
}

void UDatasmithSceneComponentTemplate::Load( const UObject* Source )
{
#if WITH_EDITORONLY_DATA
	const USceneComponent* SceneComponent = Cast< USceneComponent >( Source );

	if ( !SceneComponent )
	{
		return;
	}

	RelativeTransform = SceneComponent->GetRelativeTransform();
	Mobility = SceneComponent->Mobility;
	AttachParent = SceneComponent->GetAttachParent();
#endif // #if WITH_EDITORONLY_DATA
}