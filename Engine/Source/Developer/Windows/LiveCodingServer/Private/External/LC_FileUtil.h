// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Types.h"
#include "Windows/WindowsHWrapper.h"

namespace file
{
	struct Attributes
	{
		WIN32_FILE_ATTRIBUTE_DATA data;
	};

	void Startup(void);
	void Shutdown(void);


	Attributes GetAttributes(const wchar_t* path);
	uint64_t GetLastModificationTime(const Attributes& attributes);
	bool DoesExist(const Attributes& attributes);
	bool IsDirectory(const Attributes& attributes);
	uint64_t GetSize(const Attributes& attributes);

	void Copy(const wchar_t* srcPath, const wchar_t* destPath);
	void Delete(const wchar_t* path);
	bool DeleteIfExists(const wchar_t* path);

	bool IsRelativePath(const wchar_t* path);

	// creates a unique, temporary absolute filename, e.g. C:\Users\JohnDoe\AppData\Local\Temp\ABCD.tmp
	std::wstring CreateTempFile(void);

	// creates a file, storing the given data
	bool CreateFileWithData(const wchar_t* path, const void* data, size_t size);

	// returns the directory-only part of a given path
	std::wstring GetDirectory(const std::wstring& path);

	// returns the file-only part of a given path
	std::wstring GetFilename(const std::wstring& path);

	// returns the extension-only part of a given path, e.g. '.bat', '.exe'
	std::wstring GetExtension(const std::wstring& path);

	// returns the given path without any file extensions
	std::wstring RemoveExtension(const std::wstring& path);

	// canonicalizes/normalizes any given path
	std::wstring NormalizePath(const wchar_t* path);

	// canonicalizes/normalizes any given path without resolving any symbolic links/virtual drives.
	// note: this is not cached internally and should only be used for cosmetic purposes.
	std::wstring NormalizePathWithoutLinks(const wchar_t* path);

	// converts a relative into an absolute path
	std::wstring RelativeToAbsolutePath(const wchar_t* path);

	// moves a file
	void Move(const wchar_t* currentPath, const wchar_t* movedToPath);


	// recursively enumerates all files in a directory
	types::vector<std::wstring> EnumerateFiles(const wchar_t* directory);
}
