// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Components/RichTextBlock.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Font.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Components/RichTextBlockDecorator.h"
#include "Styling/SlateStyle.h"
#include "Framework/Text/RichTextLayoutMarshaller.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// URichTextBlock

URichTextBlock::URichTextBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URichTextBlock::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyRichTextBlock.Reset();
	StyleInstance.Reset();
}

TSharedRef<SWidget> URichTextBlock::RebuildWidget()
{
	UpdateStyleData();

	TArray< TSharedRef< class ITextDecorator > > CreatedDecorators;

	for ( URichTextBlockDecorator* Decorator : Decorators )
	{
		if ( Decorator )
		{
			TSharedPtr<ITextDecorator> TextDecorator = Decorator->CreateDecorator(this);
			if (TextDecorator.IsValid())
			{
				CreatedDecorators.Add(TextDecorator.ToSharedRef());
			}
		}
	}

	TSharedRef<FRichTextLayoutMarshaller> Marshaller = FRichTextLayoutMarshaller::Create(CreatedDecorators, StyleInstance.Get());

	MyRichTextBlock =
		SNew(SRichTextBlock)
		.TextStyle(&DefaultTextStyle)
		.Marshaller(Marshaller);
	
	return MyRichTextBlock.ToSharedRef();
}

void URichTextBlock::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyRichTextBlock->SetText(Text);

	Super::SynchronizeTextLayoutProperties( *MyRichTextBlock );
}

void URichTextBlock::UpdateStyleData()
{
	if (!StyleInstance.IsValid() || IsDesignTime())
	{
		StyleInstance = MakeShareable(new FSlateStyleSet(TEXT("RichTextStyle")));

		if (TextStyleSet && TextStyleSet->RowStruct->IsChildOf(FRichTextStyleRow::StaticStruct()))
		{
			for (const auto& Entry : TextStyleSet->RowMap)
			{
				FName SubStyleName = Entry.Key;
				FRichTextStyleRow* RichTextStyle = (FRichTextStyleRow*)Entry.Value;

				if (SubStyleName == FName(TEXT("Default")))
				{
					DefaultTextStyle = RichTextStyle->TextStyle;
				}

				StyleInstance->Set(SubStyleName, RichTextStyle->TextStyle);
			}
		}

		//Decorators.Reset();
		//for (TSubclassOf<URichTextBlockDecorator> DecoratorClass : DecoratorClasses)
		//{
		//	if (UClass* ResolvedClass = DecoratorClass.Get())
		//	{
		//		URichTextBlockDecorator* Decorator = NewObject<URichTextBlockDecorator>(this, ResolvedClass);
		//		Decorators.Add(Decorator);
		//	}
		//}
	}
}

void URichTextBlock::SetText(const FText& InText)
{
	Text = InText;
	if (MyRichTextBlock.IsValid())
	{
		MyRichTextBlock->SetText(InText);
	}
}

const FTextBlockStyle& URichTextBlock::GetDefaultTextStyle() const
{
	ensure(StyleInstance.IsValid());
	return DefaultTextStyle;
}

#if WITH_EDITOR

const FText URichTextBlock::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

void URichTextBlock::OnCreationFromPalette()
{
	//Decorators.Add(NewObject<URichTextBlockDecorator>(this, NAME_None, RF_Transactional));
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
