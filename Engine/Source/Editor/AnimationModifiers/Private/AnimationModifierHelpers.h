// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClassViewerModule.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "ClassViewerFilter.h"
#include "AnimationModifier.h"

class FAnimationModifierHelpers
{
public:
	/** ClassViewerFilter for Animation Modifier classes */
	class FModifierClassFilter : public IClassViewerFilter
	{
	public:
		bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			return InClass->IsChildOf(UAnimationModifier::StaticClass());
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			return InClass->IsChildOf(UAnimationModifier::StaticClass());
		}
	};

	static TSharedRef<SWidget> GetModifierPicker(const FOnClassPicked& OnClassPicked)
	{
		FClassViewerInitializationOptions Options;
		Options.bShowUnloadedBlueprints = true;
		Options.bShowNoneOption = false;
		TSharedPtr<FModifierClassFilter> ClassFilter = MakeShareable(new FModifierClassFilter);
		Options.ClassFilter = ClassFilter;

		return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnClassPicked)
			]
		];
	}

	/** Creates a new Modifier instance to store with the current asset */
	static UAnimationModifier* CreateModifierInstance(UObject* Outer, UClass* InClass, UObject* Template = nullptr)
	{
		checkf(Outer, TEXT("Invalid outer value for modifier instantiation"));
		UAnimationModifier* ProcessorInstance = NewObject<UAnimationModifier>(Outer, InClass, NAME_None, RF_NoFlags, Template);
		checkf(ProcessorInstance, TEXT("Unable to instantiate modifier class"));
		ProcessorInstance->SetFlags(RF_Transactional);
		return ProcessorInstance;
	}

};