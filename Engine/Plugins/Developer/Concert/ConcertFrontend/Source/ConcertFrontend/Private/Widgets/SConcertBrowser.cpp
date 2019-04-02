// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SConcertBrowser.h"

#include "CreateSessionOptions.h"
#include "IConcertModule.h"
#include "IConcertClient.h"
#include "IConcertUICoreModule.h"
#include "ConcertUIExtension.h"
#include "Widgets/SConcertSettingsDialog.h"


#include "Containers/ArrayView.h"

#include "EditorFontGlyphs.h"
#include "EditorStyleSet.h"
#include "Misc/MessageDialog.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Framework/Docking/TabManager.h"
#include "SActiveSession.h"
#include "ConcertFrontendUtils.h"

#define LOCTEXT_NAMESPACE "SConcertBrowser"

template <typename ItemType>
class SConcertListView : public SCompoundWidget
{
public:
	typedef typename SListView<ItemType>::NullableItemType NullableItemType;
	typedef typename SListView<ItemType>::FOnGenerateRow FOnGenerateRow;
	typedef typename SListView<ItemType>::FOnSelectionChanged FOnSelectionChanged;

	SLATE_BEGIN_ARGS(SConcertListView)
		: _TitleText()
		, _TitleExtraContent()
		, _ListItemsSource(nullptr)
		, _OnGenerateRow()
		, _OnSelectionChanged()
	{
	}

	SLATE_ARGUMENT(FText, TitleText)

	SLATE_NAMED_SLOT(FArguments, TitleExtraContent)

	SLATE_ARGUMENT(const TArray<ItemType>*, ListItemsSource)

	SLATE_EVENT(FOnGenerateRow, OnGenerateRow)

	SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ChildSlot
		.Padding(0.0f, 0.0f, 0.0f, 2.0f)
		[
			SAssignNew(ExpandableArea, SExpandableArea)
			.BorderImage(this, &SConcertListView::GetBackgroundImage)
			.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
			.BodyBorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.BodyBorderBackgroundColor(FLinearColor::White)
			.HeaderContent()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				[
					SNew(STextBlock)
					.Text(InArgs._TitleText)
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					InArgs._TitleExtraContent.Widget
				]
			]
			.BodyContent()
			[
				SAssignNew(ListView, SListView<ItemType>)
				.ItemHeight(20.0f)
				.SelectionMode(ESelectionMode::Single)
				.ListItemsSource(InArgs._ListItemsSource)
				.OnGenerateRow(InArgs._OnGenerateRow)
				.OnSelectionChanged(InArgs._OnSelectionChanged)
				.AllowOverscroll(EAllowOverscroll::No)
			]
		];
	}

	/** Set the selected item in list view */
	void SetSelection(ItemType SoleSelectedItem, ESelectInfo::Type SelectInfo = ESelectInfo::Direct)
	{
		if (ListView.IsValid())
		{
			ListView->SetSelection(SoleSelectedItem, SelectInfo);
		}
	}

	/** Clear the selection in list view */
	void ClearSelection()
	{
		if (ListView.IsValid())
		{
			ListView->ClearSelection();
		}
	}

	/** Get the selected item from the list view (if any) */
	NullableItemType GetSelection() const
	{
		const TArray<ItemType> SelectedItems = ListView.IsValid() ? ListView->GetSelectedItems() : TArray<ItemType>();
		check(SelectedItems.Num() <= 1);
		return SelectedItems.Num() > 0 ? NullableItemType(SelectedItems[0]) : NullableItemType(nullptr);
	}

	/** Mark the list as dirty, so that it will regenerate its widgets on next tick. */
	void RequestListRefresh()
	{
		if (ListView.IsValid())
		{
			ListView->RequestListRefresh();
		}
	}

private:
	const FSlateBrush* GetBackgroundImage() const
	{
		if (IsHovered())
		{
			return (ExpandableArea.IsValid() && ExpandableArea->IsExpanded()) ? FEditorStyle::GetBrush("DetailsView.CategoryTop_Hovered") : FEditorStyle::GetBrush("DetailsView.CollapsedCategory_Hovered");
		}
		else
		{
			return (ExpandableArea.IsValid() && ExpandableArea->IsExpanded()) ? FEditorStyle::GetBrush("DetailsView.CategoryTop") : FEditorStyle::GetBrush("DetailsView.CollapsedCategory");
		}
	}

	TSharedPtr<SExpandableArea> ExpandableArea;

	TSharedPtr<SListView<ItemType>> ListView;
};

SConcertBrowser::~SConcertBrowser()
{
	// Disarm the futures
	AvailableSessionsFutureDisarm.Reset();
	AvailableClientsFutureDisarm.Reset();


	// Once we close the browser, discovery isn't needed anymore
	if (ConcertClient.IsValid())
	{
		if (ConcertClient->IsDiscoveryEnabled())
		{
			ConcertClient->StopDiscovery();
		}
		ConcertClient->OnKnownServersUpdated().Remove(OnKnownServersUpdatedHandle);
		ConcertClient->OnSessionConnectionChanged().Remove(OnSessionConnectionChangedHandle);
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SConcertBrowser::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow)
{
	TSharedRef<SHorizontalBox> StatusBar = 
		SNew(SHorizontalBox)

		// Status Icon
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(2.0f, 1.0f, 0.0f, 1.0f))
		[
			SNew(STextBlock)
			.Font(this, &SConcertBrowser::GetConnectionIconFontInfo)
			.ColorAndOpacity(this, &SConcertBrowser::GetConnectionIconColor)
			.Text(FEditorFontGlyphs::Circle)
		]

		// Status Message
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(FMargin(4.0f, 1.0f))
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
			.Padding(FMargin(0.0f, 4.0f, 6.0f, 4.0f))
			[
				SNew(STextBlock)
				.Font(FEditorStyle::GetFontStyle("BoldFont"))
				.Text(this, &SConcertBrowser::GetConnectionStatusText)
			]
		];

	TArray<FConcertUIButtonDefinition> ButtonDefs;
	ButtonDefs.Reserve(10);
	IConcertUICoreModule::Get().GetConcertBrowserStatusButtonExtension().Broadcast(ButtonDefs);

	// Append the buttons to the status bar
	if (ConcertFrontendUtils::ShowSessionConnectionUI())
	{
		// See Active session
		FConcertUIButtonDefinition& ActiveSessionDef = ButtonDefs.AddDefaulted_GetRef();
		ActiveSessionDef.Style = EConcertUIStyle::Info;
		ActiveSessionDef.Visibility = MakeAttributeSP(this, &SConcertBrowser::IsStatusBarActiveSessionVisible);
		ActiveSessionDef.Text = FEditorFontGlyphs::Info_Circle;
		ActiveSessionDef.ToolTipText = LOCTEXT("ActiveSessionToolTip", "See the current active session");
		ActiveSessionDef.OnClicked.BindSP(this, &SConcertBrowser::OnClickActiveSession);

		// Resume Session
		FConcertUIButtonDefinition& ResumeSessionDef = ButtonDefs.AddDefaulted_GetRef();
		ResumeSessionDef.Style = EConcertUIStyle::Success;
		ResumeSessionDef.Visibility = MakeAttributeSP(this, &SConcertBrowser::IsStatusBarResumeSessionVisible);
		ResumeSessionDef.Text = FEditorFontGlyphs::Play_Circle;
		ResumeSessionDef.ToolTipText = LOCTEXT("ResumeCurrentSessionToolTip", "Resume receiving updates from the current session");
		ResumeSessionDef.OnClicked.BindSP(this, &SConcertBrowser::OnClickResumeSession);

		// Suspend Session
		FConcertUIButtonDefinition& SuspendSessionDef = ButtonDefs.AddDefaulted_GetRef();
		SuspendSessionDef.Style = EConcertUIStyle::Warning;
		SuspendSessionDef.Visibility = MakeAttributeSP(this, &SConcertBrowser::IsStatusBarSuspendSessionVisible);
		SuspendSessionDef.Text = FEditorFontGlyphs::Pause_Circle;
		SuspendSessionDef.ToolTipText = LOCTEXT("SuspendCurrentSessionToolTip", "Suspend receiving updates from the current session");
		SuspendSessionDef.OnClicked.BindSP(this, &SConcertBrowser::OnClickSuspendSession);

		// Leave Session
		FConcertUIButtonDefinition& LeaveSessionDef = ButtonDefs.AddDefaulted_GetRef();
		LeaveSessionDef.Style = EConcertUIStyle::Danger;
		LeaveSessionDef.Visibility = MakeAttributeSP(this, &SConcertBrowser::IsStatusBarLeaveSessionVisible);
		LeaveSessionDef.Text = FEditorFontGlyphs::Sign_Out;
		LeaveSessionDef.ToolTipText = LOCTEXT("LeaveCurrentSessionToolTip", "Leave the current session");
		LeaveSessionDef.OnClicked.BindSP(this, &SConcertBrowser::OnClickLeaveSession);
	}

	ConcertFrontendUtils::AppendButtons(StatusBar, ButtonDefs);

	ChildSlot
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(0.0f)
			[
				SNew(SScrollBox)

				+SScrollBox::Slot()
				[
					SAssignNew(AvailableServersListView, SConcertListView<TSharedPtr<FConcertServerInfo>>)
					.TitleText(LOCTEXT("ServerList", "Available Servers"))
					.ListItemsSource(&AvailableServers)
					.OnGenerateRow(this, &SConcertBrowser::MakeServerRowWidget)
					.OnSelectionChanged(this, &SConcertBrowser::HandleServerSelectionChanged)
				]

				+SScrollBox::Slot()
				[
					SAssignNew(AvailableSessionsListView, SConcertListView<TSharedPtr<FConcertSessionInfo>>)
					.TitleText(LOCTEXT("SessionList", "Available Sessions"))
					.ListItemsSource(&AvailableSessions)
					.OnGenerateRow(this, &SConcertBrowser::MakeSessionRowWidget)
					.OnSelectionChanged(this, &SConcertBrowser::HandleSessionSelectionChanged)
					.TitleExtraContent()
					[
						SNew(SButton)
						.ToolTipText(LOCTEXT("CreateSessionToolTip", "Create a new session on the selected server"))
						.ButtonStyle(FEditorStyle::Get(), "RoundButton")
						.ForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
						.ContentPadding(FMargin(2, 0))
						.IsEnabled(this, &SConcertBrowser::IsCreateSessionEnabled)
						.OnClicked(this, &SConcertBrowser::OnClickCreateSession)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(0, 1))
							[
								SNew(STextBlock)
								.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.8"))
								.Text(FEditorFontGlyphs::Plus)
							]

							+SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(4,0,0,0))
							[
								SNew(STextBlock)
								.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
								.Text(LOCTEXT("CreateSession", "Create Session"))
								.ShadowOffset(FVector2D(1,1))
							]
						]
					]
				]

				+SScrollBox::Slot()
				[
					SAssignNew(AvailableClientsListView, SConcertListView<TSharedPtr<FConcertSessionClientInfo>>)
					.TitleText(LOCTEXT("ClientList", "Connected Clients"))
					.ListItemsSource(&AvailableClients)
					.OnGenerateRow(this, &SConcertBrowser::MakeClientRowWidget)
					.OnSelectionChanged(this, &SConcertBrowser::HandleClientSelectionChanged)
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.HeightOverride(28.0f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(2.0f)
				[
					StatusBar
				]
			]
		]
	];

	// Get the concert client and launch discovery
	ConcertClient = IConcertModule::Get().GetClientInstance();
	if (ConcertClient.IsValid())
	{
		OnKnownServersUpdatedHandle = ConcertClient->OnKnownServersUpdated().AddSP(this, &SConcertBrowser::HandleKnownServersUpdated);
		OnSessionConnectionChangedHandle = ConcertClient->OnSessionConnectionChanged().AddSP(this, &SConcertBrowser::HandleSessionConnectionChanged);
		if (!ConcertClient->IsConfigured())
		{
			ConcertClient->Configure(GetDefault<UConcertClientConfig>());
		}
		ConcertClient->Startup();
		ConcertClient->StartDiscovery();
	}
	RegisterActiveTimer(1.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SConcertBrowser::TickDiscovery));

	HandleKnownServersUpdated();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SConcertBrowser::HandleKnownServersUpdated()
{
	TArray<TSharedPtr<FConcertServerInfo>> NewAvailableServers;
	for (const FConcertServerInfo& ServerInfo : ConcertClient->GetKnownServers())
	{
		NewAvailableServers.Add(MakeShared<FConcertServerInfo>(ServerInfo));
	}

	UpdateAvailableServers(MoveTemp(NewAvailableServers));
}

void SConcertBrowser::HandleSessionConnectionChanged(IConcertClientSession& InSession, EConcertConnectionStatus ConnectionStatus)
{
	if (ConnectionStatus == EConcertConnectionStatus::Connected)
	{
		// Ensure the newly connected server and session is selected
		PendingSelection = FPendingSelection { InSession.GetSessionInfo().ServerInstanceId, InSession.GetSessionInfo().SessionName, FGuid() };
		SetSelectedServer(InSession.GetSessionInfo().ServerInstanceId);
		SetSelectedSession(InSession.GetSessionInfo().SessionName);
		RefreshAvailableClients();
	}
	
	// Force a refresh of the UI to update extension UI that may only appear for the active session
	// We copy and reset the lists to force the widgets to be recreated rather than re-use the existing ones
	{
		TArray<TSharedPtr<FConcertServerInfo>> AvailableServersCopy = ConcertFrontendUtils::DeepCopyArrayAndClearSource(AvailableServers);
		UpdateAvailableServers(MoveTemp(AvailableServersCopy));
	}
	{
		TArray<TSharedPtr<FConcertSessionInfo>> AvailableSessionsCopy = ConcertFrontendUtils::DeepCopyArrayAndClearSource(AvailableSessions);
		UpdateAvailableSessions(MoveTemp(AvailableSessionsCopy));
	}
	{
		TArray<TSharedPtr<FConcertSessionClientInfo>> AvailableClientsCopy = ConcertFrontendUtils::DeepCopyArrayAndClearSource(AvailableClients);
		UpdateAvailableClients(MoveTemp(AvailableClientsCopy));
	}
}

void SConcertBrowser::UpdateDiscovery()
{
	TSharedPtr<FConcertServerInfo> SelectedServer = AvailableServersListView.IsValid() ? AvailableServersListView->GetSelection() : nullptr;
	TSharedPtr<FConcertSessionInfo> SelectedSession = AvailableSessionsListView.IsValid() ? AvailableSessionsListView->GetSelection() : nullptr;

	if (SelectedServer.IsValid())
	{
		if (AvailableSessionsFuture.IsValid())
		{
			if (AvailableSessionsFuture.IsReady())
			{
				AvailableSessionsFuture = TFuture<void>();
			}
		}

		if (!AvailableSessionsFuture.IsValid())
		{
			// Arm the future, also it disarm any previous future that wasn't yet realized
			AvailableSessionsFutureDisarm = MakeShared<uint8>();
			TWeakPtr<uint8> IsFutureValid = AvailableSessionsFutureDisarm;
			AvailableSessionsFuture = ConcertClient->GetServerSessions(SelectedServer->AdminEndpointId)
				.Next([this, IsFutureValid](const FConcertAdmin_GetSessionsResponse& Response)
				{
					if (IsFutureValid.IsValid())
					{
						TArray<TSharedPtr<FConcertSessionInfo>> NewAvailableSessions;
						for (const FConcertSessionInfo& SessionInfo : Response.Sessions)
						{
							NewAvailableSessions.Add(MakeShared<FConcertSessionInfo>(SessionInfo));
						}
						UpdateAvailableSessions(MoveTemp(NewAvailableSessions));
					}
				});
		}
	}
	else
	{
		AvailableSessionsFuture = TFuture<void>();
		if (AvailableSessions.Num() > 0)
		{
			AvailableSessions.Reset();
			if (AvailableServersListView.IsValid())
			{
				AvailableServersListView->RequestListRefresh();
			}
		}
	}

	if (SelectedServer.IsValid() && SelectedSession.IsValid())
	{
		if (AvailableClientsFuture.IsValid())
		{
			if (AvailableClientsFuture.IsReady())
			{
				AvailableClientsFuture = TFuture<void>();
			}
		}

		if (!AvailableClientsFuture.IsValid())
		{
			// Arm the future, also it disarm any previous future that wasn't yet realized
			AvailableClientsFutureDisarm = MakeShared<uint8>();
			TWeakPtr<uint8> IsFutureValid = AvailableClientsFutureDisarm;
			AvailableClientsFuture = ConcertClient->GetSessionClients(SelectedServer->AdminEndpointId, SelectedSession->SessionName)
				.Next([this, IsFutureValid](const FConcertAdmin_GetSessionClientsResponse& Response)
				{
					if (IsFutureValid.IsValid())
					{
						TArray<TSharedPtr<FConcertSessionClientInfo>> NewAvailableClients;
						for (const FConcertSessionClientInfo& SessionClientInfo : Response.SessionClients)
						{
							NewAvailableClients.Add(MakeShared<FConcertSessionClientInfo>(SessionClientInfo));
						}
						UpdateAvailableClients(MoveTemp(NewAvailableClients));
					}
				});
		}
	}
	else
	{
		AvailableClientsFuture = TFuture<void>();
		if (AvailableClients.Num() > 0)
		{
			AvailableClients.Reset();
			if (AvailableClientsListView.IsValid())
			{
				AvailableClientsListView->RequestListRefresh();
			}
		}
	}
}

void SConcertBrowser::UpdateAvailableServers(TArray<TSharedPtr<FConcertServerInfo>>&& InAvailableServers)
{
	TSharedPtr<FConcertServerInfo> SelectedServer = AvailableServersListView.IsValid() ? AvailableServersListView->GetSelection() : nullptr;

	ConcertFrontendUtils::SyncArraysByPredicate(AvailableServers, MoveTemp(InAvailableServers), [](const TSharedPtr<FConcertServerInfo>& ServerToFind)
	{
		return [ServerToFind](const TSharedPtr<FConcertServerInfo>& PotentialServer)
		{
			return PotentialServer->AdminEndpointId == ServerToFind->AdminEndpointId;
		};
	});

	AvailableServers.StableSort([](const TSharedPtr<FConcertServerInfo>& ServerOne, const TSharedPtr<FConcertServerInfo>& ServerTwo)
	{
		return ServerOne->ServerName < ServerTwo->ServerName;
	});

	if (AvailableServersListView.IsValid())
	{
		AvailableServersListView->RequestListRefresh();

		if (SelectedServer.IsValid())
		{
			SetSelectedServer(SelectedServer->InstanceInfo.InstanceId);
		}
		else if (PendingSelection.IsSet())
		{
			SetSelectedServer(PendingSelection.GetValue().ServerInstanceId);
		}
	}
}

void SConcertBrowser::UpdateAvailableSessions(TArray<TSharedPtr<FConcertSessionInfo>>&& InAvailableSessions)
{
	TSharedPtr<FConcertSessionInfo> SelectedSession = AvailableSessionsListView.IsValid() ? AvailableSessionsListView->GetSelection() : nullptr;

	ConcertFrontendUtils::SyncArraysByPredicate(AvailableSessions, MoveTemp(InAvailableSessions), [](const TSharedPtr<FConcertSessionInfo>& SessionToFind)
	{
		return [SessionToFind](const TSharedPtr<FConcertSessionInfo>& PotentialSession)
		{
			return PotentialSession->SessionName == SessionToFind->SessionName;
		};
	});

	AvailableSessions.StableSort([](const TSharedPtr<FConcertSessionInfo>& ServiceOne, const TSharedPtr<FConcertSessionInfo>& ServiceTwo)
	{
		return ServiceOne->SessionName < ServiceTwo->SessionName;
	});

	if (AvailableSessionsListView.IsValid())
	{
		AvailableSessionsListView->RequestListRefresh();

		if (SelectedSession.IsValid())
		{
			SetSelectedSession(SelectedSession->SessionName);
		}
		else if (PendingSelection.IsSet())
		{
			SetSelectedSession(PendingSelection.GetValue().SessionName);
		}
	}
}

void SConcertBrowser::UpdateAvailableClients(TArray<TSharedPtr<FConcertSessionClientInfo>>&& InAvailableClients)
{
	TSharedPtr<FConcertSessionClientInfo> SelectedClient = AvailableClientsListView.IsValid() ? AvailableClientsListView->GetSelection() : nullptr;

	ConcertFrontendUtils::SyncArraysByPredicate(AvailableClients, MoveTemp(InAvailableClients), [](const TSharedPtr<FConcertSessionClientInfo>& ClientToFind)
	{
		return [ClientToFind](const TSharedPtr<FConcertSessionClientInfo>& PotentialClient)
		{
			return PotentialClient->ClientEndpointId == ClientToFind->ClientEndpointId;
		};
	});

	AvailableClients.StableSort([](const TSharedPtr<FConcertSessionClientInfo>& ClientOne, const TSharedPtr<FConcertSessionClientInfo>& ClientTwo)
	{
		return ClientOne->ClientInfo.DisplayName < ClientTwo->ClientInfo.DisplayName;
	});

	if (AvailableClientsListView.IsValid())
	{
		AvailableClientsListView->RequestListRefresh();

		if (SelectedClient.IsValid())
		{
			SetSelectedClient(SelectedClient->ClientEndpointId);
		}
		else if (PendingSelection.IsSet())
		{
			SetSelectedClient(PendingSelection.GetValue().ClientEndpointId);
		}
	}
}

void SConcertBrowser::RefreshAvailableSessions()
{
	// Discard any current requests
	AvailableSessionsFuture = TFuture<void>();
	AvailableClientsFuture = TFuture<void>();

	// Empty the current lists
	UpdateAvailableSessions(TArray<TSharedPtr<FConcertSessionInfo>>());
	UpdateAvailableClients(TArray<TSharedPtr<FConcertSessionClientInfo>>());

	// Make a new request
	UpdateDiscovery();
}

void SConcertBrowser::RefreshAvailableClients()
{
	// Discard any current requests
	AvailableClientsFuture = TFuture<void>();

	// Empty the current lists
	UpdateAvailableClients(TArray<TSharedPtr<FConcertSessionClientInfo>>());

	// Make a new request
	UpdateDiscovery();
}

EActiveTimerReturnType SConcertBrowser::TickDiscovery(double InCurrentTime, float InDeltaTime)
{
	UpdateDiscovery();
	return EActiveTimerReturnType::Continue;
}

TSharedRef<ITableRow> SConcertBrowser::MakeServerRowWidget(TSharedPtr<FConcertServerInfo> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedRef<SHorizontalBox> ServerRow =
		SNew(SHorizontalBox)
		// Session Info
		+SHorizontalBox::Slot()
		[
			ConcertFrontendUtils::CreateDisplayName(FText::FromString(Item->ServerName))
		];

	//Add Icons here
	{
		TArray<ConcertFrontendUtils::FConcertBrowserIconsDefinition> IconDefs;
		{
			ConcertFrontendUtils::FConcertBrowserIconsDefinition IconDef;
			IconDef.Style = EConcertUIStyle::Warning;
			IconDef.ToolTipText = LOCTEXT("ServerIgnoreSessionRequirementsTooltip", "Careful this server won't verify that you have the right requirements before you join a session");
			IconDef.IsEnabled = true;
			IconDef.Visibility = (Item->ServerFlags & EConcertSeverFlags::IgnoreSessionRequirement) != EConcertSeverFlags::None ? EVisibility::Visible : EVisibility::Collapsed;
			IconDef.Glyph = FEditorFontGlyphs::Exclamation_Triangle;

			IconDefs.Add(IconDef);
		}

		ConcertFrontendUtils::AppendIcons(ServerRow, IconDefs);
	}

	// Append the buttons to the server row
	{
		TArray<FConcertUIButtonDefinition> ButtonDefs;
		IConcertUICoreModule::Get().GetConcertBrowserServerButtonExtension().Broadcast(*Item, ButtonDefs);

		ConcertFrontendUtils::AppendButtons(ServerRow, ButtonDefs);
	}

	return
		SNew(STableRow<TSharedPtr<FConcertServerInfo>>, OwnerTable)
		.SignalSelectionMode(ETableRowSignalSelectionMode::Instantaneous)
		.ToolTipText(Item->ToDisplayString())
		[
			ServerRow
		];
}

TSharedRef<ITableRow> SConcertBrowser::MakeSessionRowWidget(TSharedPtr<FConcertSessionInfo> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedRef<SHorizontalBox> SessionRow =
		SNew(SHorizontalBox)

		// Session Info
		+SHorizontalBox::Slot()
		[
			ConcertFrontendUtils::CreateDisplayName(FText::FromString(Item->SessionName))
		];

	TArray<FConcertUIButtonDefinition> ButtonDefs;
	ButtonDefs.Reserve(10);
	IConcertUICoreModule::Get().GetConcertBrowserSessionButtonExtension().Broadcast(*Item, ButtonDefs);

	// Append the buttons to the session row
	if (ConcertFrontendUtils::ShowSessionConnectionUI())
	{
		// Active Session
		FConcertUIButtonDefinition& ActiveSessionDef = ButtonDefs.AddDefaulted_GetRef();
		ActiveSessionDef.Style = EConcertUIStyle::Info;
		ActiveSessionDef.Visibility = MakeAttributeSP(this, &SConcertBrowser::IsActiveSessionVisible, Item->SessionName);
		ActiveSessionDef.Text = FEditorFontGlyphs::Info_Circle;
		ActiveSessionDef.ToolTipText = LOCTEXT("ActiveSessionToolTip", "See the current active session");
		ActiveSessionDef.OnClicked.BindSP(this, &SConcertBrowser::OnClickActiveSession);
		
		// Resume Session
		FConcertUIButtonDefinition& ResumeSessionDef = ButtonDefs.AddDefaulted_GetRef();
		ResumeSessionDef.Style = EConcertUIStyle::Success;
		ResumeSessionDef.Visibility = MakeAttributeSP(this, &SConcertBrowser::IsResumeSessionVisible, Item->SessionName);
		ResumeSessionDef.Text = FEditorFontGlyphs::Play_Circle;
		ResumeSessionDef.ToolTipText = LOCTEXT("ResumeSessionToolTip", "Resume receiving updates from this session");
		ResumeSessionDef.OnClicked.BindSP(this, &SConcertBrowser::OnClickResumeSession);

		// Suspend Session
		FConcertUIButtonDefinition& SuspendSessionDef = ButtonDefs.AddDefaulted_GetRef();
		SuspendSessionDef.Style = EConcertUIStyle::Warning;
		SuspendSessionDef.Visibility = MakeAttributeSP(this, &SConcertBrowser::IsSuspendSessionVisible, Item->SessionName);
		SuspendSessionDef.Text = FEditorFontGlyphs::Pause_Circle;
		SuspendSessionDef.ToolTipText = LOCTEXT("SuspendSessionToolTip", "Suspend receiving updates from this session");
		SuspendSessionDef.OnClicked.BindSP(this, &SConcertBrowser::OnClickSuspendSession);

		// Delete Session
		FConcertUIButtonDefinition& DeleteSessionDef = ButtonDefs.AddDefaulted_GetRef();
		DeleteSessionDef.Style = EConcertUIStyle::Danger;
		DeleteSessionDef.Visibility = MakeAttributeSP(this, &SConcertBrowser::IsDeleteSessionVisible, Item);
		DeleteSessionDef.Text = FEditorFontGlyphs::Trash;
		DeleteSessionDef.ToolTipText = LOCTEXT("DeleteSessionToolTip", "Delete this session");
		DeleteSessionDef.OnClicked.BindSP(this, &SConcertBrowser::OnClickDeleteSession, Item->SessionName);

		// Join Session
		FConcertUIButtonDefinition& JoinSessionDef = ButtonDefs.AddDefaulted_GetRef();
		JoinSessionDef.Style = EConcertUIStyle::Success;
		JoinSessionDef.Visibility = MakeAttributeSP(this, &SConcertBrowser::IsJoinSessionVisible, Item->SessionName);
		JoinSessionDef.Text = FEditorFontGlyphs::Sign_In;
		JoinSessionDef.ToolTipText = LOCTEXT("JoinSessionToolTip", "Join this session");
		JoinSessionDef.OnClicked.BindSP(this, &SConcertBrowser::OnClickJoinSession, Item->SessionName);

		// Leave Session
		FConcertUIButtonDefinition& LeaveSessionDef = ButtonDefs.AddDefaulted_GetRef();
		LeaveSessionDef.Style = EConcertUIStyle::Danger;
		LeaveSessionDef.Visibility = MakeAttributeSP(this, &SConcertBrowser::IsLeaveSessionVisible, Item->SessionName);
		LeaveSessionDef.Text = FEditorFontGlyphs::Sign_Out;
		LeaveSessionDef.ToolTipText = LOCTEXT("LeaveSessionToolTip", "Leave this session");
		LeaveSessionDef.OnClicked.BindSP(this, &SConcertBrowser::OnClickLeaveSession);

	}

	ConcertFrontendUtils::AppendButtons(SessionRow, ButtonDefs);

	return SNew(STableRow<TSharedPtr<FConcertServerInfo>>, OwnerTable)
		.SignalSelectionMode(ETableRowSignalSelectionMode::Instantaneous)
		.ToolTipText(Item->ToDisplayString())
		[
			SessionRow
		];
}

TSharedRef<ITableRow> SConcertBrowser::MakeClientRowWidget(TSharedPtr<FConcertSessionClientInfo> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	FLinearColor ClientNormalColor = Item->ClientInfo.AvatarColor * 0.8f;
	ClientNormalColor.A = Item->ClientInfo.AvatarColor.A;

	FLinearColor ClientOutlineColor = Item->ClientInfo.AvatarColor * 0.6f;
	ClientOutlineColor.A = Item->ClientInfo.AvatarColor.A;

	FSlateFontInfo ClientIconFontInfo = FEditorStyle::Get().GetFontStyle(ConcertFrontendUtils::ButtonIconSyle);
	ClientIconFontInfo.Size = 8;
	ClientIconFontInfo.OutlineSettings.OutlineSize = 1;
	ClientIconFontInfo.OutlineSettings.OutlineColor = ClientOutlineColor;

	FText ClientDisplayName = FText::FromString(Item->ClientInfo.DisplayName);
	if (ConcertClient.IsValid())
	{
		TSharedPtr<IConcertClientSession> ConcertClientSession = ConcertClient->GetCurrentSession();
		if (ConcertClientSession.IsValid() && Item->ClientEndpointId == ConcertClientSession->GetSessionClientEndpointId())
		{
			ClientDisplayName = FText::Format(LOCTEXT("ClientDisplayNameIsYouFmt", "{0} (You)"), ClientDisplayName);
		}
	}

	TSharedRef<SHorizontalBox> ClientRow =
		SNew(SHorizontalBox)

		// Color Icon
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(STextBlock)
			.Font(ClientIconFontInfo)
			.ColorAndOpacity(ClientNormalColor)
			.Text(FEditorFontGlyphs::Square)
		]

		// Client Info
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			ConcertFrontendUtils::CreateDisplayName(ClientDisplayName)
		];

	// Append the buttons to the client row
	{
		TArray<FConcertUIButtonDefinition> ButtonDefs;
		IConcertUICoreModule::Get().GetConcertBrowserClientButtonExtension().Broadcast(*Item, ButtonDefs);

		ConcertFrontendUtils::AppendButtons(ClientRow, ButtonDefs);
	}

	return
		SNew(STableRow<TSharedPtr<FConcertServerInfo>>, OwnerTable)
		.SignalSelectionMode(ETableRowSignalSelectionMode::Instantaneous)
		.ToolTipText(Item->ToDisplayString())
		[
			ClientRow
		];
}

TSharedPtr<FConcertServerInfo> SConcertBrowser::FindAvailableServer(const FGuid& InInstanceId) const
{
	const TSharedPtr<FConcertServerInfo>* FoundServerPtr = AvailableServers.FindByPredicate([&InInstanceId](const TSharedPtr<FConcertServerInfo>& PotentialServer)
	{
		return PotentialServer->InstanceInfo.InstanceId == InInstanceId;
	});
	return FoundServerPtr ? *FoundServerPtr : nullptr;
}

TSharedPtr<FConcertSessionInfo> SConcertBrowser::FindAvailableSession(const FString& InSessionName) const
{
	const TSharedPtr<FConcertSessionInfo>* FoundSessionPtr = AvailableSessions.FindByPredicate([&InSessionName](const TSharedPtr<FConcertSessionInfo>& PotentialSession)
	{
		return PotentialSession->SessionName == InSessionName;
	});
	return FoundSessionPtr ? *FoundSessionPtr : nullptr;
}

TSharedPtr<FConcertSessionClientInfo> SConcertBrowser::FindAvailableClient(const FGuid& InClientEndpointId) const
{
	const TSharedPtr<FConcertSessionClientInfo>* FoundClientPtr = AvailableClients.FindByPredicate([&InClientEndpointId](const TSharedPtr<FConcertSessionClientInfo>& PotentialClient)
	{
		return PotentialClient->ClientEndpointId == InClientEndpointId;
	});
	return FoundClientPtr ? *FoundClientPtr : nullptr;
}

void SConcertBrowser::SetSelectedServer(const FGuid& InInstanceId, ESelectInfo::Type SelectInfo)
{
	if (AvailableServersListView.IsValid())
	{
		TSharedPtr<FConcertServerInfo> NewSelectedServer = FindAvailableServer(InInstanceId);
		if (NewSelectedServer.IsValid())
		{
			AvailableServersListView->SetSelection(NewSelectedServer, SelectInfo);
		}
		else
		{
			AvailableServersListView->ClearSelection();
		}
	}
}

void SConcertBrowser::SetSelectedSession(const FString& InSessionName, ESelectInfo::Type SelectInfo)
{
	if (AvailableSessionsListView.IsValid())
	{
		TSharedPtr<FConcertSessionInfo> NewSelectedSession = FindAvailableSession(InSessionName);
		if (NewSelectedSession.IsValid())
		{
			AvailableSessionsListView->SetSelection(NewSelectedSession, SelectInfo);
		}
		else
		{
			AvailableSessionsListView->ClearSelection();
		}
	}
}

void SConcertBrowser::SetSelectedClient(const FGuid& InClientEndpointId, ESelectInfo::Type SelectInfo)
{
	if (AvailableClientsListView.IsValid())
	{
		TSharedPtr<FConcertSessionClientInfo> NewSelectedClient = FindAvailableClient(InClientEndpointId);
		if (NewSelectedClient.IsValid())
		{
			AvailableClientsListView->SetSelection(NewSelectedClient, SelectInfo);
		}
		else
		{
			AvailableClientsListView->ClearSelection();
		}
	}
}

void SConcertBrowser::HandleServerSelectionChanged(TSharedPtr<FConcertServerInfo> Item, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		// Ignore events triggered as part of preserving the selection state
		return;
	}

	PendingSelection.Reset();
	RefreshAvailableSessions();
}

void SConcertBrowser::HandleSessionSelectionChanged(TSharedPtr<FConcertSessionInfo> Item, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		// Ignore events triggered as part of preserving the selection state
		return;
	}

	PendingSelection.Reset();
	RefreshAvailableClients();
}

void SConcertBrowser::HandleClientSelectionChanged(TSharedPtr<FConcertSessionClientInfo> Item, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		// Ignore events triggered as part of preserving the selection state
		return;
	}

	PendingSelection.Reset();
}

bool SConcertBrowser::ShouldQueryCurrentSession(const FString& InSessionName) const
{
	return InSessionName.IsEmpty() || (ConcertClient.IsValid() && ConcertClient->GetCurrentSession().IsValid() && InSessionName == ConcertClient->GetCurrentSession()->GetSessionInfo().SessionName);
}

bool SConcertBrowser::IsSessionConnectedToSelectedServer(const FString& InSessionName) const
{
	if (ShouldQueryCurrentSession(InSessionName))
	{
		//Is connected to a session 
		if (ConcertClient->GetSessionConnectionStatus() == EConcertConnectionStatus::Connected)
		{
			//Is the session from the selected server
			TSharedPtr<FConcertServerInfo> SelectedServer = AvailableServersListView.IsValid() ? AvailableServersListView->GetSelection() : nullptr;
			if (SelectedServer.IsValid()
				&& ConcertClient.IsValid()
				&& ConcertClient->GetCurrentSession().IsValid()
				&& SelectedServer->InstanceInfo.InstanceId == ConcertClient->GetCurrentSession()->GetSessionInfo().ServerInstanceId)
			{
				return true;
			}
		}
	}
	return false;
}

bool SConcertBrowser::IsSessionSuspended(const FString& InSessionName) const
{
	const bool bQueryCurrentSession = ShouldQueryCurrentSession(InSessionName);
	return bQueryCurrentSession && ConcertClient->IsSessionSuspended();
}

FSlateFontInfo SConcertBrowser::GetConnectionIconFontInfo() const
{
	const FButtonStyle& ButtonStyle = GetConnectionIconStyle();

	FSlateFontInfo ConnectionIconFontInfo = FEditorStyle::Get().GetFontStyle(ConcertFrontendUtils::ButtonIconSyle);
	ConnectionIconFontInfo.OutlineSettings.OutlineSize = 1;
	ConnectionIconFontInfo.OutlineSettings.OutlineColor = ButtonStyle.Pressed.TintColor.GetSpecifiedColor();

	return ConnectionIconFontInfo;
}

FSlateColor SConcertBrowser::GetConnectionIconColor() const
{
	const FButtonStyle& ButtonStyle = GetConnectionIconStyle();
	return ButtonStyle.Normal.TintColor;
}

const FButtonStyle& SConcertBrowser::GetConnectionIconStyle() const
{
	EConcertUIStyle ButtonStyle = EConcertUIStyle::Danger;

	const bool bIsConnected = ConcertClient.IsValid() && ConcertClient->GetSessionConnectionStatus() == EConcertConnectionStatus::Connected;
	if (bIsConnected)
	{
		const FConcertSessionInfo& ConnectedSessionInfo = ConcertClient->GetCurrentSession()->GetSessionInfo();
		const TSharedPtr<FConcertServerInfo> FoundServer = FindAvailableServer(ConnectedSessionInfo.ServerInstanceId);
		if (FoundServer.IsValid())
		{
			const bool bIsSuspended = ConcertClient->GetCurrentSession()->IsSuspended();
			if (bIsSuspended)
			{
				ButtonStyle = EConcertUIStyle::Warning;
			}
			else
			{
				ButtonStyle = EConcertUIStyle::Success;
			}
		}
		else
		{
			ButtonStyle = EConcertUIStyle::Warning;
		}
	}
	
	return FEditorStyle::Get().GetWidgetStyle<FButtonStyle>(ConcertFrontendUtils::ButtonStyleNames[(int32)ButtonStyle]);
}

FText SConcertBrowser::GetConnectionStatusText() const
{
	FText StatusText = LOCTEXT("StatusDisconnected", "Disconnected");

	const bool bIsConnected = ConcertClient.IsValid() && ConcertClient->GetSessionConnectionStatus() == EConcertConnectionStatus::Connected;
	if (bIsConnected)
	{
		const FConcertSessionInfo& ConnectedSessionInfo = ConcertClient->GetCurrentSession()->GetSessionInfo();
		const TSharedPtr<FConcertServerInfo> FoundServer = FindAvailableServer(ConnectedSessionInfo.ServerInstanceId);
		if (FoundServer.IsValid())
		{
			const FText SessionDisplayName = FText::FromString(ConnectedSessionInfo.SessionName);
			const FText ServerDisplayName = FText::FromString(FoundServer->ServerName);

			const bool bIsSuspended = ConcertClient->GetCurrentSession()->IsSuspended();
			if (bIsSuspended)
			{
				StatusText = FText::Format(LOCTEXT("StatusSuspendedFmt", "Suspended: {0} on {1}"), SessionDisplayName, ServerDisplayName);
			}
			else
			{
				StatusText = FText::Format(LOCTEXT("StatusConnectedFmt", "Connected: {0} on {1}"), SessionDisplayName, ServerDisplayName);
			}
		}
		else
		{
			StatusText = LOCTEXT("StatusConnectedServerUnknown", "Connected (Server Unknown)");
		}
	}
	
	return StatusText;
}

bool SConcertBrowser::IsCreateSessionEnabled() const
{
	TSharedPtr<FConcertServerInfo> SelectedServer = AvailableServersListView.IsValid() ? AvailableServersListView->GetSelection() : nullptr;
	return SelectedServer.IsValid();
}

FReply SConcertBrowser::OnClickCreateSession()
{
	if (CreateSessionWindow.IsValid())
	{
		TSharedPtr<SWindow> Window = CreateSessionWindow.Pin();
		Window->FlashWindow();
	}
	else if (ConcertClient.IsValid())
	{
		TSharedPtr<FConcertServerInfo> SelectedServer = AvailableServersListView.IsValid() ? AvailableServersListView->GetSelection() : nullptr;

		// This act as a disarm. Without it some issues might occur when the concert browser close
		TWeakPtr<SWidget> BrowserPtr = this->AsShared();

		ConcertClient->GetSavedSessionNames(SelectedServer->AdminEndpointId)
			.Next([BrowserPtr, SelectedServer, ConcertClient = ConcertClient](const FConcertAdmin_GetSavedSessionNamesResponse& Response)
				{
					TSharedPtr<SWidget> BrowserSharedPtr = BrowserPtr.Pin();
					if (BrowserSharedPtr && SelectedServer.IsValid() && Response.ResponseCode == EConcertResponseCode::Success)
					{
						TSharedRef<FStructOnScope> CreateSessionSettings = MakeShared<FStructOnScope>(FCreateSessionOptions::StaticStruct());

						FCreateSessionOptions* CreateSessionSettingsPtr = reinterpret_cast<FCreateSessionOptions*>(CreateSessionSettings->GetStructMemory());
						CreateSessionSettingsPtr->ServerName = SelectedServer->ServerName;
						CreateSessionSettingsPtr->SessionToRestoreOptions = Response.SavedSessionNames;
						FConcertSettingsDialogArgs Arguments;
						Arguments.WindowLabel = LOCTEXT("CreateSessionWindowLabel", "Create A Session");
						Arguments.ConfirmText = LOCTEXT("CreateSessionCreateButtonText", "Create");
						Arguments.CancelText = LOCTEXT("CreateSessionCancelButtonText", "Cancel");
						Arguments.CancelTooltipText = LOCTEXT("CreateSessionCancelTooltip", "Cancel the creation of this session.");

						Arguments.ConfirmTooltipText = MakeAttributeLambda([CreateSessionSettings]()
							{
								FCreateSessionOptions* CreateSessionSettingsPtr = reinterpret_cast<FCreateSessionOptions*>(CreateSessionSettings->GetStructMemory());
								if (CreateSessionSettingsPtr->SessionName.IsEmpty())
								{
									return LOCTEXT("CreateSessionConfirmationTooltipWhenDisabled", "Enter a name for the session.");
								}
								return LOCTEXT("CreateSessionConfirmationTooltip", "Create the session.");
							});
						Arguments.IsConfirmEnabled = MakeAttributeLambda([CreateSessionSettings]()
							{
								FCreateSessionOptions* CreateSessionSettingsPtr = reinterpret_cast<FCreateSessionOptions*>(CreateSessionSettings->GetStructMemory());
								return !CreateSessionSettingsPtr->SessionName.IsEmpty();
							});
						Arguments.ConfirmCallback.BindLambda([ConcertClient, CreateSessionSettings, AdminEndpointId = SelectedServer->AdminEndpointId]()
							{
								if (ConcertClient.IsValid())
								{
									FCreateSessionOptions* CreateSessionSettingsPtr = reinterpret_cast<FCreateSessionOptions*>(CreateSessionSettings->GetStructMemory());
									FConcertCreateSessionArgs CreateSessionArgs;
									CreateSessionArgs.SessionName = CreateSessionSettingsPtr->SessionName;
									CreateSessionArgs.SessionToRestore = CreateSessionSettingsPtr->bSessionToRestoreEnabled ? CreateSessionSettingsPtr->SessionToRestore : FString();
									CreateSessionArgs.SaveSessionAs = CreateSessionSettingsPtr->bSaveSessionAsEnabled ? CreateSessionSettingsPtr->SaveSessionAs : FString();
									ConcertClient->CreateSession(AdminEndpointId, CreateSessionArgs);
								}
							});

						SConcertBrowser* Browser = (SConcertBrowser*) BrowserSharedPtr.Get();
						if (Browser)
						{
							Browser->CreateSessionWindow = SConcertSettingsDialog::AddWindow(MoveTemp(Arguments), CreateSessionSettings, 0.55f);
						}
					}
				});
	}

	return FReply::Handled();
}

EVisibility SConcertBrowser::IsJoinSessionVisible(FString InSessionName) const
{
	return (!IsSessionConnectedToSelectedServer(InSessionName))
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SConcertBrowser::IsLeaveSessionVisible(FString InSessionName) const
{
	return (IsSessionConnectedToSelectedServer(InSessionName))
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SConcertBrowser::IsStatusBarLeaveSessionVisible() const
{
	return (ConcertClient.IsValid() && ConcertClient->GetSessionConnectionStatus() == EConcertConnectionStatus::Connected) 
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

FReply SConcertBrowser::OnClickJoinSession(FString InSessionName)
{
	TSharedPtr<FConcertServerInfo> SelectedServer = AvailableServersListView.IsValid() ? AvailableServersListView->GetSelection() : nullptr;
	TSharedPtr<FConcertSessionInfo> SelectedSession = AvailableSessionsListView.IsValid() ? AvailableSessionsListView->GetSelection() : nullptr;

	const FString& SessionNameToJoin = (InSessionName.IsEmpty() && SelectedSession.IsValid()) ? SelectedSession->SessionName : InSessionName;
	if (SelectedServer.IsValid() && !SessionNameToJoin.IsEmpty() && ConcertClient.IsValid())
	{
		ConcertClient->JoinSession(SelectedServer->AdminEndpointId, SessionNameToJoin);
	}

	return FReply::Handled();
}

FReply SConcertBrowser::OnClickLeaveSession()
{
	if (ConcertClient.IsValid())
	{
		ConcertClient->DisconnectSession();
	}

	return FReply::Handled();
}

EVisibility SConcertBrowser::IsSuspendSessionVisible(FString InSessionName) const
{
	return (IsSessionConnectedToSelectedServer(InSessionName) && !IsSessionSuspended(InSessionName))
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SConcertBrowser::IsStatusBarSuspendSessionVisible() const
{
	return (ConcertClient.IsValid() && ConcertClient->GetSessionConnectionStatus() == EConcertConnectionStatus::Connected && !ConcertClient->IsSessionSuspended())
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SConcertBrowser::IsActiveSessionVisible(FString InSessionName) const
{
	return (IsSessionConnectedToSelectedServer(InSessionName))
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SConcertBrowser::IsResumeSessionVisible(FString InSessionName) const
{
	return (IsSessionConnectedToSelectedServer(InSessionName) && IsSessionSuspended(InSessionName))
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SConcertBrowser::IsStatusBarActiveSessionVisible() const
{
	return (ConcertClient.IsValid() && ConcertClient->GetCurrentSession().IsValid())
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SConcertBrowser::IsStatusBarResumeSessionVisible() const
{
	return (ConcertClient.IsValid() && ConcertClient->GetSessionConnectionStatus() == EConcertConnectionStatus::Connected && ConcertClient->IsSessionSuspended())
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

FReply SConcertBrowser::OnClickSuspendSession()
{
	if (ConcertClient.IsValid())
	{
		ConcertClient->SuspendSession();
	}

	return FReply::Handled();
}

FReply SConcertBrowser::OnClickActiveSession()
{
	FGlobalTabmanager::Get()->InvokeTab(FTabId("ConcertActiveSession"));

	return FReply::Handled();
}

FReply SConcertBrowser::OnClickResumeSession()
{
	if (ConcertClient.IsValid())
	{
		ConcertClient->ResumeSession();
	}

	return FReply::Handled();
}

EVisibility SConcertBrowser::IsDeleteSessionVisible(TSharedPtr<FConcertSessionInfo> InSessionInfo) const
{
	if (InSessionInfo.IsValid())
	{
		if (!IsSessionConnectedToSelectedServer(InSessionInfo->SessionName) && ConcertClient.IsValid())
		{
			if (ConcertClient->IsOwnerOf(*InSessionInfo.Get()))
			{
				return EVisibility::Visible;
			}
		}
	}

	return EVisibility::Collapsed;
}

FReply SConcertBrowser::OnClickDeleteSession(FString InSessionName)
{
	TSharedPtr<FConcertServerInfo> SelectedServer = AvailableServersListView.IsValid() ? AvailableServersListView->GetSelection() : nullptr;

	if (ConcertClient.IsValid() && SelectedServer.IsValid())
	{
		const FText SessionNameInText = FText::FromString(InSessionName);
		const FText SeverNameInText = FText::FromString(SelectedServer->ServerName);
		const FText ConfirmationMessage = FText::Format(LOCTEXT("DeleteSessionConfirmationMessage", "Do you really want to delete the session \"{0}\" from the server \"{1}\"?"), SessionNameInText, SeverNameInText);
		const FText ConfirmationTitle = LOCTEXT("DeleteSessionConfirmationTitle", "Delete Session Confirmation");
		const bool bDeleteConfirmed = EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, ConfirmationMessage, &ConfirmationTitle);
		if (bDeleteConfirmed)
		{
			ConcertClient->DeleteSession(SelectedServer->AdminEndpointId, InSessionName);
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
