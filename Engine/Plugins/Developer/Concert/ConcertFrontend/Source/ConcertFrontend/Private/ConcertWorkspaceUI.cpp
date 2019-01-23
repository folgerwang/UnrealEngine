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

//-----------------------------------------------------------------------------
// Widgets to display icons on top of the content browser assets to show when
// an asset is locked or modified by somebody else.
//-----------------------------------------------------------------------------

/**
 * Controls the appearance of the Concert workspace lock state icon. The lock state icon is displayed
 * on an asset in the editor content browser seen when a Concert user saves an assets or explicitly locks
 * it. The color of the lock depends who owns the lock. The lock can be held by the local client or by another
 * client connected to the Concert session.
 */
class SConcertWorkspaceLockStateIndicator : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConcertWorkspaceLockStateIndicator) {}
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
		SetVisibility(MakeAttributeSP(this, &SConcertWorkspaceLockStateIndicator::GetVisibility));

		ChildSlot
		[
			SNew(SImage)
			.Image(this, &SConcertWorkspaceLockStateIndicator::GetImageBrush)
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
	EVisibility GetVisibility()
	{
		// If the asset is locked, make the icon visible, collapsed/hidden otherwise.
		return WorkspaceFrontend->GetResourceLockId(AssetPath).IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	const FSlateBrush* GetImageBrush() const
	{
		FGuid LockId = WorkspaceFrontend->GetResourceLockId(AssetPath);
		if (!LockId.IsValid())
		{
			return nullptr; // The asset is not locked, don't show any icon.
		}
		else if (LockId == WorkspaceFrontend->GetWorkspaceLockId())
		{
			return MyLockBrush; // The asset is locked by this workspace user.
		}
		else
		{
			return OtherLockBrush; // The asset is locked by another user.
		}
	}

	// Brushes used to render the lock icon.
	static const FSlateBrush* MyLockBrush;
	static const FSlateBrush* OtherLockBrush;

	/** Asset path for this indicator widget.*/
	FName AssetPath;

	/** Holds pointer to the workspace front-end. */
	TSharedPtr<FConcertWorkspaceUI> WorkspaceFrontend;
};

const FSlateBrush* SConcertWorkspaceLockStateIndicator::MyLockBrush = nullptr;
const FSlateBrush* SConcertWorkspaceLockStateIndicator::OtherLockBrush = nullptr;

/**
 * Displays a tooltip when moving the mouse over the 'lock' icon displayed on asset locked
 * through Concert.
 */
class SConcertWorkspaceLockStateTooltip : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConcertWorkspaceLockStateTooltip) {}
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
		SetVisibility(MakeAttributeSP(this, &SConcertWorkspaceLockStateTooltip::GetTooltipVisibility));

		ChildSlot
		[
			SNew(STextBlock)
			.Text(this, &SConcertWorkspaceLockStateTooltip::GetTooltipText)
			.ColorAndOpacity(this, &SConcertWorkspaceLockStateTooltip::GetLockColor)
		];
	}

private:
	EVisibility GetTooltipVisibility() const
	{
		return WorkspaceFrontend->GetResourceLockId(AssetPath).IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	FText GetTooltipText() const
	{
		FGuid LockId = WorkspaceFrontend->GetResourceLockId(AssetPath);
		if (!LockId.IsValid())
		{
			return FText::GetEmpty(); // Not locked.
		}
		else if (LockId == WorkspaceFrontend->GetWorkspaceLockId())
		{
			return LOCTEXT("MyLock_Tooltip", "Locked by you"); // Locked by this client.
		}
		else
		{
			return FText::Format(LOCTEXT("OtherLock_Tooltip", "Locked by: {0}"), WorkspaceFrontend->GetUserDescriptionText(LockId)); // Locked by another client.
		}
	}

	FSlateColor GetLockColor() const
	{
		FGuid LockId = WorkspaceFrontend->GetResourceLockId(AssetPath);
		if (!LockId.IsValid())
		{
			return FLinearColor(); // Not locked.
		}
		else if (LockId == WorkspaceFrontend->GetWorkspaceLockId())
		{
			return FConcertFrontendStyle::Get()->GetColor("Concert.Color.LocalUser"); // Locked by this client.
		}
		else
		{
			return FConcertFrontendStyle::Get()->GetColor("Concert.Color.OtherUser"); // Locked by another client.
		}
	}

	/** Asset path for this indicator widget.*/
	FName AssetPath;

	/** Holds pointer to the workspace front-end. */
	TSharedPtr<FConcertWorkspaceUI> WorkspaceFrontend;
};

/**
 * Controls the appearance of the workspace 'modified by other' icon. The icon is displayed on an
 * asset in the editor content browser when the a client different from this workspace client has
 * live transaction(s) on the asset. The indicator is cleared when all live transactions from other
 * clients are cleared, usually when the asset is saved to disk.
 */
class SConcertWorkspaceModifiedByOtherIndicator : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConcertWorkspaceModifiedByOtherIndicator) {}
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
		SetVisibility(MakeAttributeSP(this, &SConcertWorkspaceModifiedByOtherIndicator::GetVisibility));

		ChildSlot
		[
			SNew(SImage)
			.Image(this, &SConcertWorkspaceModifiedByOtherIndicator::GetImageBrush)
		];
	}

	/** Caches the indicator brushes for access. */
	static void CacheIndicatorBrush()
	{
		if (ModifiedByOtherBrush == nullptr)
		{
			ModifiedByOtherBrush = FConcertFrontendStyle::Get()->GetBrush(TEXT("Concert.ModifiedByOther"));
		}
	}

private:
	EVisibility GetVisibility()
	{
		return WorkspaceFrontend->IsAssetModifiedByOtherClients(AssetPath) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	const FSlateBrush* GetImageBrush() const
	{
		return WorkspaceFrontend->IsAssetModifiedByOtherClients(AssetPath) ? ModifiedByOtherBrush : nullptr;
	}

	/* Brush indicating that the asset has been modified by another user. */
	static const FSlateBrush* ModifiedByOtherBrush;

	/** Asset path for this indicator widget.*/
	FName AssetPath;

	/** Holds pointer to the workspace front-end. */
	TSharedPtr<FConcertWorkspaceUI> WorkspaceFrontend;
};

const FSlateBrush* SConcertWorkspaceModifiedByOtherIndicator::ModifiedByOtherBrush = nullptr;

/**
 * Displays a tooltip when moving the mouse over the 'modified by...' icon displayed on asset
 * modified, through Concert, by any client other than the client workspace.
 */
class SConcertWorkspaceModifiedByOtherTooltip : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConcertWorkspaceModifiedByOtherTooltip) {}
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
		SetVisibility(MakeAttributeSP(this, &SConcertWorkspaceModifiedByOtherTooltip::GetVisibility));

		ChildSlot
		[
			SNew(STextBlock)
			.Text(this, &SConcertWorkspaceModifiedByOtherTooltip::GetTooltip)
			.ColorAndOpacity(this, &SConcertWorkspaceModifiedByOtherTooltip::GetToolTipColor)
		];
	}

private:
	EVisibility GetVisibility() const
	{
		return WorkspaceFrontend->IsAssetModifiedByOtherClients(AssetPath) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	FText GetTooltip() const
	{
		// NOTE: We expect this function to be called only when visible, so we already know the resource was modified by someone.
		TArray<FConcertClientInfo> ModifiedBy;
		int ModifyByOtherCount = 0;
		WorkspaceFrontend->IsAssetModifiedByOtherClients(AssetPath, &ModifyByOtherCount, &ModifiedBy, 1); // Read up to 1 user
		return ModifyByOtherCount == 1 ?
			FText::Format(LOCTEXT("ConcertModifiedByUser_Tooltip", "Modified by {0}"), WorkspaceFrontend->GetUserDescriptionText(ModifiedBy[0])) :
			FText::Format(LOCTEXT("ConcertModifiedByNumUsers_Tooltip", "Modified by {0} other users"), ModifyByOtherCount);
	}

	FSlateColor GetToolTipColor() const
	{
		return WorkspaceFrontend->IsAssetModifiedByOtherClients(AssetPath) ? FConcertFrontendStyle::Get()->GetColor("Concert.Color.OtherUser") : FLinearColor();
	}

	/** Asset path for this indicator widget.*/
	FName AssetPath;

	/** Holds pointer to the workspace front-end. */
	TSharedPtr<FConcertWorkspaceUI> WorkspaceFrontend;
};

//------------------------------------------------------------------------------
// FConcertWorkspaceUI implementation.
//------------------------------------------------------------------------------

FConcertWorkspaceUI::FConcertWorkspaceUI()
{
	// Extend ContentBrowser Asset Icon
	if (FContentBrowserModule* ContentBrowserModule = FModuleManager::Get().GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser")))
	{
		// Caches the icon brushes if not already cached.
		SConcertWorkspaceLockStateIndicator::CacheIndicatorBrushes();
		SConcertWorkspaceModifiedByOtherIndicator::CacheIndicatorBrush();

		// The 'lock' state icon displayed on top of the asset in the editor content browser.
		ContentBrowserAssetLockStateIconDelegateHandle = ContentBrowserModule->GetAllAssetViewExtraStateIconGenerators()
			.Add_GetRef(FOnGenerateAssetViewExtraStateIndicators::CreateRaw(this, &FConcertWorkspaceUI::OnGenerateAssetViewLockStateIcons)).GetHandle();

		// The 'Lock' state tooltip displayed when hovering the corresponding icon.
		ContentBrowserAssetLockStateTooltipDelegateHandle = ContentBrowserModule->GetAllAssetViewExtraStateTooltipGenerators()
			.Add_GetRef(FOnGenerateAssetViewExtraStateIndicators::CreateRaw(this, &FConcertWorkspaceUI::OnGenerateAssetViewLockStateTooltip)).GetHandle();

		// The 'Modified by other' icon displayed on top of the asset in the editor content browser.
		ContentBrowserAssetModifiedByOtherIconDelegateHandle = ContentBrowserModule->GetAllAssetViewExtraStateIconGenerators()
			.Add_GetRef(FOnGenerateAssetViewExtraStateIndicators::CreateRaw(this, &FConcertWorkspaceUI::OnGenerateAssetViewModifiedByOtherIcon)).GetHandle();

		// The 'Modified by...' tooltip displayed when hovering the 'Modified by other' icon.
		ContentBrowserAssetModifiedByOtherTooltipDelegateHandle = ContentBrowserModule->GetAllAssetViewExtraStateTooltipGenerators()
			.Add_GetRef(FOnGenerateAssetViewExtraStateIndicators::CreateRaw(this, &FConcertWorkspaceUI::OnGenerateAssetViewModifiedByOtherTooltip)).GetHandle();
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
	if (ContentBrowserAssetLockStateIconDelegateHandle.IsValid() && ContentBrowserModule)
	{
		ContentBrowserModule->GetAllAssetViewExtraStateIconGenerators().RemoveAll([DelegateHandle = ContentBrowserAssetLockStateIconDelegateHandle](const FOnGenerateAssetViewExtraStateIndicators& Delegate) { return Delegate.GetHandle() == DelegateHandle; });
		ContentBrowserAssetLockStateIconDelegateHandle.Reset();

		ContentBrowserModule->GetAllAssetViewExtraStateTooltipGenerators().RemoveAll([DelegateHandle = ContentBrowserAssetLockStateTooltipDelegateHandle](const FOnGenerateAssetViewExtraStateIndicators& Delegate) { return Delegate.GetHandle() == DelegateHandle; });
		ContentBrowserAssetLockStateTooltipDelegateHandle.Reset();

		ContentBrowserModule->GetAllAssetViewExtraStateIconGenerators().RemoveAll([DelegateHandle = ContentBrowserAssetModifiedByOtherIconDelegateHandle](const FOnGenerateAssetViewExtraStateIndicators& Delegate) { return Delegate.GetHandle() == DelegateHandle; });
		ContentBrowserAssetModifiedByOtherIconDelegateHandle.Reset();

		ContentBrowserModule->GetAllAssetViewExtraStateTooltipGenerators().RemoveAll([DelegateHandle = ContentBrowserAssetModifiedByOtherTooltipDelegateHandle](const FOnGenerateAssetViewExtraStateIndicators& Delegate) { return Delegate.GetHandle() == DelegateHandle; });
		ContentBrowserAssetModifiedByOtherTooltipDelegateHandle.Reset();
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

FText FConcertWorkspaceUI::GetUserDescriptionText(const FGuid& ClientId) const
{
	TSharedPtr<IConcertClientWorkspace> ClientWorkspacePin = ClientWorkspace.Pin();
	if (ClientWorkspacePin)
	{
		FConcertSessionClientInfo ClientSessionInfo;
		if (ClientWorkspacePin->GetSession()->FindSessionClient(ClientId, ClientSessionInfo))
		{
			return GetUserDescriptionText(ClientSessionInfo.ClientInfo);
		}
	}
	return FText();
}

FText FConcertWorkspaceUI::GetUserDescriptionText(const FConcertClientInfo& ClientInfo) const
{
	return (ClientInfo.DisplayName != ClientInfo.UserName) ?
		FText::Format(LOCTEXT("ConcertUserDisplayNameOnDevice", "'{0}' ({1}) on {2}"), FText::FromString(ClientInfo.DisplayName), FText::FromString(ClientInfo.UserName), FText::FromString(ClientInfo.DeviceName)) :
		FText::Format(LOCTEXT("ConcertUserNameOnDevice", "'{0}' on {1}"), FText::FromString(ClientInfo.DisplayName), FText::FromString(ClientInfo.DeviceName));
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

bool FConcertWorkspaceUI::IsAssetModifiedByOtherClients(const FName& AssetName, int* OutOtherClientsWithModifNum, TArray<FConcertClientInfo>* OutOtherClientsWithModifInfo, int OtherClientsWithModifMaxFetchNum) const
{
	if (TSharedPtr<IConcertClientWorkspace> ClientWorkspacePin = ClientWorkspace.Pin())
	{
		return ClientWorkspacePin->IsAssetModifiedByOtherClients(AssetName, OutOtherClientsWithModifNum, OutOtherClientsWithModifInfo, OtherClientsWithModifMaxFetchNum);
	}

	return false;
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

TSharedRef<SWidget> FConcertWorkspaceUI::OnGenerateAssetViewLockStateIcons(const FAssetData& AssetData)
{	return SNew(SConcertWorkspaceLockStateIndicator, AsShared())
		.AssetPath(AssetData.PackageName);
}

TSharedRef<SWidget> FConcertWorkspaceUI::OnGenerateAssetViewLockStateTooltip(const FAssetData& AssetData)
{
	return SNew(SConcertWorkspaceLockStateTooltip, AsShared())
		.AssetPath(AssetData.PackageName);
}

TSharedRef<SWidget> FConcertWorkspaceUI::OnGenerateAssetViewModifiedByOtherIcon(const FAssetData& AssetData)
{
	return SNew(SConcertWorkspaceModifiedByOtherIndicator, AsShared())
		.AssetPath(AssetData.PackageName);
}

TSharedRef<SWidget> FConcertWorkspaceUI::OnGenerateAssetViewModifiedByOtherTooltip(const FAssetData& AssetData)
{
	return SNew(SConcertWorkspaceModifiedByOtherTooltip, AsShared())
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