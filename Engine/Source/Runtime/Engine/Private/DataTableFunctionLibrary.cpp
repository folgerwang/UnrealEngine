// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Kismet/DataTableFunctionLibrary.h"
#include "Engine/CurveTable.h"

#if WITH_EDITOR
#include "Misc/FileHelper.h"
#endif //WITH_EDITOR

UDataTableFunctionLibrary::UDataTableFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDataTableFunctionLibrary::EvaluateCurveTableRow(UCurveTable* CurveTable, FName RowName, float InXY, TEnumAsByte<EEvaluateCurveTableResult::Type>& OutResult, float& OutXY,const FString& ContextString)
{
	FCurveTableRowHandle Handle;
	Handle.CurveTable = CurveTable;
	Handle.RowName = RowName;
	
	bool found = Handle.Eval(InXY, &OutXY,ContextString);
	
	if (found)
	{
	    OutResult = EEvaluateCurveTableResult::RowFound;
	}
	else
	{
	    OutResult = EEvaluateCurveTableResult::RowNotFound;
	}
}

bool UDataTableFunctionLibrary::DoesDataTableRowExist(UDataTable* Table, FName RowName)
{
	if (!Table)
	{
		return false;
	}
	else if (Table->RowStruct == nullptr)
	{
		return false;
	}
	return Table->RowMap.Find(RowName) != nullptr;
}

TArray<FString> UDataTableFunctionLibrary::GetDataTableColumnAsString(const UDataTable* DataTable, FName PropertyName)
{
	if (DataTable && PropertyName != NAME_None)
	{
		EDataTableExportFlags ExportFlags = EDataTableExportFlags::None;
		return DataTableUtils::GetColumnDataAsString(DataTable, PropertyName, ExportFlags);
	}
	return TArray<FString>();
}

bool UDataTableFunctionLibrary::Generic_GetDataTableRowFromName(UDataTable* Table, FName RowName, void* OutRowPtr)
{
	bool bFoundRow = false;

	if (OutRowPtr && Table)
	{
		void* RowPtr = Table->FindRowUnchecked(RowName);

		if (RowPtr != NULL)
		{
			UScriptStruct* StructType = Table->RowStruct;

			if (StructType != NULL)
			{
				StructType->CopyScriptStruct(OutRowPtr, RowPtr);
				bFoundRow = true;
			}
		}
	}

	return bFoundRow;
}

bool UDataTableFunctionLibrary::GetDataTableRowFromName(UDataTable* Table, FName RowName, FTableRowBase& OutRow)
{
	// We should never hit this!  stubs to avoid NoExport on the class.
	check(0);
	return false;
}

void UDataTableFunctionLibrary::GetDataTableRowNames(UDataTable* Table, TArray<FName>& OutRowNames)
{
	if (Table)
	{
		OutRowNames = Table->GetRowNames();
	}
	else
	{
		OutRowNames.Empty();
	}
}


#if WITH_EDITOR
bool UDataTableFunctionLibrary::FillDataTableFromCSVString(UDataTable* DataTable, const FString& InString)
{
	if (!DataTable)
	{
		UE_LOG(LogDataTable, Error, TEXT("Can't fill an invalid DataTable."));
		return false;
	}

	bool bResult = true;
	if (InString.Len() == 0)
	{
		DataTable->EmptyTable();
	}
	else
	{
		TArray<FString> Errors = DataTable->CreateTableFromCSVString(InString);
		if (Errors.Num())
		{
			for (const FString& Error : Errors)
			{
				UE_LOG(LogDataTable, Warning, TEXT("%s"), *Error);
			}
		}
		bResult = Errors.Num() == 0;
	}
	return bResult;
}

bool UDataTableFunctionLibrary::FillDataTableFromCSVFile(UDataTable* DataTable, const FString& InFilePath)
{
	if (!DataTable)
	{
		UE_LOG(LogDataTable, Error, TEXT("Can't fill an invalid DataTable."));
		return false;
	}

	FString Data;
	if (!FFileHelper::LoadFileToString(Data, *InFilePath))
	{
		UE_LOG(LogDataTable, Error, TEXT("Can't load the file '%s'."), *InFilePath);
		return false;
	}

	return FillDataTableFromCSVString(DataTable, Data);
}

bool UDataTableFunctionLibrary::FillDataTableFromJSONString(UDataTable* DataTable, const FString& InString)
{
	if (!DataTable)
	{
		UE_LOG(LogDataTable, Error, TEXT("Can't fill an invalid DataTable."));
		return false;
	}

	bool bResult = true;
	if (InString.Len() == 0)
	{
		DataTable->EmptyTable();
	}
	else
	{
		TArray<FString> Errors = DataTable->CreateTableFromJSONString(InString);
		if (Errors.Num())
		{
			for (const FString& Error : Errors)
			{
				UE_LOG(LogDataTable, Warning, TEXT("%s"), *Error);
			}
		}
		bResult = Errors.Num() == 0;
	}
	return bResult;
}

bool UDataTableFunctionLibrary::FillDataTableFromJSONFile(UDataTable* DataTable, const FString& InFilePath)
{
	if (!DataTable)
	{
		UE_LOG(LogDataTable, Error, TEXT("Can't fill an invalid DataTable."));
		return false;
	}

	FString Data;
	if (!FFileHelper::LoadFileToString(Data, *InFilePath))
	{
		UE_LOG(LogDataTable, Error, TEXT("Can't load the file '%s'."), *InFilePath);
		return false;
	}

	return FillDataTableFromJSONString(DataTable, Data);
}
#endif //WITH_EDITOR