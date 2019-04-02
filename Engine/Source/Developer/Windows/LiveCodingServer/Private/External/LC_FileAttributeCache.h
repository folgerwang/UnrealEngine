// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Types.h"
#include "xxhash.h"


class FileAttributeCache
{
	struct Hasher
	{
		inline size_t operator()(const std::wstring& key) const
		{
			return XXH32(key.c_str(), key.length() * sizeof(wchar_t), 0u);
		}
	};

public:
	struct Data
	{
		uint64_t lastModificationTime;
		bool exists;
	};

	FileAttributeCache(void);
	Data UpdateCacheData(const std::wstring& path);

	size_t GetEntryCount(void) const;

private:
	typedef types::unordered_map_with_hash<std::wstring, Data, Hasher> Cache;
	Cache m_data;
};
