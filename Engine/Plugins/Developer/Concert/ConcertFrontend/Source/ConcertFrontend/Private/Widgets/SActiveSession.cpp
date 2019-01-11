// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SActiveSession.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "Algo/Transform.h"
#include "EditorFontGlyphs.h"
#include "EditorStyleSet.h"
#include "IConcertClient.h"
#include "ConcertFrontendUtils.h"
#include "IConcertFrontendModule.h"
#include "IConcertUICoreModule.h"
#include "ConcertMessageData.h"
#include "SSessionHistory.h"

#define LOCTEXT_NAMESPACE "SActiveSession"

void SActiveSession::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow)
{
	IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();

	if (ConcertClient.IsValid())
	{
		ConcertClient->OnSessionStartup().AddSP(this, &SActiveSession::HandleSessionStartup);
		ConcertClient->OnSessionShutdown().AddSP(this, &SActiveSession::HandleSessionShutdown);

		TSharedPtr<IConcertClientSession> ClientSession = ConcertClient->GetCurrentSession();
		if (ClientSession.IsValid())
		{
			WeakSessionPtr = ClientSession;
			ClientInfo = FConcertSessionClientInfo({ ClientSession->GetSessionClientEndpointId(), ClientSession->GetLocalClientInfo() });
			SessionClientChangedHandle = ClientSession->OnSessionClientChanged().AddSP(this, &SActiveSession::HandleSessionClientChanged);
		}
	}

	TSharedRef<SHorizontalBox> StatusBar = 
		SNew(SHorizontalBox)

		// Status Icon
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(2.0f, 1.0f, 0.0f, 1.0f))
		[
			SNew(STextBlock)
			.Font(this, &SActiveSession::GetConnectionIconFontInfo)
			.ColorAndOpacity(this, &SActiveSession::GetConnectionIconColor)
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
				.Text(this, &SActiveSession::GetConnectionStatusText)
			]
		];

	// Append the buttons to the status bar
	{
		TArray<FConcertUIButtonDefinition> ButtonDefs;
		IConcertUICoreModule::Get().GetConcertBrowserStatusButtonExtension().Broadcast(ButtonDefs);

		// Resume Session
		FConcertUIButtonDefinition& ResumeSessionDef = ButtonDefs.AddDefaulted_GetRef();
		ResumeSessionDef.Style = EConcertUIStyle::Success;
		ResumeSessionDef.Visibility = MakeAttributeSP(this, &SActiveSession::IsStatusBarResumeSessionVisible);
		ResumeSessionDef.Text = FEditorFontGlyphs::Play_Circle;
		ResumeSessionDef.ToolTipText = LOCTEXT("ResumeCurrentSessionToolTip", "Resume receiving updates from the current session");
		ResumeSessionDef.OnClicked.BindSP(this, &SActiveSession::OnClickResumeSession);

		// Suspend Session
		FConcertUIButtonDefinition& SuspendSessionDef = ButtonDefs.AddDefaulted_GetRef();
		SuspendSessionDef.Style = EConcertUIStyle::Warning;
		SuspendSessionDef.Visibility = MakeAttributeSP(this, &SActiveSession::IsStatusBarSuspendSessionVisible);
		SuspendSessionDef.Text = FEditorFontGlyphs::Pause_Circle;
		SuspendSessionDef.ToolTipText = LOCTEXT("SuspendCurrentSessionToolTip", "Suspend receiving updates from the current session");
		SuspendSessionDef.OnClicked.BindSP(this, &SActiveSession::OnClickSuspendSession);

		// Leave Session
		FConcertUIButtonDefinition& LeaveSessionDef = ButtonDefs.AddDefaulted_GetRef();
		LeaveSessionDef.Style = EConcertUIStyle::Danger;
		LeaveSessionDef.Visibility = MakeAttributeSP(this, &SActiveSession::IsStatusBarLeaveSessionVisible);
		LeaveSessionDef.Text = FEditorFontGlyphs::Sign_Out;
		LeaveSessionDef.ToolTipText = LOCTEXT("LeaveCurrentSessionToolTip", "Leave the current session");
		LeaveSessionDef.OnClicked.BindSP(this, &SActiveSession::OnClickLeaveSession);

		ConcertFrontendUtils::AppendButtons(StatusBar, ButtonDefs);
	}

	ChildSlot
	.Padding(0.0f, 0.0f, 0.0f, 2.0f)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[	
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+SSplitter::Slot()
			.Value(0.2)
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(0.0f)
				[
					SNew(SExpandableArea)
					.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
					.BodyBorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.BodyBorderBackgroundColor(FLinearColor::White)
					.HeaderContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SessionConnectedClients", "Connected Clients"))
						.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
						.ShadowOffset(FVector2D(1.0f, 1.0f))
					]
					.BodyContent()
					[
						SAssignNew(ClientsListView, SListView<TSharedPtr<FConcertSessionClientInfo>>)
						.ItemHeight(20.0f)
						.SelectionMode(ESelectionMode::Single)
						.ListItemsSource(&Clients)
						.OnGenerateRow(this, &SActiveSession::HandleGenerateRow)
					]
				]
			]

			+SSplitter::Slot()
			.Value(0.8)
			[
 				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(0.f)
				[
					SNew(SExpandableArea)
					.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
					.BodyBorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.BodyBorderBackgroundColor(FLinearColor::White)
					.HeaderContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SessionHistory", "History"))
						.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
						.ShadowOffset(FVector2D(1.0f, 1.0f))
					]
					.BodyContent()
					[
						SNew(SSessionHistory)
					]
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

	ReloadClients();
}

TSharedRef<ITableRow> SActiveSession::HandleGenerateRow(TSharedPtr<FConcertSessionClientInfo> InClientInfo, const TSharedRef<STableViewBase>& OwnerTable) const
{
	FLinearColor ClientNormalColor = InClientInfo->ClientInfo.AvatarColor * 0.8f;
	ClientNormalColor.A = InClientInfo->ClientInfo.AvatarColor.A;

	FLinearColor ClientOutlineColor = InClientInfo->ClientInfo.AvatarColor * 0.6f;
	ClientOutlineColor.A = InClientInfo->ClientInfo.AvatarColor.A;

	FSlateFontInfo ClientIconFontInfo = FEditorStyle::Get().GetFontStyle(ConcertFrontendUtils::ButtonIconSyle);
	ClientIconFontInfo.Size = 8;
	ClientIconFontInfo.OutlineSettings.OutlineSize = 1;
	ClientIconFontInfo.OutlineSettings.OutlineColor = ClientOutlineColor;

	FText ClientDisplayName = FText::FromString(InClientInfo->ClientInfo.DisplayName);

	TSharedPtr<IConcertClientSession> ClientSession = WeakSessionPtr.Pin();

	if (ClientSession.IsValid() && InClientInfo->ClientEndpointId == ClientSession->GetSessionClientEndpointId())
	{
		ClientDisplayName = FText::Format(LOCTEXT("ClientDisplayNameIsYouFmt", "{0} (You)"), ClientDisplayName);
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
		IConcertUICoreModule::Get().GetConcertBrowserClientButtonExtension().Broadcast(*InClientInfo, ButtonDefs);

		ConcertFrontendUtils::AppendButtons(ClientRow, ButtonDefs);
	}

	return SNew(STableRow<TSharedPtr<FConcertServerInfo>>, OwnerTable)
		.SignalSelectionMode(ETableRowSignalSelectionMode::Instantaneous)
		.ToolTipText(InClientInfo->ToDisplayString())
		[
			ClientRow
		];
}

void SActiveSession::HandleSessionStartup(TSharedRef<IConcertClientSession> InClientSession)
{
	WeakSessionPtr = InClientSession;
	SessionClientChangedHandle = InClientSession->OnSessionClientChanged().AddSP(this, &SActiveSession::HandleSessionClientChanged);

	ClientInfo = FConcertSessionClientInfo({ InClientSession->GetSessionClientEndpointId(), InClientSession->GetLocalClientInfo() });

	ReloadClients();

	if (SessionHistory.IsValid())
	{
		SessionHistory->Refresh();
	}
}

void SActiveSession::HandleSessionShutdown(TSharedRef<IConcertClientSession> InClientSession)
{
	if (InClientSession == WeakSessionPtr)
	{
		WeakSessionPtr.Reset();
		InClientSession->OnSessionClientChanged().Remove(SessionClientChangedHandle);
		Clients.Reset();

		if (ClientsListView.IsValid())
		{
			ClientsListView->RequestListRefresh();
		}

		if (SessionHistory.IsValid())
		{
			SessionHistory->Refresh();
		}
	}
}

void SActiveSession::HandleSessionClientChanged(IConcertClientSession&, EConcertClientStatus ClientStatus, const FConcertSessionClientInfo& InClientInfo)
{
	if (ClientStatus == EConcertClientStatus::Connected || ClientStatus == EConcertClientStatus::Updated)
	{
		UpdateAvailableClients({ MakeShared<FConcertSessionClientInfo>(InClientInfo) });
	}
	else
	{
		int32 ClientIndex = Clients.IndexOfByPredicate([&InClientInfo](const TSharedPtr<FConcertSessionClientInfo>& PotentialClient)
		{
			return PotentialClient->ClientEndpointId == InClientInfo.ClientEndpointId;
		});

		if (ClientIndex != INDEX_NONE)
		{
			RefreshListViewAfterFunction([this, ClientIndex]() 
			{
				Clients.RemoveAt(ClientIndex);
			});
		}
	}
}

void SActiveSession::UpdateAvailableClients(TArray<TSharedPtr<FConcertSessionClientInfo>>&& InAvailableClients)
{
	RefreshListViewAfterFunction([&Clients = this->Clients, &ClientInfo = this->ClientInfo, AvailableClients = MoveTemp(InAvailableClients)]() mutable
	{
		ConcertFrontendUtils::SyncArraysByPredicate(Clients, MoveTemp(AvailableClients), [](const TSharedPtr<FConcertSessionClientInfo>& ClientToFind)
		{
			return [ClientToFind](const TSharedPtr<FConcertSessionClientInfo>& PotentialClient)
			{
				return PotentialClient->ClientEndpointId == ClientToFind->ClientEndpointId;
			};
		});

		if (ClientInfo.IsSet())
		{
			Clients.Emplace(MakeShared<FConcertSessionClientInfo>(ClientInfo.GetValue()));
		}

		Clients.StableSort([](const TSharedPtr<FConcertSessionClientInfo>& ClientOne, const TSharedPtr<FConcertSessionClientInfo>& ClientTwo)
		{
			return ClientOne->ClientInfo.DisplayName < ClientTwo->ClientInfo.DisplayName;
		});
	});
}

void SActiveSession::RefreshListViewAfterFunction(TFunctionRef<void()> InFunction)
{
	if (ensureMsgf(ClientsListView.IsValid(), TEXT("RefreshListViewAfterFunction should not be called with an invalid clients ListView.")))
	{
		TSharedPtr<FConcertSessionClientInfo> SelectedClient;
		TArray<TSharedPtr<FConcertSessionClientInfo>> SelectedClients = ClientsListView->GetSelectedItems();

		checkf(SelectedClients.Num() <= 1, TEXT("ActiveSession's client list view should not support multiple selection."));

		if (SelectedClients.Num() > 0)
		{
			SelectedClient = SelectedClients[0];
		}

		InFunction();

		ClientsListView->RequestListRefresh();

		if (SelectedClient.IsValid())
		{
			SetSelectedClient(SelectedClient->ClientEndpointId);
		}
	}
}

const FButtonStyle& SActiveSession::GetConnectionIconStyle() const
{
	EConcertUIStyle ButtonStyle = EConcertUIStyle::Danger;
	
	TSharedPtr<IConcertClientSession> ClientSession = WeakSessionPtr.Pin();
	if (ClientSession.IsValid())
	{
		if (ClientSession->GetConnectionStatus() == EConcertConnectionStatus::Connected)
		{
			if (ClientSession->IsSuspended())
			{
				ButtonStyle = EConcertUIStyle::Warning;
			}
			else
			{
				ButtonStyle = EConcertUIStyle::Success;
			}
		}
	}
	
	return FEditorStyle::Get().GetWidgetStyle<FButtonStyle>(ConcertFrontendUtils::ButtonStyleNames[(int32)ButtonStyle]);
}

FSlateColor SActiveSession::GetConnectionIconColor() const
{
	return GetConnectionIconStyle().Normal.TintColor;
}

FSlateFontInfo SActiveSession::GetConnectionIconFontInfo() const
{
	FSlateFontInfo ConnectionIconFontInfo = FEditorStyle::Get().GetFontStyle(ConcertFrontendUtils::ButtonIconSyle);
	ConnectionIconFontInfo.OutlineSettings.OutlineSize = 1;
	ConnectionIconFontInfo.OutlineSettings.OutlineColor = GetConnectionIconStyle().Pressed.TintColor.GetSpecifiedColor();

	return ConnectionIconFontInfo;
}

FText SActiveSession::GetConnectionStatusText() const
{
	FText StatusText = LOCTEXT("StatusDisconnected", "Disconnected");
	TSharedPtr<IConcertClientSession> ClientSessionPtr = WeakSessionPtr.Pin();
	if (ClientSessionPtr.IsValid() && ClientSessionPtr->GetConnectionStatus() == EConcertConnectionStatus::Connected)
	{
		const FText SessionDisplayName = FText::FromString(ClientSessionPtr->GetSessionInfo().SessionName);

		if (ClientSessionPtr->IsSuspended())
		{
			StatusText = FText::Format(LOCTEXT("StatusSuspendedFmt", "Suspended: {0}"), SessionDisplayName);
		}
		else
		{
			StatusText = FText::Format(LOCTEXT("StatusConnectedFmt", "Connected: {0}"), SessionDisplayName);
		}
	}

	return StatusText;
}

EVisibility SActiveSession::IsStatusBarSuspendSessionVisible() const
{
	EVisibility ActiveSessionVisibility = EVisibility::Collapsed;
	TSharedPtr<IConcertClientSession> ClientSession = WeakSessionPtr.Pin();
	if (ClientSession.IsValid())
	{
		if (ClientSession->GetConnectionStatus() == EConcertConnectionStatus::Connected && !ClientSession->IsSuspended())
		{
			ActiveSessionVisibility = EVisibility::Visible;
		}
	}
	
	return ActiveSessionVisibility;
}

EVisibility SActiveSession::IsStatusBarResumeSessionVisible() const
{
	EVisibility ResumeSessionVisiblity = EVisibility::Collapsed;
	TSharedPtr<IConcertClientSession> ClientSession = WeakSessionPtr.Pin();
	if (ClientSession.IsValid())
	{
		if (ClientSession->GetConnectionStatus() == EConcertConnectionStatus::Connected && ClientSession->IsSuspended())
		{
			ResumeSessionVisiblity = EVisibility::Visible;
		}
	}

	return ResumeSessionVisiblity;
}

EVisibility SActiveSession::IsStatusBarLeaveSessionVisible() const
{
	EVisibility LeaveSessionVisibility = EVisibility::Collapsed;
	TSharedPtr<IConcertClientSession> ClientSession = WeakSessionPtr.Pin();
	if (ClientSession.IsValid() && ClientSession->GetConnectionStatus() == EConcertConnectionStatus::Connected)
	{
		LeaveSessionVisibility = EVisibility::Visible;
	}

	return LeaveSessionVisibility;
}

FReply SActiveSession::OnClickSuspendSession()
{
	TSharedPtr<IConcertClientSession> ClientSession = WeakSessionPtr.Pin();
	if (ClientSession.IsValid())
	{
		ClientSession->Suspend();
	}

	return FReply::Handled();
}

FReply SActiveSession::OnClickResumeSession()
{
	TSharedPtr<IConcertClientSession> ClientSession = WeakSessionPtr.Pin();
	if (ClientSession.IsValid())
	{
		ClientSession->Resume();
	}

	return FReply::Handled();
}

FReply SActiveSession::OnClickLeaveSession()
{
	TSharedPtr<IConcertClientSession> ClientSession = WeakSessionPtr.Pin();
	if (ClientSession.IsValid())
	{
		ClientSession->Disconnect();
	}

	return FReply::Handled();
}

void SActiveSession::SetSelectedClient(const FGuid& InClientEndpointId, ESelectInfo::Type SelectInfo)
{
	if (ClientsListView.IsValid())
	{
		TSharedPtr<FConcertSessionClientInfo> NewSelectedClient = FindAvailableClient(InClientEndpointId);

		if (NewSelectedClient.IsValid())
		{
			ClientsListView->SetSelection(NewSelectedClient, SelectInfo);
		}
		else
		{
			ClientsListView->ClearSelection();
		}
	}
}

TSharedPtr<FConcertSessionClientInfo> SActiveSession::FindAvailableClient(const FGuid& InClientEndpointId) const
{
	const TSharedPtr<FConcertSessionClientInfo>* FoundClientPtr = Clients.FindByPredicate([&InClientEndpointId](const TSharedPtr<FConcertSessionClientInfo>& PotentialClient)
	{
		return PotentialClient->ClientEndpointId == InClientEndpointId;
	});

	return FoundClientPtr ? *FoundClientPtr : nullptr;
}

void SActiveSession::ReloadClients()
{
	TSharedPtr<IConcertClientSession> ClientSession = WeakSessionPtr.Pin();
	TArray<TSharedPtr<FConcertSessionClientInfo>> ClientPtrs;

	if (ClientSession.IsValid())
	{
		TArray<FConcertSessionClientInfo> AvailableClients = ClientSession->GetSessionClients();

		ClientPtrs.Reserve(AvailableClients.Num() + 1);
		Algo::Transform(AvailableClients, ClientPtrs, [](FConcertSessionClientInfo InClient)
		{
			return MakeShared<FConcertSessionClientInfo>(InClient);
		});
	}

	UpdateAvailableClients(MoveTemp(ClientPtrs));
}

#undef LOCTEXT_NAMESPACE /* SActiveSession */
