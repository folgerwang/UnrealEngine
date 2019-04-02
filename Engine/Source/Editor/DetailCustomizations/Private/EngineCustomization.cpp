// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EngineCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/Engine.h"
#include "Engine/EngineCustomTimeStep.h"
#include "Engine/TimecodeProvider.h"
#include "UnrealEngine.h"

#define LOCTEXT_NAMESPACE "EngineCustomization"

TSharedRef<IDetailCustomization> FEngineCustomization::MakeInstance()
{
	return MakeShareable(new FEngineCustomization);
}

namespace EngineCustomization
{
	void CustomizeDetailsWithApply(IDetailLayoutBuilder& DetailBuilder, FName Category, FName PropertyName, FOnClicked Lambda)
	{
		IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(Category, FText::GetEmpty(), ECategoryPriority::Uncommon);

		TSharedPtr<IPropertyHandle> PropertyHandle = DetailBuilder.GetProperty(PropertyName);
		IDetailPropertyRow& PropertyRow = CategoryBuilder.AddProperty(PropertyHandle);

		TSharedPtr<SWidget> NameWidget;
		TSharedPtr<SWidget> ValueWidget;
		PropertyRow.GetDefaultWidgets(NameWidget, ValueWidget, true);

		PropertyRow
			.ShowPropertyButtons(false)
			.CustomWidget()
			.NameContent()
			[
				NameWidget.ToSharedRef()
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					ValueWidget.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(10, 0, 0, 0))
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.OnClicked(Lambda)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Apply", "Apply"))
					]
				]
			];
	}
}

void FEngineCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	EngineCustomization::CustomizeDetailsWithApply(DetailBuilder, TEXT("Framerate"), GET_MEMBER_NAME_CHECKED(UEngine, CustomTimeStepClassName), FOnClicked::CreateLambda([]()
	{
		if (GEngine)
		{
			UEngineCustomTimeStep* NewCustomTimeStep = nullptr;
			if (GEngine->CustomTimeStepClassName.IsValid())
			{
				UClass* CustomTimeStepClass = LoadClass<UObject>(nullptr, *GEngine->CustomTimeStepClassName.ToString());
				if (CustomTimeStepClass)
				{
					NewCustomTimeStep = NewObject<UEngineCustomTimeStep>(GEngine, CustomTimeStepClass);
				}
				else
				{
					UE_LOG(LogEngine, Error, TEXT("Engine config value CustomTimeStepClassName '%s' is not a valid class name."), *GEngine->CustomTimeStepClassName.ToString());
				}
			}
			if (!GEngine->SetCustomTimeStep(NewCustomTimeStep))
			{
				UE_LOG(LogEngine, Error, TEXT("Engine config CustomTimeStepClassName '%s' could not be initialized."), *GEngine->CustomTimeStepClassName.ToString());
			}
		}

		return FReply::Handled();
	}));
	EngineCustomization::CustomizeDetailsWithApply(DetailBuilder, TEXT("Timecode"), GET_MEMBER_NAME_CHECKED(UEngine, TimecodeProviderClassName), FOnClicked::CreateLambda([]()
	{
		if (GEngine)
		{
			UTimecodeProvider* NewTimecodeProvider = nullptr;
			if (GEngine->TimecodeProviderClassName.IsValid())
			{
				UClass* TimecodeProviderClass = LoadClass<UObject>(nullptr, *GEngine->TimecodeProviderClassName.ToString());
				if (TimecodeProviderClass)
				{
					NewTimecodeProvider = NewObject<UTimecodeProvider>(GEngine, TimecodeProviderClass);
				}
				else
				{
					UE_LOG(LogEngine, Error, TEXT("Engine config value TimecodeProviderClassName '%s' is not a valid class name."), *GEngine->TimecodeProviderClassName.ToString());
				}
			}
			if (!GEngine->SetTimecodeProvider(NewTimecodeProvider))
			{
				UE_LOG(LogEngine, Error, TEXT("Engine config TimecodeProviderClassName '%s' could not be initialized."), *GEngine->TimecodeProviderClassName.ToString());
			}
		}

		return FReply::Handled();
	}));
}

#undef LOCTEXT_NAMESPACE 
