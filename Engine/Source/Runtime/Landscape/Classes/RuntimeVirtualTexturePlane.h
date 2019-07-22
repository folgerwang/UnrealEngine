// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuntimeVirtualTexturePlane.generated.h"

class UMaterialInterface;
class URuntimeVirtualTexture;
class URuntimeVirtualTextureComponent;

/** Actor used to place a URuntimeVirtualTexture in the world. */
UCLASS(hidecategories=(Actor, Collision, Cooking, Input, LOD, Replication), MinimalAPI)
class ARuntimeVirtualTexturePlane : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	/** Component that owns the runtime virtual texture. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = VirtualTexture, meta = (AllowPrivateAccess = "true"))
	class URuntimeVirtualTextureComponent* VirtualTextureComponent;

#if WITH_EDITORONLY_DATA
	/** Box for visualizing virtual texture extents. */
	UPROPERTY(Transient)
	class UBoxComponent* Box = nullptr;
#endif // WITH_EDITORONLY_DATA

protected:
	//~ Begin AActor Interface.
#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
#endif
	virtual bool IsLevelBoundsRelevant() const override { return false; }
	//~ End AActor Interface
};

/** Component used to place a URuntimeVirtualTexture in the world. */
UCLASS(ClassGroup = Rendering, collapsecategories, hidecategories = (Activation, Collision, Cooking, Mobility, LOD, Object, Physics, Rendering), editinlinenew)
class LANDSCAPE_API URuntimeVirtualTextureComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

private:
	/** The virtual texture object to use. */
	UPROPERTY(EditAnywhere, DuplicateTransient, Category = VirtualTexture)
	URuntimeVirtualTexture* VirtualTexture = nullptr;

	/** Actor to copy the bounds from to set up the transform. */
	UPROPERTY(EditAnywhere, DuplicateTransient, Category = TransformFromBounds, meta = (DisplayName = "Source Actor"))
	AActor* BoundsSourceActor = nullptr;

public:
	/** Call whenever we need to update the underlying URuntimeVirtualTexture. */
	void UpdateVirtualTexture();

	/** Call when we need to disconnect from the underlying URuntimeVirtualTexture. */
	void ReleaseVirtualTexture();

#if WITH_EDITOR
	UFUNCTION(CallInEditor)
	void OnVirtualTextureEditProperty(URuntimeVirtualTexture const* InVirtualTexture);

	/** Copy the rotation from SourceActor to this component. Called by our UI details customization. */
	void SetRotation();

	/** Set this component transform to include the SourceActor bounds. Called by our UI details customization. */
	void SetTransformToBounds();
#endif

protected:
	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	//~ End UObject Interface

	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	//~ End UActorComponent Interface
};
