// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Components/ListViewBase.h"
#include "UMGPrivate.h"
#include "Widgets/Text/STextBlock.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "UMG"

UListViewBase::UListViewBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EntryWidgetPool(*this)
{
	bIsVariable = true;
}

#if WITH_EDITOR
const FText UListViewBase::GetPaletteCategory()
{
	return LOCTEXT("Lists", "Lists");
}

void UListViewBase::ValidateCompiledDefaults(FCompilerResultsLog& CompileLog) const
{
	if (!EntryWidgetClass)
	{
		CompileLog.Error(*FText::Format(LOCTEXT("Error_MissingEntryClass", "{0} has no EntryWidgetClass specified - required for any UListViewBase to function."), FText::FromString(GetName())).ToString());
	}
}
#endif

void UListViewBase::RegenerateAllEntries()
{
	EntryWidgetPool.ReleaseAll();
	GeneratedEntriesToAnnounce.Reset();

	if (MyTableViewBase.IsValid())
	{
		MyTableViewBase->RebuildList();
	}
}

void UListViewBase::ScrollToTop()
{
	if (MyTableViewBase.IsValid())
	{
		MyTableViewBase->ScrollToTop();
	}
}

void UListViewBase::ScrollToBottom()
{
	if (MyTableViewBase.IsValid())
	{
		MyTableViewBase->ScrollToBottom();
	}
}

const TArray<UUserWidget*>& UListViewBase::GetDisplayedEntryWidgets() const
{ 
	return EntryWidgetPool.GetActiveWidgets(); 
}

TSharedRef<SWidget> UListViewBase::RebuildWidget()
{
	if (!EntryWidgetClass)
	{
		return SNew(STextBlock)
			.Text(LOCTEXT("Error_MissingEntryWidgetClass", "There is no EntryWidgetClass specified on this list.\nEven if doing custom stuff, this is always required as a fallback."));
	}

	MyTableViewBase = RebuildListWidget();
	return MyTableViewBase.ToSharedRef();
}

void UListViewBase::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyTableViewBase.Reset();
	EntryWidgetPool.ResetPool();
	GeneratedEntriesToAnnounce.Reset();
}

void UListViewBase::SynchronizeProperties()
{
	Super::SynchronizeProperties();

#if WITH_EDITORONLY_DATA
	if (IsDesignTime())
	{
		bNeedsToCallRefreshDesignerItems = true;
		OnRefreshDesignerItems();

		if (!ensureMsgf(!bNeedsToCallRefreshDesignerItems, TEXT("[%s] does not call RefreshDesignerItems<T> from within OnRefreshDesignerItems. Please do so to support design-time previewing of list entries."), *GetClass()->GetName()))
		{
			bNeedsToCallRefreshDesignerItems = false;
		}
	}
#endif
}

TSharedRef<STableViewBase> UListViewBase::RebuildListWidget()
{
	ensureMsgf(false, TEXT("All children of UListViewBase must implement RebuildListWidget using one of the static ITypedUMGListView<T>::ConstructX functions"));
	return SNew(SListView<TSharedPtr<FString>>);
}

void UListViewBase::RequestRefresh()
{
	if (MyTableViewBase.IsValid())
	{
		MyTableViewBase->RequestListRefresh();
	}
}

void UListViewBase::HandleRowReleased(const TSharedRef<ITableRow>& Row)
{
	UUserWidget* EntryWidget = StaticCastSharedRef<IObjectTableRow>(Row)->GetUserWidget();
	if (ensure(EntryWidget))
	{
		EntryWidgetPool.Release(EntryWidget);

		if (!IsDesignTime())
		{
			GeneratedEntriesToAnnounce.Remove(EntryWidget);
			OnEntryWidgetReleased().Broadcast(*EntryWidget);
			BP_OnEntryReleased.Broadcast(EntryWidget);
		}
	}
}

void UListViewBase::FinishGeneratingEntry(UUserWidget& GeneratedEntry)
{
	if (!IsDesignTime())
	{
		// Announcing the row generation now is just a bit too soon, as we haven't finished generating the row as far as the underlying list is concerned. 
		// So we cache the row/item pair here and announce their generation on the next tick
		GeneratedEntriesToAnnounce.AddUnique(&GeneratedEntry);
		if (!EntryGenAnnouncementTimerHandle.IsValid())
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimerForNextTick(this, &UListViewBase::HandleAnnounceGeneratedEntries);
			}
		}
	}
}

void UListViewBase::HandleAnnounceGeneratedEntries()
{
	EntryGenAnnouncementTimerHandle.Invalidate();
	for (TWeakObjectPtr<UUserWidget> EntryWidget : GeneratedEntriesToAnnounce)
	{
		if (EntryWidget.IsValid())
		{
			OnEntryWidgetGenerated().Broadcast(*EntryWidget);
			BP_OnEntryGenerated.Broadcast(EntryWidget.Get());
		}
	}
	GeneratedEntriesToAnnounce.Empty();
}

#undef LOCTEXT_NAMESPACE
