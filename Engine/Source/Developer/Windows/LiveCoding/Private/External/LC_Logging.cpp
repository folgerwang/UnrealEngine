// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Logging.h"
#include "Logging/LogMacros.h"
#include "LiveCodingLog.h"
#include <atomic>

namespace
{
	// we want to be able to dump large things, including environment variable blocks, but 32k on the stack is too much
	static const size_t PER_THREAD_BUFFER_SIZE = 32u * 1024u;
	static thread_local char gtls_logBuffer[PER_THREAD_BUFFER_SIZE] = {};

	// keeps track of the current indentation
	static std::atomic<int> g_indentationLevel[logging::Channel::COUNT] = {};

	// small lookup table for different levels of indentation
	static const int MAX_INDENTATION_LEVELS = 7;
	static const char* const INDENTATION_STRINGS[MAX_INDENTATION_LEVELS] =
	{
		"",						// level 0
		"  o ",					// level 1
		"    - ",				// level 2
		"      * ",				// level 3
		"        o ",			// level 4
		"          - ",			// level 5
		"            * "		// level 6
	};

	// BEGIN EPIC MOD - Redirecting log output
	static void DefaultOutputHandler(logging::Channel::Enum channel, logging::Type::Enum type, const wchar_t* const Message)
	{
		FString MessageWithoutNewline = FString(Message).TrimEnd();
		switch (type)
		{
		case logging::Type::LOG_WARNING:
			UE_LOG(LogLiveCoding, Warning, TEXT("%s"), *MessageWithoutNewline);
			break;
		case logging::Type::LOG_ERROR:
			UE_LOG(LogLiveCoding, Error, TEXT("%s"), *MessageWithoutNewline);
			break;
		default:
			UE_LOG(LogLiveCoding, Display, TEXT("%s"), *MessageWithoutNewline);
			break;
		}
	}

	static logging::OutputHandlerType OutputHandler = &DefaultOutputHandler;
	// END EPIC MOD - Redirecting log output

	// BEGIN EPIC MOD - Explicit API for enabling log channels
	static bool ChannelEnabled[logging::Channel::COUNT] = { };
	// END EPIC MOD - Explicit API for enabling log channels

	static bool IsChannelEnabled(logging::Channel::Enum channel, logging::Type::Enum type)
	{
		// warnings, errors and success logs should always be output to all channels
		if (type != logging::Type::LOG_INFO)
		{
			return true;
		}

		// BEGIN EPIC MOD - Explicit API for enabling log channels
		switch (channel)
		{
			case logging::Channel::USER:
				// user-visible logs are *always* logged
				return true;

			default:
				// disabled or unknown channel
				return ChannelEnabled[channel];
		}
		// END EPIC MOD - Explicit API for enabling log channels
	}
}


logging::Indent::Indent(Channel::Enum channel)
	: m_channel(channel)
{
	IncrementIndentation(channel);
}


logging::Indent::~Indent(void)
{
	DecrementIndentation(m_channel);
}


void logging::IncrementIndentation(Channel::Enum channel)
{
	std::atomic_fetch_add(&g_indentationLevel[channel], 1);
}


void logging::DecrementIndentation(Channel::Enum channel)
{
	std::atomic_fetch_sub(&g_indentationLevel[channel], 1);
}


const char* logging::GetIndentation(logging::Channel::Enum channel)
{
	int indent = g_indentationLevel[channel];
	if (indent >= MAX_INDENTATION_LEVELS)
	{
		return INDENTATION_STRINGS[MAX_INDENTATION_LEVELS - 1];
	}

	return INDENTATION_STRINGS[indent];
}


template <> void logging::LogNoFormat<logging::Channel::Enum::USER>(const wchar_t* const buffer)
{
	// BEGIN EPIC MOD - Redirecting log output
	OutputHandler(logging::Channel::Enum::USER, Type::LOG_INFO, buffer);
	// END EPIC MOD - Redirecting log output
}


template <> void logging::LogNoFormat<logging::Channel::Enum::DEV>(const char* const buffer)
{
	// BEGIN EPIC MOD - Redirecting log output
	OutputHandler(logging::Channel::Enum::DEV, Type::LOG_INFO, ANSI_TO_TCHAR(buffer));
	// END EPIC MOD - Redirecting log output
}

void logging::Log(Channel::Enum channel, Type::Enum type, const char* const format, ...)
{
	if (!IsChannelEnabled(channel, type))
	{
		return;
	}

	va_list argptr;
	va_start(argptr, format);

	char (&buffer)[PER_THREAD_BUFFER_SIZE] = gtls_logBuffer;
	_vsnprintf_s(buffer, _TRUNCATE, format, argptr);

	// BEGIN EPIC MOD - Redirecting log output
	OutputHandler(channel, type, ANSI_TO_TCHAR(buffer));
	// END EPIC MOD - Redirecting log output

	va_end(argptr);
}

// BEGIN EPIC MOD - Redirecting log output
void logging::EnableChannel(Channel::Enum channel, bool enabled)
{
	check(channel != logging::Channel::USER || enabled);
	ChannelEnabled[(int)channel] = enabled;
}

void logging::SetOutputHandler(logging::OutputHandlerType handler)
{
	if (handler == nullptr)
	{
		OutputHandler = &DefaultOutputHandler;
	}
	else
	{
		OutputHandler = handler;
	}
}
// END EPIC MOD - Redirecting log output
