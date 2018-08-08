// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
// Copyright 2016 Magic Leap, Inc. All Rights Reserved.

/*=============================================================================================
Lumin platform File functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformFile.h"

/**
 * File I/O implementation
**/
class CORE_API FLuminPlatformFile : public IPhysicalPlatformFile
{
protected:
	virtual FString NormalizeFilename(const TCHAR* Filename);
	virtual FString NormalizeDirectory(const TCHAR* Directory);
public:
	virtual bool FileExists(const TCHAR* Filename) override;
	bool FileExists(const TCHAR* Filename, FString& OutLuminPath);
	virtual int64 FileSize(const TCHAR* Filename) override;
	virtual bool DeleteFile(const TCHAR* Filename) override;
	virtual bool IsReadOnly(const TCHAR* Filename) override;
	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override;
	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override;


	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override;

	virtual void SetTimeStamp(const TCHAR* Filename, const FDateTime DateTime) override;

	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) override;
	virtual FString GetFilenameOnDisk(const TCHAR* Filename) override;

	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override;
	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override;
	virtual bool DirectoryExists(const TCHAR* Directory) override;
	virtual bool CreateDirectory(const TCHAR* Directory) override;
	virtual bool DeleteDirectory(const TCHAR* Directory) override;
	/**
	 *	Enables/disables application sandbox jail. When sandboxing is enabled all paths are prepended with
	 *	the app root directory. When disabled paths are not prepended or checked for safety. For example,
	 *	reading files from the directory /system/etc/security/cacerts requires reading from outside the app root directory,
	 *	which cannot be done when sandboxing is enabled. Toggling the sandbox must be done manually by calling this function both
	 *	before and after attempting to access any path outside of the app  root directory. Only disable sandboxing when certain you know what you're doing.
	 *	
	 *	@param bInEnabled True to enable sandboxing, false to disable
	 */
	virtual void SetSandboxEnabled(bool bInEnabled) override;
	/**
	 *	Returns whether sandboxing is enabled or disabled.
	 *
	 *	@return bool Returns true when sandboxing is enabled, otherwise returns false.
	*/
	virtual bool IsSandboxEnabled() const override;
	virtual FString ConvertToAbsolutePathForExternalAppForWrite(const TCHAR* AbsolutePath) override;
	virtual FString ConvertToAbsolutePathForExternalAppForRead(const TCHAR* AbsolutePath) override;

	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override;

	bool CreateDirectoriesFromPath(const TCHAR* Path);

	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override;
	virtual bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override;

	FString ConvertToLuminPath(const FString& Filename, bool bForWrite) const;

protected:
	bool IterateDirectoryCommon(const TCHAR* Directory, const TFunctionRef<bool(struct dirent*)>& Visitor);
	bool bIsSandboxEnabled = true;

private:
	bool FileExistsCaseInsensitive(const FString& NormalizedFilename) const;
	int64 FileSizeCaseInsensitive(const FString& NormalizedFilename) const;
	bool IsReadOnlyCaseInsensitive(const FString& NormalizedFilename) const;
	FDateTime GetTimeStampCaseInsensitive(const FString& NormalizedFilename) const;
	FDateTime GetAccessTimeStampCaseInsensitive(const FString& NormalizedFilename) const;
	FFileStatData GetStatDataCaseInsensitive(const FString& NormalizedFilename, bool& bFound) const;
	bool DirectoryExistsCaseInsensitive(const FString& NormalizedFilename) const;
};
