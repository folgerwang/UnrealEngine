// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Engine/CompositeCurveTable.h"
#include "Misc/MessageDialog.h"
#include "UObject/LinkerLoad.h"

#include "EditorFramework/AssetImportData.h"
#if WITH_EDITOR
#include "CurveTableEditorUtils.h"
#endif

#define LOCTEXT_NAMESPACE "CompositeCurveTables"

//////////////////////////////////////////////////////////////////////////
UCompositeCurveTable::UCompositeCurveTable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsLoading = false;
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

	bIsLoading = false;
}

void UCompositeCurveTable::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		bIsLoading = true;
	}

	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		for (UCurveTable* ParentTable : ParentTables)
		{
			if (ParentTable && ParentTable->HasAnyFlags(RF_NeedLoad))
			{
				FLinkerLoad* ParentTableLinker = ParentTable->GetLinker();
				if (ParentTableLinker)
				{
					ParentTableLinker->Preload(ParentTable);
				}
			}
		}

		OnParentTablesUpdated();
	}
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
		const FText ErrorMsg = FText::Format(LOCTEXT("FoundLoopError", "Cyclic dependency found. Table {0} depends on itself. Please fix your data"), FText::FromString(LoopTable->GetPathName()));
#if WITH_EDITOR
		if (!bIsLoading)
		{
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMsg);
		}
		else
#endif
		{
			UE_LOG(LogCurveTable, Warning, TEXT("%s"), *ErrorMsg.ToString());
		}
		bLeaveEmpty = true;
	}

	if (!bLeaveEmpty)
	{
		CurveTableMode = ECurveTableMode::SimpleCurves;

		// First determine if all our parent tables are simple
		for (const UCurveTable* ParentTable : ParentTables)
		{
			if (ParentTable && ParentTable->GetCurveTableMode() == ECurveTableMode::RichCurves)
			{
				CurveTableMode = ECurveTableMode::RichCurves;
				break;
			}
		}

		// iterate through all of the rows
		for (const UCurveTable* ParentTable : ParentTables)
		{
			if (ParentTable == nullptr)
			{
				continue;
			}

			// Add new rows or overwrite previous rows
			auto AddCurveToMap = [&RowMap=RowMap](FName CurveName, FRealCurve* NewCurve)
			{
				if (FRealCurve** Curve = RowMap.Find(CurveName))
				{
					delete *Curve;
					*Curve = NewCurve;
				}
				else
				{
					RowMap.Add(CurveName, NewCurve);
				}
			};

			// If we are using simple curves we know all our parents are also simple
			if (CurveTableMode == ECurveTableMode::SimpleCurves)
			{
				for (const TPair<FName, FSimpleCurve*>& CurveRow : ParentTable->GetSimpleCurveRowMap())
				{
					FSimpleCurve* NewCurve = new FSimpleCurve();
					FSimpleCurve* InCurve = CurveRow.Value;
					NewCurve->SetKeys(InCurve->GetConstRefOfKeys());
					NewCurve->SetKeyInterpMode(InCurve->GetKeyInterpMode());
					AddCurveToMap(CurveRow.Key, NewCurve);
				}
			}
			// If we are Rich but this Parent is Simple then we need to do a little more conversion work
			else if (ParentTable->GetCurveTableMode() == ECurveTableMode::SimpleCurves)
			{
				for (const TPair<FName, FSimpleCurve*>& CurveRow : ParentTable->GetSimpleCurveRowMap())
				{
					FRichCurve* NewCurve = new FRichCurve();
					FSimpleCurve* InCurve = CurveRow.Value;
					for (const FSimpleCurveKey& CurveKey : InCurve->GetConstRefOfKeys())
					{
						FKeyHandle KeyHandle = NewCurve->AddKey(CurveKey.Time, CurveKey.Value);
						NewCurve->SetKeyInterpMode(KeyHandle, InCurve->GetKeyInterpMode());
					}
					NewCurve->AutoSetTangents();
					AddCurveToMap(CurveRow.Key, NewCurve);
				}
			}
			// Otherwise Rich to Rich is also straightforward
			else
			{
				for (const TPair<FName, FRichCurve*>& CurveRow : ParentTable->GetRichCurveRowMap())
				{
					FRichCurve* NewCurve = new FRichCurve();
					FRichCurve* InCurve = CurveRow.Value;
					NewCurve->SetKeys(InCurve->GetConstRefOfKeys());
					AddCurveToMap(CurveRow.Key, NewCurve);
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
