// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SlateMaterialResource.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateBrush.h"


FSlateMaterialResource::FSlateMaterialResource(const UMaterialInterface& InMaterial, const FVector2D& InImageSize, FSlateShaderResource* InTextureMask )
	: MaterialObject( &InMaterial )
	, SlateProxy( new FSlateShaderResourceProxy )
	, TextureMaskResource( InTextureMask )
	, Width(FMath::RoundToInt(InImageSize.X))
	, Height(FMath::RoundToInt(InImageSize.Y))
{
	SlateProxy->ActualSize = InImageSize.IntPoint();
	SlateProxy->Resource = this;

#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	MaterialObjectWeakPtr = MaterialObject; 
	UpdateMaterialName();
#endif
}

FSlateMaterialResource::~FSlateMaterialResource()
{
	if(SlateProxy)
	{
		delete SlateProxy;
	}
}

void FSlateMaterialResource::UpdateMaterial(const UMaterialInterface& InMaterialResource, const FVector2D& InImageSize, FSlateShaderResource* InTextureMask )
{
	MaterialObject = &InMaterialResource;

#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	MaterialObjectWeakPtr = MaterialObject;
	UpdateMaterialName();
#endif

	if( !SlateProxy )
	{
		SlateProxy = new FSlateShaderResourceProxy;
	}

	TextureMaskResource = InTextureMask;

	SlateProxy->ActualSize = InImageSize.IntPoint();
	SlateProxy->Resource = this;

	Width = FMath::RoundToInt(InImageSize.X);
	Height = FMath::RoundToInt(InImageSize.Y);
}

void FSlateMaterialResource::ResetMaterial()
{
	MaterialObject = nullptr;

#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	MaterialObjectWeakPtr = nullptr;
	UpdateMaterialName();
#endif

	TextureMaskResource = nullptr;
	if (SlateProxy)
	{
		delete SlateProxy;
	}
	SlateProxy = nullptr;
	Width = 0;
	Height = 0;
}

#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
void FSlateMaterialResource::UpdateMaterialName()
{
	const UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MaterialObject);
	if(MID && MID->Parent)
	{
		// MID's don't have nice names. Get the name of the parent instead for tracking
		DebugName = MID->Parent->GetFName();
	}
	else if(MaterialObject)
	{
		DebugName = MaterialObject->GetFName();
	}
	else
	{
		DebugName = NAME_None;
	}
}

void FSlateMaterialResource::CheckForStaleResources() const
{
	if (DebugName != NAME_None)
	{
		// pending kill objects may still be rendered for a frame so it is valid for the check to pass
		const bool bEvenIfPendingKill = true;
		// This test needs to be thread safe.  It doesn't give us as many chances to trap bugs here but it is still useful
		const bool bThreadSafe = true;
		checkf(MaterialObjectWeakPtr.IsValid(bEvenIfPendingKill, bThreadSafe), TEXT("Material %s has become invalid.  This means the resource was garbage collected while slate was using it"), *DebugName.ToString());
	}
}

#endif
