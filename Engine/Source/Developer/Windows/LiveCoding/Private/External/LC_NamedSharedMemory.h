// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "Windows/MinimalWindowsApi.h"

class NamedSharedMemory
{
public:
	explicit NamedSharedMemory(const wchar_t* name);
	~NamedSharedMemory(void);

	bool IsOwnedByCallingProcess(void) const;

	void Read(void* buffer, size_t size);
	void Write(const void* buffer, size_t size);

	template <typename T>
	T Read(void)
	{
		T value = {};
		Read(&value, sizeof(T));
		return value;
	}

	template <typename T>
	void Write(const T& value)
	{
		Write(&value, sizeof(T));
	}

private:
	Windows::HANDLE m_file;
	void* m_view;
	bool m_isOwned;
};
