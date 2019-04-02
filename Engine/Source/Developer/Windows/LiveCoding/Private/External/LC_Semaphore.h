// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "Windows/MinimalWindowsApi.h"

class Semaphore
{
public:
	Semaphore(unsigned int initialValue, unsigned int maximumValue);
	~Semaphore(void);

	void Signal(void);
	void Wait(void);
	bool TryWait(void);

private:
	Windows::HANDLE m_sema;
};
