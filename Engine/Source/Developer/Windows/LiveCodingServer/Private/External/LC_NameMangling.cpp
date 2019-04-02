// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_NameMangling.h"
#include "LC_StringUtil.h"


typedef void* (*malloc_func_t)(size_t);
typedef void (*free_func_t)(void*);

// undocumented function found in MSVC CRT
extern "C" char* __cdecl __unDName(char* buffer, const char* mangled, int buflen, malloc_func_t memget, free_func_t memfree, unsigned short flags);


std::string nameMangling::UndecorateSymbol(const char* symbolName, uint32_t flags)
{
	char buffer[65536];
	__unDName(buffer, symbolName, 65536, malloc, free, static_cast<uint16_t>(flags));

	return std::string(buffer);
}


std::wstring nameMangling::UndecorateSymbolWide(const char* symbolName, uint32_t flags)
{
	char buffer[65536];
	__unDName(buffer, symbolName, 65536, malloc, free, static_cast<uint16_t>(flags));

	return string::ToWideString(buffer);
}
