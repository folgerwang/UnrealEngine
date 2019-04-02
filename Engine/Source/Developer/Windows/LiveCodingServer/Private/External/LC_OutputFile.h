// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "Windows/MinimalWindowsAPI.h"

class OutputFile
{
public:
	explicit OutputFile(const wchar_t* logFilePath);
	~OutputFile(void);

	void Log(const char* msg);
	void Log(const char* msg, int type);

private:
	void WriteToFile(const char* text);

	Windows::HANDLE m_logFile;
};
