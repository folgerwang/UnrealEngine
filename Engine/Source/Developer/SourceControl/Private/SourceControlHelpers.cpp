// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SourceControlHelpers.h"
#include "ISourceControlState.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "ISourceControlLabel.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "SourceControlHelpers"


namespace SourceControlHelpersInternal
{

/*
 * Status info set by LogError() and USourceControlHelpers methods if an error occurs
 * regardless whether their bSilent is set or not.
 * Should be empty if there is was no error.
 * @see	USourceControlHelpers::LastErrorMsg(), LogError()
 */
FText LastErrorText;

/* Store error and write to Log if bSilent is false. */
inline void LogError(const FText& ErrorText, bool bSilent)
{
	LastErrorText = ErrorText;

	if (!bSilent)
	{
		FMessageLog("SourceControl").Error(LastErrorText);
	}
}

/* Return provider if ready to go, else return nullptr. */
ISourceControlProvider* VerifySourceControl(bool bSilent)
{
	ISourceControlModule& SCModule = ISourceControlModule::Get();

	if (!SCModule.IsEnabled())
	{
		LogError(LOCTEXT("SourceControlDisabled", "Source control is not enabled."), bSilent);

		return nullptr;
	}

	ISourceControlProvider* Provider = &SCModule.GetProvider();

	if (!Provider->IsAvailable())
	{
		LogError(LOCTEXT("SourceControlServerUnavailable", "Source control server is currently not available."), bSilent);

		return nullptr;
	}

	// Clear the last error text if there hasn't been an error (yet).
	LastErrorText = FText::GetEmpty();

	return Provider;
}


/*
 * Converts specified file to fully qualified file path that is compatible with source control.
 *
 * @param	InFile		File string - can be either fully qualified path, relative path, long package name, asset path or export text path (often stored on clipboard)
 * @param	bSilent		if false then write out any error info to the Log. Any error text can be retrieved by LastErrorMsg() regardless.
 * @return	Fully qualified file path to use with source control or "" if conversion unsuccessful.
 */
FString ConvertFileToQualifiedPath(const FString& InFile, bool bSilent, const FString& AssociatedExtension = FString())
{
	// Converted to qualified file path
	FString SCFile;

	if (InFile.IsEmpty())
	{
		LogError(LOCTEXT("UnspecifiedFile", "File not specified"), bSilent);

		return SCFile;
	}

	// Try to determine if file is one of:
	// - fully qualified path
	// - relative path
	// - long package name
	// - asset path
	// - export text path (often stored on clipboard)
	//
	// For example:
	// - D:\Epic\Dev-Ent\Projects\Python3rdBP\Content\Mannequin\Animations\ThirdPersonIdle.uasset
	// - Content\Mannequin\Animations\ThirdPersonIdle.uasset
	// - /Game/Mannequin/Animations/ThirdPersonIdle
	// - /Game/Mannequin/Animations/ThirdPersonIdle.ThirdPersonIdle
	// - AnimSequence'/Game/Mannequin/Animations/ThirdPersonIdle.ThirdPersonIdle'

	SCFile = InFile;
	bool bPackage = false;


	// Is ExportTextPath (often stored in Clipboard) form?
	//  - i.e. AnimSequence'/Game/Mannequin/Animations/ThirdPersonIdle.ThirdPersonIdle'
	if (SCFile[SCFile.Len() - 1] == '\'')
	{
		SCFile = FPackageName::ExportTextPathToObjectPath(SCFile);
	}

	if (SCFile[0] == '/')
	{
		// Assume it is a package
		bPackage = true;

		// Try to get filename by finding it on disk
		if (!FPackageName::DoesPackageExist(SCFile, nullptr, &SCFile))
		{
			// The package does not exist on disk, see if we can find it in memory and predict the file extension
			// Only do this if the supplied package name is valid
			const bool bIncludeReadOnlyRoots = false;
			bPackage = FPackageName::IsValidLongPackageName(SCFile, bIncludeReadOnlyRoots);

			if (bPackage)
			{
				const FString* PackageExtension = &FPackageName::GetAssetPackageExtension();

				if (AssociatedExtension.IsEmpty())
				{
					UPackage* Package = FindPackage(nullptr, *SCFile);

					if (Package)
					{
						// This is a package in memory that has not yet been saved. Determine the extension and convert to a filename
						PackageExtension = Package->ContainsMap() ? &FPackageName::GetMapPackageExtension() : &FPackageName::GetAssetPackageExtension();
					}
				}
				else
				{
					PackageExtension = &AssociatedExtension;
				}

				bPackage = FPackageName::TryConvertLongPackageNameToFilename(SCFile, SCFile, *PackageExtension);
			}
		}

		if (bPackage)
		{
			SCFile = FPaths::ConvertRelativePathToFull(SCFile);

			return SCFile;
		}
	}

	// Assume it is a qualified or relative file path

	// Could normalize it
	//FPaths::NormalizeFilename(SCFile);

	if (!FPaths::IsRelative(SCFile))
	{
		return SCFile;
	}

	// Qualify based on process base directory.
	// Something akin to "C:/Epic/UE4/Engine/Binaries/Win64/" as a current path.
	SCFile = FPaths::ConvertRelativePathToFull(InFile);

	if (FPaths::FileExists(SCFile))
	{
		return SCFile;
	}

	// Qualify based on project directory.
	SCFile = FPaths::ConvertRelativePathToFull(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), InFile);

	if (FPaths::FileExists(SCFile))
	{
		return SCFile;
	}

	// Qualify based on Engine directory
	SCFile = FPaths::ConvertRelativePathToFull(FPaths::ConvertRelativePathToFull(FPaths::EngineDir()), InFile);

	return SCFile;
}


/**
 * Converts specified files to fully qualified file paths that are compatible with source control.
 *
 * @param	InFiles			File strings - can be either fully qualified path, relative path, long package name, asset name or export text path (often stored on clipboard)
 * @param	OutFilePaths	Fully qualified file paths to use with source control or "" if conversion unsuccessful.
 * @param	bSilent			if false then write out any error info to the Log. Any error text can be retrieved by LastErrorMsg() regardless.
 * @return	true if all files successfully converted, false if any had errors
 */
bool ConvertFilesToQualifiedPaths(const TArray<FString>& InFiles, TArray<FString>& OutFilePaths, bool bSilent)
{
	uint32 SkipNum = 0u;

	for (const FString& File : InFiles)
	{
		FString SCFile = ConvertFileToQualifiedPath(File, bSilent);

		if (SCFile.IsEmpty())
		{
			SkipNum++;
		}
		else
		{
			OutFilePaths.Add(MoveTemp(SCFile));
		}
	}

	if (SkipNum)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("SkipNum"), FText::AsNumber(SkipNum));
		LogError(FText::Format(LOCTEXT("FilesSkipped", "During conversion to qualified file paths, {SkipNum} files were skipped!"), Arguments), bSilent);

		return false;
	}

	return true;
}

}  // namespace SourceControlHelpersInternal


FString USourceControlHelpers::CurrentProvider()
{
	// Note that if there is no provider there is still a dummy default provider object
	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();

	return Provider.GetName().ToString();
}


bool USourceControlHelpers::IsEnabled()
{
	return ISourceControlModule::Get().IsEnabled();
}


bool USourceControlHelpers::IsAvailable()
{
	ISourceControlModule& SCModule = ISourceControlModule::Get();

	return SCModule.IsEnabled() && SCModule.GetProvider().IsAvailable();
}


FText USourceControlHelpers::LastErrorMsg()
{
	return SourceControlHelpersInternal::LastErrorText;
}


bool USourceControlHelpers::CheckOutFile(const FString& InFile, bool bSilent)
{
	// Determine file type and ensure it is in form source control wants
	FString SCFile = SourceControlHelpersInternal::ConvertFileToQualifiedPath(InFile, bSilent);

	if (SCFile.IsEmpty())
	{
		return false;
	}

	// Ensure source control system is up and running
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		return false;
	}

	FSourceControlStatePtr SCState = Provider->GetState(SCFile, EStateCacheUsage::ForceUpdate);

	if (!SCState.IsValid())
	{
		// Improper or invalid SCC state
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("InFile"), FText::FromString(InFile));
		Arguments.Add(TEXT("SCFile"), FText::FromString(SCFile));
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("CouldNotDetermineState", "Could not determine source control state of file '{InFile}' ({SCFile})."), Arguments), bSilent);

		return false;
	}

	if (SCState->IsCheckedOut() || SCState->IsAdded())
	{
		// Already checked out or opened for add
		return true;
	}

	bool bCheckOutFail = false;

	if (SCState->CanCheckout())
	{
		if (Provider->Execute(ISourceControlOperation::Create<FCheckOut>(), SCFile) == ECommandResult::Succeeded)
		{
			return true;
		}

		bCheckOutFail = true;
	}

	// Only error info after this point

	FString SimultaneousCheckoutUser;
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("InFile"), FText::FromString(InFile));
	Arguments.Add(TEXT("SCFile"), FText::FromString(SCFile));

	if (bCheckOutFail)
	{
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("CheckoutFailed", "Failed to check out file '{InFile}' ({SCFile})."), Arguments), bSilent);
	}
	else if (!SCState->IsSourceControlled())
	{
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("NotSourceControlled", "Could not check out the file '{InFile}' because it is not under source control ({SCFile})."), Arguments), bSilent);
	}
	else if (!SCState->IsCurrent())
	{
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("NotAtHeadRevision", "File '{InFile}' is not at head revision ({SCFile})."), Arguments), bSilent);
	}
	else if (SCState->IsCheckedOutOther(&(SimultaneousCheckoutUser)))
	{
		Arguments.Add(TEXT("SimultaneousCheckoutUser"), FText::FromString(SimultaneousCheckoutUser));
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("SimultaneousCheckout", "File '{InFile}' is checked out by another ({SimultaneousCheckoutUser}) ({SCFile})."), Arguments), bSilent);
	}
	else
	{
		// Improper or invalid SCC state
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("CouldNotDetermineState", "Could not determine source control state of file '{InFile}' ({SCFile})."), Arguments), bSilent);
	}

	return false;
}


bool USourceControlHelpers::CheckOutFiles(const TArray<FString>& InFiles, bool bSilent)
{
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		return false;
	}

	TArray<FString> FilePaths;

	// Even if some files were skipped, still apply to the others
	bool bFilesSkipped = !SourceControlHelpersInternal::ConvertFilesToQualifiedPaths(InFiles, FilePaths, bSilent);

	// Less error checking and info is made for multiple files than the single file version.
	// This multi-file version could be made similarly more sophisticated.
	ECommandResult::Type Result = Provider->Execute(ISourceControlOperation::Create<FCheckOut>(), FilePaths);

	return !bFilesSkipped && (Result == ECommandResult::Succeeded);
}


bool USourceControlHelpers::CheckOutOrAddFile(const FString& InFile, bool bSilent)
{
	// Determine file type and ensure it is in form source control wants
	FString SCFile = SourceControlHelpersInternal::ConvertFileToQualifiedPath(InFile, bSilent);

	if (SCFile.IsEmpty())
	{
		return false;
	}

	// Ensure source control system is up and running
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		return false;
	}

	FSourceControlStatePtr SCState = Provider->GetState(SCFile, EStateCacheUsage::ForceUpdate);

	if (!SCState.IsValid())
	{
		// Improper or invalid SCC state
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("InFile"), FText::FromString(InFile));
		Arguments.Add(TEXT("SCFile"), FText::FromString(SCFile));
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("CouldNotDetermineState", "Could not determine source control state of file '{InFile}' ({SCFile})."), Arguments), bSilent);

		return false;
	}

	if (SCState->IsCheckedOut() || SCState->IsAdded())
	{
		// Already checked out or opened for add
		return true;
	}

	// Stuff single file in array for functions that require array
	TArray<FString> FilesToBeCheckedOut;
	FilesToBeCheckedOut.Add(SCFile);

	if (SCState->CanCheckout())
	{
		if (Provider->Execute(ISourceControlOperation::Create<FCheckOut>(), FilesToBeCheckedOut) != ECommandResult::Succeeded)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("InFile"), FText::FromString(InFile));
			Arguments.Add(TEXT("SCFile"), FText::FromString(SCFile));
			SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("CheckoutFailed", "Failed to check out file '{InFile}' ({SCFile})."), Arguments), bSilent);

			return false;
		}

		return true;
	}

	bool bAddFail = false;

	if (!SCState->IsSourceControlled())
	{
		if (Provider->Execute(ISourceControlOperation::Create<FMarkForAdd>(), FilesToBeCheckedOut) == ECommandResult::Succeeded)
		{
			return true;
		}

		bAddFail = true;;
	}

	FString SimultaneousCheckoutUser;
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("InFile"), FText::FromString(InFile));
	Arguments.Add(TEXT("SCFile"), FText::FromString(SCFile));

	if (bAddFail)
	{
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("AddFailed", "Failed to add file '{InFile}' to source control ({SCFile})."), Arguments), bSilent);
	}
	else if (!SCState->IsCurrent())
	{
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("NotAtHeadRevision", "File '{InFile}' is not at head revision ({SCFile})."), Arguments), bSilent);
	}
	else if (SCState->IsCheckedOutOther(&(SimultaneousCheckoutUser)))
	{
		Arguments.Add(TEXT("SimultaneousCheckoutUser"), FText::FromString(SimultaneousCheckoutUser));
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("SimultaneousCheckout", "File '{InFile}' is checked out by another ({SimultaneousCheckoutUser}) ({SCFile})."), Arguments), bSilent);
	}
	else
	{
		// Improper or invalid SCC state
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("CouldNotDetermineState", "Could not determine source control state of file '{InFile}' ({SCFile})."), Arguments), bSilent);
	}

	return false;
}


bool USourceControlHelpers::MarkFileForAdd(const FString& InFile, bool bSilent)
{
	// Determine file type and ensure it is in form source control wants
	FString SCFile = SourceControlHelpersInternal::ConvertFileToQualifiedPath(InFile, bSilent);

	if (SCFile.IsEmpty())
	{
		return false;
	}

	// Ensure source control system is up and running
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		return false;
	}

	// Mark for add now if needed
	FSourceControlStatePtr SCState = Provider->GetState(SCFile, EStateCacheUsage::Use);

	if (!SCState.IsValid())
	{
		// Improper or invalid SCC state
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("InFile"), FText::FromString(InFile));
		Arguments.Add(TEXT("SCFile"), FText::FromString(SCFile));
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("CouldNotDetermineState", "Could not determine source control state of file '{InFile}' ({SCFile})."), Arguments), bSilent);

		return false;
	}

	// Add if necessary
	if (SCState->IsUnknown() || (!SCState->IsSourceControlled() && !SCState->IsAdded()))
	{
		if (Provider->Execute(ISourceControlOperation::Create<FMarkForAdd>(), SCFile) != ECommandResult::Succeeded)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("InFile"), FText::FromString(InFile));
			Arguments.Add(TEXT("SCFile"), FText::FromString(SCFile));
			SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("MarkForAddFailed", "Failed to add file '{InFile}' to source control ({SCFile})."), Arguments), bSilent);

			return false;
		}
	}

	return true;
}


bool USourceControlHelpers::MarkFilesForAdd(const TArray<FString>& InFiles, bool bSilent)
{
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		return false;
	}

	TArray<FString> FilePaths;

	// Even if some files were skipped, still apply to the others
	bool bFilesSkipped = !SourceControlHelpersInternal::ConvertFilesToQualifiedPaths(InFiles, FilePaths, bSilent);

	// Less error checking and info is made for multiple files than the single file version.
	// This multi-file version could be made similarly more sophisticated.
	ECommandResult::Type Result = Provider->Execute(ISourceControlOperation::Create<FMarkForAdd>(), FilePaths);

	return !bFilesSkipped && (Result == ECommandResult::Succeeded);
}


bool USourceControlHelpers::MarkFileForDelete(const FString& InFile, bool bSilent)
{
	// Determine file type and ensure it is in form source control wants
	FString SCFile = SourceControlHelpersInternal::ConvertFileToQualifiedPath(InFile, bSilent);

	if (SCFile.IsEmpty())
	{
		return false;
	}

	// Ensure source control system is up and running
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		// Error or can't communicate with source control
		// Could erase it anyway, though keeping it for now.
		return false;
	}

	FSourceControlStatePtr SCState = Provider->GetState(SCFile, EStateCacheUsage::ForceUpdate);

	if (!SCState.IsValid())
	{
		// Improper or invalid SCC state
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("InFile"), FText::FromString(InFile));
		Arguments.Add(TEXT("SCFile"), FText::FromString(SCFile));
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("CouldNotDetermineState", "Could not determine source control state of file '{InFile}' ({SCFile})."), Arguments), bSilent);

		return false;
	}

	bool bDelete = false;

	if (SCState->IsSourceControlled())
	{
		bool bAdded = SCState->IsAdded();

		if (bAdded || SCState->IsCheckedOut())
		{
			if (Provider->Execute(ISourceControlOperation::Create<FRevert>(), SCFile) != ECommandResult::Succeeded)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("InFile"), FText::FromString(InFile));
				Arguments.Add(TEXT("SCFile"), FText::FromString(SCFile));
				SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("CouldNotRevert", "Could not revert source control state of file '{InFile}' ({SCFile})."), Arguments), bSilent);

				return false;
			}
		}

		if (!bAdded)
		{
			// Was previously added to source control so mark it for delete
			if (Provider->Execute(ISourceControlOperation::Create<FDelete>(), SCFile) != ECommandResult::Succeeded)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("InFile"), FText::FromString(InFile));
				Arguments.Add(TEXT("SCFile"), FText::FromString(SCFile));
				SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("CouldNotDelete", "Could not delete file '{InFile}' from source control ({SCFile})."), Arguments), bSilent);

				return false;
			}
		}
	}

	// Delete file if it still exists
	IFileManager& FileManager = IFileManager::Get();

	if (FileManager.FileExists(*SCFile))
	{
		// Just a regular file not tracked by source control so erase it.
		// Don't bother checking if it exists since Delete doesn't care.
		return FileManager.Delete(*SCFile, false, true);
	}

	return false;
}


bool USourceControlHelpers::RevertFile(const FString& InFile, bool bSilent)
{
	// Determine file type and ensure it is in form source control wants
	FString SCFile = SourceControlHelpersInternal::ConvertFileToQualifiedPath(InFile, bSilent);

	if (SCFile.IsEmpty())
	{
		return false;
	}

	// Ensure source control system is up and running
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		return false;
	}

	// Revert file regardless of whether it has had any changes made
	ECommandResult::Type Result = Provider->Execute(ISourceControlOperation::Create<FRevert>(), SCFile);

	return Result == ECommandResult::Succeeded;
}


bool USourceControlHelpers::RevertFiles(const TArray<FString>& InFiles,	bool bSilent)
{
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		return false;
	}

	// Less error checking and info is made for multiple files than the single file version.
	// This multi-file version could be made similarly more sophisticated.

	// Revert files regardless of whether they've had any changes made
	ECommandResult::Type Result = Provider->Execute(ISourceControlOperation::Create<FRevert>(), InFiles);

	return Result == ECommandResult::Succeeded;
}


bool USourceControlHelpers::RevertUnchangedFile(const FString& InFile, bool bSilent)
{
	// Determine file type and ensure it is in form source control wants
	FString SCFile = SourceControlHelpersInternal::ConvertFileToQualifiedPath(InFile, bSilent);

	if (SCFile.IsEmpty())
	{
		return false;
	}

	// Ensure source control system is up and running
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		return false;
	}

	// Only revert file if they haven't had any changes made

	// Stuff single file in array for functions that require array
	TArray<FString> InFiles;
	InFiles.Add(SCFile);

	RevertUnchangedFiles(*Provider, InFiles);

	// Assume it succeeded
	return true;
}


bool USourceControlHelpers::RevertUnchangedFiles(const TArray<FString>& InFiles, bool bSilent)
{
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		return false;
	}

	// Less error checking and info is made for multiple files than the single file version.
	// This multi-file version could be made similarly more sophisticated.

	// Only revert files if they haven't had any changes made
	RevertUnchangedFiles(*Provider, InFiles);

	// Assume it succeeded
	return true;
}


bool USourceControlHelpers::CheckInFile(const FString& InFile, const FString& InDescription, bool bSilent)
{
	// Determine file type and ensure it is in form source control wants
	FString SCFile = SourceControlHelpersInternal::ConvertFileToQualifiedPath(InFile, bSilent);

	if (SCFile.IsEmpty())
	{
		return false;
	}

	// Ensure source control system is up and running
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		return false;
	}

	TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOp = ISourceControlOperation::Create<FCheckIn>();
	CheckInOp->SetDescription(FText::FromString(InDescription));

	ECommandResult::Type Result = Provider->Execute(CheckInOp, SCFile);

	return Result == ECommandResult::Succeeded;
}


bool USourceControlHelpers::CheckInFiles(const TArray<FString>& InFiles, const FString& InDescription, bool bSilent)
{
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		return false;
	}

	TArray<FString> FilePaths;

	// Even if some files were skipped, still apply to the others
	bool bFilesSkipped = !SourceControlHelpersInternal::ConvertFilesToQualifiedPaths(InFiles, FilePaths, bSilent);

	// Less error checking and info is made for multiple files than the single file version.
	// This multi-file version could be made similarly more sophisticated.
	TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOp = ISourceControlOperation::Create<FCheckIn>();
	CheckInOp->SetDescription(FText::FromString(InDescription));

	ECommandResult::Type Result = Provider->Execute(CheckInOp, FilePaths);

	return !bFilesSkipped && (Result == ECommandResult::Succeeded);
}


bool USourceControlHelpers::CopyFile(const FString& InSourcePath, const FString& InDestPath, bool bSilent)
{
	// Determine file type and ensure it is in form source control wants
	FString SCSource = SourceControlHelpersInternal::ConvertFileToQualifiedPath(InSourcePath, bSilent);

	if (SCSource.IsEmpty())
	{
		return false;
	}

	// Determine file type and ensure it is in form source control wants
	FString SCSourcExt(FPaths::GetExtension(SCSource, true));
	FString SCDest(SourceControlHelpersInternal::ConvertFileToQualifiedPath(InDestPath, bSilent, SCSourcExt));

	if (SCDest.IsEmpty())
	{
		return false;
	}

	// Ensure source control system is up and running
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		return false;
	}

	TSharedRef<FCopy, ESPMode::ThreadSafe> CopyOp = ISourceControlOperation::Create<FCopy>();
	CopyOp->SetDestination(SCDest);

	ECommandResult::Type Result = Provider->Execute(CopyOp, SCSource);

	return Result == ECommandResult::Succeeded;
}


FSourceControlState USourceControlHelpers::QueryFileState(const FString& InFile, bool bSilent)
{
	FSourceControlState State;

	State.bIsValid = false;

	// Determine file type and ensure it is in form source control wants
	FString SCFile = SourceControlHelpersInternal::ConvertFileToQualifiedPath(InFile, bSilent);

	if (SCFile.IsEmpty())
	{
		State.Filename = InFile;
		return State;
	}

	State.Filename = SCFile;

	// Ensure source control system is up and running
	ISourceControlProvider* Provider = SourceControlHelpersInternal::VerifySourceControl(bSilent);

	if (!Provider)
	{
		return State;
	}

	// Make sure we update the modified state of the files (Perforce requires this
	// since can be a more expensive test).
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateStatusOperation->SetUpdateModifiedState(true);
	Provider->Execute(UpdateStatusOperation, SCFile);

	FSourceControlStatePtr SCState = Provider->GetState(SCFile, EStateCacheUsage::Use);

	if (!SCState.IsValid())
	{
		// Improper or invalid SCC state
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("InFile"), FText::FromString(InFile));
		Arguments.Add(TEXT("SCFile"), FText::FromString(SCFile));
		SourceControlHelpersInternal::LogError(FText::Format(LOCTEXT("CouldNotDetermineState", "Could not determine source control state of file '{InFile}' ({SCFile})."), Arguments), bSilent);

		return State;
	}

	// Return FSourceControlState rather than a ISourceControlState directly so that
	// scripting systems can access it.

	State.bIsValid = true;

	// Copy over state info
	// - make these assignments a method of FSourceControlState if anything else sets a state
	State.bIsUnknown			= SCState->IsUnknown();
	State.bIsSourceControlled	= SCState->IsSourceControlled();
	State.bCanCheckIn			= SCState->CanCheckIn();
	State.bCanCheckOut			= SCState->CanCheckout();
	State.bIsCheckedOut			= SCState->IsCheckedOut();
	State.bIsCurrent			= SCState->IsCurrent();
	State.bIsAdded				= SCState->IsAdded();
	State.bIsDeleted			= SCState->IsDeleted();
	State.bIsIgnored			= SCState->IsIgnored();
	State.bCanEdit				= SCState->CanEdit();
	State.bCanDelete			= SCState->CanDelete();
	State.bCanAdd				= SCState->CanAdd();
	State.bIsConflicted			= SCState->IsConflicted();
	State.bCanRevert			= SCState->CanRevert();
	State.bIsModified			= SCState->IsModified();
	State.bIsCheckedOutOther	= SCState->IsCheckedOutOther();

	if (State.bIsCheckedOutOther)
	{
		SCState->IsCheckedOutOther(&State.CheckedOutOther);
	}

	return State;
}



static FString PackageFilename_Internal( const FString& InPackageName )
{
	FString Filename = InPackageName;

	// Get the filename by finding it on disk first
	if ( !FPackageName::DoesPackageExist(InPackageName, nullptr, &Filename) )
	{
		// The package does not exist on disk, see if we can find it in memory and predict the file extension
		// Only do this if the supplied package name is valid
		const bool bIncludeReadOnlyRoots = false;
		if ( FPackageName::IsValidLongPackageName(InPackageName, bIncludeReadOnlyRoots) )
		{
			UPackage* Package = FindPackage(nullptr, *InPackageName);
			if ( Package )
			{
				// This is a package in memory that has not yet been saved. Determine the extension and convert to a filename
				const FString PackageExtension = Package->ContainsMap() ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
				Filename = FPackageName::LongPackageNameToFilename(InPackageName, PackageExtension);
			}
		}
	}

	return Filename;
}


FString USourceControlHelpers::PackageFilename( const FString& InPackageName )
{
	return FPaths::ConvertRelativePathToFull(PackageFilename_Internal(InPackageName));
}


FString USourceControlHelpers::PackageFilename( const UPackage* InPackage )
{
	FString Filename;
	if(InPackage != nullptr)
	{
		Filename = FPaths::ConvertRelativePathToFull(PackageFilename_Internal(InPackage->GetName()));
	}
	return Filename;
}


TArray<FString> USourceControlHelpers::PackageFilenames( const TArray<UPackage*>& InPackages )
{
	TArray<FString> OutNames;
	for (int32 PackageIndex = 0; PackageIndex < InPackages.Num(); PackageIndex++)
	{
		OutNames.Add(FPaths::ConvertRelativePathToFull(PackageFilename(InPackages[PackageIndex])));
	}

	return OutNames;
}


TArray<FString> USourceControlHelpers::PackageFilenames( const TArray<FString>& InPackageNames )
{
	TArray<FString> OutNames;
	for (int32 PackageIndex = 0; PackageIndex < InPackageNames.Num(); PackageIndex++)
	{
		OutNames.Add(FPaths::ConvertRelativePathToFull(PackageFilename_Internal(InPackageNames[PackageIndex])));
	}

	return OutNames;
}


TArray<FString> USourceControlHelpers::AbsoluteFilenames( const TArray<FString>& InFileNames )
{
	TArray<FString> AbsoluteFiles;

	for (const FString& FileName : InFileNames)
	{
		if(!FPaths::IsRelative(FileName))
		{
			AbsoluteFiles.Add(FileName);
		}
		else
		{
			AbsoluteFiles.Add(FPaths::ConvertRelativePathToFull(FileName));
		}

		FPaths::NormalizeFilename(AbsoluteFiles[AbsoluteFiles.Num() - 1]);
	}

	return AbsoluteFiles;
}


void USourceControlHelpers::RevertUnchangedFiles( ISourceControlProvider& InProvider, const TArray<FString>& InFiles )
{
	// Make sure we update the modified state of the files
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateStatusOperation->SetUpdateModifiedState(true);
	InProvider.Execute(UpdateStatusOperation, InFiles);

	TArray<FString> UnchangedFiles;
	TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> > OutStates;
	InProvider.GetState(InFiles, OutStates, EStateCacheUsage::Use);

	for(TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >::TConstIterator It(OutStates); It; It++)
	{
		TSharedRef<ISourceControlState, ESPMode::ThreadSafe> SourceControlState = *It;
		if(SourceControlState->IsCheckedOut() && !SourceControlState->IsModified())
		{
			UnchangedFiles.Add(SourceControlState->GetFilename());
		}
	}

	if(UnchangedFiles.Num())
	{
		InProvider.Execute( ISourceControlOperation::Create<FRevert>(), UnchangedFiles );
	}
}


bool USourceControlHelpers::AnnotateFile( ISourceControlProvider& InProvider, const FString& InLabel, const FString& InFile, TArray<FAnnotationLine>& OutLines )
{
	TArray< TSharedRef<ISourceControlLabel> > Labels = InProvider.GetLabels( InLabel );
	if(Labels.Num() > 0)
	{
		TSharedRef<ISourceControlLabel> Label = Labels[0];
		TArray< TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> > Revisions;
		Label->GetFileRevisions(InFile, Revisions);
		if(Revisions.Num() > 0)
		{
			TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> Revision = Revisions[0];
			if(Revision->GetAnnotated(OutLines))
			{
				return true;
			}
		}
	}

	return false;
}

bool USourceControlHelpers::AnnotateFile( ISourceControlProvider& InProvider, int32 InCheckInIdentifier, const FString& InFile, TArray<FAnnotationLine>& OutLines )
{
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateStatusOperation->SetUpdateHistory(true);
	if(InProvider.Execute(UpdateStatusOperation, InFile) == ECommandResult::Succeeded)
	{
		FSourceControlStatePtr State = InProvider.GetState(InFile, EStateCacheUsage::Use);
		if(State.IsValid())
		{
			for(int32 HistoryIndex = State->GetHistorySize() - 1; HistoryIndex >= 0; HistoryIndex--)
			{
				// check that the changelist corresponds to this revision - we assume history is in latest-first order
				TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision = State->GetHistoryItem(HistoryIndex);
				if(Revision.IsValid() && Revision->GetCheckInIdentifier() >= InCheckInIdentifier)
				{
					if(Revision->GetAnnotated(OutLines))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}


bool USourceControlHelpers::CheckoutOrMarkForAdd( const FString& InDestFile, const FText& InFileDescription, const FOnPostCheckOut& OnPostCheckOut, FText& OutFailReason )
{
	bool bSucceeded = true;

	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();

	// first check for source control check out
	if (ISourceControlModule::Get().IsEnabled())
	{
		FSourceControlStatePtr SourceControlState = Provider.GetState(InDestFile, EStateCacheUsage::ForceUpdate);
		if (SourceControlState.IsValid())
		{
			if (SourceControlState->IsSourceControlled() && SourceControlState->CanCheckout())
				{
					ECommandResult::Type Result = Provider.Execute(ISourceControlOperation::Create<FCheckOut>(), InDestFile);
					bSucceeded = (Result == ECommandResult::Succeeded);
					if (!bSucceeded)
					{
						OutFailReason = FText::Format(LOCTEXT("SourceControlCheckoutError", "Could not check out {0} file."), InFileDescription);
					}
				}
		}
	}

	if (bSucceeded)
	{
		if(OnPostCheckOut.IsBound())
		{
			bSucceeded = OnPostCheckOut.Execute(InDestFile, InFileDescription, OutFailReason);
		}
	}

	// mark for add now if needed
	if (bSucceeded && ISourceControlModule::Get().IsEnabled())
	{
		FSourceControlStatePtr SourceControlState = Provider.GetState(InDestFile, EStateCacheUsage::Use);
		if (SourceControlState.IsValid())
		{
			if (!SourceControlState->IsSourceControlled())
			{
				ECommandResult::Type Result = Provider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), InDestFile);
				bSucceeded = (Result == ECommandResult::Succeeded);
				if (!bSucceeded)
				{
					OutFailReason = FText::Format(LOCTEXT("SourceControlMarkForAddError", "Could not mark {0} file for add."), InFileDescription);
				}
			}
		}
	}

	return bSucceeded;
}


bool USourceControlHelpers::CopyFileUnderSourceControl( const FString& InDestFile, const FString& InSourceFile, const FText& InFileDescription, FText& OutFailReason)
{
	struct Local
	{
		static bool CopyFile(const FString& InDestinationFile, const FText& InFileDesc, FText& OutFailureReason, FString InFileToCopy)
		{
			const bool bReplace = true;
			const bool bEvenIfReadOnly = true;
			bool bSucceeded = (IFileManager::Get().Copy(*InDestinationFile, *InFileToCopy, bReplace, bEvenIfReadOnly) == COPY_OK);
			if (!bSucceeded)
			{
				OutFailureReason = FText::Format(LOCTEXT("ExternalImageCopyError", "Could not overwrite {0} file."), InFileDesc);
			}

			return bSucceeded;
		}
	};

	return CheckoutOrMarkForAdd(InDestFile, InFileDescription, FOnPostCheckOut::CreateStatic(&Local::CopyFile, InSourceFile), OutFailReason);
}


bool USourceControlHelpers::BranchPackage( UPackage* DestPackage, UPackage* SourcePackage )
{
	if(ISourceControlModule::Get().IsEnabled())
	{
		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

		const FString SourceFilename = PackageFilename(SourcePackage);
		const FString DestFilename = PackageFilename(DestPackage);
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceFilename, EStateCacheUsage::ForceUpdate);
		if(SourceControlState.IsValid() && SourceControlState->IsSourceControlled())
		{
			TSharedRef<FCopy, ESPMode::ThreadSafe> CopyOperation = ISourceControlOperation::Create<FCopy>();
			CopyOperation->SetDestination(DestFilename);
			
			return (SourceControlProvider.Execute(CopyOperation, SourceFilename) == ECommandResult::Succeeded);
		}
	}

	return false;
}


const FString& USourceControlHelpers::GetSettingsIni()
{
	if (ISourceControlModule::Get().GetUseGlobalSettings())
	{
		return GetGlobalSettingsIni();
	}
	else
	{
		static FString SourceControlSettingsIni;
		if (SourceControlSettingsIni.Len() == 0)
		{
			const FString SourceControlSettingsDir = FPaths::GeneratedConfigDir();
			FConfigCacheIni::LoadGlobalIniFile(SourceControlSettingsIni, TEXT("SourceControlSettings"), nullptr, false, false, true, *SourceControlSettingsDir);
		}
		return SourceControlSettingsIni;
	}
}


const FString& USourceControlHelpers::GetGlobalSettingsIni()
{
	static FString SourceControlGlobalSettingsIni;
	if (SourceControlGlobalSettingsIni.Len() == 0)
	{
		const FString SourceControlSettingsDir = FPaths::EngineSavedDir() + TEXT("Config/");
		FConfigCacheIni::LoadGlobalIniFile(SourceControlGlobalSettingsIni, TEXT("SourceControlSettings"), nullptr, false, false, true, *SourceControlSettingsDir);
	}
	return SourceControlGlobalSettingsIni;
}


FScopedSourceControl::FScopedSourceControl()
{
	ISourceControlModule::Get().GetProvider().Init();
}


FScopedSourceControl::~FScopedSourceControl()
{
	ISourceControlModule::Get().GetProvider().Close();
}


ISourceControlProvider& FScopedSourceControl::GetProvider()
{
	return ISourceControlModule::Get().GetProvider();
}


#undef LOCTEXT_NAMESPACE
