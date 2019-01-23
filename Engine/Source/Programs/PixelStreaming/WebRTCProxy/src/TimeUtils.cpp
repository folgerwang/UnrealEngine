// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TimeUtils.h"
#include "StringUtils.h"

const char* FDateTime::ToString(bool bIncludeMSec)
{
	if (bIncludeMSec)
	{
		return FormatString("%04d.%02d.%02d-%02d.%02d.%02d:%03d",
			Year, Month, Day, Hour, Minute, Second, MSec);
	}
	else
	{
		return FormatString("%04d.%02d.%02d-%02d.%02d.%02d",
			Year, Month, Day, Hour, Minute, Second);
	}
}

#if EG_PLATFORM == EG_PLATFORM_WINDOWS
FDateTime Now()
{
	SYSTEMTIME st;
	GetLocalTime( &st );
	int Year		= st.wYear;
	int Month		= st.wMonth;
	int Day			= st.wDay;
	int Hour		= st.wHour;
	int Min			= st.wMinute;
	int Sec			= st.wSecond;
	int MSec		= st.wMilliseconds;

	return FDateTime(Year, Month, Day, Hour, Min, Sec, MSec);
}

FDateTime UtcNow()
{
	SYSTEMTIME st;
	GetSystemTime( &st );
	int Year		= st.wYear;
	int Month		= st.wMonth;
	int Day			= st.wDay;
	int Hour		= st.wHour;
	int Min			= st.wMinute;
	int Sec			= st.wSecond;
	int MSec		= st.wMilliseconds;

	return FDateTime(Year, Month, Day, Hour, Min, Sec, MSec);
}

#elif EG_PLATFORM == EG_PLATFORM_LINUX

// #LINUX : These were copied from UE4, but untested in WebRTCProxy itself so far.
// Once porting to Linux is done, fix any problems this might have

FDateTime Now()
{
	// query for calendar time
	struct timeval Time;
	gettimeofday(&Time, NULL);

	// convert it to local time
	struct tm LocalTime;
	localtime_r(&Time.tv_sec, &LocalTime);

	// pull out data/time
	int Year		= LocalTime.tm_year + 1900;
	int Month		= LocalTime.tm_mon + 1;
	int Day			= LocalTime.tm_mday;
	int Hour		= LocalTime.tm_hour;
	int Min			= LocalTime.tm_min;
	int Sec			= LocalTime.tm_sec;
	int MSec		= Time.tv_usec / 1000;

	return FDateTime(Year, Month, Day, Hour, Min, Sec, MSec);
}

FDateTime UtcNow()
{
	// query for calendar time
	struct timeval Time;
	gettimeofday(&Time, NULL);

	// convert it to UTC
	struct tm LocalTime;
	gmtime_r(&Time.tv_sec, &LocalTime);

	// pull out data/time
	int Year		= LocalTime.tm_year + 1900;
	int Month		= LocalTime.tm_mon + 1;
	int Day			= LocalTime.tm_mday;
	int Hour		= LocalTime.tm_hour;
	int Min			= LocalTime.tm_min;
	int Sec			= LocalTime.tm_sec;
	int MSec		= Time.tv_usec / 1000;

	return FDateTime(Year, Month, Day, Hour, Min, Sec, MSec);
}

#else

	#error "Unknown platform"

#endif

