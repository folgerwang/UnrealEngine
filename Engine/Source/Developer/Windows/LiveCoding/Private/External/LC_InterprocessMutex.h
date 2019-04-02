// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "Windows/MinimalWindowsAPI.h"

class InterprocessMutex
{
public:
	explicit InterprocessMutex(const wchar_t* name);
	~InterprocessMutex(void);

	void Lock(void);
	void Unlock(void);

	class ScopedLock
	{
	public:
		explicit ScopedLock(InterprocessMutex* mutex)
			: m_mutex(mutex)
		{
			mutex->Lock();
		}

		~ScopedLock(void)
		{
			m_mutex->Unlock();
		}

	private:
		InterprocessMutex* m_mutex;
	};

private:
	Windows::HANDLE m_mutex;
};
