// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SSessionHistory.h"
#include "IConcertSession.h"
#include "IConcertSyncClientModule.h"
#include "IConcertClientWorkspace.h"
#include "ConcertTransactionEvents.h"
#include "ConcertWorkspaceData.h"
#include "ConcertMessageData.h"
#include "ConcertMessages.h"
#include "ConcertFrontendStyle.h"
#include "Editor/Transactor.h"
#include "Algo/Transform.h"
#include "EditorStyleSet.h"
#include "SPackageDetails.h"
#include "Widgets/SUndoHistoryDetails.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Colors/SColorBlock.h"
#include "ConcertActivityEvents.h"
#include "UObject/StructOnScope.h"
#include "ConcertActivityLedger.h"
#include "SConcertScrollBox.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "SSessionHistory"

namespace ConcertSessionHistoryUI
{
	bool FilterPackageName(const FName& PackageNameFilter, const FStructOnScope& InEvent)
	{
		if (PackageNameFilter.IsNone())
		{
			return true;
		}

		if (InEvent.GetStruct()->IsChildOf(FConcertTransactionActivityEvent::StaticStruct()))
		{
			if (FConcertTransactionActivityEvent* Event = (FConcertTransactionActivityEvent*)InEvent.GetStructMemory())
			{
				return Event->PackageName == PackageNameFilter;
			}
		}
		else if (InEvent.GetStruct()->IsChildOf(FConcertPackageUpdatedActivityEvent::StaticStruct()))
		{
			if (FConcertPackageUpdatedActivityEvent* Event = (FConcertPackageUpdatedActivityEvent*)InEvent.GetStructMemory())
			{
				return Event->PackageName == PackageNameFilter;
			}
		}

		return false;
	}
};

void SSessionHistory::Construct(const FArguments& InArgs)
{
	PackageNameFilter = InArgs._PackageFilter;
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SAssignNew(ScrollBox, SConcertScrollBox)
			+SConcertScrollBox::Slot()
			[
				SAssignNew(ActivityListView, SListView<TSharedPtr<FStructOnScope>>)
				.OnGenerateRow(this, &SSessionHistory::HandleGenerateRow)
				.OnSelectionChanged(this, &SSessionHistory::HandleSelectionChanged)
				.ListItemsSource(&Activities)
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(ExpandableDetails, SExpandableArea)
			.Visibility(EVisibility::Visible)
			.InitiallyCollapsed(true)
			.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
			.BodyBorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.BodyBorderBackgroundColor(FLinearColor::White)
			.HeaderContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString("Details")))
				.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
			]
			.BodyContent()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				[
					SAssignNew(TransactionDetails, SUndoHistoryDetails)
					.Visibility(EVisibility::Collapsed)
				]
				
 				+SVerticalBox::Slot()
 				[
 					SAssignNew(PackageDetails, SPackageDetails)
 					.Visibility(EVisibility::Collapsed)
 				]
			]
		]
	];

	ExpandableDetails->SetEnabled(false);

	if (IConcertSyncClientModule::IsAvailable())
	{
		IConcertSyncClientModule& ClientModule = IConcertSyncClientModule::Get();
		ClientModule.OnWorkspaceStartup().AddSP(this, &SSessionHistory::HandleWorkspaceStartup);
		ClientModule.OnWorkspaceShutdown().AddSP(this, &SSessionHistory::HandleWorkspaceShutdown);

		TSharedPtr<IConcertClientWorkspace> WorkspacePtr = ClientModule.GetWorkspace();
		if (WorkspacePtr.IsValid())
		{
			Workspace = WorkspacePtr;
			RegisterWorkspaceHandler();
			ReloadActivities();
		}
	}
}

SSessionHistory::~SSessionHistory()
{
	if (IConcertSyncClientModule::IsAvailable())
	{
		IConcertSyncClientModule& ClientModule = IConcertSyncClientModule::Get();
		ClientModule.OnWorkspaceShutdown().RemoveAll(this);
		ClientModule.OnWorkspaceStartup().RemoveAll(this);
	}
}

void SSessionHistory::Refresh()
{
	ReloadActivities();
}

TSharedRef<ITableRow> SSessionHistory::HandleGenerateRow(TSharedPtr<FStructOnScope> InEvent, const TSharedRef<STableViewBase>& OwnerTable) const
{
	FText ActivityText = LOCTEXT("InvalidActivity", "INVALID_ACTIVITY");
	FLinearColor AvatarColor;

	if (InEvent.IsValid())
	{
		if (FConcertActivityEvent* ActivityEvent = ((FConcertActivityEvent*)InEvent->GetStructMemory()))
		{
			ActivityText = FText::Format(INVTEXT("{0}  {1}"), FText::FromString(ActivityEvent->TimeStamp.ToString(TEXT("%Y-%m-%d %H:%M:%S"))), ActivityEvent->ToDisplayText(true));
			AvatarColor = ActivityEvent->ClientInfo.AvatarColor;

			if (ActivityEvent->ClientInfo.DisplayName.IsEmpty())
			{
				// The client info is malformed or the user has disconnected.
				AvatarColor = FConcertFrontendStyle::Get()->GetColor("Concert.DisconnectedColor");
			}
		}
	}

	return SNew(STableRow<TSharedPtr<FText>>, OwnerTable)
		.Padding(2.0)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(0.f, 0.f, 3.f, 0.f)
			.AutoWidth()
			[
				SNew(SColorBlock)
				.Color(AvatarColor)
				.Size(FVector2D(4.f, 20.f))
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(SRichTextBlock)
				.DecoratorStyleSet(FConcertFrontendStyle::Get().Get())
				.Text(ActivityText)
			]
		];
}

void SSessionHistory::HandleSelectionChanged(TSharedPtr<FStructOnScope> InItem, ESelectInfo::Type SelectInfo)
{
	if (!InItem.IsValid())
	{
		return;
	}

	TSharedPtr<IConcertClientWorkspace> WorkspacePtr = Workspace.Pin();
	if (InItem->GetStruct()->IsChildOf(FConcertTransactionActivityEvent::StaticStruct()))
	{
		if (FConcertTransactionActivityEvent* ActivityEvent = (FConcertTransactionActivityEvent*)InItem->GetStructMemory())
		{
			FConcertTransactionFinalizedEvent TransactionEvent;
			if (WorkspacePtr->FindTransactionEvent(ActivityEvent->TransactionIndex, TransactionEvent))
			{
				DisplayTransactionDetails(TransactionEvent, ActivityEvent->TransactionTitle.ToString());
			}
		}
	}
	else if (InItem->GetStruct()->IsChildOf(FConcertPackageUpdatedActivityEvent::StaticStruct()))
	{
		if (FConcertPackageUpdatedActivityEvent* ActivityEvent = (FConcertPackageUpdatedActivityEvent*)InItem->GetStructMemory())
		{
			FConcertPackageInfo PackageInfo;
			if (WorkspacePtr->FindPackageEvent(ActivityEvent->PackageName, ActivityEvent->Revision, PackageInfo))
			{
				DisplayPackageDetails(PackageInfo, ActivityEvent->Revision, ActivityEvent->ClientInfo.DisplayName);
			}
		}
	}
	else
	{
		TransactionDetails->SetVisibility(EVisibility::Collapsed);
		PackageDetails->SetVisibility(EVisibility::Collapsed);
		ExpandableDetails->SetEnabled(false);
		ExpandableDetails->SetExpanded(false);
	}
}

void SSessionHistory::ReloadActivities()
{
	TSharedPtr<IConcertClientWorkspace> WorkspacePtr = Workspace.Pin();
	Activities.Reset();
	if (WorkspacePtr.IsValid())
	{
		TArray<FStructOnScope> ActivitiesToCopy;
		WorkspacePtr->GetLastActivities(MaximumNumberOfActivities, ActivitiesToCopy);

		auto FilterPredicate = [PackageNameFilter = this->PackageNameFilter](const FStructOnScope& Activity)
		{
			return ConcertSessionHistoryUI::FilterPackageName(PackageNameFilter, Activity);
		};
				
		Algo::TransformIf(ActivitiesToCopy, Activities, FilterPredicate, [](const FStructOnScope& InActivity)
		{
			FStructOnScope ActivityCopy;
			CopyActivityEvent(InActivity, ActivityCopy);
			return MakeShared<FStructOnScope>(MoveTemp(ActivityCopy));
		});
	}
	ActivityListView->RequestListRefresh();
}

void SSessionHistory::HandleNewActivity(const FStructOnScope& InActivityEvent, uint64 ActivityIndex)
{
	if (ConcertSessionHistoryUI::FilterPackageName(PackageNameFilter, InActivityEvent))
	{
		FStructOnScope ActivityCopy;
		CopyActivityEvent(InActivityEvent, ActivityCopy);
		Activities.Add(MakeShared<FStructOnScope>(MoveTemp(ActivityCopy)));
	}
	ActivityListView->RequestListRefresh();
}

void SSessionHistory::HandleWorkspaceStartup(const TSharedPtr<IConcertClientWorkspace>& NewWorkspace)
{
	Workspace = NewWorkspace;
	RegisterWorkspaceHandler();
}

void SSessionHistory::HandleWorkspaceShutdown(const TSharedPtr<IConcertClientWorkspace>& WorkspaceShuttingDown)
{
	if (WorkspaceShuttingDown == Workspace)
	{
		Workspace.Reset();
		ReloadActivities();
	}
}

void SSessionHistory::RegisterWorkspaceHandler()
{
	TSharedPtr<IConcertClientWorkspace> WorkspacePtr = Workspace.Pin();
	if (WorkspacePtr.IsValid())
	{
		WorkspacePtr->OnAddActivity().AddSP(this, &SSessionHistory::HandleNewActivity);
		WorkspacePtr->OnWorkspaceSynchronized().AddSP(this, &SSessionHistory::ReloadActivities);
	}
}

void SSessionHistory::DisplayTransactionDetails(const FConcertTransactionEventBase& InTransaction, const FString& InTransactionTitle)
{
	FTransactionDiff TransactionDiff{ InTransaction.TransactionId, InTransactionTitle };

	for (const auto& ExportedObject : InTransaction.ExportedObjects)
	{
		FTransactionObjectDeltaChange DeltaChange;

		Algo::Transform(ExportedObject.PropertyDatas, DeltaChange.ChangedProperties, [](FConcertSerializedPropertyData PropertyData) { return PropertyData.PropertyName; });

		DeltaChange.bHasNameChange = ExportedObject.ObjectData.NewOuterPathName != FName();
		DeltaChange.bHasOuterChange = ExportedObject.ObjectData.NewOuterPathName != FName();
		DeltaChange.bHasPendingKillChange = ExportedObject.ObjectData.bIsPendingKill;

		FString ObjectPathName = ExportedObject.ObjectId.ObjectOuterPathName.ToString() + TEXT(".") + ExportedObject.ObjectId.ObjectName.ToString();

		TSharedPtr<FTransactionObjectEvent> Event = MakeShared<FTransactionObjectEvent>(InTransaction.TransactionId, InTransaction.OperationId, ETransactionObjectEventType::Finalized, MoveTemp(DeltaChange), nullptr, ExportedObject.ObjectId.ObjectName, FName(*MoveTemp(ObjectPathName)), ExportedObject.ObjectId.ObjectOuterPathName, ExportedObject.ObjectId.ObjectClassPathName);

		TransactionDiff.DiffMap.Emplace(FName(*ObjectPathName), MoveTemp(Event));
	}

	TransactionDetails->SetSelectedTransaction(MoveTemp(TransactionDiff));

	PackageDetails->SetVisibility(EVisibility::Collapsed);
	TransactionDetails->SetVisibility(EVisibility::Visible);

	ExpandableDetails->SetEnabled(true);
	ExpandableDetails->SetExpanded(true);
}

void SSessionHistory::DisplayPackageDetails(const FConcertPackageInfo& InPackageInfo, const uint32 InRevision, const FString& InModifiedBy)
{
	PackageDetails->SetPackageInfo(InPackageInfo, InRevision, InModifiedBy);

	TransactionDetails->SetVisibility(EVisibility::Collapsed);
	PackageDetails->SetVisibility(EVisibility::Visible);

	ExpandableDetails->SetEnabled(true);
	ExpandableDetails->SetExpanded(true);
}

void SSessionHistory::CopyActivityEvent(const FStructOnScope& InActivityEvent, FStructOnScope& OutCopiedActivityEvent)
{
	const UScriptStruct* ScriptStruct = CastChecked<const UScriptStruct>(InActivityEvent.GetStruct());
	OutCopiedActivityEvent = FStructOnScope(InActivityEvent.GetStruct());
	ScriptStruct->CopyScriptStruct(OutCopiedActivityEvent.GetStructMemory(), InActivityEvent.GetStructMemory());
}

#undef LOCTEXT_NAMESPACE /* SSessionHistory */
