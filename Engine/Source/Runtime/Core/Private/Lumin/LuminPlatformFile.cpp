// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
// Copyright 2016 Magic Leap, Inc. All Rights Reserved.

#include "Lumin/LuminPlatformFile.h"
#include "Android/AndroidMisc.h"
#include "Lumin/LuminLifecycle.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include <sys/stat.h>   // mkdirp()
#include <dirent.h>

DEFINE_LOG_CATEGORY_STATIC(LogLuminPlatformFile, Log, All);

// make an FTimeSpan object that represents the "epoch" for time_t (from a stat struct)
const FDateTime UnixEpoch(1970, 1, 1);

namespace
{
	FFileStatData UnixStatToUEFileData(struct stat& FileInfo)
	{
		const bool bIsDirectory = S_ISDIR(FileInfo.st_mode);

		int64 FileSize = -1;
		if (!bIsDirectory)
		{
			FileSize = FileInfo.st_size;
		}

		return FFileStatData(
			UnixEpoch + FTimespan(0, 0, FileInfo.st_ctime), 
			UnixEpoch + FTimespan(0, 0, FileInfo.st_atime), 
			UnixEpoch + FTimespan(0, 0, FileInfo.st_mtime), 
			FileSize,
			bIsDirectory,
			!!(FileInfo.st_mode & S_IWUSR)
			);
	}
}

/**
* File handle implementation which limits number of open files per thread. This
* is to prevent running out of system file handles. Should not be neccessary when
* using pak file (e.g., SHIPPING?) so not particularly optimized. Only manages
* files which are opened READ_ONLY.
*/
// @todo lumin - Consider if we need managed handles or not.
#define MANAGE_FILE_HANDLES 0

/**
* File handle implementation
*/
class CORE_API FFileHandleLumin : public IFileHandle
{
	enum {READWRITE_SIZE = 1024 * 1024};

	FORCEINLINE bool IsValid()
	{
		return FileHandle != -1;
	}

public:
	FFileHandleLumin(int32 InFileHandle, const TCHAR* InFilename, bool bIsReadOnly)
		: FileHandle(InFileHandle)
#if MANAGE_FILE_HANDLES
		, Filename(InFilename)
		, HandleSlot(-1)
		, FileOffset(0)
		, FileSize(0)
#endif // MANAGE_FILE_HANDLES
	{
		check(FileHandle > -1);
#if MANAGE_FILE_HANDLES
		check(Filename.Len() > 0);
#endif // MANAGE_FILE_HANDLES

#if MANAGE_FILE_HANDLES
		// Only files opened for read will be managed
		if (bIsReadOnly)
		{
			ReserveSlot();
			ActiveHandles[HandleSlot] = this;
			struct stat FileInfo;
			fstat(FileHandle, &FileInfo);
			FileSize = FileInfo.st_size;
		}
#endif // MANAGE_FILE_HANDLES
	}

	virtual ~FFileHandleLumin()
	{
#if MANAGE_FILE_HANDLES
		if( IsManaged() )
		{
			if( ActiveHandles[ HandleSlot ] == this )
			{
				close(FileHandle);
				ActiveHandles[ HandleSlot ] = NULL;
			}
		}
		else
#endif // MANAGE_FILE_HANDLES
		{
			close(FileHandle);
		}
		FileHandle = -1;
	}

	virtual int64 Tell() override
	{
#if MANAGE_FILE_HANDLES
		if( IsManaged() )
		{
			return FileOffset;
		}
		else
#endif // MANAGE_FILE_HANDLES
		{
			check(IsValid());
			return lseek(FileHandle, 0, SEEK_CUR);
		}
	}

	virtual bool Seek(int64 NewPosition) override
	{
		check(NewPosition >= 0);
		
#if MANAGE_FILE_HANDLES
		if( IsManaged() )
		{
			FileOffset = NewPosition >= FileSize ? FileSize - 1 : NewPosition;
			return IsValid() && ActiveHandles[ HandleSlot ] == this ? lseek(FileHandle, FileOffset, SEEK_SET) != -1 : true;
		}
		else
#endif // MANAGE_FILE_HANDLES
		{
			check(IsValid());
			return lseek(FileHandle, NewPosition, SEEK_SET) != -1;
		}
	}

	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override
	{
		check(NewPositionRelativeToEnd <= 0);

#if MANAGE_FILE_HANDLES
		if( IsManaged() )
		{
			FileOffset = (NewPositionRelativeToEnd >= FileSize) ? 0 : ( FileSize + NewPositionRelativeToEnd - 1 );
			return IsValid() && ActiveHandles[ HandleSlot ] == this ? lseek(FileHandle, FileOffset, SEEK_SET) != -1 : true;
		}
		else
#endif // MANAGE_FILE_HANDLES
		{
			check(IsValid());
			return lseek(FileHandle, NewPositionRelativeToEnd, SEEK_END) != -1;
		}
	}

	virtual bool Read(uint8* Destination, int64 BytesToRead) override
	{
#if MANAGE_FILE_HANDLES
		if( IsManaged() )
		{
			ActivateSlot();
			int64 BytesRead = ReadInternal(Destination, BytesToRead);
			FileOffset += BytesRead;
			return BytesRead == BytesToRead;
		}
		else
#endif // MANAGE_FILE_HANDLES
		{
			return ReadInternal(Destination, BytesToRead) == BytesToRead;
		}
	}

	virtual bool Write(const uint8* Source, int64 BytesToWrite) override
	{
		check(IsValid());
		while (BytesToWrite)
		{
			check(BytesToWrite >= 0);
			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToWrite);
			check(Source);
			if (write(FileHandle, Source, ThisSize) != ThisSize)
			{
				return false;
			}
			Source += ThisSize;
			BytesToWrite -= ThisSize;
		}
		return true;
	}

	virtual int64 Size() override
	{
		#if MANAGE_FILE_HANDLES
		if( IsManaged() )
		{
			return FileSize;
		}
		else
		#endif
		{
			struct stat FileInfo;
			fstat(FileHandle, &FileInfo);
			return FileInfo.st_size;
		}
	}

	
private:

#if MANAGE_FILE_HANDLES
	FORCEINLINE bool IsManaged()
	{
		return HandleSlot != -1;
	}

	void ActivateSlot()
	{
		if( IsManaged() )
		{
			if( ActiveHandles[ HandleSlot ] != this || (ActiveHandles[ HandleSlot ] && ActiveHandles[ HandleSlot ]->FileHandle == -1) )
			{
				ReserveSlot();
				
				FileHandle = open(TCHAR_TO_UTF8(*Filename), O_RDONLY | O_CLOEXEC);
				if( FileHandle != -1 )
				{
					lseek(FileHandle, FileOffset, SEEK_SET);
					ActiveHandles[ HandleSlot ] = this;
				}
				else
				{
					UE_LOG(LogLuminPlatformFile, Warning, TEXT("Could not (re)activate slot for file '%s'"), *Filename);
				}
			}
			else
			{
				AccessTimes[ HandleSlot ] = FPlatformTime::Seconds();
			}
		}
	}

	void ReserveSlot()
	{
		HandleSlot = -1;
		
		// Look for non-reserved slot
		for( int32 i = 0; i < ACTIVE_HANDLE_COUNT; ++i )
		{
			if( ActiveHandles[ i ] == NULL )
			{
				HandleSlot = i;
				break;
			}
		}
		
		// Take the oldest handle
		if( HandleSlot == -1 )
		{
			int32 Oldest = 0;
			for( int32 i = 1; i < ACTIVE_HANDLE_COUNT; ++i )
			{
				if( AccessTimes[ Oldest ] > AccessTimes[ i ] )
				{
					Oldest = i;
				}
			}
			
			close( ActiveHandles[ Oldest ]->FileHandle );
			ActiveHandles[ Oldest ]->FileHandle = -1;
			HandleSlot = Oldest;
		}
		
		ActiveHandles[ HandleSlot ] = NULL;
		AccessTimes[ HandleSlot ] = FPlatformTime::Seconds();
	}
#endif // MANAGE_FILE_HANDLES

	int64 ReadInternal(uint8* Destination, int64 BytesToRead)
	{
		check(IsValid());
		int64 BytesRead = 0;
		while (BytesToRead)
		{
			check(BytesToRead >= 0);
			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToRead);
			check(Destination);
			int64 ThisRead = read(FileHandle, Destination, ThisSize);
			BytesRead += ThisRead;
			if (ThisRead != ThisSize)
			{
				return BytesRead;
			}
			Destination += ThisSize;
			BytesToRead -= ThisSize;
		}
		return BytesRead;
	}

	// Holds the internal file handle.
	int32 FileHandle;

#if MANAGE_FILE_HANDLES
	// Holds the name of the file that this handle represents. Kept around for possible reopen of file.
	FString Filename;
	
	// Most recent valid slot index for this handle; >=0 for handles which are managed.
	int32 HandleSlot;
	
	// Current file offset; valid if a managed handle.
	int64 FileOffset;
	
	// Cached file size; valid if a managed handle.
	int64 FileSize;
	
	// Each thread keeps a collection of active handles with access times.
	static const int32 ACTIVE_HANDLE_COUNT = 256;
	static __thread FFileHandleLumin* ActiveHandles[ACTIVE_HANDLE_COUNT];
	static __thread double AccessTimes[ACTIVE_HANDLE_COUNT];
#endif // MANAGE_FILE_HANDLES
};

#if MANAGE_FILE_HANDLES
__thread FFileHandleLumin* FFileHandleLumin::ActiveHandles[FFileHandleLumin::ACTIVE_HANDLE_COUNT];
__thread double FFileHandleLumin::AccessTimes[FFileHandleLumin::ACTIVE_HANDLE_COUNT];
#endif // MANAGE_FILE_HANDLES

/**
 * A class to handle case insensitive file opening. This is a band-aid, non-performant approach,
 * without any caching.
 */
class FLuminFileMapper
{

public:

	FLuminFileMapper()
	{
	}

	FString GetPathComponent(const FString & Filename, int NumPathComponent)
	{
		// skip over empty part
		int StartPosition = (Filename[0] == TEXT('/')) ? 1 : 0;
		
		for (int ComponentIdx = 0; ComponentIdx < NumPathComponent; ++ComponentIdx)
		{
			int FoundAtIndex = Filename.Find(TEXT("/"), ESearchCase::CaseSensitive,
											 ESearchDir::FromStart, StartPosition);
			
			if (FoundAtIndex == INDEX_NONE)
			{
				checkf(false, TEXT("Asked to get %d-th path component, but filename '%s' doesn't have that many!"), 
					   NumPathComponent, *Filename);
				break;
			}
			
			StartPosition = FoundAtIndex + 1;	// skip the '/' itself
		}

		// now return the 
		int NextSlash = Filename.Find(TEXT("/"), ESearchCase::CaseSensitive,
									  ESearchDir::FromStart, StartPosition);
		if (NextSlash == INDEX_NONE)
		{
			// just return the rest of the string
			return Filename.RightChop(StartPosition);
		}
		else if (NextSlash == StartPosition)
		{
			return TEXT("");	// encountered an invalid path like /foo/bar//baz
		}
		
		return Filename.Mid(StartPosition, NextSlash - StartPosition);
	}

	int32 CountPathComponents(const FString & Filename)
	{
		if (Filename.Len() == 0)
		{
			return 0;
		}

		// if the first character is not a separator, it's part of a distinct component
		int NumComponents = (Filename[0] != TEXT('/')) ? 1 : 0;
		for (const auto & Char : Filename)
		{
			if (Char == TEXT('/'))
			{
				++NumComponents;
			}
		}

		// cannot be 0 components if path is non-empty
		return FMath::Max(NumComponents, 1);
	}

	/**
	 * Tries to recursively find (using case-insensitive comparison) and open the file.
	 * The first file found will be opened.
	 * 
	 * @param Filename Original file path as requested (absolute)
	 * @param PathComponentToLookFor Part of path we are currently trying to find.
	 * @param MaxPathComponents Maximum number of path components (directories), i.e. how deep the path is.
	 * @param ConstructedPath The real (absolute) path that we have found so far
	 * 
	 * @return a handle opened with open()
	 */
	bool MapFileRecursively(const FString & Filename, int PathComponentToLookFor, int MaxPathComponents, FString & ConstructedPath)
	{
		// get the directory without the last path component
		FString BaseDir = ConstructedPath;

		// get the path component to compare
		FString PathComponent = GetPathComponent(Filename, PathComponentToLookFor);
		FString PathComponentLower = PathComponent.ToLower();

		bool bFound = false;

		// see if we can open this (we should)
		DIR* DirHandle = opendir(TCHAR_TO_UTF8(*BaseDir));
		if (DirHandle)
		{
			struct dirent *Entry;
			while ((Entry = readdir(DirHandle)) != nullptr)
			{
				FString DirEntry = UTF8_TO_TCHAR(Entry->d_name);
				if (DirEntry.ToLower() == PathComponentLower)
				{
					if (PathComponentToLookFor < MaxPathComponents - 1)
					{
						// make sure this is a directory
						bool bIsDirectory = Entry->d_type == DT_DIR;
						if(Entry->d_type == DT_UNKNOWN)
						{
							struct stat StatInfo;
							if(stat(TCHAR_TO_UTF8(*(BaseDir / Entry->d_name)), &StatInfo) == 0)
							{
								bIsDirectory = S_ISDIR(StatInfo.st_mode);
							}
						}

						if (bIsDirectory)
						{
							// recurse with the new filename
							FString NewConstructedPath = ConstructedPath;
							NewConstructedPath /= DirEntry;

							bFound = MapFileRecursively(Filename, PathComponentToLookFor + 1, MaxPathComponents, NewConstructedPath);
							if (bFound)
							{
								ConstructedPath = NewConstructedPath;
								break;
							}
						}
					}
					else
					{
						// last level, try opening directly
						FString ConstructedFilename = ConstructedPath;
						ConstructedFilename /= DirEntry;

						struct stat StatInfo;
						bFound = (stat(TCHAR_TO_UTF8(*ConstructedFilename), &StatInfo) == 0);
						if (bFound)
						{
							ConstructedPath = ConstructedFilename;
							break;
						}
					}
				}
			}
			closedir(DirHandle);
		}
		
		return bFound;
	}

	/**
	 * Tries to map a filename (one with a possibly wrong case) to one that exists.
	 * 
	 * @param PossiblyWrongFilename absolute filename (that has possibly a wrong case)
	 * @param ExistingFilename filename that exists (only valid to use if the function returned success).
	 */
	bool MapCaseInsensitiveFile(const FString & PossiblyWrongFilename, FString & ExistingFilename)
	{
		ExistingFilename = PossiblyWrongFilename;
		return true;

		// Cannot log anything here, as this may result in infinite recursion when this function is called on log file itself

		// We can get some "absolute" filenames like "D:/Blah/" here (e.g. non-Lumin paths to source files embedded in assets).
		// In that case, fail silently.
		if (PossiblyWrongFilename.IsEmpty() || PossiblyWrongFilename[0] != TEXT('/'))
		{
			return false;
		}

		// try the filename as given first
		struct stat StatInfo;
		bool bFound = stat(TCHAR_TO_UTF8(*PossiblyWrongFilename), &StatInfo) == 0;

		if (bFound)
		{
			ExistingFilename = PossiblyWrongFilename;
		}
		else
		{
			// perform a case-insensitive search from /

			int MaxPathComponents = CountPathComponents(PossiblyWrongFilename);
			if (MaxPathComponents > 0)
			{
				FString FoundFilename(TEXT("/"));	// start with root
				bFound = MapFileRecursively(PossiblyWrongFilename, 0, MaxPathComponents, FoundFilename);
				if (bFound)
				{
					ExistingFilename = FoundFilename;
				}
			}
		}

		return bFound;
	}

	/**
	 * Opens a file for reading, disregarding the case.
	 * 
	 * @param Filename absolute filename
	 * @param MappedToFilename absolute filename that we mapped the Filename to (always filled out on success, even if the same as Filename)
	 */
	int32 OpenCaseInsensitiveRead(const FString & Filename, FString & MappedToFilename)
	{
		// We can get some "absolute" filenames like "D:/Blah/" here (e.g. non-Lumin paths to source files embedded in assets).
		// In that case, fail silently.
		if (Filename.IsEmpty() || Filename[0] != TEXT('/'))
		{
			return -1;
		}

		// try opening right away
		int32 Handle = open(TCHAR_TO_UTF8(*Filename), O_RDONLY | O_CLOEXEC);
		if (Handle != -1)
		{
			MappedToFilename = Filename;
		}
		else
		{
			// log non-standard errors only
			if (ENOENT != errno)
			{
				int ErrNo = errno;
				UE_LOG(LogLuminPlatformFile, Warning, TEXT("open('%s', O_RDONLY | O_CLOEXEC) failed: errno=%d (%s)"), *Filename, ErrNo, ANSI_TO_TCHAR(strerror(ErrNo)));
			}
			else
			{
				// perform a case-insensitive search
				// make sure we get the absolute filename
				checkf(Filename[0] == TEXT('/'), TEXT("Filename '%s' given to OpenCaseInsensitiveRead is not absolute!"), *Filename);
				
				int MaxPathComponents = CountPathComponents(Filename);
				if (MaxPathComponents > 0)
				{
					FString FoundFilename(TEXT("/"));	// start with root
					if (MapFileRecursively(Filename, 0, MaxPathComponents, FoundFilename))
					{
						Handle = open(TCHAR_TO_UTF8(*FoundFilename), O_RDONLY | O_CLOEXEC);
						if (Handle != -1)
						{
							MappedToFilename = FoundFilename;
							if (Filename != MappedToFilename)
							{
								UE_LOG(LogLuminPlatformFile, Log, TEXT("Mapped '%s' to '%s'"), *Filename, *MappedToFilename);
							}
						}
					}
				}
			}
		}
		return Handle;
	}
};

FLuminFileMapper GCaseInsensMapper;

/**
* Lumin File I/O implementation
**/
FString FLuminPlatformFile::NormalizeFilename(const TCHAR* Filename)
{
	FString Result(Filename);
	FPaths::NormalizeFilename(Result);
	// Don't convert relative path to full path. 
	// When jailing is on, the BaseDir() is /package/bin/. The incoming paths are usually of the format ../../../ProjectName/
	// When ConvertRelativePathToFull() tries to collapse the relative path, we run out of the root directory, and hit an edge case and the path is set to /../ProjectName/
	// This still works when jailing is enabled because FLuminPlatformFile::ConvertToLuminPath() gets rid of all relative path prepends and constructs with its own base path.
	// When jailing is disabled, ConvertRelativePathToFull() collapses the incoming path to something else, which is then prepended by FLuminPlatformFile::ConvertToLuminPath()
	// with its own base path and we end up with an invalid path.
	// e.g. when jailing is ofd, BaseDir() is of the format /folder1/folder2/folder3/folder4 and so on. ../../../ProjectName gives us the path as
	// /folder1/ProjectName which is wrong.
	return Result; //FPaths::ConvertRelativePathToFull(Result);
}

FString FLuminPlatformFile::NormalizeDirectory(const TCHAR* Directory)
{
	FString Result(Directory);
	FPaths::NormalizeDirectoryName(Result);
	// Don't convert relative path to full path.
	// See comment in FLuminPlatformFile::NormalizeFilename
	return Result; //FPaths::ConvertRelativePathToFull(Result);
}

bool FLuminPlatformFile::FileExists(const TCHAR* Filename)
{
	FString NormalizedFilename = NormalizeFilename(Filename);

	// Check the read path first, if it doesnt exist, check for the write path instead.
	return (FileExistsCaseInsensitive(ConvertToLuminPath(NormalizedFilename, false))) ? true : FileExistsCaseInsensitive(ConvertToLuminPath(NormalizedFilename, true));
}

bool FLuminPlatformFile::FileExists(const TCHAR* Filename, FString& OutLuminPath)
{
	bool bExists = false;
	FString NormalizedFilename = NormalizeFilename(Filename);
	FString ReadFilePath = ConvertToLuminPath(NormalizedFilename, false);
	
	if (FileExistsCaseInsensitive(ReadFilePath))
	{
		OutLuminPath = ReadFilePath;
		bExists = true;
	}
	else
	{
		FString WriteFilePath = ConvertToLuminPath(NormalizedFilename, true);
		if (FileExistsCaseInsensitive(WriteFilePath))
		{
			OutLuminPath = WriteFilePath;
			bExists = true;
		}
	}

	return bExists;
}

int64 FLuminPlatformFile::FileSize(const TCHAR* Filename)
{
	// Checking that the file exists will also give us the true location of the file.
	// Which can be either in the read-only or the read-write areas of the application.
	FString LuminPath;
	if (FileExists(Filename, LuminPath))
	{
		return FileSizeCaseInsensitive(LuminPath);
	}
	return -1;
}

bool FLuminPlatformFile::DeleteFile(const TCHAR* Filename)
{
	FString CaseSensitiveFilename;
	// Only delete from write path
	FString IntendedFilename(ConvertToLuminPath(NormalizeFilename(Filename), true));
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(IntendedFilename, CaseSensitiveFilename))
	{
		// could not find the file
		return false;
	}

	// removing mapped file is too dangerous
	if (IntendedFilename != CaseSensitiveFilename)
	{
		UE_LOG(LogLuminPlatformFile, Warning, TEXT("Could not find file '%s', deleting file '%s' instead (for consistency with the rest of file ops)"), *IntendedFilename, *CaseSensitiveFilename);
	}
	return unlink(TCHAR_TO_UTF8(*CaseSensitiveFilename)) == 0;
}

bool FLuminPlatformFile::IsReadOnly(const TCHAR* Filename)
{
	// Checking that the file exists will also give us the true location of the file.
	// Which can be either in the read-only or the read-write areas of the application.
	FString LuminPath;
	if (FileExists(Filename, LuminPath))
	{
		return IsReadOnlyCaseInsensitive(LuminPath);
	}
	return false;
}

bool FLuminPlatformFile::MoveFile(const TCHAR* To, const TCHAR* From)
{
	// Move to write path only.
	FString ToLuminFilename = ConvertToLuminPath(NormalizeFilename(To), true);
	FString FromLuminFilename = ConvertToLuminPath(NormalizeFilename(From), true);
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(FromLuminFilename, CaseSensitiveFilename))
	{
		// could not find the file
		return false;
	}

	return rename(TCHAR_TO_UTF8(*CaseSensitiveFilename), TCHAR_TO_UTF8(*ToLuminFilename)) == 0;
}

bool FLuminPlatformFile::SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue)
{
	FString LuminFilename = ConvertToLuminPath(NormalizeFilename(Filename), false);
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(LuminFilename, CaseSensitiveFilename))
	{
		// could not find the file
		return false;
	}

	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) == 0)
	{
		if (bNewReadOnlyValue)
		{
			FileInfo.st_mode &= ~S_IWUSR;
		}
		else
		{
			FileInfo.st_mode |= S_IWUSR;
		}
		return chmod(TCHAR_TO_UTF8(*CaseSensitiveFilename), FileInfo.st_mode) == 0;
	}
	return false;
}

FDateTime FLuminPlatformFile::GetTimeStamp(const TCHAR* Filename)
{
	// Checking that the file exists will also give us the true location of the file.
	// Which can be either in the read-only or the read-write areas of the application.
	FString LuminPath;
	if (FileExists(Filename, LuminPath))
	{
		return GetTimeStampCaseInsensitive(LuminPath);
	}
	return FDateTime::MinValue();
}

void FLuminPlatformFile::SetTimeStamp(const TCHAR* Filename, const FDateTime DateTime)
{
	FString CaseSensitiveFilename;
	// Update timestamp on a file in the write path only.
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(ConvertToLuminPath(NormalizeFilename(Filename), true), CaseSensitiveFilename))
	{
		// could not find the file
		return;
	}

	// get file times
	struct stat FileInfo;
	if(stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) != 0)
	{
		return;
	}

	// change the modification time only
	struct utimbuf Times;
	Times.actime = FileInfo.st_atime;
	Times.modtime = (DateTime - UnixEpoch).GetTotalSeconds();
	utime(TCHAR_TO_UTF8(*CaseSensitiveFilename), &Times);
}

FDateTime FLuminPlatformFile::GetAccessTimeStamp(const TCHAR* Filename)
{
	// Checking that the file exists will also give us the true location of the file.
	// Which can be either in the read-only or the read-write areas of the application.
	FString LuminPath;
	if (FileExists(Filename, LuminPath))
	{
		return GetAccessTimeStampCaseInsensitive(LuminPath);
	}
	return FDateTime::MinValue();
}

FString FLuminPlatformFile::GetFilenameOnDisk(const TCHAR* Filename)
{
	return Filename;
/*
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizeFilename(Filename), CaseSensitiveFilename))
	{
		return Filename;
	}

	return CaseSensitiveFilename;
*/
}

IFileHandle* FLuminPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	FString NormalizedFilename = NormalizeFilename(Filename);
	FString MappedToName;
	// Check the read path.
	int32 Handle = GCaseInsensMapper.OpenCaseInsensitiveRead(ConvertToLuminPath(NormalizedFilename, false), MappedToName);
	if (Handle == -1)
	{
		// If not in the read path, check the write path.
		Handle = GCaseInsensMapper.OpenCaseInsensitiveRead(ConvertToLuminPath(NormalizedFilename, true), MappedToName);
		if (Handle == -1)
		{
			return nullptr;
		}
	}
	return new FFileHandleLumin(Handle, *MappedToName, true);
}

IFileHandle* FLuminPlatformFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
	int Flags = O_CREAT | O_CLOEXEC;	// prevent children from inheriting this

	if (bAllowRead)
	{
		Flags |= O_RDWR;
	}
	else
	{
		Flags |= O_WRONLY;
	}

	// Writable files only in the write path.
	FString LuminFilename = ConvertToLuminPath(Filename, true);

	// create directories if needed.
	if (!CreateDirectoriesFromPath(*LuminFilename))
	{
		return NULL;
	}

	// Caveat: cannot specify O_TRUNC in flags, as this will corrupt the file which may be "locked" by other process. We will ftruncate() it once we "lock" it
	int32 Handle = open(TCHAR_TO_UTF8(*LuminFilename), Flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (Handle != -1)
	{
		if (!bAppend)
		{
			if (ftruncate(Handle, 0) != 0)
			{
				int ErrNo = errno;
				UE_LOG(LogLuminPlatformFile, Warning, TEXT("ftruncate() failed for '%s': errno=%d (%s)"),
					*LuminFilename, ErrNo, ANSI_TO_TCHAR(strerror(ErrNo)));
				close(Handle);
				return nullptr;
			}
		}

#if MANAGE_FILE_HANDLES
		FFileHandleLumin* FileHandleLumin = new FFileHandleLumin(Handle, *NormalizeDirectory(*LuminFilename), false);
#else
		FFileHandleLumin* FileHandleLumin = new FFileHandleLumin(Handle, *LuminFilename, false);
#endif // MANAGE_FILE_HANDLES

		if (bAppend)
		{
			FileHandleLumin->SeekFromEnd(0);
		}
		return FileHandleLumin;
	}

	int ErrNo = errno;
	UE_LOG(LogLuminPlatformFile, Warning, TEXT("open('%s', Flags=0x%08X) failed: errno=%d (%s)"), *LuminFilename, Flags, ErrNo, ANSI_TO_TCHAR(strerror(ErrNo)));
	return nullptr;
}

bool FLuminPlatformFile::DirectoryExists(const TCHAR* Directory)
{
	FString NormalizedFilename = NormalizeFilename(Directory);
	// Check the read path first, if it doesnt exist, check for the write path instead.
	return (DirectoryExistsCaseInsensitive(ConvertToLuminPath(NormalizedFilename, false))) ? true : DirectoryExistsCaseInsensitive(ConvertToLuminPath(NormalizedFilename, true));
}

bool FLuminPlatformFile::CreateDirectory(const TCHAR* Directory)
{
	// Create directory in write path only.
	FString LuminFilename = ConvertToLuminPath(NormalizeFilename(Directory), true);
	bool result = mkdir(TCHAR_TO_UTF8(*LuminFilename), 0755) == 0;
	return result;
}

bool FLuminPlatformFile::DeleteDirectory(const TCHAR* Directory)
{
	FString CaseSensitiveFilename;
	// Delete directory from write path only.
	FString IntendedFilename(ConvertToLuminPath(NormalizeFilename(Directory), true));
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(IntendedFilename, CaseSensitiveFilename))
	{
		// could not find the directory
		return false;
	}

	// removing mapped directory is too dangerous
	if (IntendedFilename != CaseSensitiveFilename)
	{
		UE_LOG(LogLuminPlatformFile, Warning, TEXT("Could not find directory '%s', deleting '%s' instead (for consistency with the rest of file ops)"), *IntendedFilename, *CaseSensitiveFilename);
	}
	return rmdir(TCHAR_TO_UTF8(*CaseSensitiveFilename)) == 0;
}

void FLuminPlatformFile::SetSandboxEnabled(bool bInEnabled)
{
	bIsSandboxEnabled = bInEnabled;
	UE_LOG(LogLuminPlatformFile, Log, TEXT("Application sandbox jail has been %s."), bIsSandboxEnabled ? TEXT("enabled") : TEXT("disabled"));
}

bool FLuminPlatformFile::IsSandboxEnabled() const
{
	return bIsSandboxEnabled;
}

FString FLuminPlatformFile::ConvertToAbsolutePathForExternalAppForWrite(const TCHAR* AbsolutePath)
{
	return ConvertToLuminPath(FString(AbsolutePath), true);
}

FString FLuminPlatformFile::ConvertToAbsolutePathForExternalAppForRead(const TCHAR* AbsolutePath)
{
	return ConvertToLuminPath(FString(AbsolutePath), false);
}

FFileStatData FLuminPlatformFile::GetStatData(const TCHAR* FilenameOrDirectory)
{
	FString NormalizedFilename = NormalizeFilename(FilenameOrDirectory);
	bool found = false;

	// Check the read path first, if it doesnt exist, check for the write path instead.
	FFileStatData result = GetStatDataCaseInsensitive(ConvertToLuminPath(NormalizedFilename, false), found);
	return (found) ? result : GetStatDataCaseInsensitive(ConvertToLuminPath(NormalizedFilename, true), found);
}

bool FLuminPlatformFile::IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor)
{
	const FString DirectoryStr = Directory;
	const FString NormalizedDirectoryStr = NormalizeFilename(Directory);

	return IterateDirectoryCommon(Directory, [&](struct dirent* InEntry) -> bool
	{
		const FString UnicodeEntryName = UTF8_TO_TCHAR(InEntry->d_name);
				
		bool bIsDirectory = false;
		if (InEntry->d_type != DT_UNKNOWN)
		{
			bIsDirectory = InEntry->d_type == DT_DIR;
		}
		else
		{
			// filesystem does not support d_type, fallback to stat
			struct stat FileInfo;
			const FString AbsoluteUnicodeName = NormalizedDirectoryStr / UnicodeEntryName;	
			if (stat(TCHAR_TO_UTF8(*AbsoluteUnicodeName), &FileInfo) != -1)
			{
				bIsDirectory = ((FileInfo.st_mode & S_IFMT) == S_IFDIR);
			}
			else
			{
				int ErrNo = errno;
				UE_LOG(LogLuminPlatformFile, Warning, TEXT( "Cannot determine whether '%s' is a directory - d_type not supported and stat() failed with errno=%d (%s)"), *AbsoluteUnicodeName, ErrNo, UTF8_TO_TCHAR(strerror(ErrNo)));
			}
		}

		return Visitor.Visit(*(DirectoryStr / UnicodeEntryName), bIsDirectory);
	});
}

bool FLuminPlatformFile::IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor)
{
	const FString DirectoryStr = Directory;
	const FString NormalizedDirectoryStr = NormalizeFilename(Directory);

	return IterateDirectoryCommon(Directory, [&](struct dirent* InEntry) -> bool
	{
		const FString UnicodeEntryName = UTF8_TO_TCHAR(InEntry->d_name);
				
		struct stat FileInfo;
		const FString AbsoluteUnicodeName = NormalizedDirectoryStr / UnicodeEntryName;	
		// Check the read path first.
		if (stat(TCHAR_TO_UTF8(*ConvertToLuminPath(AbsoluteUnicodeName, false)), &FileInfo) != -1)
		{
			return Visitor.Visit(*(DirectoryStr / UnicodeEntryName), UnixStatToUEFileData(FileInfo));
		}
		// If it doesnt exist, check for the write path instead.
		else if(stat(TCHAR_TO_UTF8(*ConvertToLuminPath(AbsoluteUnicodeName, true)), &FileInfo) != -1)
		{
			return Visitor.Visit(*(DirectoryStr / UnicodeEntryName), UnixStatToUEFileData(FileInfo));
		}

		return true;
	});
}

bool FLuminPlatformFile::IterateDirectoryCommon(const TCHAR* Directory, const TFunctionRef<bool(struct dirent*)>& Visitor)
{
	bool Result = false;

	FString NormalizedDirectory = NormalizeFilename(Directory);
	// Check the read path first.
	DIR* Handle = opendir(TCHAR_TO_UTF8(*ConvertToLuminPath(NormalizedDirectory, false)));
	if (!Handle)
	{
		// If it doesnt exist, check for the write path instead.
		Handle = opendir(TCHAR_TO_UTF8(*ConvertToLuminPath(NormalizedDirectory, true)));
	}
	if (Handle)
	{
		Result = true;
		struct dirent* Entry;
		while ((Entry = readdir(Handle)) != NULL)
		{
			if (FCString::Strcmp(UTF8_TO_TCHAR(Entry->d_name), TEXT(".")) && FCString::Strcmp(UTF8_TO_TCHAR(Entry->d_name), TEXT("..")))
			{
				Result = Visitor(Entry);
			}
		}
		closedir(Handle);
	}
	return Result;
}

bool FLuminPlatformFile::CreateDirectoriesFromPath(const TCHAR* Path)
{
	// if the file already exists, then directories exist.
	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(*NormalizeFilename(Path)), &FileInfo) != -1)
	{
		return true;
	}

	// convert path to native char string.
	int32 Len = strlen(TCHAR_TO_UTF8(*NormalizeFilename(Path)));
	char *DirPath = reinterpret_cast<char *>(FMemory::Malloc((Len+2) * sizeof(char)));
	char *SubPath = reinterpret_cast<char *>(FMemory::Malloc((Len+2) * sizeof(char)));
	strcpy(DirPath, TCHAR_TO_UTF8(*NormalizeFilename(Path)));

	for (int32 i=0; i<Len; ++i)
	{
		SubPath[i] = DirPath[i];

		if (SubPath[i] == '/')
		{
			SubPath[i+1] = 0;

			// directory exists?
			struct stat SubPathFileInfo;
			if (stat(SubPath, &SubPathFileInfo) == -1)
			{
				// nope. create it.
				if (mkdir(SubPath, 0755) == -1)
				{
					int ErrNo = errno;
					UE_LOG(LogLuminPlatformFile, Warning, TEXT("create dir('%s') failed: errno=%d (%s)"),
						UTF8_TO_TCHAR(DirPath), ErrNo, ANSI_TO_TCHAR(strerror(ErrNo)));
					FMemory::Free(DirPath);
					FMemory::Free(SubPath);
					return false;
				}
			}
		}
	}

	FMemory::Free(DirPath);
	FMemory::Free(SubPath);
	return true;
}

FString FLuminPlatformFile::ConvertToLuminPath(const FString& Filename, bool bForWrite) const
{
	if (!IsSandboxEnabled())
	{
		return Filename;
	}
	FString Result = Filename;
	Result.ReplaceInline(TEXT("../"), TEXT(""));
	Result.ReplaceInline(TEXT(".."), TEXT(""));
	
	// Remove the base app path if present, we will prepend it the correct base path as needed.
	Result.ReplaceInline(*FLuminPlatformMisc::GetApplicationPackageDirectoryPath(), TEXT(""));
	// Remove the writable path if present, we will prepend it the correct base path as needed.
	Result.ReplaceInline(*FLuminPlatformMisc::GetApplicationWritableDirectoryPath(), TEXT(""));

	// If the file is for writing, then add it to the app writable directory path.
	if (bForWrite)
	{
		FString lhs = FLuminPlatformMisc::GetApplicationWritableDirectoryPath();
		FString rhs = Result;
		lhs.RemoveFromEnd(TEXT("/"));
		rhs.RemoveFromStart(TEXT("/"));
		Result = lhs / rhs;
	}
	else
	{
		// If filehostip exists in the command line, cook on the fly read path should be used.
		FString Value;
		// Cache this value as the command line doesn't change.
		static bool bHasHostIP = FParse::Value(FCommandLine::Get(), TEXT("filehostip"), Value) || FParse::Value(FCommandLine::Get(), TEXT("streaminghostip"), Value);
		// @todo Lumin support iterative deployment!! See LuminPlatform.Automation.cs
		static bool bIsIterative = FParse::Value(FCommandLine::Get(), TEXT("iterative"), Value);

		if (bHasHostIP)
		{
			FString lhs = FLuminPlatformMisc::GetApplicationWritableDirectoryPath();
			FString rhs = Result;
			lhs.RemoveFromEnd(TEXT("/"));
			rhs.RemoveFromStart(TEXT("/"));
			Result = lhs / rhs;
		}
		else if (bIsIterative)
		{
			FString lhs = FLuminPlatformMisc::GetApplicationWritableDirectoryPath();
			FString rhs = Result;
			lhs.RemoveFromEnd(TEXT("/"));
			rhs.RemoveFromStart(TEXT("/"));
			Result = lhs / rhs;
		}
		else
		{
//			Result = Filename;
			// @todo Lumin: Don't know why the Lumin code didn't have this
			FString lhs = FLuminPlatformMisc::GetApplicationPackageDirectoryPath();
			FString rhs = Result;
			lhs.RemoveFromEnd(TEXT("/"));
			rhs.RemoveFromStart(TEXT("/"));
			Result = lhs / rhs;

		}
	}

#if 0
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("LOG_LUMIN_PATH Write = %d Input = %s Output = %s"), bForWrite, *Filename, *Result.ToLower());
#endif

	// always use lower case ... always
	return Result.ToLower();
}

bool FLuminPlatformFile::FileExistsCaseInsensitive(const FString& NormalizedFilename) const
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizedFilename, CaseSensitiveFilename))
	{
		// could not find the file
		return false;
	}

	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) != -1)
	{
		return S_ISREG(FileInfo.st_mode);
	}

	return false;
}

int64 FLuminPlatformFile::FileSizeCaseInsensitive(const FString& NormalizedFilename) const
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizedFilename, CaseSensitiveFilename))
	{
		// could not find the file
		return -1;
	}

	struct stat FileInfo;
	FileInfo.st_size = -1;
	if (stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) != -1)
	{
		// make sure to return -1 for directories
		if (S_ISDIR(FileInfo.st_mode))
		{
			FileInfo.st_size = -1;
		}
	}
	return FileInfo.st_size;
}

bool FLuminPlatformFile::IsReadOnlyCaseInsensitive(const FString& NormalizedFilename) const
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizedFilename, CaseSensitiveFilename))
	{
		// could not find the file
		return false;
	}

	// skipping checking F_OK since this is already taken care of by case mapper

	if (access(TCHAR_TO_UTF8(*CaseSensitiveFilename), W_OK) == -1)
	{
		return errno == EACCES;
	}
	return false;
}

FDateTime FLuminPlatformFile::GetTimeStampCaseInsensitive(const FString& NormalizedFilename) const
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizedFilename, CaseSensitiveFilename))
	{
		// could not find the file
		return FDateTime::MinValue();
	}

	// get file times
	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) == -1)
	{
		if (errno == EOVERFLOW)
		{
			// hacky workaround for files mounted on Samba (see https://bugzilla.samba.org/show_bug.cgi?id=7707)
			return FDateTime::Now();
		}
		else
		{
			return FDateTime::MinValue();
		}
	}

	// convert _stat time to FDateTime
	FTimespan TimeSinceEpoch(0, 0, FileInfo.st_mtime);
	return UnixEpoch + TimeSinceEpoch;
}

FDateTime FLuminPlatformFile::GetAccessTimeStampCaseInsensitive(const FString& NormalizedFilename) const
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizedFilename, CaseSensitiveFilename))
	{
		// could not find the file
		return FDateTime::MinValue();
	}

	// get file times
	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) == -1)
	{
		return FDateTime::MinValue();
	}

	// convert _stat time to FDateTime
	FTimespan TimeSinceEpoch(0, 0, FileInfo.st_atime);
	return UnixEpoch + TimeSinceEpoch;
}

FFileStatData FLuminPlatformFile::GetStatDataCaseInsensitive(const FString& NormalizedFilename, bool& bFound) const
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizedFilename, CaseSensitiveFilename))
	{
		// could not find the file
		bFound = false;
		return FFileStatData();
	}

	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) == -1)
	{
		bFound = false;
		return FFileStatData();
	}

	bFound = true;
	return UnixStatToUEFileData(FileInfo);
}

bool FLuminPlatformFile::DirectoryExistsCaseInsensitive(const FString& NormalizedFilename) const
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizedFilename, CaseSensitiveFilename))
	{
		// could not find the file
		return false;
	}

	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) != -1)
	{
		return S_ISDIR(FileInfo.st_mode);
	}
	return false;
}

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FLuminPlatformFile Singleton;
	return Singleton;
}

