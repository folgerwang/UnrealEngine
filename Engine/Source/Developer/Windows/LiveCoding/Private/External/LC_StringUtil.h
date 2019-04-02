// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include <string>

namespace string
{
	std::wstring ToWideString(const char* utf8Str);
	std::wstring ToWideString(const char* utf8Str, size_t count);
	std::wstring ToWideString(const std::string& str);

	std::string Replace(const std::string& str, const std::string& from, const std::string& to);
	std::wstring Replace(const std::wstring& str, const std::wstring& from, const std::wstring& to);

	std::wstring ReplaceAll(const std::wstring& str, const std::wstring& from, const std::wstring& to);
	std::wstring EraseAll(const std::wstring& str, const std::wstring& subString);

	char* Find(char* str, const char* subString);
	wchar_t* Find(wchar_t* str, const wchar_t* subString);

	const char* Find(const char* str, const char* subString);
	const wchar_t* Find(const wchar_t* str, const wchar_t* subString);

	bool Matches(const char* str1, const char* str2);
	bool Matches(const wchar_t* str1, const wchar_t* str2);

	bool Contains(const char* str, const char* subString);
	bool Contains(const wchar_t* str, const wchar_t* subString);

	bool StartsWith(const char* str, const char* subString);
	bool StartsWith(const wchar_t* str, const wchar_t* subString);

	std::string ToUpper(const char* str);
	std::string ToUpper(const std::string& str);
	std::wstring ToUpper(const wchar_t* str);
	std::wstring ToUpper(const std::wstring& str);

	std::wstring ToLower(const wchar_t* str);
	std::wstring ToLower(const std::wstring& str);

	// Turns invalid characters (\ / : * ? " < > | : ; , .) in file names, names for OS objects, etc. into underscores
	std::wstring MakeSafeName(const std::wstring& name);
}
