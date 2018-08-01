// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"

#include "GoogleARCoreAugmentedImageDatabase.generated.h"

USTRUCT(BlueprintType)
struct FGoogleARCoreAugmentedImageDatabaseEntry
{
	GENERATED_BODY()

	/**
	 * Name of the image. This can be retrieved from an active
	 * UGoogleARCoreAugmentedImage with the GetImageName function.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoogleARCore|AugmentedImages")
	FName Name;

	/**
	 * Texture to use for this image. Valid formats are RGBA8 and
	 * BGRA8.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoogleARCore|AugmentedImages")
	UTexture2D *ImageAsset;

	/**
	 * Width of the image in meters.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoogleARCore|AugmentedImages")
	float Width;

	FGoogleARCoreAugmentedImageDatabaseEntry()
		: Width(0.0f) { }
};

UCLASS(BlueprintType)
class UGoogleARCoreAugmentedImageDatabase : public UDataAsset
{
	GENERATED_BODY()

public:

	virtual void Serialize(FArchive& Ar) override;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GoogleARCore|AugmentedImages")
	TArray<FGoogleARCoreAugmentedImageDatabaseEntry> Entries;

	UPROPERTY()
	TArray<uint8> SerializedDatabase;

};


