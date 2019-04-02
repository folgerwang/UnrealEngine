// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "WebRTCProxyCommon.h"
#include "StringUtils.h"
#include "Logging.h"

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
