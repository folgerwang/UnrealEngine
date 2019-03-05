// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "Windows/MinimalWindowsApi.h"

class CriticalSection
{
public:
	CriticalSection(void);
	~CriticalSection(void);

	void Enter(void);
	void Leave(void);

	class ScopedLock
	{
	public:
		explicit ScopedLock(CriticalSection* cs)
			: m_cs(cs)
		{
			cs->Enter();
		}

		~ScopedLock(void)
		{
			m_cs->Leave();
		}

	private:
		CriticalSection* m_cs;
	};

private:
	Windows::CRITICAL_SECTION m_cs;
};
