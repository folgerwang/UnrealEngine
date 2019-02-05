// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "IConcertModule.h"
#include "ConcertMessages.h"

class IConcertClientSession;
struct FConcertSessionClientInfo;
class SDockTab;

/**
 * Implements the active session tab.
 */
class SActiveSession : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SActiveSession) { }
	SLATE_END_ARGS();

	/**
	* Constructs the active session tab.
	*
	* @param InArgs The Slate argument list.
	* @param ConstructUnderMajorTab The major tab which will contain the session front-end.
	* @param ConstructUnderWindow The window in which this widget is being constructed.
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow);

private:

	/** Generate a new client row */
	TSharedRef<ITableRow> HandleGenerateRow(TSharedPtr<FConcertSessionClientInfo> InClientInfo, const TSharedRef<STableViewBase>& OwnerTable) const;

	/** Handle a session startup */
	void HandleSessionStartup(TSharedRef<IConcertClientSession> InClientSession);

	/** Handle a session shutdown */
	void HandleSessionShutdown(TSharedRef<IConcertClientSession> InClientSession);

	/** Handle a session client change */
	void HandleSessionClientChanged(IConcertClientSession&, EConcertClientStatus ClientStatus, const FConcertSessionClientInfo& ClientInfo);

	/** Polls the local client info and detect if it changed in order to update its representation in real time.*/
	EActiveTimerReturnType HandleLocalClientInfoChangePollingTimer(double InCurrentTime, float InDeltaTime);

	/** Update the list of clients while keeping the alphabetical sorting */
	void UpdateSessionClientListView(const FConcertSessionClientInfo* Client = nullptr, EConcertClientStatus Status = EConcertClientStatus::Updated);

	/** Set the selected client in the clients list view*/
	void SetSelectedClient(const FGuid& InClientEndpointId, ESelectInfo::Type SelectInfo = ESelectInfo::Direct);

	/** Find a client with its endpoint id */
	TSharedPtr<FConcertSessionClientInfo> FindAvailableClient(const FGuid& InClientEndpointId) const;
	
	/** Handling for the status icon and text */
	const FButtonStyle& GetConnectionIconStyle() const;
	FSlateColor GetConnectionIconColor() const;
	FSlateFontInfo GetConnectionIconFontInfo() const;
	FText GetConnectionStatusText() const;

	/** Handling for the suspend, resume and leave session buttons */
	EVisibility IsStatusBarSuspendSessionVisible() const;
	EVisibility IsStatusBarResumeSessionVisible() const;
	EVisibility IsStatusBarLeaveSessionVisible() const;
	FReply OnClickSuspendSession();
	FReply OnClickResumeSession();
	FReply OnClickLeaveSession();

private:
	/** Holds a concert client session. */
	TWeakPtr<IConcertClientSession> WeakSessionPtr;

	/** List view for AvailableClients. */
	TSharedPtr<SListView<TSharedPtr<FConcertSessionClientInfo>>> ClientsListView;

	/** List of clients for the current session. */
	TArray<TSharedPtr<FConcertSessionClientInfo>> Clients;

	/** Information about the machine's client. */
	TSharedPtr<FConcertSessionClientInfo> ClientInfo;

	/** Delegate handle for session clients state changes. */
	FDelegateHandle SessionClientChangedHandle;

	/** Holds a concert activity log. */
	TSharedPtr<class SSessionHistory> SessionHistory;
};
