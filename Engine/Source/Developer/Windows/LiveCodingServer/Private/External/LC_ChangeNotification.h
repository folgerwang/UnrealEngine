// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "Windows/MinimalWindowsAPI.h"

class ChangeNotification
{
public:
	ChangeNotification(void);
	~ChangeNotification(void);

	void Create(const wchar_t* path);
	void Destroy(void);

	bool Check(unsigned int timeoutMs);
	bool CheckOnce(void);
	void CheckNext(unsigned int timeoutMs);

private:
	bool WaitForNotification(unsigned int timeoutMs);

	Windows::HANDLE m_handle;
};
