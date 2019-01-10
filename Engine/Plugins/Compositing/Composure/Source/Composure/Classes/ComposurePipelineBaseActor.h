// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ComposurePipelineBaseActor.generated.h"

class FComposureViewExtension;

/**
 * Actor designed to implement compositing pipeline in a blueprint.
 */
UCLASS(BlueprintType, Blueprintable, config=Engine, meta=(ShortTooltip="Actor designed to implement compositing pipeline in a blueprint."))
class COMPOSURE_API AComposurePipelineBaseActor
	: public AActor
{
	GENERATED_UCLASS_BODY()

public:
	/** 
	 * When set, we'll call EnqueueRendering() each frame automatically. If left 
	 * off, it is up to the user to manually call their composure rendering. 
	 * Toggle this on/off at runtime to enable/disable this pipeline.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetAutoRun, Category="Composure|Ticking")
	bool bAutoRun;

#if WITH_EDITORONLY_DATA
	/** With bAutoRun, this will run EnqueueRendering() in editor - enqueuing render calls along with Editor scene rendering. */
	UPROPERTY(EditAnywhere, Category="Composure|Ticking", meta = (EditCondition = "bAutoRun"))
	bool bRunInEditor;
#endif 

	UFUNCTION(BlueprintSetter)
	virtual void SetAutoRun(bool bNewAutoRunVal) { bAutoRun = bNewAutoRunVal; }

public:	
	/** 
	 *
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Composure", meta = (CallInEditor = "true"))
	bool IsActivelyRunning() const;
#if WITH_EDITOR
	bool IsAutoRunSuspended() const;
#endif

	/** 
	 * Entry point for a composure Blueprint to do its render enqueuing from.
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Composure", meta = (CallInEditor = "true"))
	void EnqueueRendering(bool bCameraCutThisFrame);

	virtual int32 GetRenderPriority() const { return 0; }

public:
	//~ AActor interface
	virtual void RerunConstructionScripts() override;

private: 
	TSharedPtr<FComposureViewExtension, ESPMode::ThreadSafe> ViewExtension;
};
