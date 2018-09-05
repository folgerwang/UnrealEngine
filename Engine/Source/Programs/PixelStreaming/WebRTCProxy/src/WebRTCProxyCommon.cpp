// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "WebRTCProxyPCH.h"
#include "WebRTCProxyCommon.h"
#include "StringUtils.h"
#include "Logging.h"

namespace detail
{
	void BreakImpl()
	{
#if EG_PLATFORM == EG_PLATFORM_WINDOWS
		__debugbreak();
#elif EG_PLATFORM == EG_PLATFORM_LINUX
		#error Not implemented yet
#else
		#error Unknown platform
#endif
	}
}

void DoAssert(const char* File, int Line, _Printf_format_string_ const char* Fmt, ...)
{
	// The actual call to break
	auto DoBreak = []() {
		detail::BreakImpl();
		exit(EXIT_FAILURE);
	};

	// Detect reentrancy, since we call a couple of things from here that
	// can end up asserting
	static bool Executing;
	if (Executing)
	{
		DoBreak();
		return;
	}
	Executing = true;

	char Msg[1024];
	va_list Args;
	va_start(Args, Fmt);
	VSNPrintf(Msg, 1024, Fmt, Args);
	va_end(Args);

	EG_LOG(LogDefault, Error, "ASSERT: %s, %d: %s\n", File, Line, Msg);

	DoBreak();
}

#if EG_PLATFORM == EG_PLATFORM_WINDOWS
std::string GetProcessPath(std::string* Filename)
{
	wchar_t Buf[MAX_PATH];
	GetModuleFileNameW(NULL, Buf, MAX_PATH);

	std::string Res = Narrow(Buf);
	std::string::size_type Index = Res.rfind("\\");

	if (Index != std::string::npos)
	{
		if (Filename)
		{
			*Filename = Res.substr(Index + 1);
		}

		Res = Res.substr(0, Index + 1);
	}
	else
	{
		return "";
	}

	return Res;
}
#elif EG_PLATFORM == EG_PLATFORM_LINUX
#error Not implemented yet
#else
#error Unknown platform
#endif

std::string GetExtension(const std::string& FullFilename, std::string* Basename)
{
	size_t SlashPos = FullFilename.find_last_of("/\\");
	size_t P = FullFilename.find_last_of(".");

	// Where the file name starts (we ignore directories)
	size_t NameStart = SlashPos != std::string::npos ? SlashPos + 1 : 0;

	// Account for the fact there might not be an extension, but there is a dot character,
	// as for example in relative paths. E.g:  ..\SomeFile
	if (P == std::string::npos || (SlashPos != std::string::npos && P < SlashPos))
	{
		if (Basename)
		{
			*Basename = FullFilename.substr(NameStart);
		}

		return "";
	}
	else
	{
		std::string Res = FullFilename.substr(P + 1);
		if (Basename)
		{
			*Basename = FullFilename.substr(NameStart, P - NameStart);
		}

		return Res;
	}
}
