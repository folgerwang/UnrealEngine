// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Types.h"
#include "LC_ChangeNotification.h"


class DirectoryCache
{
public:
	struct Directory
	{
		ChangeNotification changeNotification;
		bool hadChange;
	};

	explicit DirectoryCache(size_t expectedDirectoryCount);
	~DirectoryCache(void);

	Directory* AddDirectory(const std::wstring& directory);
	void PrimeNotifications(void);
	void RestartNotifications(void);

private:
	types::unordered_map<std::wstring, Directory*> m_directories;
};
