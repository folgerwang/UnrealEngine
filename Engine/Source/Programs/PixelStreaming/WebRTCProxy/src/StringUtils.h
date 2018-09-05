// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"

//
// Temporary strings provide a temporary scratchpad for formatting/logging,
// which requiring memory allocation
// User code should not keep hold of these string pointers, since they are
// reused. Reuse period is specified by the "NESTING" macro
#define EG_TEMPORARY_STRING_MAX_SIZE (1024*8)
#define EG_TEMPORARY_STRING_MAX_NESTING 20

/**
* Typical vsnprintf/snprintf.
* By using these we avoid the usual windows deprecation warnings
*/
void VSNPrintf(char* OutBuffer, int BufSize, const char* Fmt, va_list Args);
void SNPrintf(char* OutBuffer, int BufSize, _Printf_format_string_ const char* Fmt, ...);

/**
 * @return
 *	A string buffer of EG_TEMPORARY_STRING_MAX_SIZE characters that be be used as a scratchpad.
 */
char* GetTemporaryString();

/**
 * Akin to snprintf, but uses a temporary string.
 * @return A temporary string.
 */
const char* FormatString(_Printf_format_string_ const char* Fmt, ...);
char* FormatStringVA(const char* Fmt, va_list Argptr);

/**
 * Converts a utf8 string to wide string
 */
std::wstring Widen(const std::string& Utf8);

/**
 * Converts a wide string to utf8
 */
std::string Narrow(const std::wstring& Str);

//
// Case insensitive string search:
// Copied from http://stackoverflow.com/questions/3152241/case-insensitive-stdstring-find
//

namespace detail
{
	// templated version of my_equal so it could work with both char and wchar_t
	template <typename charT>
	struct TCharEqual
	{
		TCharEqual(const std::locale& loc)
		    : loc_(loc)
		{
		}
		bool operator()(charT ch1, charT ch2) { return std::toupper(ch1, loc_) == std::toupper(ch2, loc_); }

	  private:
		const std::locale& loc_;
	};
}

/**
 * Search for a substring (case insensitive)
 * @param Where String to search in
 * @param What string to search for in "Where"
 * @return Position where the substring was found, or -1 if not found.
 */
// find substring (case insensitive)
template <typename T>
static int CiFindSubStr(const T& Where, const T& What, const std::locale& loc = std::locale())
{
	typename T::const_iterator It =
	    std::search(Where.begin(), Where.end(), What.begin(), What.end(), detail::TCharEqual<typename T::value_type>(loc));
	if (It != Where.end())
		return It - Where.begin();
	else
		return -1;  // not found
}

/**
 * Checks if two strings are equal (case insensitive)
 */
template <typename T>
static bool CiEquals(const T& Str1, const T& Str2, const std::locale& loc = std::locale())
{
	if (Str1.size() != Str2.size())
		return false;
	typename T::const_iterator It1 = Str1.begin();
	typename T::const_iterator It2 = Str2.begin();
	detail::TCharEqual<typename T::value_type> Eq(loc);
	while (It1 != Str1.end())
	{
		if (!Eq(*It1, *It2))
			return false;
		++It1;
		++It2;
	}
	return true;
}
