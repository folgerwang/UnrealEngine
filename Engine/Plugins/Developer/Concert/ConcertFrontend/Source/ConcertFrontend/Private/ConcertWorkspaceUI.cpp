// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertWorkspaceUI.h"

#include "IConcertClientWorkspace.h"
#include "IConcertSession.h"
#include "ConcertFrontendStyle.h"

#include "Algo/Transform.h"
#include "Modules/ModuleManager.h"
#include "ContentBrowserModule.h"
#include "LevelEditor.h"
#include "ISourceControlState.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "SourceControlOperations.h"
#include "SourceControlHelpers.h"
#include "EditorStyleSet.h"
#include "Logging/MessageLog.h"
#include "Misc/AsyncTaskNotification.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"

#include "Widgets/SConcertSandboxPersistWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SSessionHistory.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "ConcertFrontend"

static const FName ConcertHistoryTabName(TEXT("ConcertHistory"));

/**
 * Widget to represent Concert workspace state indicators
 */
class SConcertWorkspaceStateIndicators : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConcertWorkspaceStateIndicators) {}
		SLATE_ARGUMENT(FName, AssetPath)
	SLATE_END_ARGS();

	/**
	 * Construct this widget.
	 * @param InArgs Slate arguments
	 */
	void Construct(const FArguments& InArgs, TSharedPtr<FConcertWorkspaceUI> InWorkspaceFrontend)
	{
		WorkspaceFrontend = MoveTemp(InWorkspaceFrontend);
		AssetPath = InArgs._AssetPath;

		ChildSlot
		[
			SNew(SImage)
				.Image(this, &SConcertWorkspaceStateIndicators::GetLockImage)
		];
	}

	/** Cache the indicator brushes for access. */
	static void CacheIndicatorBrushes()
	{
		if (MyLockBrush == nullptr)
		{
			MyLockBrush = FConcertFrontendStyle::Get()->GetBrush(TEXT("Concert.MyLock"));
			OtherLockBrush = FConcertFrontendStyle::Get()->GetBrush(TEXT("Concert.OtherLock"));
		}
	}

private:
	/** Gets the brush for the lock indicator image */
	const FSlateBrush* GetLockImage() const
	{
		FGuid LockId = WorkspaceFrontend->GetResourceLockId(AssetPath);
		if (!LockId.IsValid())
		{
			return nullptr;
		}
		else if (LockId == WorkspaceFrontend->GetWorkspaceLockId())
		{
			return MyLockBrush;
		}
		else
		{
			return OtherLockBrush;
		}
	}

	/* Indicator brushes */
	static const FSlateBrush* MyLockBrush;
	static const FSlateBrush* OtherLockBrush;

	/** Asset path for this indicator widget.*/
	FName AssetPath;

	/** Holds pointer to the workspace frontend. */
	TSharedPtr<FConcertWorkspaceUI> WorkspaceFrontend;
};

const FSlateBrush* SConcertWorkspaceStateIndicators::MyLockBrush = nullptr;
const FSlateBrush* SConcertWorkspaceStateIndicators::OtherLockBrush = nullptr;

/**
 * Widget to represent Concert workspace state tooltip
 */
class SConcertWorkspaceStateTooltip : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConcertWorkspaceStateTooltip) {}
		SLATE_ARGUMENT(FName, AssetPath)
	SLATE_END_ARGS();

	/**
	 * Construct this widget.
	 * @param InArgs Slate arguments
	 */
	void Construct(const FArguments& InArgs, TSharedPtr<FConcertWorkspaceUI> InWorkspaceFrontend)
	{
		WorkspaceFrontend = MoveTemp(InWorkspaceFrontend);
		AssetPath = InArgs._AssetPath;

		ChildSlot
		[
			SNew(STextBlock)
			.Visibility(this, &SConcertWorkspaceStateTooltip::GetLockTooltipVisibility)
			.Text(this, &SConcertWorkspaceStateTooltip::GetLockTooltip)
			.ColorAndOpacity(this, &SConcertWorkspaceStateTooltip::GetLockColor)
		];
	}

private:
	EVisibility GetLockTooltipVisibility() const
	{
		return !WorkspaceFrontend->GetResourceLockId(AssetPath).IsValid() ? EVisibility::Collapsed : EVisibility::Visible;
	}

	FText GetLockTooltip() const
	{
		FGuid LockId = WorkspaceFrontend->GetResourceLockId(AssetPath);
		if (!LockId.IsValid())
		{
			return FText::GetEmpty();
		}
		else if (LockId == WorkspaceFrontend->GetWorkspaceLockId())
		{
			return LOCTEXT("MyLock_Tooltip", "Locked by you");
		}
		else
		{
			return FText::Format(LOCTEXT("OtherLock_Tooltip", "Locked by: {0}"), WorkspaceFrontend->GetSessionClientUser(LockId));
		}
	}

	FSlateColor GetLockColor() const
	{
		FGuid LockId = WorkspaceFrontend->GetResourceLockId(AssetPath);
		if (!LockId.IsValid())
		{
			return FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		}
		else if (LockId == WorkspaceFrontend->GetWorkspaceLockId())
		{
			return FLinearColor(1.0f, 0.5f, 0.1f, 1.0f);
		}
		else
		{
			return FLinearColor(0.1f, 0.5f, 1.f, 1.f);
		}

	}

	/** Asset path for this indicator widget.*/
	FName AssetPath;

	/** Holds pointer to the workspace frontend. */
	TSharedPtr<FConcertWorkspaceUI> WorkspaceFrontend;
};

FConcertWorkspaceUI::FConcertWorkspaceUI()
{
	// Extend ContentBrowser Asset Icon
	if (FContentBrowserModule* ContentBrowserModule = FModuleManager::Get().GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser")))
	{
		SConcertWorkspaceStateIndicators::CacheIndicatorBrushes();

		ContentBrowserAssetStateIconDelegateHandle = ContentBrowserModule->GetAllAssetViewExtraStateIconGenerators()
			.Add_GetRef(FOnGenerateAssetViewExtraStateIndicators::CreateRaw(this, &FConcertWorkspaceUI::OnGenerateAssetViewExtraStateIcon)).GetHandle();

		ContentBrowserAssetStateTooltipDelegateHandle = ContentBrowserModule->GetAllAssetViewExtraStateTooltipGenerators()
			.Add_GetRef(FOnGenerateAssetViewExtraStateIndicators::CreateRaw(this, &FConcertWorkspaceUI::OnGenerateAssetViewExtraStateTooltip)).GetHandle();
	}

	AssetHistoryLayout = FTabManager::NewLayout("ConcertAssetHistory_Layout")
		->AddArea
		(
			FTabManager::NewArea(700, 700)
			->SetOrientation(EOrientation::Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(ConcertHistoryTabName, ETabState::ClosedTab)
			)
		);
}

FConcertWorkspaceUI::~FConcertWorkspaceUI()
{
	// Remove Content Browser Asset Icon extensions
	FContentBrowserModule* ContentBrowserModule = FModuleManager::Get().GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser"));
	if (ContentBrowserAssetStateIconDelegateHandle.IsValid() && ContentBrowserModule)
	{
		ContentBrowserModule->GetAllAssetViewExtraStateIconGenerators().RemoveAll([DelegateHandle = ContentBrowserAssetStateIconDelegateHandle](const FOnGenerateAssetViewExtraStateIndicators& Delegate) { return Delegate.GetHandle() == DelegateHandle; });
		ContentBrowserAssetStateIconDelegateHandle.Reset();

		ContentBrowserModule->GetAllAssetViewExtraStateTooltipGenerators().RemoveAll([DelegateHandle = ContentBrowserAssetStateTooltipDelegateHandle](const FOnGenerateAssetViewExtraStateIndicators& Delegate) { return Delegate.GetHandle() == DelegateHandle; });
		ContentBrowserAssetStateTooltipDelegateHandle.Reset();
	}
}

void FConcertWorkspaceUI::InstallWorkspaceExtensions(TWeakPtr<IConcertClientWorkspace> InClientWorkspace)
{
	UninstallWorspaceExtensions();
	ClientWorkspace = InClientWorkspace;

	// Extend ContentBrowser for session
	if (FContentBrowserModule* ContentBrowserModule = FModuleManager::Get().GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser")))
	{
		// Asset Context Menu Extension
		ContentBrowserAssetExtenderDelegateHandle = ContentBrowserModule->GetAllAssetViewContextMenuExtenders()
			.Add_GetRef(FContentBrowserMenuExtender_SelectedAssets::CreateRaw(this, &FConcertWorkspaceUI::OnExtendContentBrowserAssetSelectionMenu)).GetHandle();
	}

	// Setup Concert Source Control Extension
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
	{
		SourceControlExtensionDelegateHandle = LevelEditorModule->GetAllLevelEditorToolbarSourceControlMenuExtenders()
			.Add_GetRef(FLevelEditorModule::FLevelEditorMenuExtender::CreateRaw(this, &FConcertWorkspaceUI::OnExtendLevelEditorSourceControlMenu)).GetHandle();
	}
}

void FConcertWorkspaceUI::UninstallWorspaceExtensions()
{
	// Remove Content Browser extensions
	FContentBrowserModule* ContentBrowserModule = FModuleManager::Get().GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser"));
	if (ContentBrowserAssetExtenderDelegateHandle.IsValid() && ContentBrowserModule)
	{
		ContentBrowserModule->GetAllAssetViewContextMenuExtenders().RemoveAll([DelegateHandle = ContentBrowserAssetExtenderDelegateHandle](const FContentBrowserMenuExtender_SelectedAssets& Delegate) { return Delegate.GetHandle() == DelegateHandle; });
		ContentBrowserAssetExtenderDelegateHandle.Reset();
	}

	FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
	if (SourceControlExtensionDelegateHandle.IsValid() && LevelEditorModule)
	{
		LevelEditorModule->GetAllLevelEditorToolbarSourceControlMenuExtenders().RemoveAll([DelegateHandle = SourceControlExtensionDelegateHandle](const FLevelEditorModule::FLevelEditorMenuExtender& Extender) { return Extender.GetHandle() == DelegateHandle; });
		SourceControlExtensionDelegateHandle.Reset();
	}

	ClientWorkspace.Reset();
}

void FConcertWorkspaceUI::PromptPersistSessionChanges()
{
	TArray<FSourceControlStateRef> States;
	TSharedPtr<IConcertClientWorkspace> ClientWorkspacePin = ClientWorkspace.Pin();
	if (ClientWorkspacePin.IsValid())
	{
		// Get file status of packages and config
		ISourceControlModule::Get().GetProvider().GetState(ClientWorkspacePin->GatherSessionChanges(), States, EStateCacheUsage::ForceUpdate);
	}
	
	TSharedRef<SWindow> NewWindow = SNew(SWindow)
		.Title(LOCTEXT("PersistSubmitWindowTitle", "Persist & Submit Files"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(600, 600))
		.SupportsMaximize(true)
		.SupportsMinimize(false);

	TSharedRef<SConcertSandboxPersistWidget> PersistWidget =
		SNew(SConcertSandboxPersistWidget)
		.ParentWindow(NewWindow)
		.Items(States);

	NewWindow->SetContent(
		PersistWidget
	);
	FSlateApplication::Get().AddModalWindow(NewWindow, nullptr);

	// if cancelled, just exit
	if (!PersistWidget->IsDialogConfirmed())
	{
		return;
	}
	FConcertPersistCommand PersistCmd = PersistWidget->GetPersistCommand();

	// Prepare the operation notification
	FAsyncTaskNotificationConfig NotificationConfig;
	NotificationConfig.bKeepOpenOnFailure = true;
	NotificationConfig.LogCategory = &LogConcert;
	NotificationConfig.TitleText = LOCTEXT("PersistingChanges", "Persisting Session Changes");
	FAsyncTaskNotification Notification(NotificationConfig);
	FText NotificationSub;

	TArray<FText> PersistFailures;
	bool bSuccess = ClientWorkspacePin->PersistSessionChanges(PersistCmd.FilesToPersist, &ISourceControlModule::Get().GetProvider(), &PersistFailures);
	if (bSuccess)
	{
		bSuccess = SubmitChangelist(PersistCmd, NotificationSub);
	}
	else
	{
		NotificationSub = FText::Format(LOCTEXT("FailedPersistNotification", "Failed to persist session files. Reported {0} {0}|plural(one=error,other=errors)."), PersistFailures.Num());
		FMessageLog ConcertLog("Concert");
		for (const FText& Failure : PersistFailures)
		{
			ConcertLog.Error(Failure);
		}
	}

	Notification.SetProgressText(LOCTEXT("SeeMessageLog", "See Message Log"));
	Notification.SetComplete(bSuccess ? LOCTEXT("PersistChangeSuccessHeader", "Successfully Persisted Session Changes") : LOCTEXT("PersistChangeFailedHeader", "Failed to Persist Session Changes"), NotificationSub, bSuccess);
}

bool FConcertWorkspaceUI::SubmitChangelist(const FConcertPersistCommand& PersistCommand, FText& OperationMessage)
{
	if (!PersistCommand.bShouldSubmit || PersistCommand.FilesToPersist.Num() == 0)
	{
		OperationMessage = LOCTEXT("PersistChangeSuccess", "Succesfully persisted session files");
		return true;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	// Revert any unchanged files first
	SourceControlHelpers::RevertUnchangedFiles(SourceControlProvider, PersistCommand.FilesToPersist);

	// Re-update the cache state with the modified flag
	TSharedPtr<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOp = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateStatusOp->SetUpdateModifiedState(true);
	SourceControlProvider.Execute(UpdateStatusOp.ToSharedRef(), PersistCommand.FilesToPersist);

	// Build the submit list, skipping unchanged files.
	TArray<FString> FilesToSubmit;
	FilesToSubmit.Reserve(PersistCommand.FilesToPersist.Num());
	for (const FString& File : PersistCommand.FilesToPersist)
	{
		FSourceControlStatePtr FileState = SourceControlProvider.GetState(File, EStateCacheUsage::Use);
		if (FileState.IsValid() &&
			(FileState->IsAdded() ||
				FileState->IsDeleted() ||
				FileState->IsModified() ||
				(SourceControlProvider.UsesCheckout() && FileState->IsCheckedOut())
			))
		{
			FilesToSubmit.Add(File);
		}
	}

	// Check in files
	bool bCheckinSuccess = false;
	if (FilesToSubmit.Num() > 0)
	{
		TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
		CheckInOperation->SetDescription(PersistCommand.ChangelistDescription);

		bCheckinSuccess = SourceControlProvider.Execute(CheckInOperation, FilesToSubmit) == ECommandResult::Succeeded;
		if (bCheckinSuccess)
		{
			OperationMessage = CheckInOperation->GetSuccessMessage();
		}
		else
		{
			OperationMessage = LOCTEXT("SourceControlSubmitFailed", "Failed to check in persisted files!");
		}
	}
	else
	{
		OperationMessage = LOCTEXT("SourceControlNoSubmitFail", "No file to submit after persisting!");
	}
	return bCheckinSuccess;
}

FText FConcertWorkspaceUI::GetSessionClientUser(const FGuid& ClientId) const
{
	FConcertSessionClientInfo ClientInfo;
	TSharedPtr<IConcertClientWorkspace> ClientWorkspacePin = ClientWorkspace.Pin();
	if (ClientWorkspacePin.IsValid() && ClientWorkspacePin->GetSession()->FindSessionClient(ClientId, ClientInfo))
	{
		return FText::Format(LOCTEXT("ConcertOtherLockTooltip", "'{0}'({1}) on {2}"), FText::FromString(ClientInfo.ClientInfo.DisplayName), FText::FromString(ClientInfo.ClientInfo.UserName), FText::FromString(ClientInfo.ClientInfo.DeviceName));
	}
	return FText();
}

FGuid FConcertWorkspaceUI::GetWorkspaceLockId() const
{
	TSharedPtr<IConcertClientWorkspace> ClientWorkspacePin = ClientWorkspace.Pin();
	if (ClientWorkspacePin.IsValid())
	{
		return ClientWorkspacePin->GetWorkspaceLockId();
	}
	return FGuid();
}

FGuid FConcertWorkspaceUI::GetResourceLockId(const FName InResourceName) const
{
	TSharedPtr<IConcertClientWorkspace> ClientWorkspacePin = ClientWorkspace.Pin();
	if (ClientWorkspacePin.IsValid())
	{
		return ClientWorkspacePin->GetResourceLockId(InResourceName);
	}
	return FGuid();
}

bool FConcertWorkspaceUI::CanLockResources(TArray<FName> InResourceNames) const
{
	TSharedPtr<IConcertClientWorkspace> ClientWorkspacePin = ClientWorkspace.Pin();
	return ClientWorkspacePin.IsValid() && ClientWorkspacePin->AreResourcesLockedBy(InResourceNames, FGuid());
}

bool FConcertWorkspaceUI::CanUnlockResources(TArray<FName> InResourceNames) const
{
	TSharedPtr<IConcertClientWorkspace> ClientWorkspacePin = ClientWorkspace.Pin();
	return ClientWorkspacePin.IsValid() && ClientWorkspacePin->AreResourcesLockedBy(InResourceNames, ClientWorkspacePin->GetWorkspaceLockId());
}

void FConcertWorkspaceUI::ExecuteLockResources(TArray<FName> InResourceNames)
{
	TSharedPtr<IConcertClientWorkspace> ClientWorkspacePin = ClientWorkspace.Pin();
	if (ClientWorkspacePin.IsValid())
	{
		ClientWorkspacePin->LockResources(MoveTemp(InResourceNames)); // TODO: then notifications
	}
}

void FConcertWorkspaceUI::ExecuteUnlockResources(TArray<FName> InResourceNames)
{
	TSharedPtr<IConcertClientWorkspace> ClientWorkspacePin = ClientWorkspace.Pin();
	if (ClientWorkspacePin.IsValid())
	{
		ClientWorkspacePin->UnlockResources(MoveTemp(InResourceNames)); // TODO: then notifications
	}
}

void FConcertWorkspaceUI::ExecuteViewHistory(TArray<FName> InResourceNames)
{
	FGlobalTabmanager::Get()->RestoreFrom(AssetHistoryLayout.ToSharedRef(), nullptr);

	for (const FName& ResourceName : InResourceNames)
	{
		FGlobalTabmanager::Get()->InsertNewDocumentTab(ConcertHistoryTabName, FTabManager::ESearchPreference::PreferLiveTab, CreateHistoryTab(ResourceName));
	}
}

TSharedRef<FExtender> FConcertWorkspaceUI::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	// Menu extender for Content Browser context menu when an asset is selected
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	if (SelectedAssets.Num() > 0)
	{
		TArray<FName> TransformedAssets;
		Algo::Transform(SelectedAssets, TransformedAssets, [](const FAssetData& AssetData)
		{
			return AssetData.PackageName;
		});

		Extender->AddMenuExtension("AssetContextSourceControl", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateLambda(
			[this, AssetObjectPaths = MoveTemp(TransformedAssets)](FMenuBuilder& MenuBuilder) mutable
			{
				MenuBuilder.AddMenuSeparator();
				MenuBuilder.AddSubMenu(
					LOCTEXT("Concert_ContextMenu", "Multi-User"),
					FText(),
					FNewMenuDelegate::CreateRaw(this, &FConcertWorkspaceUI::GenerateConcertAssetContextMenu, MoveTemp(AssetObjectPaths)),
					false,
					FSlateIcon(FConcertFrontendStyle::GetStyleSetName(), "Concert.Concert")
				);
			}));
	}
	return Extender;
}

void FConcertWorkspaceUI::GenerateConcertAssetContextMenu(FMenuBuilder& MenuBuilder, TArray<FName> AssetObjectPaths)
{
	MenuBuilder.BeginSection("AssetConcertActions", LOCTEXT("AssetConcertActionsMenuHeading", "Multi-User"));

	// Lock Resource Action
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ConcertWVLock", "Lock Asset(s)"),
			LOCTEXT("ConcertWVLockTooltip", "Lock the asset(s) for editing."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FConcertWorkspaceUI::ExecuteLockResources, AssetObjectPaths),
				FCanExecuteAction::CreateRaw(this, &FConcertWorkspaceUI::CanLockResources, AssetObjectPaths)
			)
		);
	}
	
	// Unlock Resource Action
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ConcertWVUnlock", "Unlock Asset(s)"),
			LOCTEXT("ConcertWVUnlockTooltip", "Unlock the asset(s)."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FConcertWorkspaceUI::ExecuteUnlockResources, AssetObjectPaths),
				FCanExecuteAction::CreateRaw(this, &FConcertWorkspaceUI::CanUnlockResources, AssetObjectPaths)
			)
		);
	}

	// Lookup history for the asset
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ConcertWVHistory", "Asset history..."),
			LOCTEXT("ConcertWVHistoryToolTip", "View the asset's session history."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FConcertWorkspaceUI::ExecuteViewHistory, AssetObjectPaths)
			)
		);
	}

	MenuBuilder.EndSection();
}

TSharedRef<FExtender> FConcertWorkspaceUI::OnExtendLevelEditorSourceControlMenu(const TSharedRef<FUICommandList>)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	Extender->AddMenuExtension(
		"SourceControlConnectionSeparator",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ConcertWVPersist", "Persist Session Changes..."),
				LOCTEXT("ConcertWVPersistTooltip", "Persist the session changes and prepare the files for source control submission."),
				FSlateIcon(FConcertFrontendStyle::GetStyleSetName(), "Concert.Persist"),
				FUIAction(FExecuteAction::CreateRaw(this, &FConcertWorkspaceUI::PromptPersistSessionChanges))
			);
		})
	);
	return Extender;
}

TSharedRef<SWidget> FConcertWorkspaceUI::OnGenerateAssetViewExtraStateIcon(const FAssetData& AssetData)
{
	return SNew(SConcertWorkspaceStateIndicators, AsShared())
		.AssetPath(AssetData.PackageName);
}

TSharedRef<SWidget> FConcertWorkspaceUI::OnGenerateAssetViewExtraStateTooltip(const FAssetData& AssetData)
{
	return SNew(SConcertWorkspaceStateTooltip, AsShared())
		.AssetPath(AssetData.PackageName);
}

TSharedRef<SDockTab> FConcertWorkspaceUI::CreateHistoryTab(const FName& ResourceName) const
{
	return SNew(SDockTab)
		.TabRole(ETabRole::DocumentTab)
		.ContentPadding(FMargin(3.0f))
		.Label(FText::FromName(ResourceName))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(3.0f)
				.AutoWidth()
				[
					SNew(SImage)
					// Todo: Find another icon for the history tab.
					.Image( FEditorStyle::GetBrush( "LevelEditor.Tabs.Details" ) )
				]

				+SHorizontalBox::Slot()
				.Padding(3.0f)	
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("AssetsHistory", "{0}'s history."), FText::FromString(ResourceName.ToString())))
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
				]
			]

			+SVerticalBox::Slot()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(4.0f, 2.0f))
				[
					SNew(SSessionHistory)
					.PackageFilter(ResourceName)
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE