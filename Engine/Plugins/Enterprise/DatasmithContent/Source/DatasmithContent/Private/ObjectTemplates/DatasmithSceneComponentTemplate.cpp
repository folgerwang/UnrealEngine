// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ObjectTemplates/DatasmithSceneComponentTemplate.h"

#include "DatasmithAssetUserData.h"
#include "Components/SceneComponent.h"

namespace
{
	bool AreTransformsEqual( const FTransform& A, const FTransform& B )
	{
		return A.TranslationEquals( B, THRESH_POINTS_ARE_NEAR ) && A.RotationEquals( B, KINDA_SMALL_NUMBER) && A.Scale3DEquals( B, KINDA_SMALL_NUMBER );
	}
}

void UDatasmithSceneComponentTemplate::Apply( UObject* Destination, bool bForce )
{
#if WITH_EDITORONLY_DATA
	USceneComponent* SceneComponent = Cast< USceneComponent >( Destination );

	if ( !SceneComponent )
	{
		return;
	}

	UDatasmithSceneComponentTemplate* PreviousTemplate = !bForce ? FDatasmithObjectTemplateUtils::GetObjectTemplate< UDatasmithSceneComponentTemplate >( Destination ) : nullptr;

	if ( !PreviousTemplate || PreviousTemplate->Mobility == SceneComponent->Mobility )
	{
		SceneComponent->SetMobility( Mobility );
	}

	if ( !PreviousTemplate || PreviousTemplate->AttachParent == SceneComponent->GetAttachParent() )
	{
		FAttachmentTransformRules AttachmentTransformRules = FAttachmentTransformRules::KeepRelativeTransform;

		// We assume that all Datasmith components were created with a parent.
		// If we already have a component template but no parent, it means that we got detached since the last import,
		// in which case we want to keep the world position when reattaching.
		const bool bLostItsParent = PreviousTemplate && PreviousTemplate->AttachParent == nullptr;

		if ( bLostItsParent )
		{
			AttachmentTransformRules = FAttachmentTransformRules::KeepWorldTransform;
		}

		SceneComponent->AttachToComponent( AttachParent, AttachmentTransformRules );
	}

	if ( !PreviousTemplate || AreTransformsEqual( PreviousTemplate->RelativeTransform, SceneComponent->GetRelativeTransform() ) )
	{
		SceneComponent->SetRelativeTransform( RelativeTransform );
	}


	if ( !PreviousTemplate )
	{
		SceneComponent->ComponentTags = Tags.Array();
	}
	else
	{
		SceneComponent->ComponentTags = FDatasmithObjectTemplateUtils::ThreeWaySetMerge(PreviousTemplate->Tags, TSet<FName>(SceneComponent->ComponentTags), Tags).Array();
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
	Tags = TSet<FName>(SceneComponent->ComponentTags);

#endif // #if WITH_EDITORONLY_DATA
}

bool UDatasmithSceneComponentTemplate::Equals( const UDatasmithObjectTemplate* Other ) const
{
	const UDatasmithSceneComponentTemplate* TypedOther = Cast< UDatasmithSceneComponentTemplate >( Other );

	if ( !TypedOther )
	{
		return false;
	}

	bool bEquals = AreTransformsEqual( RelativeTransform, TypedOther->RelativeTransform );
	bEquals = bEquals && ( Mobility == TypedOther->Mobility );
	bEquals = bEquals && ( AttachParent == TypedOther->AttachParent );
	bEquals = bEquals && FDatasmithObjectTemplateUtils::SetsEquals(Tags, TypedOther->Tags);

	return bEquals;
}
