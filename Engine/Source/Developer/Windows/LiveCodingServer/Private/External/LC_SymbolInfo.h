// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Platform.h"

class SymbolInfo
{
public:
	SymbolInfo(const char* const function, const char* const filename, unsigned int line);

	inline const char* GetFunction(void) const
	{
		return m_function;
	}

	inline const char* GetFilename(void) const
	{
		return m_filename;
	}

	inline unsigned int GetLine(void) const
	{
		return m_line;
	}

private:
	LC_DISABLE_ASSIGNMENT(SymbolInfo);

	char m_function[512u];
	char m_filename[WINDOWS_MAX_PATH];
	unsigned int m_line;
};
