// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_FileUtil.h"
#include "LC_CriticalSection.h"
#include "LC_Logging.h"
#include "xxhash.h"
#include <stack>
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")


namespace detail
{
	class NormalizedFilenameCache
	{
		struct Hasher
		{
			inline size_t operator()(const std::wstring& key) const
			{
				return XXH32(key.c_str(), key.length() * sizeof(wchar_t), 0u);
			}
		};

	public:
		NormalizedFilenameCache(void)
			: m_data()
			, m_cs()
		{
			// make space for 128k entries
			m_data.reserve(128u * 1024u);
		}

		std::wstring UpdateCacheData(const wchar_t* path)
		{
			CriticalSection::ScopedLock lock(&m_cs);

			// try to insert the element into the cache. if it exists, return the cached data.
			// if it doesn't exist, get the file name once and store it.
			const std::pair<Cache::iterator, bool>& optional = m_data.emplace(std::wstring(path), std::wstring());

			std::wstring& data = optional.first->second;
			if (optional.second)
			{
				// value was inserted, update it with the correct data
				HANDLE file = ::CreateFileW(path, FILE_READ_ATTRIBUTES | STANDARD_RIGHTS_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
				if (file != INVALID_HANDLE_VALUE)
				{
					wchar_t buffer[MAX_PATH] = {};
					::GetFinalPathNameByHandleW(file, buffer, MAX_PATH, 0u);
					::CloseHandle(file);

					// the path returned by GetFinalPathNameByHandle starts with "\\?\", cut that off
					data.assign(buffer + 4u);
				}
				else
				{
					data.assign(path);
				}
			}

			return data;
		}

	private:
		typedef types::unordered_map_with_hash<std::wstring, std::wstring, Hasher> Cache;
		Cache m_data;
		CriticalSection m_cs;
	};
}


namespace
{
	static detail::NormalizedFilenameCache* g_normalizedFilenameCache = nullptr;
}


void file::Startup(void)
{
	g_normalizedFilenameCache = new detail::NormalizedFilenameCache;
}


void file::Shutdown(void)
{
	delete g_normalizedFilenameCache;
}


file::Attributes file::GetAttributes(const wchar_t* path)
{
	const WIN32_FILE_ATTRIBUTE_DATA initialData = { INVALID_FILE_ATTRIBUTES };
	Attributes attributes = { initialData };
	::GetFileAttributesExW(path, GetFileExInfoStandard, &attributes.data);

	return attributes;
}


uint64_t file::GetLastModificationTime(const Attributes& attributes)
{
	::ULARGE_INTEGER integer = {};
	integer.LowPart = attributes.data.ftLastWriteTime.dwLowDateTime;
	integer.HighPart = attributes.data.ftLastWriteTime.dwHighDateTime;

	return static_cast<uint64_t>(integer.QuadPart);
}


bool file::DoesExist(const Attributes& attributes)
{
	return (attributes.data.dwFileAttributes != INVALID_FILE_ATTRIBUTES);
}


bool file::IsDirectory(const Attributes& attributes)
{
	if (!DoesExist(attributes))
	{
		return false;
	}

	return (attributes.data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}


uint64_t file::GetSize(const Attributes& attributes)
{
	::ULARGE_INTEGER integer = {};
	integer.LowPart = attributes.data.nFileSizeLow;
	integer.HighPart = attributes.data.nFileSizeHigh;

	return static_cast<uint64_t>(integer.QuadPart);
}


void file::Copy(const wchar_t* srcPath, const wchar_t* destPath)
{
	const BOOL success = ::CopyFileW(srcPath, destPath, Windows::FALSE);
	if (success == Windows::FALSE)
	{
		LC_ERROR_USER("Failed to copy file from %S to %S. Error: 0x%X", srcPath, destPath, ::GetLastError());
	}
}


void file::Delete(const wchar_t* path)
{
	const BOOL success = ::DeleteFileW(path);
	if (success == Windows::FALSE)
	{
		LC_ERROR_USER("Failed to delete file %S. Error: 0x%X", path, ::GetLastError());
	}
}


bool file::DeleteIfExists(const wchar_t* path)
{
	const BOOL success = ::DeleteFileW(path);
	return (success != Windows::FALSE);
}


bool file::IsRelativePath(const wchar_t* path)
{
	// empty paths are not considered to be relative
	if (path[0] == L'\0')
	{
		return false;
	}

	return (::PathIsRelativeW(path) == Windows::TRUE);
}


std::wstring file::CreateTempFile(void)
{
	wchar_t path[MAX_PATH] = {};
	const DWORD pathLength = ::GetTempPathW(MAX_PATH, path);

	wchar_t filename[MAX_PATH] = {};
	wchar_t prefix[1] = { '\0' };
	::GetTempFileNameW(path, prefix, 0u, filename);

	return std::wstring(filename);
}


bool file::CreateFileWithData(const wchar_t* path, const void* data, size_t size)
{
	HANDLE file = ::CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		LC_ERROR_USER("Cannot open file %S for writing. Error: 0x%X", path, ::GetLastError());
		return false;
	}

	DWORD bytesWritten = 0u;
	::WriteFile(file, data, static_cast<DWORD>(size), &bytesWritten, NULL);
	::CloseHandle(file);

	return true;
}


std::wstring file::GetDirectory(const std::wstring& path)
{
	const size_t lastDelimiter = path.find_last_of('\\');
	if (lastDelimiter != std::wstring::npos)
	{
		const std::wstring dir(path.c_str(), path.c_str() + lastDelimiter);
		return dir;
	}

	return path;
}


std::wstring file::GetFilename(const std::wstring& path)
{
	const size_t lastDelimiter = path.find_last_of('\\');
	if (lastDelimiter != std::wstring::npos)
	{
		const std::wstring filename(path.c_str() + lastDelimiter + 1u, path.c_str() + path.length());
		return filename;
	}

	return path;
}


std::wstring file::GetExtension(const std::wstring& path)
{
	const size_t extensionDot = path.find_last_of('.');
	if (extensionDot == std::wstring::npos)
	{
		return std::wstring(L"");
	}

	const std::wstring filename(path.c_str() + extensionDot, path.c_str() + path.length());
	return filename;
}


std::wstring file::RemoveExtension(const std::wstring& path)
{
	const size_t extensionDot = path.find_last_of('.');
	if (extensionDot == std::wstring::npos)
	{
		return path;
	}

	const std::wstring filename(path.c_str(), path.c_str() + extensionDot);
	return filename;
}


std::wstring file::NormalizePath(const wchar_t* path)
{
	// normalizing files is really costly on Windows, so we cache results
	return g_normalizedFilenameCache->UpdateCacheData(path);
}


std::wstring file::NormalizePathWithoutLinks(const wchar_t* path)
{
	// use the old trick of converting to short and to long path names to get a path with correct casing
	wchar_t shortPath[MAX_PATH] = {};
	{
		const DWORD charsWritten = ::GetShortPathNameW(path, shortPath, MAX_PATH);
		if (charsWritten == 0u)
		{
			return path;
		}
	}

	wchar_t longPath[MAX_PATH] = {};
	{
		const DWORD charsWritten = ::GetLongPathNameW(shortPath, longPath, MAX_PATH);
		if (charsWritten == 0u)
		{
			return path;
		}
	}

	return longPath;
}


std::wstring file::RelativeToAbsolutePath(const wchar_t* path)
{
	wchar_t* absolutePath = _wfullpath(NULL, path, MAX_PATH);
	if (absolutePath)
	{
		std::wstring result(absolutePath);
		free(absolutePath);

		return result;
	}

	return std::wstring(path);
}


void file::Move(const wchar_t* currentPath, const wchar_t* movedToPath)
{
	const BOOL success = ::MoveFileExW(currentPath, movedToPath, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	if (success == Windows::FALSE)
	{
		LC_ERROR_USER("Failed to move file from %S to %S. Error: 0x%X", currentPath, movedToPath, ::GetLastError());
	}
}


types::vector<std::wstring> file::EnumerateFiles(const wchar_t* directory)
{
	types::vector<std::wstring> files;
	files.reserve(1024u);

	HANDLE findHandle = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATAW findData = {};

	std::wstring path(directory);
	std::wstring searchTerm;
	searchTerm.reserve(MAX_PATH);

	// recursively walk through directories, enumerating all files from a directory first,
	// pushing found directories onto a stack, walking those directories until no more files
	// can be found.
	std::stack<std::wstring> directories;
	directories.push(path);

	while (!directories.empty())
	{
		path = directories.top();
		directories.pop();

		searchTerm = path;
		searchTerm += L"\\*.*";

		findHandle = ::FindFirstFileW(searchTerm.c_str(), &findData);
		if (findHandle == INVALID_HANDLE_VALUE)
		{
			return files;
		}

		do
		{
			if (wcscmp(findData.cFileName, L".") != 0 &&
				wcscmp(findData.cFileName, L"..") != 0)
			{
				std::wstring newPath;
				newPath.reserve(MAX_PATH);

				newPath = path;
				newPath += L"\\";
				newPath += findData.cFileName;

				if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					directories.push(newPath);
				}
				else
				{
					files.push_back(newPath);
				}
			}
		}
		while (::FindNextFile(findHandle, &findData) != 0);

		if (GetLastError() != ERROR_NO_MORE_FILES)
		{
			::FindClose(findHandle);
			return files;
		}

		::FindClose(findHandle);
		findHandle = INVALID_HANDLE_VALUE;
	}

	return files;
}
