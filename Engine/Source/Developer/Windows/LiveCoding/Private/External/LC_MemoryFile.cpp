// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_MemoryFile.h"
#include "LC_Logging.h"
#include "Windows/WindowsHWrapper.h"


namespace detail
{
	static inline DWORD GetDesiredAccess(file::OpenMode::Enum openMode)
	{
		switch (openMode)
		{
			case file::OpenMode::READ_ONLY:
				return GENERIC_READ;

			case file::OpenMode::READ_AND_WRITE:
				return GENERIC_READ | GENERIC_WRITE;

			default:
				return 0u;
		}
	}


	static inline DWORD GetShareMode(file::OpenMode::Enum openMode)
	{
		switch (openMode)
		{
			case file::OpenMode::READ_ONLY:
				return FILE_SHARE_READ;

			case file::OpenMode::READ_AND_WRITE:
				return FILE_SHARE_READ | FILE_SHARE_WRITE;

			default:
				return 0u;
		}
	}


	static inline DWORD GetFileMappingPageProtection(file::OpenMode::Enum openMode)
	{
		switch (openMode)
		{
			case file::OpenMode::READ_ONLY:
				return PAGE_READONLY;

			case file::OpenMode::READ_AND_WRITE:
				return PAGE_READWRITE;

			default:
				return 0u;
		}
	}


	static inline DWORD GetFileMappingDesiredAccess(file::OpenMode::Enum openMode)
	{
		switch (openMode)
		{
			case file::OpenMode::READ_ONLY:
				return FILE_MAP_READ;

			case file::OpenMode::READ_AND_WRITE:
				return FILE_MAP_READ | FILE_MAP_WRITE;

			default:
				return 0u;
		}
	}
}


file::MemoryFile* file::Open(const wchar_t* path, OpenMode::Enum openMode)
{
	HANDLE file = ::CreateFileW(path, detail::GetDesiredAccess(openMode), detail::GetShareMode(openMode), NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		LC_ERROR_USER("Cannot open file %S. Error: 0x%X", path, ::GetLastError());
		return nullptr;
	}

	// create memory-mapped file
	HANDLE mappedFile = ::CreateFileMappingW(file, NULL, detail::GetFileMappingPageProtection(openMode), 0, 0, NULL);
	if (mappedFile == NULL)
	{
		LC_ERROR_USER("Cannot create mapped file %S. Error: 0x%X", path, ::GetLastError());
		::CloseHandle(file);
		return nullptr;
	}

	void* base = ::MapViewOfFile(mappedFile, detail::GetFileMappingDesiredAccess(openMode), 0, 0, 0);
	if (base == NULL)
	{
		LC_ERROR_USER("Cannot map file %S. Error: 0x%X", path, ::GetLastError());
		::CloseHandle(mappedFile);
		::CloseHandle(file);
		return nullptr;
	}

	MemoryFile* memoryFile = new MemoryFile;
	memoryFile->file = file;
	memoryFile->memoryMappedFile = mappedFile;
	memoryFile->base = base;

	return memoryFile;
}


const void* file::GetData(const MemoryFile* file)
{
	return file->base;
}


void file::Close(MemoryFile*& memoryFile)
{
	::UnmapViewOfFile(memoryFile->base);
	::CloseHandle(memoryFile->memoryMappedFile);
	::CloseHandle(memoryFile->file);

	delete memoryFile;
	memoryFile = nullptr;
}
