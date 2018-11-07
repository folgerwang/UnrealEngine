// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DataValidationManager.h"

#include "Modules/ModuleManager.h"
#include "Developer/MessageLog/Public/MessageLogModule.h"
#include "Logging/MessageLog.h"
#include "Misc/ScopedSlowTask.h"
#include "AssetRegistryModule.h"
#include "Editor.h"

#include "CoreGlobals.h"

#define LOCTEXT_NAMESPACE "DataValidationManager"

UDataValidationManager* GDataValidationManager = nullptr;

/**
 * UDataValidationManager
 */

UDataValidationManager::UDataValidationManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DataValidationManagerClassName = FSoftClassPath(TEXT("/Script/DataValidation.DataValidationManager"));
	bValidateOnSave = true;
}

UDataValidationManager* UDataValidationManager::Get()
{
	if (GDataValidationManager == nullptr)
	{
		FSoftClassPath DataValidationManagerClassName = (UDataValidationManager::StaticClass()->GetDefaultObject<UDataValidationManager>())->DataValidationManagerClassName;

		UClass* SingletonClass = DataValidationManagerClassName.TryLoadClass<UObject>();
		checkf(SingletonClass != nullptr, TEXT("Data validation config value DataValidationManagerClassName is not a valid class name."));

		GDataValidationManager = NewObject<UDataValidationManager>(GetTransientPackage(), SingletonClass, NAME_None);
		checkf(GDataValidationManager != nullptr, TEXT("Data validation config value DataValidationManagerClassName is not a subclass of UDataValidationManager."))

		GDataValidationManager->AddToRoot();
		GDataValidationManager->Initialize();
	}

	return GDataValidationManager;
}

void UDataValidationManager::Initialize()
{
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowFilters = true;

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing("DataValidation", LOCTEXT("DataValidation", "Data Validation"), InitOptions);
}

UDataValidationManager::~UDataValidationManager()
{
}

EDataValidationResult UDataValidationManager::IsObjectValid(UObject* InObject, TArray<FText>& ValidationErrors) const
{
	if (ensure(InObject))
	{
		return InObject->IsDataValid(ValidationErrors);
	}

	return EDataValidationResult::NotValidated;
}

EDataValidationResult UDataValidationManager::IsAssetValid(FAssetData& AssetData, TArray<FText>& ValidationErrors) const
{
	if (AssetData.IsValid())
	{
		UObject* Obj = AssetData.GetAsset();
		if (Obj)
		{
			return IsObjectValid(Obj, ValidationErrors);
		}
	}

	return EDataValidationResult::Invalid;
}

int32 UDataValidationManager::ValidateAssets(TArray<FAssetData> AssetDataList, bool bSkipExcludedDirectories, bool bShowIfNoFailures) const
{
	FScopedSlowTask SlowTask(1.0f, LOCTEXT("ValidatingDataTask", "Validating Data..."));
	SlowTask.Visibility = bShowIfNoFailures ? ESlowTaskVisibility::ForceVisible : ESlowTaskVisibility::Invisible;
	if (bShowIfNoFailures)
	{
		SlowTask.MakeDialogDelayed(.1f);
	}

	FMessageLog DataValidationLog("DataValidation");

	int32 NumAdded = 0;

	int32 NumFilesChecked = 0;
	int32 NumValidFiles = 0;
	int32 NumInvalidFiles = 0;
	int32 NumFilesSkipped = 0;
	int32 NumFilesUnableToValidate = 0;

	int32 NumFilesToValidate = AssetDataList.Num();

	// Now add to map or update as needed
	for (FAssetData& Data : AssetDataList)
	{
		SlowTask.EnterProgressFrame(1.0f / NumFilesToValidate, FText::Format(LOCTEXT("ValidatingFilename", "Validating {0}"), FText::FromString(Data.GetFullName())));

		// Check exclusion path
		if (bSkipExcludedDirectories && IsPathExcludedFromValidation(Data.PackageName.ToString()))
		{
			++NumFilesSkipped;
			continue;
		}

		TArray<FText> ValidationErrors;
		EDataValidationResult Result = IsAssetValid(Data, ValidationErrors);
		++NumFilesChecked;

		for (const FText& ErrorMsg : ValidationErrors)
		{
			DataValidationLog.Error()->AddToken(FTextToken::Create(ErrorMsg));
		}

		if (Result == EDataValidationResult::Valid)
		{
			++NumValidFiles;
		}
		else
		{
			if (Result == EDataValidationResult::Invalid)
			{
				DataValidationLog.Error()->AddToken(FAssetNameToken::Create(Data.PackageName.ToString()))
					->AddToken(FTextToken::Create(LOCTEXT("InvalidDataResult", "contains invalid data.")));
				++NumInvalidFiles;
			}
			else if (Result == EDataValidationResult::NotValidated)
			{
				if (bShowIfNoFailures)
				{
					DataValidationLog.Info()->AddToken(FAssetNameToken::Create(Data.PackageName.ToString()))
						->AddToken(FTextToken::Create(LOCTEXT("NotValidatedDataResult", "has no data data validation.")));
				}
				++NumFilesUnableToValidate;
			}
		}
	}

	const bool bFailed = (NumInvalidFiles > 0);

	if (bFailed || bShowIfNoFailures)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Result"), bFailed ? LOCTEXT("Failed", "FAILED") : LOCTEXT("Succeeded", "SUCCEEDED"));
		Arguments.Add(TEXT("NumChecked"), NumFilesChecked);
		Arguments.Add(TEXT("NumValid"), NumValidFiles);
		Arguments.Add(TEXT("NumInvalid"), NumInvalidFiles);
		Arguments.Add(TEXT("NumSkipped"), NumFilesSkipped);
		Arguments.Add(TEXT("NumUnableToValidate"), NumFilesUnableToValidate);

		TSharedRef<FTokenizedMessage> ValidationLog = bFailed ? DataValidationLog.Error() : DataValidationLog.Info();
		ValidationLog->AddToken(FTextToken::Create(FText::Format(LOCTEXT("SuccessOrFailure", "Data validation {Result}."), Arguments)));
		ValidationLog->AddToken(FTextToken::Create(FText::Format(LOCTEXT("ResultsSummary", "Files Checked: {NumChecked}, Passed: {NumValid}, Failed: {NumInvalid}, Skipped: {NumSkipped}, Unable to validate: {NumUnableToValidate}"), Arguments)));

		DataValidationLog.Open(EMessageSeverity::Info, true);
	}

	return NumInvalidFiles;
}

void UDataValidationManager::ValidateOnSave(TArray<FAssetData> AssetDataList) const
{
	// Only validate if enabled and not auto saving
	if (!bValidateOnSave || GEditor->IsAutosaving())
	{
		return;
	}

	FMessageLog DataValidationLog("DataValidation");
	if (ValidateAssets(AssetDataList, true, false) > 0)
	{
		const FText ErrorMessageNotification = FText::Format(
			LOCTEXT("ValidationFailureNotification", "Validation failed when saving {0}, check Data Validation log"),
			AssetDataList.Num() == 1 ? FText::FromName(AssetDataList[0].AssetName) : LOCTEXT("MultipleErrors", "multiple assets"));
		DataValidationLog.Notify(ErrorMessageNotification, EMessageSeverity::Warning, /*bForce=*/ true);
	}
}

void UDataValidationManager::ValidateSavedPackage(FName PackageName)
{
	// Only validate if enabled and not auto saving
	if (!bValidateOnSave || GEditor->IsAutosaving())
	{
		return;
	}

	SavedPackagesToValidate.AddUnique(PackageName);

	GEditor->GetTimerManager()->SetTimerForNextTick(this, &UDataValidationManager::ValidateAllSavedPackages);
}

bool UDataValidationManager::IsPathExcludedFromValidation(const FString& Path) const
{
	for (const FDirectoryPath& ExcludedPath : ExcludedDirectories)
	{
		if (Path.Contains(ExcludedPath.Path))
		{
			return true;
		}
	}

	return false;
}

void UDataValidationManager::ValidateAllSavedPackages()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> Assets;

	for (FName PackageName : SavedPackagesToValidate)
	{
		// We need to query the in-memory data as the disk cache may not be accurate
		AssetRegistryModule.Get().GetAssetsByPackageName(PackageName, Assets);
	}

	ValidateOnSave(Assets);

	SavedPackagesToValidate.Empty();
}

#undef LOCTEXT_NAMESPACE
