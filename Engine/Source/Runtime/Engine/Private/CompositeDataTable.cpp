// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Engine/CompositeDataTable.h"
#include "Serialization/PropertyLocalizationDataGathering.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include "UObject/LinkerLoad.h"
#include "DataTableCSV.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "DataTableJSON.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/UserDefinedStruct.h"
#include "Misc/MessageDialog.h"
#if WITH_EDITOR
#include "DataTableEditorUtils.h"
#endif

#define LOCTEXT_NAMESPACE "CompositeDataTables"

UCompositeDataTable::UCompositeDataTable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsLoading = false;
}

void UCompositeDataTable::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);
	for (UDataTable* Parent : ParentTables)
	{
		if (Parent != nullptr)
		{
			OutDeps.Add(Parent);
		}
	}
}

void UCompositeDataTable::PostLoad()
{
	bIsLoading = false;

	Super::PostLoad();

	OnParentTablesUpdated();
}

#if WITH_EDITORONLY_DATA
UCompositeDataTable::ERowState UCompositeDataTable::GetRowState(FName RowName) const
{
	const ERowState* RowState = RowSourceMap.Find(RowName);

	return RowState != nullptr ? *RowState : ERowState::Invalid;
}
#endif

void UCompositeDataTable::UpdateCachedRowMap()
{
	bool bLeaveEmpty = false;
	// Throw up an error message and stop if any loops are found
	if (const UCompositeDataTable* LoopTable = FindLoops(TArray<const UCompositeDataTable*>()))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("FoundLoopError", "Cyclic dependency found. Table {0} depends on itself. Please fix your data"), FText::FromString(LoopTable->GetPathName())));
		bLeaveEmpty = true;

		// if the rowmap is empty, stop. We don't need to do the pre and post change since no changes will actually be done
		if (RowMap.Num() == 0)
		{
			return;
		}
	}

#if WITH_EDITOR
	FDataTableEditorUtils::BroadcastPreChange(this, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
#endif
	UDataTable::EmptyTable();

	if (!bLeaveEmpty)
	{
		bool bParentsHaveDifferentRowStruct = false;
		// iterate through all of the rows
		for (const UDataTable* ParentTable : ParentTables)
		{
			if (ParentTable == nullptr)
			{
				continue;
			}
			if (ParentTable->RowStruct != RowStruct)
			{
				bParentsHaveDifferentRowStruct = true;
				FString CompositeRowStructName = RowStruct ? RowStruct->GetName() : "Missing row struct";
				FString ParentRowStructName = ParentTable->RowStruct ? ParentTable->RowStruct->GetName() : "Missing row struct";
				UE_LOG(LogDataTable, Error, TEXT("Composite tables must have the same row struct as their parent tables. Composite Table: %s, Composite Row Struct: %s, Parent Table: %s, Parent Row Struct: %s."), *GetName(), *CompositeRowStructName, *ParentTable->GetName(), *ParentRowStructName);
				continue;
			}

			// Add new rows or overwrite previous rows
			for (TMap<FName, uint8*>::TConstIterator RowMapIter(ParentTable->GetRowMap().CreateConstIterator()); RowMapIter; ++RowMapIter)
			{
				if (ensure(RowMapIter.Value() != nullptr))
				{
					// UDataTable::AddRow will first remove the row if it already exists so we don't need to do anything special here
					FTableRowBase* TableRowBase = (FTableRowBase*)RowMapIter.Value();
					UDataTable::AddRow(RowMapIter.Key(), *TableRowBase);
				}
			}
		}

		if (bParentsHaveDifferentRowStruct)
		{
			// warn in editor
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ParentsIncludesOtherRowStructError", "Parent tables must have the same row struct as this table. Please fix your data. See log for details."));
		}
	}
#if WITH_EDITOR
	FDataTableEditorUtils::BroadcastPostChange(this, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
#endif
}

const UCompositeDataTable* UCompositeDataTable::FindLoops(TArray<const UCompositeDataTable*> AlreadySeenTables) const
{
	AlreadySeenTables.Add(this);

	for (const UDataTable* DataTable : ParentTables)
	{
		// we only care about composite tables since regular tables terminate the chain and can't be in loops
		if (const UCompositeDataTable* CompositeDataTable = Cast<UCompositeDataTable>(DataTable))
		{
			// if we've seen this table before then we have a loop
			for (const UCompositeDataTable* SeenTable : AlreadySeenTables)
			{
				if (SeenTable == CompositeDataTable)
				{
					return CompositeDataTable;
				}
			}

			// recurse
			if (const UCompositeDataTable* FoundLoop = CompositeDataTable->FindLoops(AlreadySeenTables))
			{
				return FoundLoop;
			}
		}
	}

	// no loops
	return nullptr;
}

void UCompositeDataTable::EmptyTable()
{
	if (!bIsLoading)
	{
		ParentTables.Empty();
	}
#if WITH_EDITORONLY_DATA
	RowSourceMap.Empty();
#endif
	Super::EmptyTable();
}

void UCompositeDataTable::RemoveRow(FName RowName)
{
	// do nothing
}

void UCompositeDataTable::AddRow(FName RowName, const FTableRowBase& RowData)
{
	// do nothing
}

void UCompositeDataTable::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		bIsLoading = true;
	}

	Super::Serialize(Ar); // When loading, this should load our RowStruct!	
}

#if WITH_EDITOR
void UCompositeDataTable::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static FName Name_ParentTables = GET_MEMBER_NAME_CHECKED(UCompositeDataTable, ParentTables);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	const FName PropertyName = PropertyThatChanged != nullptr ? PropertyThatChanged->GetFName() : NAME_None;

	if (PropertyName == Name_ParentTables)
	{
		OnParentTablesUpdated();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UCompositeDataTable::OnParentTablesUpdated()
{
	for (UDataTable* Table : OldParentTables)
	{
		if (Table && ParentTables.Find(Table) == INDEX_NONE)
		{
			Table->OnDataTableChanged().RemoveAll(this);
		}
	}

	UpdateCachedRowMap();

	for (UDataTable* Table : ParentTables)
	{
		if (Table && OldParentTables.Find(Table) == INDEX_NONE)
		{
			Table->OnDataTableChanged().AddUObject(this, &UCompositeDataTable::UpdateCachedRowMap);
		}
	}

	OldParentTables = ParentTables;
}

#undef LOCTEXT_NAMESPACE
