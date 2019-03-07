// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"
#include "Widgets/SWidget.h"
#include "Components/TextWidgetTypes.h"
#include "TextBlock.generated.h"

class STextBlock;

/**
 * A simple static text widget.
 *
 * * No Children
 * * Text
 */
UCLASS(meta=(DisplayName="Text"))
class UMG_API UTextBlock : public UTextLayoutWidget
{
	GENERATED_UCLASS_BODY()

public:
	/**  
	 * Sets the color and opacity of the text in this text block
	 *
	 * @param InColorAndOpacity		The new text color and opacity
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetColorAndOpacity(FSlateColor InColorAndOpacity);

	/**
	* Sets the opacity of the text in this text block
	*
	* @param InOpacity		The new text opacity
	*/
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetOpacity(float InOpacity);

	/**  
	 * Sets the color and opacity of the text drop shadow
	 * Note: if opacity is zero no shadow will be drawn
	 *
	 * @param InShadowColorAndOpacity		The new drop shadow color and opacity
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetShadowColorAndOpacity(FLinearColor InShadowColorAndOpacity);

	/**  
	 * Sets the offset that the text drop shadow should be drawn at
	 *
	 * @param InShadowOffset		The new offset
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetShadowOffset(FVector2D InShadowOffset);

	/**
	 * Dynamically set the font info for this text block
	 * 
	 * @param InFontInfo THe new font info
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetFont(FSlateFontInfo InFontInfo);

	/**
	 * Dynamically set the strike brush for this text block
	 *
	 * @param InStrikeBrush The new brush to use to strike through text
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetStrikeBrush(FSlateBrush InStrikeBrush);

	/**
	 *  Set the text justification for this text block
	 *
	 *  @param InJustification new justification
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetJustification(ETextJustify::Type InJustification);
	
	/**
	 *  Set the minimum desired width for this text block
	 *
	 *  @param InMinDesiredWidth new minimum desired width
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetMinDesiredWidth(float InMinDesiredWidth);
	
	/**
	 * Set the auto wrap for this text block.
	 *
	 * @param InAutoTextWrap to turn wrap on or off.
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetAutoWrapText(bool InAutoTextWrap);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	UMaterialInstanceDynamic* GetDynamicFontMaterial();

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	UMaterialInstanceDynamic* GetDynamicOutlineMaterial();

public:
	/** The text to display */
	UPROPERTY(EditAnywhere, Category=Content, meta=( MultiLine="true" ))
	FText Text;
	
	/** A bindable delegate to allow logic to drive the text of the widget */
	UPROPERTY()
	FGetText TextDelegate;

	/** The color of the text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateColor ColorAndOpacity;

	/** A bindable delegate for the ColorAndOpacity. */
	UPROPERTY()
	FGetSlateColor ColorAndOpacityDelegate;
	
	/** The font to render the text with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateFontInfo Font;

	/** The brush to strike through text with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance)
	FSlateBrush StrikeBrush;

	/** The direction the shadow is cast */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FVector2D ShadowOffset;

	/** The color of the shadow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=( DisplayName="Shadow Color" ))
	FLinearColor ShadowColorAndOpacity;

	/** A bindable delegate for the ShadowColorAndOpacity. */
	UPROPERTY()
	FGetLinearColor ShadowColorAndOpacityDelegate;

	/** The minimum desired size for the text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	float MinDesiredWidth;

	/** If true, it will automatically wrap this text widget with an invalidation panel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Performance, AdvancedDisplay)
	bool bWrapWithInvalidationPanel;

	/** Whether the text should automatically wrap */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "bAutoWrapText is deprecated. Please use AutoWrapText instead."))
	bool bAutoWrapText_DEPRECATED;

	/** 
	 * Gets the widget text
	 * @return The widget text
	 */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="GetText (Text)"))
	FText GetText() const;

	/**
	 * Directly sets the widget text.
	 * Warning: This will wipe any binding created for the Text property!
	 * @param InText The text to assign to the widget
	 */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetText (Text)"))
	virtual void SetText(FText InText);

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

#if WITH_EDITOR
	//~ Begin UWidget Interface
	virtual const FText GetPaletteCategory() override;
	virtual void OnCreationFromPalette() override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	//~ End UWidget Interface

	virtual FString GetLabelMetadata() const override;

	void HandleTextCommitted(const FText& InText, ETextCommit::Type CommitteType);
#endif

protected:
	/**
	 * If this is enabled, text shaping, wrapping, justification are disabled in favor of much faster text layout and measurement.
	 * This feature is only suitable for "simple" text (ie, text containing only numbers or basic ASCII) as it disables the complex text rendering support required for certain languages (such as Arabic and Thai).
	 * It is significantly faster for text that can take advantage of it (particularly if that text changes frequently), but shouldn't be used for localized user-facing text.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Performance, AdvancedDisplay, meta=(AllowPrivateAccess, DesignerRebuild))
	bool bSimpleTextMode;

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnBindingChanged(const FName& Property) override;
	//~ End UWidget Interface

	/** Get the text that should be displayed in the internal Slate widget (allows flags to mutate the display text without modifying the persistent designer property data) */
	virtual TAttribute<FText> GetDisplayText();

	EVisibility GetTextWarningImageVisibility() const;

protected:

	TSharedPtr<STextBlock> MyTextBlock;
	
	PROPERTY_BINDING_IMPLEMENTATION(FText, Text);
	PROPERTY_BINDING_IMPLEMENTATION(FSlateColor, ColorAndOpacity);
	PROPERTY_BINDING_IMPLEMENTATION(FLinearColor, ShadowColorAndOpacity);
};
