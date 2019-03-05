// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_StringUtil.h"
#include "Windows/WindowsHWrapper.h"


namespace detail
{
	std::wstring ToWideString(const char* utf8Str, size_t length)
	{
		const int sizeNeeded = ::MultiByteToWideChar(CP_UTF8, 0, utf8Str, static_cast<int>(length), NULL, 0);

		wchar_t* wstrTo = static_cast<wchar_t*>(_alloca(sizeNeeded * sizeof(wchar_t)));

		::MultiByteToWideChar(CP_UTF8, 0, utf8Str, static_cast<int>(length), wstrTo, sizeNeeded);
		return std::wstring(wstrTo, static_cast<size_t>(sizeNeeded));
	}


	template <typename T>
	static bool Matches(const T* str1, const T* str2)
	{
		unsigned int index = 0u;
		T c1 = str1[index];
		T c2 = str2[index];

		while ((c1 != '\0') && (c2 != '\0'))
		{
			if (c1 != c2)
			{
				// at least one character is different
				return false;
			}

			++index;
			c1 = str1[index];
			c2 = str2[index];
		}

		// reached the end of at least one string, but the string could be of different length
		return (c1 == c2);
	}


	template <typename T>
	static bool StartsWith(const T* str, const T* subString)
	{
		unsigned int index = 0u;
		T c1 = str[index];
		T c2 = subString[index];

		while ((c1 != '\0') && (c2 != '\0'))
		{
			if (c1 != c2)
			{
				// at least one character is different
				return false;
			}

			++index;
			c1 = str[index];
			c2 = subString[index];
		}

		// reached the end of at least one string.
		// if str has ended but subString has not, it cannot be fully contained in str.
		if ((c1 == '\0') && (c2 != '\0'))
		{
			return false;
		}

		return true;
	}
}


namespace string
{
	std::wstring ToWideString(const char* utf8Str)
	{
		return detail::ToWideString(utf8Str, strlen(utf8Str));
	}


	std::wstring ToWideString(const char* utf8Str, size_t count)
	{
		size_t length = 0u;
		while ((utf8Str[length] != '\0') && (length < count))
		{
			// find null-terminator
			++length;
		}

		return detail::ToWideString(utf8Str, length);
	}


	std::wstring ToWideString(const std::string& str)
	{
		return detail::ToWideString(str.c_str(), str.length());
	}


	std::wstring Replace(const std::wstring& str, const std::wstring& from, const std::wstring& to)
	{
		std::wstring result = str;

		size_t startPos = str.find(from);
		if (startPos == std::wstring::npos)
			return result;

		result.replace(startPos, from.length(), to);
		return result;
	}


	std::string Replace(const std::string& str, const std::string& from, const std::string& to)
	{
		std::string result = str;

		size_t startPos = str.find(from);
		if (startPos == std::string::npos)
			return result;

		result.replace(startPos, from.length(), to);
		return result;
	}


	std::wstring ReplaceAll(const std::wstring& str, const std::wstring& from, const std::wstring& to)
	{
		std::wstring result(str);

		for (;;)
		{
			const size_t pos = result.find(from);
			if (pos == std::wstring::npos)
			{
				return result;
			}

			result.replace(pos, from.length(), to);
		}
	}


	std::wstring EraseAll(const std::wstring& str, const std::wstring& subString)
	{
		const size_t subStringLength = subString.length();
		std::wstring result(str);

		for (;;)
		{
			const size_t pos = result.find(subString);
			if (pos == std::wstring::npos)
			{
				return result;
			}

			result.erase(pos, subStringLength);
		}
	}


	char* Find(char* str, const char* subString)
	{
		return strstr(str, subString);
	}


	wchar_t* Find(wchar_t* str, const wchar_t* subString)
	{
		return wcsstr(str, subString);
	}


	const char* Find(const char* str, const char* subString)
	{
		return strstr(str, subString);
	}


	const wchar_t* Find(const wchar_t* str, const wchar_t* subString)
	{
		return wcsstr(str, subString);
	}


	bool Matches(const char* str1, const char* str2)
	{
		return detail::Matches(str1, str2);
	}


	bool Matches(const wchar_t* str1, const wchar_t* str2)
	{
		return detail::Matches(str1, str2);
	}


	bool Contains(const char* str, const char* subString)
	{
		return Find(str, subString) != nullptr;
	}


	bool Contains(const wchar_t* str, const wchar_t* subString)
	{
		return Find(str, subString) != nullptr;
	}


	bool StartsWith(const char* str, const char* subString)
	{
		return detail::StartsWith(str, subString);
	}


	bool StartsWith(const wchar_t* str, const wchar_t* subString)
	{
		return detail::StartsWith(str, subString);
	}


	std::string ToUpper(const char* str)
	{
		std::string upperStr(str);

		char* dest = &upperStr[0];
		for (size_t i = 0u; i < upperStr.size(); ++i)
		{
			dest[i] = static_cast<char>(::toupper(dest[i]));
		}

		return upperStr;
	}


	std::string ToUpper(const std::string& str)
	{
		return ToUpper(str.c_str());
	}


	std::wstring ToUpper(const wchar_t* str)
	{
		std::wstring upperStr(str);

		wchar_t* dest = &upperStr[0];
		for (size_t i = 0u; i < upperStr.size(); ++i)
		{
			dest[i] = static_cast<wchar_t>(::towupper(dest[i]));
		}

		return upperStr;
	}


	std::wstring ToUpper(const std::wstring& str)
	{
		return ToUpper(str.c_str());
	}


	std::wstring ToLower(const wchar_t* str)
	{
		std::wstring lowerStr(str);

		wchar_t* dest = &lowerStr[0];
		for (size_t i = 0u; i < lowerStr.size(); ++i)
		{
			dest[i] = static_cast<wchar_t>(::towlower(dest[i]));
		}

		return lowerStr;
	}


	std::wstring ToLower(const std::wstring& str)
	{
		return ToLower(str.c_str());
	}


	std::wstring MakeSafeName(const std::wstring& name)
	{
		std::wstring safeName(name);

		const size_t length = name.length();
		for (size_t i = 0u; i < length; ++i)
		{
			if ((name[i] == '\\') ||
				(name[i] == '/') ||
				(name[i] == '*') ||
				(name[i] == '?') ||
				(name[i] == '"') ||
				(name[i] == '<') ||
				(name[i] == '>') ||
				(name[i] == '|') ||
				(name[i] == ':') ||
				(name[i] == ';') ||
				(name[i] == ',') ||
				(name[i] == '.'))
			{
				safeName[i] = '_';
			}
		}

		return safeName;
	}
}
