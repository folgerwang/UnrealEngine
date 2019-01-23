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
#include "IConcertSyncClientModule.h"

#define LOCTEXT_NAMESPACE "SActiveSession"

namespace ActiveSessionDetailsUI
{
	static const FName DisplayNameColumnName(TEXT("DisplayName"));
	static const FName PresenceColumnName(TEXT("Presence"));
	static const FName LevelColumnName(TEXT("Level"));
}


class SActiveSessionDetailsRow : public SMultiColumnTableRow<TSharedPtr<FConcertSessionClientInfo>>
{
	SLATE_BEGIN_ARGS(SActiveSessionDetailsRow) {}
	SLATE_END_ARGS()

public:
	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InClientInfo The client displayed by this row.
	 * @param InClientSession The session in which the client is, used to determine if the client is the local one, so that we can suffix it with a "you".
	 * @param InOwnerTableView The table to which the row must be added.
	 */
	void Construct(const FArguments& InArgs, TSharedPtr<FConcertSessionClientInfo> InClientInfo, TWeakPtr<IConcertClientSession> InClientSession, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		SessionClientInfo = MoveTemp(InClientInfo);
		ClientSession = MoveTemp(InClientSession);
		SMultiColumnTableRow<TSharedPtr<FConcertSessionClientInfo>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);

		// Set the tooltip for the entire row. Will show up unless there is another item with a tooltip hovered in the row, such as the "presence" icons.
		SetToolTipText(MakeAttributeSP(this, &SActiveSessionDetailsRow::GetRowToolTip));
	}

public:
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == ActiveSessionDetailsUI::DisplayNameColumnName)
		{
			// Displays a colored square from a special font (using avatar color) followed by the the display name -> [x] John Smith
			return SNew(SHorizontalBox)
				// The 'square' glyph in front of the client name, rendered using special font glyph, in the client avatar color.
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.Font(this, &SActiveSessionDetailsRow::GetAvatarFont)
					.ColorAndOpacity(this, &SActiveSessionDetailsRow::GetAvatarColor)
					.Text(FEditorFontGlyphs::Square)
				]

				// The client display name.
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					ConcertFrontendUtils::CreateDisplayName(MakeAttributeSP(this, &SActiveSessionDetailsRow::GetDisplayName))
				];
		}
		else if (ColumnName == ActiveSessionDetailsUI::PresenceColumnName)
		{
			// Displays a set of icons corresponding to the client presence. The set may be extended later to include other functionalities.
			TArray<FConcertUIButtonDefinition> ButtonDefs;
			TSharedRef<SHorizontalBox> PresenceCell = SNew(SHorizontalBox);

			TSharedPtr<FConcertSessionClientInfo> ClientInfoPin = SessionClientInfo.Pin();
			if (ClientInfoPin.IsValid())
			{
				IConcertUICoreModule::Get().GetConcertBrowserClientButtonExtension().Broadcast(*ClientInfoPin, ButtonDefs);
				ConcertFrontendUtils::AppendButtons(PresenceCell, ButtonDefs);
			}
			return PresenceCell;
		}
		else // LevelColumnName
		{
			check(ColumnName == ActiveSessionDetailsUI::LevelColumnName); // If this fail, was a column added/removed/renamed ?

			// Displays which "level" the client is editing, playing (PIE) or simulating (SIE).
			return SNew(SBox)
				.Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock)
					.Text(this, &SActiveSessionDetailsRow::GetLevel)
				];
		}
	}

	FText GetRowToolTip() const
	{
		// This is a tooltip for the entire row. Like display name, the tooltip will not update in real time if the user change its
		// settings. See GetDisplayName() for more info.
		TSharedPtr<FConcertSessionClientInfo> ClientInfoPin = SessionClientInfo.Pin();
		return ClientInfoPin.IsValid() ? ClientInfoPin->ToDisplayString() : FText();
	}

	FText GetDisplayName() const
	{
		TSharedPtr<FConcertSessionClientInfo> ClientInfoPin = SessionClientInfo.Pin();
		if (ClientInfoPin.IsValid())
		{
			// NOTE: The display name doesn't update in real time at the moment because the concert setting are not propagated
			//       until the client disconnect/reconnect. Since those settings should not change often, this should not
			//       be a major deal breaker for the users.
			TSharedPtr<IConcertClientSession> ClientSessionPin = ClientSession.Pin();
			if (ClientSessionPin.IsValid() && ClientInfoPin->ClientEndpointId == ClientSessionPin->GetSessionClientEndpointId())
			{
				return FText::Format(LOCTEXT("ClientDisplayNameIsYouFmt", "{0} (You)"), FText::FromString(ClientSessionPin->GetLocalClientInfo().DisplayName));
			}

			// Return the ClientInfo cached.
			return FText::FromString(ClientInfoPin->ClientInfo.DisplayName);
		}

		return FText();
	}

	FText GetLevel() const
	{
		TSharedPtr<FConcertSessionClientInfo> ClientInfoPin = SessionClientInfo.Pin();
		if (ClientInfoPin.IsValid())
		{
			// The world path is returned as something like /Game/MyMap.MyMap, but we are only interested to keep the
			// string left to the '.' to display "/Game/MyMap"
			FString WorldPath = IConcertSyncClientModule::Get().GetPresenceWorldPath(ClientInfoPin->ClientEndpointId);
			int Pos;
			if (WorldPath.FindLastChar('.', Pos))
			{
				return FText::FromString(WorldPath.LeftChop(WorldPath.Len() - Pos));
			}

			// Maybe the '.' was not found, just output the world path as is.
			return FText::FromString(WorldPath);
		}

		return FText();
	}

	FSlateFontInfo GetAvatarFont() const
	{
		// This font is used to render a small square box filled with the avatar color.
		FSlateFontInfo ClientIconFontInfo = FEditorStyle::Get().GetFontStyle(ConcertFrontendUtils::ButtonIconSyle);
		ClientIconFontInfo.Size = 8;
		ClientIconFontInfo.OutlineSettings.OutlineSize = 1;

		TSharedPtr<FConcertSessionClientInfo> ClientInfoPin = SessionClientInfo.Pin();
		if (ClientInfoPin.IsValid())
		{
			FLinearColor ClientOutlineColor = ClientInfoPin->ClientInfo.AvatarColor * 0.6f; // Make the font outline darker.
			ClientOutlineColor.A = ClientInfoPin->ClientInfo.AvatarColor.A; // Put back the original alpha.
			ClientIconFontInfo.OutlineSettings.OutlineColor = ClientOutlineColor;
		}
		else
		{
			ClientIconFontInfo.OutlineSettings.OutlineColor = FLinearColor(0.75, 0.75, 0.75); // This is an arbitrary color.
		}

		return ClientIconFontInfo;
	}

	FSlateColor GetAvatarColor() const
	{
		TSharedPtr<FConcertSessionClientInfo> ClientInfoPin = SessionClientInfo.Pin();
		if (ClientInfoPin.IsValid())
		{
			return ClientInfoPin->ClientInfo.AvatarColor;
		}

		return FSlateColor(FLinearColor(0.75, 0.75, 0.75)); // This is an arbitrary color.
	}

private:
	TWeakPtr<FConcertSessionClientInfo> SessionClientInfo;
	TWeakPtr<IConcertClientSession> ClientSession;
};


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
						.HeaderRow
						(
							SNew(SHeaderRow)
							+SHeaderRow::Column(ActiveSessionDetailsUI::DisplayNameColumnName)
							.DefaultLabel(LOCTEXT("UserDisplayName", "Display Name"))
							+SHeaderRow::Column(ActiveSessionDetailsUI::PresenceColumnName)
							.DefaultLabel(LOCTEXT("UserPresence", "User Presence"))
							+SHeaderRow::Column(ActiveSessionDetailsUI::LevelColumnName)
							.DefaultLabel(LOCTEXT("UserLevel", "Level"))
						)
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
	// Generate a row for the client corresponding to InClientInfo.
	return SNew(SActiveSessionDetailsRow, InClientInfo, WeakSessionPtr, OwnerTable);
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
