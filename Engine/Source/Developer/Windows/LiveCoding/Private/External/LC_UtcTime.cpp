// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_UtcTime.h"
#include "Windows/WindowsHWrapper.h"


uint64_t utcTime::GetCurrent(void)
{
	FILETIME fileTime = {};
	::GetSystemTimeAsFileTime(&fileTime);

	ULARGE_INTEGER asLargeInteger = {};
	asLargeInteger.LowPart = fileTime.dwLowDateTime;
	asLargeInteger.HighPart = fileTime.dwHighDateTime;

	return asLargeInteger.QuadPart;
}
