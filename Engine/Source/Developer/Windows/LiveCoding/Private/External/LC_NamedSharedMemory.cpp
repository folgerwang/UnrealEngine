// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_NamedSharedMemory.h"
#include "LC_Logging.h"
#include "Windows/WindowsHWrapper.h"


namespace
{
	static const DWORD MEMORY_SIZE = 4096u;
}


NamedSharedMemory::NamedSharedMemory(const wchar_t* name)
	: m_file(nullptr)
	, m_view(nullptr)
	, m_isOwned(true)
{
	m_file = ::CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, MEMORY_SIZE, name);
	const DWORD error = ::GetLastError();
	if (m_file == NULL)
	{
		LC_ERROR_USER("Cannot create named shared memory. Error: 0x%X", error);
	}
	else if (error == ERROR_ALREADY_EXISTS)
	{
		// another process already created this file mapping. the handle points to the already existing object.
		m_isOwned = false;
	}

	if (m_file)
	{
		m_view = ::MapViewOfFile(m_file, FILE_MAP_ALL_ACCESS, 0, 0, MEMORY_SIZE);
		if (m_view == NULL)
		{
			LC_ERROR_USER("Cannot map view for named shared memory. Error: 0x%X", ::GetLastError());

			::CloseHandle(m_file);
			m_file = nullptr;
		}
	}
}


NamedSharedMemory::~NamedSharedMemory(void)
{
	::UnmapViewOfFile(m_view);
	::CloseHandle(m_file);
}


bool NamedSharedMemory::IsOwnedByCallingProcess(void) const
{
	return m_isOwned;
}


void NamedSharedMemory::Read(void* buffer, size_t size)
{
	if (m_view)
	{
		memcpy(buffer, m_view, size);
	}
}


void NamedSharedMemory::Write(const void* buffer, size_t size)
{
	if (m_view)
	{
		memcpy(m_view, buffer, size);
	}
}
