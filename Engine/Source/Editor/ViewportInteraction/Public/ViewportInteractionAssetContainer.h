// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateWidgetStyleContainerBase.h"
#include "SlateWidgetStyle.h"
#include "Sound/SoundBase.h"
#include "ViewportInteractionAssetContainer.generated.h"

/**
 * Represents the common menu sounds used in viewport interaction
 */
USTRUCT()
struct FViewportInteractionAssetContainer : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FViewportInteractionAssetContainer();
	virtual ~FViewportInteractionAssetContainer();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FViewportInteractionAssetContainer& GetDefault();

	/** The sound that should play when starting the game */
	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* GizmoHandleSelectedSound;
	FViewportInteractionAssetContainer& SetGizmoHandleSelectedSound(USoundBase* InSound) { GizmoHandleSelectedSound = InSound; return *this; }

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* GizmoHandleDropSound;
	FViewportInteractionAssetContainer& SetGizmoHandleDropSound(USoundBase* InSound) { GizmoHandleDropSound = InSound; return *this; }


	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* SelectionChangeSound;
	FViewportInteractionAssetContainer& SetSelectionChangeSound(USoundBase* InSound) { SelectionChangeSound = InSound; return *this; }

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* SelectionDropSound;
	FViewportInteractionAssetContainer& SetSelectionDropSound(USoundBase* InSound) { SelectionDropSound = InSound; return *this; }

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* SelectionStartDragSound;
	FViewportInteractionAssetContainer& SetSelectionStartDragSound(USoundBase* InSound) { SelectionStartDragSound = InSound; return *this; }

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* GridSnapSound;
	FViewportInteractionAssetContainer& SetGridSnapSound(USoundBase* InSound) { GridSnapSound = InSound; return *this; }

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* ActorSnapSound;
	FViewportInteractionAssetContainer& SetActorSnapSound(USoundBase* InSound) { ActorSnapSound = InSound; return *this; }
};


/**
 */
UCLASS(hidecategories=Object, MinimalAPI)
class UViewportInteractionStyleContainer : public USlateWidgetStyleContainerBase
{
	GENERATED_UCLASS_BODY()

public:
	/** The actual data describing the sounds */
	UPROPERTY(Category=Appearance, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FViewportInteractionAssetContainer SoundsStyle;

	virtual const struct FSlateWidgetStyle* const GetStyle() const override
	{
		return static_cast< const struct FSlateWidgetStyle* >( &SoundsStyle );
	}
};
