// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AnimationModifiersModule.h"
#include "IAssetTools.h"

#include "Animation/AnimSequence.h"
#include "AnimationModifier.h"

#include "SAnimationModifiersTab.h"
#include "AnimationModifierDetailCustomization.h"
#include "AnimationModifiersTabSummoner.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h" 
#include "SAnimationModifierContentBrowserWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"

#define LOCTEXT_NAMESPACE "AnimationModifiersModule"

IMPLEMENT_MODULE(FAnimationModifiersModule, AnimationModifiers);

void FAnimationModifiersModule::StartupModule()
{
	// Register class/struct customizations
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.RegisterCustomClassLayout("AnimationModifier", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimationModifierDetailCustomization::MakeInstance));
	
	// Add application mode extender
	Extender = FWorkflowApplicationModeExtender::CreateRaw(this, &FAnimationModifiersModule::ExtendApplicationMode);
	FWorkflowCentricApplication::GetModeExtenderList().Add(Extender);
}

TSharedRef<FApplicationMode> FAnimationModifiersModule::ExtendApplicationMode(const FName ModeName, TSharedRef<FApplicationMode> InMode)
{
	// For skeleton and animation editor modes add our custom tab factory to it
	if (ModeName == TEXT("SkeletonEditorMode") || ModeName == TEXT("AnimationEditorMode"))
	{
		InMode->AddTabFactory(FCreateWorkflowTabFactory::CreateStatic(&FAnimationModifiersTabSummoner::CreateFactory));
		RegisteredApplicationModes.Add(InMode);
	}
	
	return InMode;
}

void FAnimationModifiersModule::ShutdownModule()
{
	// Make sure we unregister the class layout 
	FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		PropertyEditorModule->UnregisterCustomClassLayout("AnimationModifier");
	}

	// Remove extender delegate
	FWorkflowCentricApplication::GetModeExtenderList().RemoveAll([this](FWorkflowApplicationModeExtender& StoredExtender) { return StoredExtender.GetHandle() == Extender.GetHandle(); });

	// During shutdown clean up all factories from any modes which are still active/alive
	for (TWeakPtr<FApplicationMode> WeakMode : RegisteredApplicationModes)
	{
		if (WeakMode.IsValid())
		{
			TSharedPtr<FApplicationMode> Mode = WeakMode.Pin();
			Mode->RemoveTabFactory(FAnimationModifiersTabSummoner::AnimationModifiersName);
		}
	}

	RegisteredApplicationModes.Empty();
}

void FAnimationModifiersModule::ShowAddAnimationModifierWindow(const TArray<UAnimSequence*>& InSequences)
{
	TSharedPtr<SAnimationModifierContentBrowserWindow> WindowContent;

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Add Animation Modifier(s)"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(500, 500));

	Window->SetContent
	(
		SAssignNew(WindowContent, SAnimationModifierContentBrowserWindow)
		.WidgetWindow(Window)
		.AnimSequences(InSequences)
	);

	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
}

#undef LOCTEXT_NAMESPACE // "AnimationModifiersModule"
