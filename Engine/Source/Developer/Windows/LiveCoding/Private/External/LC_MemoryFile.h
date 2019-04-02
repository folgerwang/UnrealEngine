// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Platform.h"
#include "Windows/MinimalWindowsApi.h"

namespace file
{
	struct MemoryFile
	{
		Windows::HANDLE file;
		Windows::HANDLE memoryMappedFile;
		void* base;

		LC_DISABLE_ASSIGNMENT(MemoryFile);
	};

	struct OpenMode
	{
		enum Enum
		{
			READ_ONLY = 0,
			READ_AND_WRITE = 1
		};
	};

	MemoryFile* Open(const wchar_t* path, OpenMode::Enum openMode);
	const void* GetData(const MemoryFile* file);
	void Close(MemoryFile*& file);
}
