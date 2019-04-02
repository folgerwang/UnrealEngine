// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "StringUtils.h"

void VSNPrintf(char* OutBuffer, int BufSize, const char* Fmt, va_list Args)
{
	int res;
#if EG_PLATFORM == EG_PLATFORM_WINDOWS
	res = _vsnprintf_s(OutBuffer, BufSize, _TRUNCATE, Fmt, Args);
#elif EG_PLATFORM == EG_PLATFORM_LINUX
	res = vsnprintf(OutBuffer, BufSize, fmt, Args);
#else
#error Unknown platform
#endif

	if (res < 0)
	{
		// If this happens, it means we are probably using temporary strings,
		// and we need to increase their sizes
		// Leaving this assert here, so we can catch these situations in Debug builds.
		// In Release builds, the string just stays truncated
		assert(false);
	}
}

void SNPrintf(char* OutBuffer, int BufSize, _Printf_format_string_ const char* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);
	VSNPrintf(OutBuffer, BufSize, Fmt, Args);
	va_end(Args);

}

char* GetTemporaryString()
{
	// Per-thread scratchpad, with an array of several strings that keep
	// cycling, to allow the caller to have some nesting before a string is reused.
	thread_local static char Bufs[EG_TEMPORARY_STRING_MAX_NESTING][EG_TEMPORARY_STRING_MAX_SIZE];
	thread_local static int BufIndex = 0;

	char* Buf = Bufs[BufIndex];
	BufIndex++;
	if (BufIndex == EG_TEMPORARY_STRING_MAX_NESTING)
	{
		BufIndex = 0;
	}

	return Buf;
}

const char* FormatString(_Printf_format_string_ const char* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);
	const char* Str = FormatStringVA(Fmt, Args);
	va_end(Args);
	return Str;
}

char* FormatStringVA(const char* Fmt, va_list Argptr)
{
	char* Buf = GetTemporaryString();
	VSNPrintf(Buf, EG_TEMPORARY_STRING_MAX_SIZE, Fmt, Argptr);
	return Buf;
}

#if EG_PLATFORM == EG_PLATFORM_WINDOWS
std::wstring Widen(const std::string& Utf8)
{
	if (Utf8.empty())
	{
		return std::wstring();
	}

	// Get length (in wchar_t's), so we can reserve the size we need before the
	// actual conversion
	const int Length = ::MultiByteToWideChar(
	    CP_UTF8,             // convert from UTF-8
	    0,                   // default flags
	    Utf8.data(),         // source UTF-8 string
	    (int)Utf8.length(),  // length (in chars) of source UTF-8 string
	    NULL,                // unused - no conversion done in this step
	    0                    // request size of destination buffer, in wchar_t's
	);
	if (Length == 0)
		throw std::exception("Can't get length of UTF-16 string");

	std::wstring Utf16;
	Utf16.resize(Length);

	// Do the actual conversion
	if (!::MultiByteToWideChar(
	        CP_UTF8,             // convert from UTF-8
	        0,                   // default flags
	        Utf8.data(),         // source UTF-8 string
	        (int)Utf8.length(),  // length (in chars) of source UTF-8 string
	        &Utf16[0],           // destination buffer
	        (int)Utf16.length()  // size of destination buffer, in wchar_t's
	        ))
	{
		throw std::exception("Can't convert string from UTF-8 to UTF-16");
	}

	return Utf16;
}

std::string Narrow(const std::wstring& Str)
{
	if (Str.empty())
	{
		return std::string();
	}

	// Get length (in wchar_t's), so we can reserve the size we need before the
	// actual conversion
	const int Utf8_length = ::WideCharToMultiByte(
	    CP_UTF8,            // convert to UTF-8
	    0,                  // default flags
	    Str.data(),         // source UTF-16 string
	    (int)Str.length(),  // source string length, in wchar_t's,
	    NULL,               // unused - no conversion required in this step
	    0,                  // request buffer size
	    NULL,
	    NULL  // unused
	);

	if (Utf8_length == 0)
	{
		throw "Can't get length of UTF-8 string";
	}

	std::string Utf8;
	Utf8.resize(Utf8_length);

	// Do the actual conversion
	if (!::WideCharToMultiByte(
	        CP_UTF8,             // convert to UTF-8
	        0,                   // default flags
	        Str.data(),          // source UTF-16 string
	        (int)Str.length(),   // source string length, in wchar_t's,
	        &Utf8[0],            // destination buffer
	        (int)Utf8.length(),  // destination buffer size, in chars
	        NULL,
	        NULL  // unused
	        ))
	{
		throw "Can't convert from UTF-16 to UTF-8";
	}

	return Utf8;
}
#endif
