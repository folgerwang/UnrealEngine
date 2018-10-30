// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Engine/CompositeCurveTable.h"
#include "Misc/MessageDialog.h"

#include "EditorFramework/AssetImportData.h"
#if WITH_EDITOR
#include "CurveTableEditorUtils.h"
#endif

#define LOCTEXT_NAMESPACE "CompositeCurveTables"

//////////////////////////////////////////////////////////////////////////
UCompositeCurveTable::UCompositeCurveTable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCompositeCurveTable::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);
	for (const TSoftObjectPtr<UCurveTable>& ParentTable : ParentTables)
	{
		if (UCurveTable* Parent = ParentTable.Get())
		{
			OutDeps.Add(Parent);
		}
	}
}

void UCompositeCurveTable::PostLoad()
{
	Super::PostLoad();

	OnParentTablesUpdated();
}

void UCompositeCurveTable::UpdateCachedRowMap()
{
#if WITH_EDITOR
	FCurveTableEditorUtils::BroadcastPreChange(this, FCurveTableEditorUtils::ECurveTableChangeInfo::RowList);
#endif
	UCurveTable::EmptyTable();

	bool bLeaveEmpty = false;
	// Throw up an error message and stop if any loops are found
	if (const UCompositeCurveTable* LoopTable = FindLoops(TArray<const UCompositeCurveTable*>()))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("FoundLoopError", "Cyclic dependency found. Table {0} depends on itself. Please fix your data"), FText::FromString(LoopTable->GetPathName())));
		bLeaveEmpty = true;
	}

	if (!bLeaveEmpty)
	{
		// iterate through all of the rows
		for (const UCurveTable* ParentTable : ParentTables)
		{
			if (ParentTable == nullptr)
			{
				continue;
			}

			// Add new rows or overwrite previous rows
			for (TMap<FName, FRichCurve*>::TConstIterator RowMapIter(ParentTable->GetRowMap().CreateConstIterator()); RowMapIter; ++RowMapIter)
			{
				FRichCurve* NewCurve = new FRichCurve();
				FRichCurve* InCurve = RowMapIter.Value();
				NewCurve->SetKeys(InCurve->GetConstRefOfKeys());

				if (FRichCurve** Curve = RowMap.Find(RowMapIter.Key()))
				{
					delete *Curve;
					*Curve = NewCurve;
				}
				else
				{
					RowMap.Add(RowMapIter.Key(), NewCurve);
				}
			}
		}
	}

#if WITH_EDITOR
	FCurveTableEditorUtils::BroadcastPostChange(this, FCurveTableEditorUtils::ECurveTableChangeInfo::RowList);
#endif
}

#if WITH_EDITOR
void UCompositeCurveTable::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static FName Name_ParentTables = GET_MEMBER_NAME_CHECKED(UCompositeCurveTable, ParentTables);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	const FName PropertyName = PropertyThatChanged != nullptr ? PropertyThatChanged->GetFName() : NAME_None;

	if (PropertyName == Name_ParentTables)
	{
		OnParentTablesUpdated();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UCompositeCurveTable::PostEditUndo()
{
	Super::PostEditUndo();

	OnParentTablesUpdated();
}
#endif // WITH_EDITOR

void UCompositeCurveTable::OnParentTablesUpdated()
{
	UpdateCachedRowMap();

	for (UCurveTable* Table : OldParentTables)
	{
		if (Table && ParentTables.Find(Table) == INDEX_NONE)
		{
			Table->OnCurveTableChanged().RemoveAll(this);
		}
	}

	for (UCurveTable* Table : ParentTables)
	{
		if (Table && OldParentTables.Find(Table) == INDEX_NONE)
		{
			Table->OnCurveTableChanged().AddUObject(this, &UCompositeCurveTable::UpdateCachedRowMap);
		}
	}

	OldParentTables = ParentTables;
}

void UCompositeCurveTable::EmptyTable()
{
	// clear the parent tables
	ParentTables.Empty();

	Super::EmptyTable();
}

const UCompositeCurveTable* UCompositeCurveTable::FindLoops(TArray<const UCompositeCurveTable*> AlreadySeenTables) const
{
	AlreadySeenTables.Add(this);

	for (const TSoftObjectPtr<UCurveTable>& CurveTable : ParentTables)
	{
		// we only care about composite tables since regular tables terminate the chain and can't be in loops
		if (UCompositeCurveTable* CompositeCurveTable = Cast<UCompositeCurveTable>(CurveTable.Get()))
		{
			// if we've seen this table before then we have a loop
			for (const UCompositeCurveTable* SeenTable : AlreadySeenTables)
			{
				if (SeenTable == CompositeCurveTable)
				{
					return CompositeCurveTable;
				}
			}

			// recurse
			if (const UCompositeCurveTable* FoundLoop = CompositeCurveTable->FindLoops(AlreadySeenTables))
			{
				return FoundLoop;
			}
		}
	}

	// no loops
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
