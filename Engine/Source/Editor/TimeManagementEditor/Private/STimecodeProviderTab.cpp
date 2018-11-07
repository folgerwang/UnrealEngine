// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "STimecodeProviderTab.h"

#include "EditorStyleSet.h"
#include "Engine/Engine.h"
#include "Engine/TimecodeProvider.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/SToolBarComboButtonBlock.h"
#include "STimecodeProvider.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SComboButton.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"


#define LOCTEXT_NAMESPACE "TimecodeProviderTab"

namespace TimecodeProviderTab
{
	static const FName NAME_TimecodeProviderTab = FName("TimecodeProviderTab");

	TSharedRef<SDockTab> CreateTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(STimecodeProviderTab)
			];
	}
}

void STimecodeProviderTab::RegisterNomadTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TimecodeProviderTab::NAME_TimecodeProviderTab, FOnSpawnTab::CreateStatic(&TimecodeProviderTab::CreateTab))
		.SetDisplayName(NSLOCTEXT("TimecodeProviderTab", "DisplayName", "Timecode Provider"))
		.SetTooltipText(NSLOCTEXT("TimecodeProviderTab", "TooltipText", "Displays the Timecode and the state of the current Timecode Provider."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "TimecodeProvider.TabIcon"));
}

void STimecodeProviderTab::UnregisterNomadTabSpawner()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterTabSpawner(TimecodeProviderTab::NAME_TimecodeProviderTab);
	}
}

void STimecodeProviderTab::Construct(const FArguments& InArgs)
{
	TSharedRef< SWidget > ButtonContent = SNew(SComboButton)
		.ContentPadding(0)
		.ButtonStyle(&FCoreStyle::Get(), "ToolBar.Button")
		.ForegroundColor(FCoreStyle::Get().GetSlateColor("DefaultForeground"))
		.ButtonContent()
		[
			SNullWidget::NullWidget
		]
		.OnGetMenuContent(this, &STimecodeProviderTab::OnGetMenuContent);

	ButtonContent->SetEnabled(MakeAttributeLambda([] { return (GEngine && GEngine->GetTimecodeProvider() != nullptr); }));

	ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(0, 3, 0, 0))
		.BorderImage(FEditorStyle::GetBrush("NoBorder"))
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4, -4, 2, 0)
					[
						SNew(STimecodeProvider)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0, 2, 0)
					[
						ButtonContent
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> STimecodeProviderTab::OnGetMenuContent()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	check(GEngine);
	if (GEngine->GetTimecodeProvider())
	{
		MenuBuilder.BeginSection(TEXT("TimecodeProvider"), LOCTEXT("TimecodeProvider", "Timecode Provider"));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ReapplyMenuLabel", "Reinitialize"),
			LOCTEXT("ReapplyMenuToolTip", "Reinitialize the current Timecode Provider."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateUObject(GEngine, &UEngine::ReinitializeTimecodeProvider))
			);

		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
