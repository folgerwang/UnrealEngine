// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VPCustomUIHandler.h"

#include "Blueprint/UserWidget.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditorActions.h"
#include "IVREditorModule.h"
#include "LevelEditorActions.h"
#include "VREditorMode.h"
#include "UI/VREditorUISystem.h"
#include "UObject/ConstructorHelpers.h"
#include "VREditorStyle.h"
#include "WidgetBlueprint.h"
#include "Widgets/SWidget.h"


#define LOCTEXT_NAMESPACE "VPCustomUIHandler"

static const FName VirtualProductionToolsLabel = TEXT("VirtualProductionTools");


UVPCustomUIHandler::UVPCustomUIHandler(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UWidgetBlueprint> DefaultWidget(TEXT("/VirtualProductionUtilities/VirtualProductionWidget"));
	if (DefaultWidget.Object)
	{
		VirtualProductionWidget = DefaultWidget.Object->GeneratedClass;
	}
}

void UVPCustomUIHandler::Init()
{
	VRRadialMenuWindowsExtension = IVREditorModule::Get().GetRadialMenuExtender()->AddMenuExtension(
		"Windows",
		EExtensionHook::After,
		nullptr, 
		FMenuExtensionDelegate::CreateUObject(this, &UVPCustomUIHandler::FillVRRadialMenuWindows));
}


void UVPCustomUIHandler::Uninit()
{
	if (IVREditorModule::IsAvailable())
	{
		IVREditorModule::Get().GetRadialMenuExtender()->RemoveExtension(VRRadialMenuWindowsExtension.ToSharedRef());
	}
}


void UVPCustomUIHandler::FillVRRadialMenuWindows(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("VirtualProductionTools", "Virtual Production"),
		FText(),
		FSlateIcon(), // FSlateIcon(FVREditorStyle::GetStyleSetName(), "VREditorStyle.WorldSpace"),
		FUIAction
		(
			FExecuteAction::CreateUObject(this, &UVPCustomUIHandler::UpdateUMGUIForVR, VirtualProductionWidget, VirtualProductionToolsLabel, FVector2D(800.0f, 600.0f)),
			FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::DefaultCanExecuteAction)
		),
		NAME_None,
		EUserInterfaceActionType::CollapsedButton
	);
}


void UVPCustomUIHandler::UpdateUMGUIForVR(TSubclassOf<UUserWidget> InWidget, FName Name, FVector2D InSize)
{
	bool bPanelVisible = IVREditorModule::Get().GetVRMode()->GetUISystem().IsShowingEditorUIPanel(Name);

	if (bPanelVisible)
	{
		IVREditorModule::Get().UpdateExternalUMGUI(nullptr, Name, InSize);
	}
	else
	{
		IVREditorModule::Get().UpdateExternalUMGUI(InWidget, Name, InSize);
	}
}


void UVPCustomUIHandler::UpdateSlateUIForVR(TSharedRef<SWidget> InWidget, FName Name, FVector2D InSize)
{
	bool bPanelVisible = IVREditorModule::Get().GetVRMode()->GetUISystem().IsShowingEditorUIPanel(Name);

	if (bPanelVisible)
	{
		IVREditorModule::Get().UpdateExternalSlateUI(SNullWidget::NullWidget, Name);
	}
	else
	{
		IVREditorModule::Get().UpdateExternalSlateUI(InWidget, Name);
	}
}


#undef LOCTEXT_NAMESPACE