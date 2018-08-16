// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/**
 * 
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/SceneCapture.h"
#include "SceneCaptureCube.generated.h"

UCLASS(hidecategories = (Collision, Material, Attachment, Actor))
class ENGINE_API ASceneCaptureCube : public ASceneCapture
{
	GENERATED_UCLASS_BODY()

private:
	/** Scene capture component. */
	UPROPERTY(Category = DecalActor, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	class USceneCaptureComponentCube* CaptureComponentCube;

#if WITH_EDITORONLY_DATA
	/** To allow drawing the camera frustum in the editor. */
	UPROPERTY()
	class UDrawFrustumComponent* DrawFrustum;
#endif

public:

#if WITH_EDITOR
	//~ Begin AActor Interface
	virtual void PostActorCreated() override;
	virtual void PostEditMove(bool bFinished) override;
	//~ End AActor Interface.

	/** Used to synchronize the DrawFrustumComponent with the SceneCaptureComponentCube settings. */
	void UpdateDrawFrustum();

	/** Returns DrawFrustum subobject **/
	class UDrawFrustumComponent* GetDrawFrustum() const { return DrawFrustum; }
#endif

	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void OnInterpToggle(bool bEnable);

	/** Returns CaptureComponentCube subobject **/
	class USceneCaptureComponentCube* GetCaptureComponentCube() const { return CaptureComponentCube; }
};



