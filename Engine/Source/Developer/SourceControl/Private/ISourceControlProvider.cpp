// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ISourceControlProvider.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"


#define LOCTEXT_NAMESPACE "ISourceControlProvider"


ECommandResult::Type ISourceControlProvider::Login(const FString& InPassword, EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate)
{
	TSharedRef<FConnect, ESPMode::ThreadSafe> ConnectOperation = ISourceControlOperation::Create<FConnect>();
	ConnectOperation->SetPassword(InPassword);
	return Execute(ConnectOperation, InConcurrency, InOperationCompleteDelegate);
}

ECommandResult::Type ISourceControlProvider::GetState(const TArray<UPackage*>& InPackages, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage)
{
	TArray<FString> Files = SourceControlHelpers::PackageFilenames(InPackages);
	return GetState(Files, OutState, InStateCacheUsage);
}

TSharedPtr<ISourceControlState, ESPMode::ThreadSafe> ISourceControlProvider::GetState(const UPackage* InPackage, EStateCacheUsage::Type InStateCacheUsage)
{
	return GetState(SourceControlHelpers::PackageFilename(InPackage), InStateCacheUsage);
}

TSharedPtr<ISourceControlState, ESPMode::ThreadSafe> ISourceControlProvider::GetState(const FString& InFile, EStateCacheUsage::Type InStateCacheUsage)
{
	TArray<FString> Files;
	TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> > States;
	Files.Add(InFile);
	if (GetState(Files, States, InStateCacheUsage) == ECommandResult::Succeeded)
	{
		TSharedRef<ISourceControlState, ESPMode::ThreadSafe> State = States[0];
		return State;
	}
	return NULL;
}

ECommandResult::Type ISourceControlProvider::Execute(const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate)
{
	return Execute(InOperation, TArray<FString>(), InConcurrency, InOperationCompleteDelegate);
}

ECommandResult::Type ISourceControlProvider::Execute(const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const UPackage* InPackage, const EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate)
{
	return Execute(InOperation, SourceControlHelpers::PackageFilename(InPackage), InConcurrency, InOperationCompleteDelegate);
}

ECommandResult::Type ISourceControlProvider::Execute(const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const FString& InFile, const EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate)
{
	TArray<FString> FileArray;
	FileArray.Add(InFile);
	return Execute(InOperation, FileArray, InConcurrency, InOperationCompleteDelegate);
}

ECommandResult::Type ISourceControlProvider::Execute(const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TArray<UPackage*>& InPackages, const EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate)
{
	TArray<FString> FileArray = SourceControlHelpers::PackageFilenames(InPackages);
	return Execute(InOperation, FileArray, InConcurrency, InOperationCompleteDelegate);
}

TSharedPtr<class ISourceControlLabel> ISourceControlProvider::GetLabel(const FString& InLabelName) const
{
	TArray< TSharedRef<class ISourceControlLabel> > Labels = GetLabels(InLabelName);
	if (Labels.Num() > 0)
	{
		return Labels[0];
	}

	return NULL;
}

#undef LOCTEXT_NAMESPACE
