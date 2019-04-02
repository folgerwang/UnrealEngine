// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"
#include "Logging.h"
#include "SharedQueue.h"

/**
 * Reusable file logging.
 * This is split from the EG_LOG macro file logging, so it can be reused without
 * accepting the EG_LOG macros.
 */
class FThreadedFileLogging
{
public:
	/**
	 * @param Filename
	 *	Full path to the log file name. If not specified, it will use
	 *	"<ProcessPath>\<ProcessName>_YYYY-MM-DD_HH-MM-SS<PostFix>
	 * @param PostFix Post-fix to the filename.
	 *	This can be useful to for example split logs into categories. Eg:
	 *	<Filename>.log and <Filename>_WebRTC.log
	 */
	FThreadedFileLogging(const char* Filename, const char* PostFix=".log");
	~FThreadedFileLogging();

	/**
	* @param bAutoNewLine if true, ever Log call will automatically append a '\n' to the message
	*/
	void SetAutoNewLine(bool bInAutoNewLine)
	{
		this->bAutoNewLine = bInAutoNewLine;
	}

	void Write(const char* Msg);

private:

	std::ofstream Out;
	FWorkQueue WorkQueue;
	std::thread WorkThread;
	bool bFinish = false;

	// If true, it will automatically append a '\n' to the logged messages
	bool bAutoNewLine = false;
};

/**
 * Logs EG_LOG macros calls to a file
 */
class FFileLogOutput
    : public FThreadedFileLogging
    , public ILogOutput
{
public:

	/**
	 * See the FThreadedFileLogging constructor for what the parameters do
	 */
	FFileLogOutput(const char* Filename, const char* PostFix=".log");

	//
	// ILogOutput interface
	//
	void Log(const char* File, int Line, const FLogCategoryBase* Category, ELogVerbosity Verbosity, const char* Msg) override;
};
