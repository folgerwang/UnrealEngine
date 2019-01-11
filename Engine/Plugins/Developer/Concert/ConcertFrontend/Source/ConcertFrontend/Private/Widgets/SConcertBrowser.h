// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

#include "IConcertModule.h"  // Change to use Fwd or Ptr.h?
#include "ConcertMessages.h"

class IConcertClientSession;

template <typename ItemType>
class SConcertListView;

/**
 * Implements the Concert Browser.
 */
class SConcertBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConcertBrowser) { }
	SLATE_END_ARGS();


	~SConcertBrowser();
	/**
	* Constructs the Browser.
	*
	* @param InArgs The Slate argument list.
	* @param ConstructUnderMajorTab The major tab which will contain the session front-end.
	* @param ConstructUnderWindow The window in which this widget is being constructed.
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow);

private:
	/** Called when the known servers list is updated to refresh the UI */
	void HandleKnownServersUpdated();

	/** Called when the session connection state is changed */
	void HandleSessionConnectionChanged(IConcertClientSession& InSession, EConcertConnectionStatus ConnectionStatus);

	/** Update session/client discovery based on the current selection */
	void UpdateDiscovery();

	/** Update the list of discovered servers */
	void UpdateAvailableServers(TArray<TSharedPtr<FConcertServerInfo>>&& InAvailableServers);

	/** Update the list of discovered sessions */
	void UpdateAvailableSessions(TArray<TSharedPtr<FConcertSessionInfo>>&& InAvailableSessions);

	/** Update the list of discovered clients */
	void UpdateAvailableClients(TArray<TSharedPtr<FConcertSessionClientInfo>>&& InAvailableClients);

	/** Discard the current list of sessions/clients and request new ones (eg, when the selected server is changed) */
	void RefreshAvailableSessions();

	/** Discard the current list of clients and request new ones (eg, when the selected session is changed) */
	void RefreshAvailableClients();

	/** Tick session/client discovery based on the current selection */
	EActiveTimerReturnType TickDiscovery(double InCurrentTime, float InDeltaTime);

	/** Generate a new server row */
	TSharedRef<ITableRow> MakeServerRowWidget(TSharedPtr<FConcertServerInfo> Item, const TSharedRef<STableViewBase>& OwnerTable) const;

	/** Generate a new session row */
	TSharedRef<ITableRow> MakeSessionRowWidget(TSharedPtr<FConcertSessionInfo> Item, const TSharedRef<STableViewBase>& OwnerTable) const;

	/** Generate a new client row */
	TSharedRef<ITableRow> MakeClientRowWidget(TSharedPtr<FConcertSessionClientInfo> Item, const TSharedRef<STableViewBase>& OwnerTable) const;

	/** Find the available server entry for the given ID */
	TSharedPtr<FConcertServerInfo> FindAvailableServer(const FGuid& InInstanceId) const;

	/** Find the available session entry for the given ID */
	TSharedPtr<FConcertSessionInfo> FindAvailableSession(const FString& InSessionName) const;

	/** Find the available client entry for the given ID */
	TSharedPtr<FConcertSessionClientInfo> FindAvailableClient(const FGuid& InClientEndpointId) const;

	/** Set the selected server */
	void SetSelectedServer(const FGuid& InAdminEndpointId, ESelectInfo::Type SelectInfo = ESelectInfo::Direct);

	/** Set the selected session */
	void SetSelectedSession(const FString& InSessionName, ESelectInfo::Type SelectInfo = ESelectInfo::Direct);

	/** Set the selected client */
	void SetSelectedClient(const FGuid& InClientEndpointId, ESelectInfo::Type SelectInfo = ESelectInfo::Direct);

	/** Handle the server selection changing */
	void HandleServerSelectionChanged(TSharedPtr<FConcertServerInfo> Item, ESelectInfo::Type SelectInfo);

	/** Handle the session selection changing */
	void HandleSessionSelectionChanged(TSharedPtr<FConcertSessionInfo> Item, ESelectInfo::Type SelectInfo);

	/** Handle the client selection changing */
	void HandleClientSelectionChanged(TSharedPtr<FConcertSessionClientInfo> Item, ESelectInfo::Type SelectInfo);

	/** Is the given session name the current session? (an empty string always refers to the current session) */
	bool ShouldQueryCurrentSession(const FString& InSessionName) const;

	/** Is the given session the currently connected on the selected server ? */
	bool IsSessionConnectedToSelectedServer(const FString& InSessionName) const;

	/** Is the given session the currently suspended session? */
	bool IsSessionSuspended(const FString& InSessionName) const;

	/** Handling for the status icon and text */
	FSlateFontInfo GetConnectionIconFontInfo() const;
	FSlateColor GetConnectionIconColor() const;
	const FButtonStyle& GetConnectionIconStyle() const;
	FText GetConnectionStatusText() const;

	/** Handling for the create session button */
	bool IsCreateSessionEnabled() const;
	FReply OnClickCreateSession();
	
	/** Handling for the join and leave session buttons */
	EVisibility IsJoinSessionVisible(FString InSessionName) const;
	EVisibility IsLeaveSessionVisible(FString InSessionName) const;
	EVisibility IsStatusBarLeaveSessionVisible() const;
	FReply OnClickJoinSession(FString InSessionName);
	FReply OnClickLeaveSession();

	/** Handling for the suspend and resume session buttons */
	EVisibility IsSuspendSessionVisible(FString InSessionName) const;
	EVisibility IsStatusBarSuspendSessionVisible() const;
	EVisibility IsActiveSessionVisible(FString InSessionName) const;
	EVisibility IsResumeSessionVisible(FString InSessionName) const;
	EVisibility IsStatusBarResumeSessionVisible() const;
	EVisibility IsStatusBarActiveSessionVisible() const;

	FReply OnClickSuspendSession();
	FReply OnClickActiveSession();
	FReply OnClickResumeSession();

	/** Handling for the delete session button */ 
	EVisibility IsDeleteSessionVisible(TSharedPtr<FConcertSessionInfo> InSessionInfo) const;
	FReply OnClickDeleteSession(FString InSessionName);

	/** Holds a concert client instance */
	IConcertClientPtr ConcertClient;

	/** Delegate handle for server list updates */
	FDelegateHandle OnKnownServersUpdatedHandle;

	/** Delegate handle for session connection state changes */
	FDelegateHandle OnSessionConnectionChangedHandle;

	/** List of discovered servers */
	TArray<TSharedPtr<FConcertServerInfo>> AvailableServers;
	/** List view for AvailableServers */
	TSharedPtr<SConcertListView<TSharedPtr<FConcertServerInfo>>> AvailableServersListView;

	/** List of discovered sessions for the selected server */
	TArray<TSharedPtr<FConcertSessionInfo>> AvailableSessions;
	/** List view for AvailableSessions */
	TSharedPtr<SConcertListView<TSharedPtr<FConcertSessionInfo>>> AvailableSessionsListView;
	/** Future for the pending request of AvailableSessions for the selected server */
	TFuture<void> AvailableSessionsFuture;
	/** This share pointer is used as a workaround to disarm the AvailableSessionsFuture */
	TSharedPtr<uint8> AvailableSessionsFutureDisarm;

	/** List of discovered clients for the selected server and session */
	TArray<TSharedPtr<FConcertSessionClientInfo>> AvailableClients;
	/** List view for AvailableClients */
	TSharedPtr<SConcertListView<TSharedPtr<FConcertSessionClientInfo>>> AvailableClientsListView;
	/** Future for the pending request of AvailableClients for the selected server and session */
	TFuture<void> AvailableClientsFuture;
	/** This share pointer is used as a workaround to disarm the AvailableClientsFuture */
	TSharedPtr<uint8> AvailableClientsFutureDisarm;

	/** Optional pending server to select */
	struct FPendingSelection
	{
		FGuid ServerInstanceId;
		FString SessionName;
		FGuid ClientEndpointId;
	};
	TOptional<FPendingSelection> PendingSelection;

	TWeakPtr<SWindow> CreateSessionWindow;
};
