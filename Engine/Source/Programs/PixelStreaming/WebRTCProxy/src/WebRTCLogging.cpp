// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "WebRTCLogging.h"
#include "StringUtils.h"
#include "FileLogOutput.h"
#include "TimeUtils.h"

extern bool PARAM_DbgWindow_WebRTC;

/**
 * Receives logging from WebRTC internals, and writes it to a log file
 * and VS's Output window
 */
class FWebRTCLogger : public rtc::LogSink
{
  public:
	FWebRTCLogger()
		: FileLog(nullptr, "-WebRTC.log")
	{
		// Disable WebRTC's internal calls to VS's OutputDebugString, because we are calling here,
		// so we can add timestamps.
		rtc::LogMessage::LogToDebug(rtc::LS_NONE);
	}

	~FWebRTCLogger()
	{
	}

  private:
	void OnLogMessage(const std::string& message) override
	{
		FDateTime DateTime = PARAM_LocalTime ? Now() : UtcNow();
		const char* Msg = FormatString(
			"[%s]: WEBRTC: %s",
			DateTime.ToString(),
			message.c_str());

		if (PARAM_DbgWindow_WebRTC)
		{
			OutputDebugStringA(Msg);
		}

		FileLog.Write(Msg);
	}

	FFileLogOutput FileLog;
};

namespace
{
	std::unique_ptr<FWebRTCLogger> WebRTCLogger;
}

void InitializeWebRTCLogging(rtc::LoggingSeverity Verbosity)
{
	WebRTCLogger = std::make_unique<FWebRTCLogger>();
	rtc::LogMessage::AddLogToStream(WebRTCLogger.get(), Verbosity);
	rtc::LogMessage::SetLogToStderr(false);
}

void StopWebRTCLogging()
{
	EG_LOG(LogDefault, Log, "Stopping WebRTC logging");
	rtc::LogMessage::RemoveLogToStream(WebRTCLogger.get());
	WebRTCLogger.reset();
}

