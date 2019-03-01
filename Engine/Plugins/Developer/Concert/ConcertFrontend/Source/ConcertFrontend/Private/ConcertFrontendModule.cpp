// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "IConcertFrontendModule.h"

#include "ConcertMessages.h"
#include "ConcertTransportMessages.h"
#include "ConcertSettings.h"
#include "ConcertSyncSettings.h"
#include "IConcertClient.h"
#include "IConcertSyncClientModule.h"
#include "ConcertWorkspaceUI.h"
#include "ConcertLogGlobal.h"
#include "Widgets/SConcertBrowser.h"
#include "Widgets/SActiveSession.h"

#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AsyncTaskNotification.h"

// Docks
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

// Toolbar 
#include "ConcertFrontendStyle.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

// Notifications
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "MessageLogModule.h"

#include "Delegates/IDelegateInstance.h"
#include "Interfaces/IEditorStyleModule.h"

// Custom property editor
#include "PropertyEditorModule.h"
#include "PropertyEditorDelegates.h"
#include "CreateSessionOptions.h"

#if WITH_EDITOR
	#include "ISettingsModule.h"
	#include "ISettingsSection.h"
	#include "LevelEditor.h"
	#include "WorkspaceMenuStructure.h"
	#include "WorkspaceMenuStructureModule.h"
#endif

static const FName ConcertBrowserTabName("ConcertBrowser");
static const FName ConcertActiveSessionTabName("ConcertActiveSession");

#define LOCTEXT_NAMESPACE "ConcertFrontend"

class FConcertUICommands : public TCommands<FConcertUICommands>
{
public:
	FConcertUICommands()
		: TCommands<FConcertUICommands>("Concert", LOCTEXT("ConcertCommands", "Multi-User"), NAME_None, FConcertFrontendStyle::GetStyleSetName())
	{}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(GoLive, "Go Live", "Join the default Multi-User session", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(OpenBrowser, "Session Browser...", "Open the Multi-User session browser", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(OpenActiveSession, "Active Session...", "Open the active session tab", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(OpenSettings, "Multi-User Settings...", "Open the Multi-User settings", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(LaunchServer, "Launch Multi-User Server", "Launch a local Multi-User server", EUserInterfaceActionType::Button, FInputChord());
	}

	TSharedPtr<FUICommandInfo> GoLive;
	TSharedPtr<FUICommandInfo> OpenBrowser;
	TSharedPtr<FUICommandInfo> OpenActiveSession;
	TSharedPtr<FUICommandInfo> OpenSettings;
	TSharedPtr<FUICommandInfo> LaunchServer;
};

TSharedRef<SWidget> GenerateConcertMenuContent(TSharedRef<FUICommandList> InCommandList)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, InCommandList);

	MenuBuilder.BeginSection("Concert", LOCTEXT("ConcertToolbarMenu", "Multi-User Menu"));
	{
		MenuBuilder.AddMenuEntry(FConcertUICommands::Get().OpenBrowser);
		MenuBuilder.AddMenuEntry(FConcertUICommands::Get().OpenActiveSession);
		MenuBuilder.AddMenuEntry(FConcertUICommands::Get().OpenSettings);
		MenuBuilder.AddMenuEntry(FConcertUICommands::Get().LaunchServer);
	}
	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

/** Implement the Concert Frontend Module */
class FConcertFrontendModule : public IConcertFrontendModule
{
public:
	virtual void RegisterTabSpawner(const TSharedPtr<FWorkspaceItem>& WorkspaceGroup) override
	{
		if (bHasRegisteredTabSpawners)
		{
			UnregisterTabSpawner();
		}
		bHasRegisteredTabSpawners = true;

		{
			FTabSpawnerEntry& BrowserSpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ConcertBrowserTabName,
				FOnSpawnTab::CreateRaw(this, &FConcertFrontendModule::SpawnConcertBrowserTab))
				.SetDisplayName(LOCTEXT("BrowserTabTitle", "Multi-User Browser"))
				.SetTooltipText(LOCTEXT("BrowserTooltipText", "Open the Multi-User session browser"))
				.SetMenuType(ETabSpawnerMenuType::Hidden);
			
			FTabSpawnerEntry& SessionSpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ConcertActiveSessionTabName,
				FOnSpawnTab::CreateRaw(this, &FConcertFrontendModule::SpawnActiveSessionTab))
				.SetDisplayName(LOCTEXT("ActiveSessionTabTitle", "Multi-User Session"))
				.SetTooltipText(LOCTEXT("ActiveSessionTooltipText", "Open the active session tab"))
				.SetMenuType(ETabSpawnerMenuType::Hidden);

			if (WorkspaceGroup.IsValid())
			{
				BrowserSpawnerEntry.SetGroup(WorkspaceGroup.ToSharedRef());
				SessionSpawnerEntry.SetGroup(WorkspaceGroup.ToSharedRef());
			}
		}
	}

	virtual void UnregisterTabSpawner() override
	{
		bHasRegisteredTabSpawners = false;

		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ConcertActiveSessionTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ConcertBrowserTabName);
	}

public:
	virtual void StartupModule() override
	{
		bHasRegisteredTabSpawners = false;

		// Initialize Style
		FConcertFrontendStyle::Initialize();

		// Concert Frontend currently relies on EditorStyle being loaded
		FModuleManager::LoadModuleChecked<IEditorStyleModule>("EditorStyle");

		// Register Concert Browser tab
		{
			TSharedPtr<FWorkspaceItem> WorkspaceGroup;
#if WITH_EDITOR
			WorkspaceGroup = WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory();
#endif
			RegisterTabSpawner(WorkspaceGroup);
		}

#if WITH_EDITOR
		// Register Workspace view
		RegisterWorkspaceUI();

		RegisterSettings();

		if (GIsEditor)
		{
			// Setup Concert Toolbar
			FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

			// Register command list
			FConcertUICommands::Register();
			TSharedPtr<FUICommandList> CommandList = MakeShared<FUICommandList>();

			// Connect to the default server and session
			CommandList->MapAction(FConcertUICommands::Get().GoLive,
				FExecuteAction::CreateRaw(this, &FConcertFrontendModule::GoLive)
			);

			// Browser menu
			CommandList->MapAction(FConcertUICommands::Get().OpenBrowser,
				FExecuteAction::CreateRaw(this, &FConcertFrontendModule::OpenConcertBrowser)
			);

			// Active session
			CommandList->MapAction(FConcertUICommands::Get().OpenActiveSession,
				FExecuteAction::CreateRaw(this, &FConcertFrontendModule::OpenActiveSession)
			);

			// Concert Settings
			CommandList->MapAction(FConcertUICommands::Get().OpenSettings,
				FExecuteAction::CreateRaw(this, &FConcertFrontendModule::OpenConcertSettings)
			);

			// Launch Server
			CommandList->MapAction(FConcertUICommands::Get().LaunchServer,
				FExecuteAction::CreateRaw(this, &FConcertFrontendModule::LaunchConcertServer)
			);

			// Extend toolbar
			TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();
			ToolbarExtender->AddToolBarExtension("Game", EExtensionHook::After, CommandList, FToolBarExtensionDelegate::CreateLambda([CommandList](FToolBarBuilder& ToolbarBuilder)
			{
				ToolbarBuilder.BeginSection("Concert");
				{
					ToolbarBuilder.AddToolBarButton
					(
						FConcertUICommands::Get().GoLive,
						NAME_None,
						LOCTEXT("ConnectDefault", "Go Live"),
						TAttribute<FText>::Create(&FConcertFrontendModule::GetConcertToolbarTooltip),
						TAttribute<FSlateIcon>::Create(&FConcertFrontendModule::GetConcertToolbarIcon)
					);

					// Add a simple drop-down menu (no label, no icon for the drop-down button itself) to list
					ToolbarBuilder.AddComboButton(
						FUIAction(),
						FOnGetContent::CreateStatic(&GenerateConcertMenuContent, CommandList.ToSharedRef()),
						LOCTEXT("ConcertToolbarMenu_Label", "Multi-User Utilities"),
						LOCTEXT("ConcertToolbarMenu_Tooltip", "Multi-User Commands"),
						FSlateIcon(),
						true
					);
				}
				ToolbarBuilder.EndSection();
			}));
			LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
			WeakToolbarExtender = ToolbarExtender;
		}
#endif

		OpenBrowserConsoleCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("Concert.OpenBrowser"),
			TEXT("Open the Multi-User session browser"),
			FExecuteAction::CreateRaw(this, &FConcertFrontendModule::OpenConcertBrowser)
			);

#if WITH_EDITOR
		OpenSettingsConsoleCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("Concert.OpenSettings"),
			TEXT("Open the Multi-User settings"),
			FExecuteAction::CreateRaw(this, &FConcertFrontendModule::OpenConcertSettings)
			);
#endif

		DefaultConnectConsoleCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("Concert.DefaultConnect"),
			TEXT("Connect to the default Multi-User session (as defined in the Multi-User settings)"),
			FExecuteAction::CreateRaw(this, &FConcertFrontendModule::DefaultConnect)
			);

		DisconnectConsoleCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("Concert.Disconnect"),
			TEXT("Disconnect from the current session"),
			FExecuteAction::CreateRaw(this, &FConcertFrontendModule::Disconnect)
			);

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("CreateSessionOptions", FOnGetDetailCustomizationInstance::CreateStatic(&FCreateSessionDetails::MakeInstance));

		// Register Message Log
		{
			FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
			FMessageLogInitializationOptions MessageLogOptions;
			MessageLogOptions.bShowPages = true;
			MessageLogOptions.bAllowClear = true;
			MessageLogOptions.MaxPageCount = 5;
			MessageLogModule.RegisterLogListing(TEXT("Concert"), LOCTEXT("ConcertLogLabel", "Multi-User"));
		}
	}

	virtual void ShutdownModule() override
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("CreateSessionOptions");

		// Unregister Message Log
		{
			FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
			MessageLogModule.UnregisterLogListing(TEXT("Concert"));
		}

		UnregisterTabSpawner();

#if WITH_EDITOR
		UnregisterWorkspaceUI();

		UnregisterSettings();

		if (GIsEditor)
		{
			FConcertUICommands::Unregister();

			FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
			LevelEditorModule.GetToolBarExtensibilityManager()->RemoveExtender(WeakToolbarExtender.Pin());
		}
#endif

		OpenBrowserConsoleCommand.Reset();
		OpenSettingsConsoleCommand.Reset();
		DefaultConnectConsoleCommand.Reset();
		DisconnectConsoleCommand.Reset();
	}
private:
	/** 
	 * Return the proper connection state icon for the toolbar button
	 */
	static FSlateIcon GetConcertToolbarIcon()
	{
		IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
		if (ConcertClient.IsValid() && ConcertClient->GetSessionConnectionStatus() == EConcertConnectionStatus::Connected)
		{
			return FSlateIcon(FConcertFrontendStyle::GetStyleSetName(), "Concert.Online", "Concert.Online.Small");
		}
		return FSlateIcon(FConcertFrontendStyle::GetStyleSetName(), "Concert.Offline", "Concert.Offline.Small");
	}

	/**
	 * Return the proper tooltip for the toolbar button
	 */
	static FText GetConcertToolbarTooltip()
	{
		IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
		if (ConcertClient.IsValid() && ConcertClient->GetSessionConnectionStatus() == EConcertConnectionStatus::Connected)
		{
			FTextBuilder TextBuilder;
			TextBuilder.AppendLine(ConcertClient->GetCurrentSession()->GetSessionInfo().ToDisplayString());
			TextBuilder.AppendLineFormat(LOCTEXT("SessionInfoClients", "Connected Clients: {0}"), ConcertClient->GetCurrentSession()->GetSessionClients().Num() + 1);
			return TextBuilder.ToText();
		}
		return LOCTEXT("ConnectDescription", "Join a default Multi-User session matching your settings");
	}

	void OpenConcertBrowser()
	{
		FGlobalTabmanager::Get()->InvokeTab(FTabId(ConcertBrowserTabName));
	}

	void OpenActiveSession()
	{
		FGlobalTabmanager::Get()->InvokeTab(FTabId(ConcertActiveSessionTabName));
	}

	void GoLive()
	{
		IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
		if (ConcertClient.IsValid())
		{
			// if connected, disconnect
			if (ConcertClient->GetSessionConnectionStatus() == EConcertConnectionStatus::Connected)
			{
				ConcertClient->DisconnectSession();
				return;
			}


			// if not connected and not connecting
			if (!ConcertClient->HasAutoConnection())
			{
				const UConcertClientConfig* ClientConfig = GetDefault<UConcertClientConfig>();
				if (!ClientConfig->DefaultServerURL.IsEmpty() && !ClientConfig->DefaultSessionName.IsEmpty())
				{
					DefaultConnect();
				}
				else
				{
					FGlobalTabmanager::Get()->InvokeTab(FTabId(ConcertBrowserTabName));
				}
			}
			// otherwise just reset the current auto connection
			else
			{
				ConcertClient->ResetAutoConnect();
			}
		}
	}

	/** 
	 * Connect to the default connection setup
	 */
	void DefaultConnect()
	{
		const UConcertClientConfig* ClientConfig = GetDefault<UConcertClientConfig>();
		if (!ClientConfig->DefaultServerURL.IsEmpty() && !ClientConfig->DefaultSessionName.IsEmpty())
		{
			IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
			if (ConcertClient.IsValid())
			{
				if (ConcertClient->GetSessionConnectionStatus() == EConcertConnectionStatus::Disconnected)
				{
					ConcertClient->DefaultConnect();
				}
			}
		}
	}

	/** 
	 * Disconnect from the current session
	 */
	void Disconnect()
	{
		IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
		if (ConcertClient.IsValid() && ConcertClient->GetSessionConnectionStatus() != EConcertConnectionStatus::Disconnected)
		{
			ConcertClient->DisconnectSession();
		}
	}

	/**
	 * Creates a new Concert Browser front-end tab.
	 *
	 * @param SpawnTabArgs The arguments for the tab to spawn.
	 * @return The spawned tab.
	 */
	TSharedRef<SDockTab> SpawnConcertBrowserTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
			.TabRole(ETabRole::NomadTab);

		TSharedRef<SConcertBrowser> Browser = SNew(SConcertBrowser, DockTab, SpawnTabArgs.GetOwnerWindow());

		DockTab->SetContent(Browser);

		return DockTab;
	}

	/**
	 * Creates a new Concert active session tab.
	 *
	 * @param SpawnTabArgs The arguments for the tab to spawn.
	 * @return The spawned tab.
	 */
	TSharedRef<SDockTab> SpawnActiveSessionTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
			.TabRole(ETabRole::NomadTab);

		TSharedRef<SActiveSession> ActiveSessionTab = SNew(SActiveSession, DockTab, SpawnTabArgs.GetOwnerWindow());

		DockTab->SetContent(ActiveSessionTab);

		return DockTab;
	}

	/** 
	 * Launch a Concert collaboration server on the local machine.
	 */
	void LaunchConcertServer()
	{
		FAsyncTaskNotificationConfig NotificationConfig;
		NotificationConfig.bKeepOpenOnFailure = true;
		NotificationConfig.TitleText = LOCTEXT("LaunchingUnrealMultiUserServer", "Launching Unreal Multi-User Server...");
		NotificationConfig.LogCategory = &LogConcert;

		FAsyncTaskNotification Notification(NotificationConfig);

		// Find concert server location for our build configuration
		FString ServerPath = FPlatformProcess::GenerateApplicationPath(TEXT("UnrealMultiUserServer"), FApp::GetBuildConfiguration());

		// Validate it exists
		if (!IFileManager::Get().FileExists(*ServerPath))
		{
			Notification.SetComplete(
				LOCTEXT("LaunchUnrealMultiUserServerErrorTitle", "Failed to Launch the Unreal Multi-User Server"), 
				LOCTEXT("LaunchUnrealMultiUserServerError_ExecutableMissing", "Could not find the executable. Have you compiled the Unreal Multi-User Server?"), 
				false
				);
			return;
		}

		// Validate we do not have it running locally 
		FString ServerAppName = FPaths::GetCleanFilename(ServerPath);
		if (FPlatformProcess::IsApplicationRunning(*ServerAppName))
		{
			Notification.SetComplete(
				LOCTEXT("LaunchUnrealMultiUserServerErrorTitle", "Failed to Launch the Unreal Multi-User Server"), 
				LOCTEXT("LaunchUnrealMultiUserServerError_AlreadyRunning", "An Unreal Multi-User Server instance is already running."), 
				false
				);
			return;
		}
		
		FPlatformProcess::CreateProc(*ServerPath, TEXT(""), true, false, false, nullptr, 0, nullptr, nullptr, nullptr);

		Notification.SetComplete(LOCTEXT("LaunchedUnrealMultiUserServer", "Launched Unreal Multi-User Server"), FText(), true);
	}

	void RegisterWorkspaceUI()
	{
		WorkspaceFrontend = MakeShared<FConcertWorkspaceUI>();
		IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
		if (ConcertClient.IsValid())
		{
			ConcertClient->OnSessionStartup().AddRaw(this, &FConcertFrontendModule::InstallWorkspaceUI);
			ConcertClient->OnSessionShutdown().AddRaw(this, &FConcertFrontendModule::UninstallWorkspaceUI);
		}
	}

	void UnregisterWorkspaceUI()
	{
		WorkspaceFrontend.Reset();
		IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
		if (ConcertClient.IsValid())
		{
			ConcertClient->OnSessionStartup().RemoveAll(this);
			ConcertClient->OnSessionShutdown().RemoveAll(this);
		}
	}

	void InstallWorkspaceUI(TSharedRef<IConcertClientSession>)
	{
		if (WorkspaceFrontend.IsValid())
		{
			WorkspaceFrontend->InstallWorkspaceExtensions(IConcertSyncClientModule::Get().GetWorkspace());
		}
	}

	void UninstallWorkspaceUI(TSharedRef<IConcertClientSession>)
	{
		if (WorkspaceFrontend.IsValid())
		{
			WorkspaceFrontend->UninstallWorspaceExtensions();
		}
	}

#if WITH_EDITOR
	void RegisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			// TODO: make only one section for both settings objects
			ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "Concert",
				LOCTEXT("ConcertFrontendSettingsName", "Multi-User Editing"),
				LOCTEXT("ConcertFrontendSettingsDescription", "Configure the Multi-User settings."),
				GetMutableDefault<UConcertClientConfig>());

			if (SettingsSection.IsValid())
			{
				SettingsSection->OnModified().BindRaw(this, &FConcertFrontendModule::HandleSettingsSaved);
			}

			SettingsModule->RegisterSettings("Project", "Plugins", "Concert Sync",
				LOCTEXT("ConcertFrontendSyncSettingsName", "Multi-User Transactions"),
				LOCTEXT("ConcertFrontendSyncSettingsDescription", "Configure the Multi-User Transactions settings."),
				GetMutableDefault<UConcertSyncConfig>());
		}
	}

	void UnregisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "Concert");
			SettingsModule->UnregisterSettings("Project", "Plugins", "Concert Sync");
		}
	}

	bool HandleSettingsSaved()
	{
		IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
		if (ConcertClient.IsValid())
		{
			ConcertClient->Configure(GetDefault<UConcertClientConfig>());
		}
		return true;
	}

	/**
	 * Hot-links to Concert Settings.
	 */
	void OpenConcertSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			SettingsModule->ShowViewer("Project", "Plugins", "Concert");
		}
	}
#endif

	/** True if the tab spawners have been registered for this module */
	bool bHasRegisteredTabSpawners;

	/** Pointer to the Toolbar extender. */
	TWeakPtr<FExtender> WeakToolbarExtender;

	/** UI view and commands on the Concert client workspace. */
	TSharedPtr<FConcertWorkspaceUI> WorkspaceFrontend;

	/** Console command for opening the Concert Browser */
	TUniquePtr<FAutoConsoleCommand> OpenBrowserConsoleCommand;

	/** Console command for opening the Concert Settings */
	TUniquePtr<FAutoConsoleCommand> OpenSettingsConsoleCommand;

	/** Console command for connecting to the default Concert session */
	TUniquePtr<FAutoConsoleCommand> DefaultConnectConsoleCommand;

	/** Console command for disconnecting from the current Concert session */
	TUniquePtr<FAutoConsoleCommand> DisconnectConsoleCommand;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FConcertFrontendModule, ConcertFrontend);
