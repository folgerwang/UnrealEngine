// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Process.h"


namespace virtualMemory
{
	struct PageType
	{
		enum Enum
		{
			READ_WRITE = PAGE_READWRITE,
			EXECUTE_READ_WRITE = PAGE_EXECUTE_READWRITE
		};
	};

	void* Allocate(size_t size);
	void Free(void* ptr);

	void* Allocate(process::Handle handle, size_t size, PageType::Enum pageType);
	void Free(process::Handle handle, void* ptr);

	uint32_t GetAllocationGranularity(void);
	uint32_t GetPageSize(void);
}
