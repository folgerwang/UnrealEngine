// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FileLogOutput.h"
#include "StringUtils.h"
#include "TimeUtils.h"

//////////////////////////////////////////////////////////////////////////
//
// FThreadedFileLogging
//
//////////////////////////////////////////////////////////////////////////

 FThreadedFileLogging::FThreadedFileLogging(const char* Filename, const char* PostFix)
{
	if (!Filename)
	{
		FDateTime DateTime = PARAM_LocalTime ? Now() : UtcNow();

		std::string ExeName;
		std::string Basename;
		std::string ExePath = GetProcessPath(&ExeName);

		GetExtension(ExeName, &Basename);
		Filename = FormatString(
			"%s\\%s-%s",
			ExePath.c_str(),
			Basename.c_str(),
			DateTime.ToString(false));
	}

	Filename = FormatString("%s%s", Filename, PostFix ? PostFix : "");

#if EG_PLATFORM == EG_PLATFORM_WINDOWS
	// Visual Studio has an "ofstream::open" overload that takes a wchar_t string"
	Out.open(Widen(Filename), std::ios::out | std::ios::trunc);
#else
	Out.open(Filename, std::ios::out | std::ios::trunc);
#endif

	if (!Out.is_open())
	{
		EG_LOG(LogDefault, Error, "Failed to open log file '%s'", Filename);
	}

	WorkThread = std::thread([this]()
	{
		while (!bFinish)
		{
			std::function<void()> Work;
			WorkQueue.Pop(Work);
			Work();
		}
	});
}

FThreadedFileLogging::~FThreadedFileLogging()
{
	WorkQueue.Push([this]() { bFinish = true; });
	WorkThread.join();
}

void FThreadedFileLogging::Write(
    const char* Msg)
{
	WorkQueue.Push([ this, Msg = std::string(Msg) ]()
	{
		Out << Msg;
		if (bAutoNewLine)
		{
			Out << std::endl;
		}
		Out.flush();
	});
}

//////////////////////////////////////////////////////////////////////////
//
// FFileLogOutput
//
//////////////////////////////////////////////////////////////////////////

FFileLogOutput::FFileLogOutput(const char* Filename, const char* PostFix)
	: FThreadedFileLogging(Filename, PostFix)
{
}

void FFileLogOutput::Log(
    const char* File, int Line, const FLogCategoryBase* Category, ELogVerbosity Verbosity, const char* Msg)
{
	Write(Msg);
}
