// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/App.h"
#include "Textures/SlateShaderResource.h"
#include "TextureResource.h"
#include "Engine/Texture.h"

/** A resource for rendering a UTexture object in Slate */
class FSlateBaseUTextureResource : public FSlateShaderResource
{
public:
	FSlateBaseUTextureResource(UTexture* InTexture);
	virtual ~FSlateBaseUTextureResource();

	/**
	* FSlateShaderRsourceInterface
	*/
	virtual uint32 GetWidth() const;
	virtual uint32 GetHeight() const;
	virtual ESlateShaderResource::Type GetType() const;

	/** Gets the RHI resource used for rendering and updates the last render time for texture streaming */
	FTextureRHIRef AccessRHIResource()
	{
		if ( TextureObject && TextureObject->Resource )
		{
			FTexture* TextureResource = TextureObject->Resource;
			TextureResource->LastRenderTime = FApp::GetCurrentTime();

			return TextureResource->TextureRHI;
		}

		return FTextureRHIRef();
	}

#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	void CheckIfValid() const;
#else
	FORCEINLINE void CheckIfValid() const { }
#endif

private:
#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	void UpdateDebugName();
#endif

public:
	/** Texture UObject.  Note: lifetime is managed externally */
	UTexture* TextureObject;

#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	// Used to guard against crashes when the material object is deleted.  This is expensive so we do not do it in shipping
	TWeakObjectPtr<UTexture> ObjectWeakPtr;
	FName DebugName;
#endif
};


/** A resource for rendering a UTexture object in Slate */
class FSlateUTextureResource : public FSlateBaseUTextureResource
{
public:
	static TSharedPtr<FSlateUTextureResource> NullResource;

	FSlateUTextureResource(UTexture* InTexture);
	virtual ~FSlateUTextureResource();

	/**
	 * Updates the rendering resource with a potentially new texture
	 */
	void UpdateRenderResource(FTexture* InFTexture);

public:
	/** Slate rendering proxy */
	FSlateShaderResourceProxy* Proxy;
};
