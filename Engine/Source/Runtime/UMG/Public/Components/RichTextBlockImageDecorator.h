// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"
#include "Framework/Text/TextLayout.h"
#include "Framework/Text/ISlateRun.h"
#include "Framework/Text/ITextDecorator.h"
#include "Components/RichTextBlockDecorator.h"
#include "Engine/DataTable.h"
#include "RichTextBlockImageDecorator.generated.h"

class ISlateStyle;

/** Simple struct for rich text styles */
USTRUCT()
struct FRichImageRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush Brush;
};

/**
 * Allows you to setup an image decorator that can be configured
 * to map certain keys to certain images.  We recommend you subclass this
 * as a blueprint to configure the instance.
 *
 * Understands the format <img id="NameOfBrushInTable"></>
 */
UCLASS(Abstract)
class UMG_API URichTextBlockImageDecorator : public URichTextBlockDecorator
{
	GENERATED_BODY()

public:
	URichTextBlockImageDecorator(const FObjectInitializer& ObjectInitializer);

	virtual TSharedPtr<ITextDecorator> CreateDecorator(URichTextBlock* InOwner) override;

	virtual const FSlateBrush* FindImageBrush(FName TagOrId, bool bWarnIfMissing);

protected:

	FRichImageRow* FindImageRow(FName TagOrId, bool bWarnIfMissing);

	UPROPERTY(EditAnywhere, Category=Appearance, meta=(RowType="RichImageRow"))
	class UDataTable* ImageSet;
};
