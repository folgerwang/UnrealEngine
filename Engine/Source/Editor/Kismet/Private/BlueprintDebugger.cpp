// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BlueprintDebugger.h"

#include "CallStackViewer.h"
#include "Debugging/SKismetDebuggingView.h"
#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layout/WidgetPath.h"
#include "WatchPointViewer.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "BlueprintDebugger"

struct FBlueprintDebuggerCommands : public TCommands<FBlueprintDebuggerCommands>
{
	FBlueprintDebuggerCommands()
		: TCommands<FBlueprintDebuggerCommands>(
			TEXT("BlueprintDebugger"), // Context name for fast lookup
			LOCTEXT("BlueprintDebugger", "Blueprint Debugger"), // Localized context name for displaying
			NAME_None, // Parent
			FCoreStyle::Get().GetStyleSetName() // Icon Style Set
		)
	{
	}

	// TCommand<> interface
	virtual void RegisterCommands() override;
	// End of TCommand<> interface

	TSharedPtr<FUICommandInfo> ShowCallStackViewer;
	TSharedPtr<FUICommandInfo> ShowWatchViewer;
	TSharedPtr<FUICommandInfo> ShowExecutionTrace;
};

void FBlueprintDebuggerCommands::RegisterCommands()
{
	UI_COMMAND(ShowCallStackViewer, "Call Stack", "Toggles visibility of the Call Stack window", EUserInterfaceActionType::Check, FInputChord());
	UI_COMMAND(ShowWatchViewer, "Watches", "Toggles visibility of the Watches window", EUserInterfaceActionType::Check, FInputChord());
	UI_COMMAND(ShowExecutionTrace, "Execution Flow", "Toggles visibility of the Execution Flow window", EUserInterfaceActionType::Check, FInputChord());
}

struct FBlueprintDebuggerImpl
{
	FBlueprintDebuggerImpl();
	~FBlueprintDebuggerImpl();

	/** Function registered with tab manager to create the bluepring debugger */
	TSharedRef<SDockTab> CreateBluprintDebuggerTab(const FSpawnTabArgs& Args);

	TSharedPtr<FTabManager> DebuggingToolsTabManager;
	TSharedPtr<FTabManager::FLayout> BlueprintDebuggerLayout;

private:
	// prevent copying:
	FBlueprintDebuggerImpl(const FBlueprintDebuggerImpl&);
	FBlueprintDebuggerImpl(FBlueprintDebuggerImpl&&);
	FBlueprintDebuggerImpl& operator=(FBlueprintDebuggerImpl const&);
	FBlueprintDebuggerImpl& operator=(FBlueprintDebuggerImpl&&);
};

const FName DebuggerAppName = FName(TEXT("DebuggerApp"));

FBlueprintDebuggerImpl::FBlueprintDebuggerImpl()
{
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	FBlueprintDebuggerCommands::Register();
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DebuggerAppName, FOnSpawnTab::CreateRaw(this, &FBlueprintDebuggerImpl::CreateBluprintDebuggerTab))
		.SetDisplayName(NSLOCTEXT("BlueprintDebugger", "TabTitle", "Blueprint Debugger"))
		.SetTooltipText(NSLOCTEXT("BlueprintDebugger", "TooltipText", "Open the Blueprint Debugger tab."))
		.SetGroup(MenuStructure.GetDeveloperToolsDebugCategory())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintDebugger.TabIcon"));
}

FBlueprintDebuggerImpl::~FBlueprintDebuggerImpl()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DebuggerAppName);
	}

	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	if (DebuggingToolsTabManager.IsValid())
	{
		FGlobalTabmanager::Get()->UnregisterTabSpawner(DebuggerAppName);
		BlueprintDebuggerLayout = TSharedPtr<FTabManager::FLayout>();
		DebuggingToolsTabManager = TSharedPtr<FTabManager>();
	}
	FBlueprintDebuggerCommands::Unregister();
}

TSharedRef<SDockTab> FBlueprintDebuggerImpl::CreateBluprintDebuggerTab(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> NomadTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(NSLOCTEXT("BlueprintDebugger", "TabTitle", "Blueprint Debugger"));

	if (!DebuggingToolsTabManager.IsValid())
	{
		DebuggingToolsTabManager = FGlobalTabmanager::Get()->NewTabManager(NomadTab);
		// on persist layout will handle saving layout if the editor is shut down:
		DebuggingToolsTabManager->SetOnPersistLayout(
			FTabManager::FOnPersistLayout::CreateStatic(
				[](const TSharedRef<FTabManager::FLayout>& InLayout)
				{
					if (InLayout->GetPrimaryArea().Pin().IsValid())
					{
						FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, InLayout);
					}
				}
			)
		);
	}
	else
	{
		ensure(BlueprintDebuggerLayout.IsValid());
	}

	const FName ExecutionFlowTabName = FName(TEXT("ExecutionFlowApp"));
	const FName CallStackTabName = CallStackViewer::GetTabName();
	const FName WatchViewerTabName = WatchViewer::GetTabName();

	TWeakPtr<FTabManager> DebuggingToolsTabManagerWeak = DebuggingToolsTabManager;
	// On tab close will save the layout if the debugging window itself is closed,
	// this handler also cleans up any floating debugging controls. If we don't close
	// all areas we need to add some logic to the tab manager to reuse existing tabs:
	NomadTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateStatic(
		[](TSharedRef<SDockTab> Self, TWeakPtr<FTabManager> TabManager)
		{
			TSharedPtr<FTabManager> OwningTabManager = TabManager.Pin();
			if (OwningTabManager.IsValid())
			{
				FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, OwningTabManager->PersistLayout());
				OwningTabManager->CloseAllAreas();
			}
		}
		, DebuggingToolsTabManagerWeak
	));

	if (!BlueprintDebuggerLayout.IsValid())
	{
		DebuggingToolsTabManager->RegisterTabSpawner(
			ExecutionFlowTabName,
			FOnSpawnTab::CreateStatic(
				[](const FSpawnTabArgs&)->TSharedRef<SDockTab>
				{
				return SNew(SDockTab)
					.TabRole(ETabRole::PanelTab)
					.Label(NSLOCTEXT("BlueprintExecutionFlow", "TabTitle", "Execution Flow"))
					[
						SNew(SKismetDebuggingView)
					];
				}
			)
		)
		.SetDisplayName(NSLOCTEXT("BlueprintDebugger", "ExecutionFlowTabTitle", "Blueprint Execution Flow"))
		.SetTooltipText(NSLOCTEXT("BlueprintDebugger", "ExecutionFlowTooltipText", "Open the Blueprint Execution Flow tab."));

		CallStackViewer::RegisterTabSpawner(*DebuggingToolsTabManager);
		WatchViewer::RegisterTabSpawner(*DebuggingToolsTabManager);

		BlueprintDebuggerLayout = FTabManager::NewLayout("Standalone_BlueprintDebugger_Layout_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(.4f)
				->SetHideTabWell(true)
				->AddTab(CallStackTabName, ETabState::OpenedTab)
				->AddTab(WatchViewerTabName, ETabState::OpenedTab)
				->AddTab(ExecutionFlowTabName, ETabState::OpenedTab)
				->SetForegroundTab(CallStackTabName)
			)
		);
	}

	BlueprintDebuggerLayout = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, BlueprintDebuggerLayout.ToSharedRef());

	TSharedRef<SWidget> TabContents = DebuggingToolsTabManager->RestoreFrom(BlueprintDebuggerLayout.ToSharedRef(), TSharedPtr<SWindow>()).ToSharedRef();

	// build command list for tab restoration menu:
	TSharedPtr<FUICommandList> CommandList = MakeShareable(new FUICommandList());

	TWeakPtr<FTabManager> DebuggingToolsManagerWeak = DebuggingToolsTabManager;

	const auto ToggleTabVisibility = [](TWeakPtr<FTabManager> InDebuggingToolsManagerWeak, FName InTabName)
	{
		TSharedPtr<FTabManager> InDebuggingToolsManager = InDebuggingToolsManagerWeak.Pin();
		if (InDebuggingToolsManager.IsValid())
		{
			TSharedPtr<SDockTab> ExistingTab = InDebuggingToolsManager->FindExistingLiveTab(InTabName);
			if (ExistingTab.IsValid())
			{
				ExistingTab->RequestCloseTab();
			}
			else
			{
				InDebuggingToolsManager->InvokeTab(InTabName);
			}
		}
	};

	const auto IsTabVisible = [](TWeakPtr<FTabManager> InDebuggingToolsManagerWeak, FName InTabName)
	{
		TSharedPtr<FTabManager> InDebuggingToolsManager = InDebuggingToolsManagerWeak.Pin();
		if (InDebuggingToolsManager.IsValid())
		{
			return InDebuggingToolsManager->FindExistingLiveTab(InTabName).IsValid();
		}
		return false;
	};

	CommandList->MapAction(
		FBlueprintDebuggerCommands::Get().ShowCallStackViewer,
		FExecuteAction::CreateStatic(
			ToggleTabVisibility,
			DebuggingToolsManagerWeak,
			CallStackTabName
		),
		FCanExecuteAction::CreateStatic(
			[]() { return true; }
		),
		FIsActionChecked::CreateStatic(
			IsTabVisible,
			DebuggingToolsManagerWeak,
			CallStackTabName
		)
	);

	CommandList->MapAction(
		FBlueprintDebuggerCommands::Get().ShowWatchViewer,
		FExecuteAction::CreateStatic(
			ToggleTabVisibility,
			DebuggingToolsManagerWeak,
			WatchViewerTabName
		),
		FCanExecuteAction::CreateStatic(
			[]() { return true; }
		),
		FIsActionChecked::CreateStatic(
			IsTabVisible,
			DebuggingToolsManagerWeak,
			WatchViewerTabName
		)
	);

	CommandList->MapAction(
		FBlueprintDebuggerCommands::Get().ShowExecutionTrace,
		FExecuteAction::CreateStatic(
			ToggleTabVisibility,
			DebuggingToolsManagerWeak,
			ExecutionFlowTabName
		),
		FCanExecuteAction::CreateStatic(
			[]() { return true; }
		),
		FIsActionChecked::CreateStatic(
			IsTabVisible,
			DebuggingToolsManagerWeak,
			ExecutionFlowTabName
		)
	);

	TWeakPtr<SWidget> OwningWidgetWeak = NomadTab;
	TabContents->SetOnMouseButtonUp(
		FPointerEventHandler::CreateStatic(
			[]( /** The geometry of the widget*/
				const FGeometry&,
				/** The Mouse Event that we are processing */
				const FPointerEvent& PointerEvent,
				TWeakPtr<SWidget> InOwnerWeak,
				TSharedPtr<FUICommandList> InCommandList) -> FReply
			{
				if (PointerEvent.GetEffectingButton() == EKeys::RightMouseButton)
				{
					// if the tab manager is still available then make a context window that allows users to
					// show and hide tabs:
					TSharedPtr<SWidget> InOwner = InOwnerWeak.Pin();
					if (InOwner.IsValid())
					{
						FMenuBuilder MenuBuilder(true, InCommandList);

						MenuBuilder.PushCommandList(InCommandList.ToSharedRef());
						{
							MenuBuilder.AddMenuEntry(FBlueprintDebuggerCommands::Get().ShowCallStackViewer);
							MenuBuilder.AddMenuEntry(FBlueprintDebuggerCommands::Get().ShowWatchViewer);
							MenuBuilder.AddMenuEntry(FBlueprintDebuggerCommands::Get().ShowExecutionTrace);
						}
						MenuBuilder.PopCommandList();


						FWidgetPath WidgetPath = PointerEvent.GetEventPath() != nullptr ? *PointerEvent.GetEventPath() : FWidgetPath();
						FSlateApplication::Get().PushMenu(InOwner.ToSharedRef(), WidgetPath, MenuBuilder.MakeWidget(), PointerEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

						return FReply::Handled();
					}
				}
				
				return FReply::Unhandled();
			}
			, OwningWidgetWeak
			, CommandList
		)
	);

	NomadTab->SetContent(
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(FMargin(0.f, 2.f))
		[
			TabContents
		]
	);

	return NomadTab;
}

FBlueprintDebugger::FBlueprintDebugger()
	: Impl(MakeUnique<FBlueprintDebuggerImpl>())
{
}

FBlueprintDebugger::~FBlueprintDebugger()
{
}

#undef LOCTEXT_NAMESPACE 
