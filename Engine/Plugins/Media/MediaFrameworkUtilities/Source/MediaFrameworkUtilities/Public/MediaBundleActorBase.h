// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "MediaBundleActorBase.generated.h"

class UMediaBundle;
class UMediaSoundComponent;
class UPrimitiveComponent;

/**
 * A base actor that 
 */
UCLASS(abstract, Blueprintable)
class MEDIAFRAMEWORKUTILITIES_API AMediaBundleActorBase : public AActor
{
	GENERATED_BODY()

public:
	/**
	 * Get the Media Bundle.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaBundle")
	UMediaBundle* GetMediaBundle() { return MediaBundle; }

	/**
	 * Play the Media Source.
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaBundle")
	bool RequestOpenMediaSource();

	/**
	 * Close the Media Source.
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaBundle")
	void RequestCloseMediaSource();


	/**
	 * Whether this actor requested the media to play.
	 */
	bool IsPlayRequested() const { return bPlayingMedia; }

protected:
	/**
	 * Assign the primitive to render on. Will change the material for the Media material.
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaBundle")
	void SetComponent(UPrimitiveComponent* InPrimitive, UMediaSoundComponent* InMediaSound);

protected:
	UPROPERTY(EditAnywhere, NoClear, Category="MediaBundle")
	UMediaBundle* MediaBundle;

	UPROPERTY(EditDefaultsOnly, Category="MediaBundle")
	bool bAutoPlay;

	UPROPERTY(EditDefaultsOnly, Category="MediaBundle", meta=(EditCondition="bAutoPlay"))
	bool bPlayWhileEditing;

	UPROPERTY(EditDefaultsOnly, Category="MediaBundle")
	UPrimitiveComponent* PrimitiveCmp;

	UPROPERTY(EditDefaultsOnly, NoClear, Category = "MediaBundle")
	UMediaSoundComponent* MediaSoundCmp;

	UPROPERTY(AdvancedDisplay, EditAnywhere, Category="MediaBundle")
	int32 PrimitiveMaterialIndex;

	bool bPlayingMedia;

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph);
	virtual void PostActorCreated() override;
	virtual void Destroyed() override;

#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	friend class UActorFactoryMediaBundle;
#endif
};
