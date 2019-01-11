// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

struct FConcertActivityEvent;
class IConcertClientWorkspace;
struct FConcertTransactionEventBase;
struct FConcertPackageInfo;
class SUndoHistoryDetails;
class SPackageDetails;
class SExpandableArea;
class SScrollBar;
class SConcertScrollBox;
class FStructOnScope;

class SSessionHistory : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSessionHistory) {}
		SLATE_ARGUMENT(FName, PackageFilter)
	SLATE_END_ARGS()

	/**
	* Constructs the Session History widget.
	*
	* @param InArgs The Slate argument list.
	* @param ConstructUnderMajorTab The major tab which will contain the Session History widget.
	*/
	void Construct(const FArguments& InArgs);

	/** Fetches the activities and updates the UI. */
	void Refresh();

	~SSessionHistory();

private:
	
	/** Generates a new event row. */
	TSharedRef<ITableRow> HandleGenerateRow(TSharedPtr<FStructOnScope> InClientInfo, const TSharedRef<STableViewBase>& OwnerTable) const;

	/** Callback for selecting an activity in the list view. */
	void HandleSelectionChanged(TSharedPtr<FStructOnScope> InItem, ESelectInfo::Type SelectInfo);

	/** Fetches activities from the server and updates the list view. */
	void ReloadActivities();

	/** Callback for handling the creation of a new activity. */ 
	void HandleNewActivity(const FStructOnScope& InActivityEvent, uint64 ActivityIndex);

	/** Callback for handling the startup of a workspace.  */
	void HandleWorkspaceStartup(const TSharedPtr<IConcertClientWorkspace>& NewWorkspace);

	/** Callback for handling the shutdown of a workspace. */
	void HandleWorkspaceShutdown(const TSharedPtr<IConcertClientWorkspace>& WorkspaceShuttingDown);

	/** Registers callbacks with the current workspace. */
	void RegisterWorkspaceHandler();

	/** Open the details section and display transaction details. */
	void DisplayTransactionDetails(const FConcertTransactionEventBase& InTransaction, const FString& InTransactionTitle);

	/** Open the package details section and display the package details. */
	void DisplayPackageDetails(const FConcertPackageInfo& InPackageInfo, const uint32 InRevision, const FString& InModifiedBy);

	/** Deep copy an activity event. */
	static void CopyActivityEvent(const FStructOnScope& InActivityEvent, FStructOnScope& OutCopiedActivityEvent);

private:

	/** Maximum number of activities displayed on screen. */ 
	static const uint32 MaximumNumberOfActivities = 1000;
	
	/** Holds the concert activities. */
	TArray<TSharedPtr<FStructOnScope>> Activities;

	/** Holds an instance of an undo history details panel. */
	TSharedPtr<SUndoHistoryDetails> TransactionDetails;

	TSharedPtr<SPackageDetails> PackageDetails;

	/** Holds activities. */
	TSharedPtr<SListView<TSharedPtr<FStructOnScope>>> ActivityListView;

	/** Holds the expandable area containing details about a given activity. */
	TSharedPtr<SExpandableArea> ExpandableDetails;

	/** Holds the history log scroll bar. */
	TSharedPtr<SConcertScrollBox> ScrollBox;

	/** Holds a weak pointer to the current workspace. */ 
	TWeakPtr<IConcertClientWorkspace> Workspace;

	/** Holds the session history scroll bar. */
	TSharedPtr<SScrollBar> ScrollBar;

	FName PackageNameFilter;

};