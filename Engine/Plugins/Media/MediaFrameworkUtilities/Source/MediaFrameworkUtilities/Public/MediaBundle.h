// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"

#if WITH_EDITORONLY_DATA
#include "MediaBundleActorBase.h"
#endif

#include "MediaBundle.generated.h"

class UMediaTexture;
class UMediaPlayer;
class UMediaSource;
class UMaterialInterface;

/**
 * A bundle of the Media Asset necessary to play a video & audio
 */
UCLASS(BlueprintType, hidecategories=(Object))
class MEDIAFRAMEWORKUTILITIES_API UMediaBundle : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Get the material interface.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaBundle")
	UMaterialInterface* GetMaterial() { return Material; }

	/**
	 * Get the media player.
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaBundle")
	UMediaPlayer* GetMediaPlayer() { return MediaPlayer; }

	/**
	 * Get the media texture.
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaBundle")
	UMediaTexture* GetMediaTexture() { return MediaTexture; }

	/**
	 * Get the media source.
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaBundle")
	UMediaSource* GetMediaSource() { return MediaSource; }

public:
	UPROPERTY(EditAnywhere, Instanced, NoClear, Category="MediaBundle")
	UMediaSource* MediaSource;

#if WITH_EDITORONLY_DATA
	/* Class to spawn for that asset. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, NoClear, Category="MediaBundle")
	TSubclassOf<AMediaBundleActorBase> MediaBundleActorClass;
#endif

protected:
	UPROPERTY(Instanced)
	UMediaPlayer* MediaPlayer;

	UPROPERTY(Instanced)
	UMediaTexture* MediaTexture;

	UPROPERTY(Instanced)
	UMaterialInterface* Material;

private:
	UPROPERTY(AdvancedDisplay, DuplicateTransient, Transient, VisibleDefaultsOnly, Category="MediaBundle", meta=(DisplayName="Debug: Reference Count"))
	int32 ReferenceCount;

public:
	/**
	 * Play the media source. Only open the source if the reference count is 0. (ie. no one else opened it)
	 */
	bool OpenMediaSource();

	/**
	 * Close the media source. Only close the source if the reference count is 1. (ie. last one to close it)
	 */
	void CloseMediaSource();

	//~ UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	friend class UMediaBundleFactoryNew;
#endif
};
