// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateWidgetStyleContainerBase.h"
#include "SlateWidgetStyle.h"
#include "Sound/SoundBase.h"
#include "VREditorAssetContainer.generated.h"

/**
 * Represents the common menu sounds used in viewport interaction
 */
USTRUCT()
struct FVREditorAssetContainer : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FVREditorAssetContainer();
	virtual ~FVREditorAssetContainer();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FVREditorAssetContainer& GetDefault();

	/** The sound that should play when starting the game */
	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* DockableWindowCloseSound;
	FVREditorAssetContainer& SetDockableWindowCloseSound(USoundBase* InSound) { DockableWindowCloseSound = InSound; return *this; }

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* DockableWindowOpenSound;
	FVREditorAssetContainer& SetDockableWindowOpenSound(USoundBase* InSound) { DockableWindowOpenSound = InSound; return *this; }

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* DockableWindowDropSound;
	FVREditorAssetContainer& SetDockableWindowDropSound(USoundBase* InSound) { DockableWindowDropSound = InSound; return *this; }

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* DockableWindowDragSound;
	FVREditorAssetContainer& SetDockableWindowDragSound(USoundBase* InSound) { DockableWindowDragSound = InSound; return *this; }

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* DropFromContentBrowserSound;
	FVREditorAssetContainer& SetDropFromContentBrowserSound(USoundBase* InSound) { DropFromContentBrowserSound = InSound; return *this; }

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* RadialMenuOpenSound;
	FVREditorAssetContainer& SetRadialMenuOpenSound(USoundBase* InSound) { RadialMenuOpenSound = InSound; return *this; }

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* RadialMenuCloseSound;
	FVREditorAssetContainer& SetRadialMenuCloseSound(USoundBase* InSound) { RadialMenuCloseSound = InSound; return *this; }

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* TeleportSound;
	FVREditorAssetContainer& SetTeleportSound(USoundBase* InSound) { TeleportSound = InSound; return *this; }

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundCue* ButtonPressSound;
	FVREditorAssetContainer& SetTeleportSound(USoundCue* InSound) { ButtonPressSound = InSound; return *this; }
};


/**
 */
UCLASS(hidecategories=Object, MinimalAPI)
class UVREditorStyleContainer : public USlateWidgetStyleContainerBase
{
	GENERATED_UCLASS_BODY()

public:
	/** The actual data describing the sounds */
	UPROPERTY(Category=Appearance, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FVREditorAssetContainer SoundsStyle;

	virtual const struct FSlateWidgetStyle* const GetStyle() const override
	{
		return static_cast< const struct FSlateWidgetStyle* >( &SoundsStyle );
	}
};
