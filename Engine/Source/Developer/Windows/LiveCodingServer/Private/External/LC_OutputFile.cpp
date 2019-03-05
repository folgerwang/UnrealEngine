// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_OutputFile.h"
#include "LC_FileUtil.h"
#include "LC_Logging.h"


namespace
{
	static const char* const TYPE_PREFIXES[4u] =
	{
		nullptr,			// Type::INFO
		"WARNING: ",		// Type::WARNING
		"ERROR: ",			// Type::ERROR
		"SUCCESS: ",		// Type::SUCCESS
	};
}


OutputFile::OutputFile(const wchar_t* logFilePath)
	: m_logFile(nullptr)
{
	m_logFile = ::CreateFileW(logFilePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (m_logFile == INVALID_HANDLE_VALUE)
	{
		LC_ERROR_USER("Cannot obtain handle for file %S. Error: 0x%X", logFilePath, ::GetLastError());
	}
	else
	{
		LC_LOG_USER("Creating log file at %S", file::NormalizePath(logFilePath).c_str());
	}
}


OutputFile::~OutputFile(void)
{
	::CloseHandle(m_logFile);
}


void OutputFile::Log(const char* msg)
{
	WriteToFile(msg);
}


void OutputFile::Log(const char* msg, int type)
{
	const char* const prefix = TYPE_PREFIXES[type];
	if (prefix)
	{
		WriteToFile(prefix);
	}

	WriteToFile(msg);
}


void OutputFile::WriteToFile(const char* text)
{
	const bool isValidFile = (m_logFile && (m_logFile != INVALID_HANDLE_VALUE));
	if (isValidFile)
	{
		DWORD bytesWritten = 0u;
		const size_t length = strlen(text);
		::WriteFile(m_logFile, text, static_cast<DWORD>(length), &bytesWritten, nullptr);
	}
}
