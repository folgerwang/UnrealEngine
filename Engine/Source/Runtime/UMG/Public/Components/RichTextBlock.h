// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/TextWidgetTypes.h"
#include "Engine/DataTable.h"
#include "RichTextBlock.generated.h"

class SRichTextBlock;
class URichTextBlockDecorator;

/** Simple struct for rich text styles */
USTRUCT()
struct FRichTextStyleRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(EditAnywhere, Category = Appearance)
	FTextBlockStyle TextStyle;
};

/**
 * The rich text block
 *
 * * Fancy Text
 * * No Children
 */
UCLASS()
class UMG_API URichTextBlock : public UTextLayoutWidget
{
	GENERATED_BODY()

public:
	URichTextBlock(const FObjectInitializer& ObjectInitializer);
	
	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

#if WITH_EDITOR
	// UWidget interface
	virtual const FText GetPaletteCategory() override;
	virtual void OnCreationFromPalette() override;
	// End UWidget interface
#endif

	/**  */
	UFUNCTION(BlueprintCallable, Category = "Widget")
	void SetText(const FText& InText);

	const FTextBlockStyle& GetDefaultTextStyle() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category = "Widget")
	URichTextBlockDecorator* GetDecoratorByClass(TSubclassOf<URichTextBlockDecorator> DecoratorClass);

protected:
	/** The text to display */
	UPROPERTY(EditAnywhere, Category=Content, meta=( MultiLine="true" ))
	FText Text;

	/**  */
	UPROPERTY(EditAnywhere, Category=Appearance, meta=(RowType="RichTextStyleRow"))
	class UDataTable* TextStyleSet;

	/**  */
	UPROPERTY(EditAnywhere, Category=Appearance)
	TArray<TSubclassOf<URichTextBlockDecorator>> DecoratorClasses;

protected:
	FTextBlockStyle DefaultTextStyle;

	TSharedPtr<class FSlateStyleSet> StyleInstance;

	UPROPERTY(Transient)
	TArray<URichTextBlockDecorator*> InstanceDecorators;

	/** Native Slate Widget */
	TSharedPtr<SRichTextBlock> MyRichTextBlock;

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	virtual void UpdateStyleData();
	virtual void CreateDecorators(TArray< TSharedRef< class ITextDecorator > >& OutDecorators);
	virtual TSharedPtr< class IRichTextMarkupParser > CreateMarkupParser();
	virtual TSharedPtr< class IRichTextMarkupWriter > CreateMarkupWriter();
};
