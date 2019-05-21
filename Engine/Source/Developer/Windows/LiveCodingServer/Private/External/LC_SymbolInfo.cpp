// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_SymbolInfo.h"
#include <stdio.h>


SymbolInfo::SymbolInfo(const char* const function, const char* const filename, unsigned int line)
	: m_function()
	, m_filename()
	, m_line(line)
{
	strcpy_s(m_function, function);
	strcpy_s(m_filename, filename);
}
