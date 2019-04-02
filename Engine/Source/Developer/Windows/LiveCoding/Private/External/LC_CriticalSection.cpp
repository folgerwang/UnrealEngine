// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_CriticalSection.h"


CriticalSection::CriticalSection(void)
{
	Windows::InitializeCriticalSection(&m_cs);
}


CriticalSection::~CriticalSection(void)
{
	Windows::DeleteCriticalSection(&m_cs);
}


void CriticalSection::Enter(void)
{
	Windows::EnterCriticalSection(&m_cs);
}


void CriticalSection::Leave(void)
{
	Windows::LeaveCriticalSection(&m_cs);
}
