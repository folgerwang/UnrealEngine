// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Styling/SlateBrush.h"
#include "SlateGlobals.h"
#include "Application/SlateApplicationBase.h"

FSlateBrush::FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const FLinearColor& InTint, UObject* InObjectResource, bool bInDynamicallyLoaded )
	: ImageSize( InImageSize )
	, Margin( InMargin )
#if WITH_EDITORONLY_DATA
	, Tint_DEPRECATED(FLinearColor::White)
#endif
	, TintColor( InTint )
	, ResourceObject( InObjectResource )
	, ResourceName( InResourceName )
	, UVRegion( ForceInit )
	, DrawAs( InDrawType )
	, Tiling( InTiling )
	, Mirroring( ESlateBrushMirrorType::NoMirror )
	, ImageType( InImageType )
	, bIsDynamicallyLoaded( bInDynamicallyLoaded )
{
	bHasUObject_DEPRECATED = (InObjectResource != nullptr) || InResourceName.ToString().StartsWith(FSlateBrush::UTextureIdentifier());

	//Useful for debugging style breakages
	//if ( !bHasUObject_DEPRECATED && InResourceName.IsValid() && InResourceName != NAME_None )
	//{
	//	checkf( FPaths::FileExists( InResourceName.ToString() ), *FPaths::ConvertRelativePathToFull( InResourceName.ToString() ) );
	//}
}

FSlateBrush::FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const TSharedRef< FLinearColor >& InTint, UObject* InObjectResource, bool bInDynamicallyLoaded )
	: ImageSize( InImageSize )
	, Margin( InMargin )
#if WITH_EDITORONLY_DATA
	, Tint_DEPRECATED(FLinearColor::White)
#endif
	, TintColor( InTint )
	, ResourceObject( InObjectResource )
	, ResourceName( InResourceName )
	, UVRegion( ForceInit )
	, DrawAs( InDrawType )
	, Tiling( InTiling )
	, Mirroring( ESlateBrushMirrorType::NoMirror )
	, ImageType( InImageType )
	, bIsDynamicallyLoaded( bInDynamicallyLoaded )
{
	bHasUObject_DEPRECATED = (InObjectResource != nullptr) || InResourceName.ToString().StartsWith(FSlateBrush::UTextureIdentifier());

	//Useful for debugging style breakages
	//if ( !bHasUObject_DEPRECATED && InResourceName.IsValid() && InResourceName != NAME_None )
	//{
	//	checkf( FPaths::FileExists( InResourceName.ToString() ), *FPaths::ConvertRelativePathToFull( InResourceName.ToString() ) );
	//}
}

FSlateBrush::FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const FSlateColor& InTint, UObject* InObjectResource, bool bInDynamicallyLoaded )
	: ImageSize(InImageSize)
	, Margin(InMargin)
#if WITH_EDITORONLY_DATA
	, Tint_DEPRECATED(FLinearColor::White)
#endif
	, TintColor(InTint)
	, ResourceObject(InObjectResource)
	, ResourceName(InResourceName)
	, UVRegion(ForceInit)
	, DrawAs(InDrawType)
	, Tiling(InTiling)
	, Mirroring( ESlateBrushMirrorType::NoMirror )
	, ImageType(InImageType)
	, bIsDynamicallyLoaded(bInDynamicallyLoaded)
{
	bHasUObject_DEPRECATED = (InObjectResource != nullptr) || InResourceName.ToString().StartsWith(FSlateBrush::UTextureIdentifier());

	//Useful for debugging style breakages
	//if ( !bHasUObject_DEPRECATED && InResourceName.IsValid() && InResourceName != NAME_None )
	//{
	//	checkf( FPaths::FileExists( InResourceName.ToString() ), *FPaths::ConvertRelativePathToFull( InResourceName.ToString() ) );
	//}
}

const FString FSlateBrush::UTextureIdentifier()
{
	return FString(TEXT("texture:/"));
}

void FSlateBrush::UpdateRenderingResource() const
{
	if (DrawAs != ESlateBrushDrawType::NoDrawType && (ResourceName != NAME_None || ResourceObject != nullptr))
	{
		ResourceHandle = FSlateApplicationBase::Get().GetRenderer()->GetResourceHandle(*this);
	}
}

bool FSlateBrush::CanRenderResourceObject(UObject* InResourceObject) const
{
	if (InResourceObject)
	{
		if (FSlateApplicationBase::IsInitialized())
		{
			return FSlateApplicationBase::Get().GetRenderer()->CanRenderResource(*InResourceObject);
		}
	}

	return true;
}