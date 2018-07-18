// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Components/RichTextBlockImageDecorator.h"
#include "UObject/SoftObjectPtr.h"
#include "Rendering/DrawElements.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/SlateTextLayout.h"
#include "Slate/SlateGameResources.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Math/UnrealMathUtility.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "UMG"


class SRichInlineImage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRichInlineImage)
	{}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const FRichImageRow* ImageRow, const FTextBlockStyle& TextStyle)
	{
		if (ensure(ImageRow))
		{
			const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const float IconSize = FMath::Min((float)FontMeasure->GetMaxCharacterHeight(TextStyle.Font, 1.0f), ImageRow->Brush.ImageSize.Y);

			ChildSlot
			[
				SNew(SBox)
				.HeightOverride(IconSize)
				.WidthOverride(IconSize)
				[
					SNew(SScaleBox)
					.Stretch(EStretch::ScaleToFit)
					.StretchDirection(EStretchDirection::DownOnly)
					[
						SNew(SImage)
						.Image(&ImageRow->Brush)
					]
				]
			];
		}
	}
};

class FRichInlineImage : public FRichTextDecorator
{
public:
	FRichInlineImage(URichTextBlock* InOwner, URichTextBlockImageDecorator* InDecorator)
		: FRichTextDecorator(InOwner)
		, Decorator(InDecorator)
	{
	}

	virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override
	{
		if (RunParseResult.Name == TEXT("img") && RunParseResult.MetaData.Contains(TEXT("id")))
		{
			const FTextRange& IdRange = RunParseResult.MetaData[TEXT("id")];
			const FString TagId = Text.Mid(IdRange.BeginIndex, IdRange.EndIndex - IdRange.BeginIndex);

			const bool bWarnIfMissing = false;
			return Decorator->FindImageRow(*TagId, bWarnIfMissing) != nullptr;
		}

		return false;
	}

protected:
	virtual TSharedPtr<SWidget> CreateDecoratorWidget(const FTextRunInfo& RunInfo, const FTextBlockStyle& TextStyle) const override
	{
		const bool bWarnIfMissing = true;
		const FRichImageRow* ImageRow = Decorator->FindImageRow(*RunInfo.MetaData[TEXT("id")], bWarnIfMissing);

		return SNew(SRichInlineImage, ImageRow, TextStyle);
	}

private:
	URichTextBlockImageDecorator* Decorator;
};

/////////////////////////////////////////////////////
// URichTextBlockImageDecorator

URichTextBlockImageDecorator::URichTextBlockImageDecorator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FRichImageRow* URichTextBlockImageDecorator::FindImageRow(FName TagOrId, bool bWarnIfMissing)
{
	if (ImageSet)
	{
		FString ContextString;
		return ImageSet->FindRow<FRichImageRow>(TagOrId, ContextString, bWarnIfMissing);
	}
	
	return nullptr;
}

TSharedPtr<ITextDecorator> URichTextBlockImageDecorator::CreateDecorator(URichTextBlock* InOwner)
{
	return MakeShareable(new FRichInlineImage(InOwner, this));
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
