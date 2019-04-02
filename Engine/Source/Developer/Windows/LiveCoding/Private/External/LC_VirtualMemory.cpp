// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_VirtualMemory.h"


namespace
{
	static uint32_t GetAllocationGranularity(void)
	{
		SYSTEM_INFO info = {};
		::GetSystemInfo(&info);
		return info.dwAllocationGranularity;
	}


	static uint32_t GetPageSize(void)
	{
		SYSTEM_INFO info = {};
		::GetSystemInfo(&info);
		return info.dwPageSize;
	}
}


void* virtualMemory::Allocate(size_t size)
{
	return ::VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}


void virtualMemory::Free(void* ptr)
{
	::VirtualFree(ptr, 0u, MEM_RELEASE);
}


void* virtualMemory::Allocate(process::Handle handle, size_t size, PageType::Enum pageType)
{
	return ::VirtualAllocEx(handle, nullptr, size, MEM_COMMIT | MEM_RESERVE, pageType);
}


void virtualMemory::Free(process::Handle handle, void* ptr)
{
	::VirtualFreeEx(handle, ptr, 0u, MEM_RELEASE);
}


uint32_t virtualMemory::GetAllocationGranularity(void)
{
	static const uint32_t allocationGranularity = ::GetAllocationGranularity();
	return allocationGranularity;
}


uint32_t virtualMemory::GetPageSize(void)
{
	static const uint32_t pageSize = ::GetPageSize();
	return pageSize;
}
