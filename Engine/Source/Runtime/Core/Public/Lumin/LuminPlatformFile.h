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

	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override;

	bool CreateDirectoriesFromPath(const TCHAR* Path);

	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override;
	virtual bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override;

	FString ConvertToLuminPath(const FString& Filename, bool bForWrite) const;

protected:
	bool IterateDirectoryCommon(const TCHAR* Directory, const TFunctionRef<bool(struct dirent*)>& Visitor);

private:
	bool FileExistsCaseInsensitive(const FString& NormalizedFilename) const;
	int64 FileSizeCaseInsensitive(const FString& NormalizedFilename) const;
	bool IsReadOnlyCaseInsensitive(const FString& NormalizedFilename) const;
	FDateTime GetTimeStampCaseInsensitive(const FString& NormalizedFilename) const;
	FDateTime GetAccessTimeStampCaseInsensitive(const FString& NormalizedFilename) const;
	FFileStatData GetStatDataCaseInsensitive(const FString& NormalizedFilename, bool& bFound) const;
	bool DirectoryExistsCaseInsensitive(const FString& NormalizedFilename) const;
};
