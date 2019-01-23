// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CrashDetection.h"
#include "Logging.h"

#if EG_PLATFORM == EG_PLATFORM_WINDOWS

namespace detail
{
	/**
	 * Detects a crash, logs the reason, and waits a bit, so the logs have time
	 * to flush
	 */
	LONG WINAPI WindowsExceptionHandler(EXCEPTION_POINTERS* ExceptionInfo)
	{
		switch (ExceptionInfo->ExceptionRecord->ExceptionCode)
		{
		case EXCEPTION_ACCESS_VIOLATION:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_ACCESS_VIOLATION");
			break;
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_ARRAY_BOUNDS_EXCEEDED");
			break;
		case EXCEPTION_BREAKPOINT:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_BREAKPOINT");
			break;
		case EXCEPTION_DATATYPE_MISALIGNMENT:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_DATATYPE_MISALIGNMENT");
			break;
		case EXCEPTION_FLT_DENORMAL_OPERAND:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_FLT_DENORMAL_OPERAND");
			break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_FLT_DIVIDE_BY_ZERO");
			break;
		case EXCEPTION_FLT_INEXACT_RESULT:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_FLT_INEXACT_RESULT");
			break;
		case EXCEPTION_FLT_INVALID_OPERATION:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_FLT_INVALID_OPERATION");
			break;
		case EXCEPTION_FLT_OVERFLOW:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_FLT_OVERFLOW");
			break;
		case EXCEPTION_FLT_STACK_CHECK:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_FLT_STACK_CHECK");
			break;
		case EXCEPTION_FLT_UNDERFLOW:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_FLT_UNDERFLOW");
			break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_ILLEGAL_INSTRUCTION");
			break;
		case EXCEPTION_IN_PAGE_ERROR:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_IN_PAGE_ERROR");
			break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_INT_DIVIDE_BY_ZERO");
			break;
		case EXCEPTION_INT_OVERFLOW:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_INT_OVERFLOW");
			break;
		case EXCEPTION_INVALID_DISPOSITION:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_INVALID_DISPOSITION");
			break;
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_NONCONTINUABLE_EXCEPTION");
			break;
		case EXCEPTION_PRIV_INSTRUCTION:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_PRIV_INSTRUCTION");
			break;
		case EXCEPTION_SINGLE_STEP:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_SINGLE_STEP");
			break;
		case EXCEPTION_STACK_OVERFLOW:
			EG_LOG(LogDefault, Fatal, "Crash: EXCEPTION_STACK_OVERFLOW");
			break;
		default:
			EG_LOG(LogDefault, Fatal, "Crash: Unrecognized Exception");
			break;
		}

		// Give some time for logs to flush
		std::this_thread::sleep_for(std::chrono::seconds(1));

		exit(EXIT_FAILURE);
		return EXCEPTION_EXECUTE_HANDLER;
	}

}  // namespace detail

void SetupCrashDetection()
{
	::SetUnhandledExceptionFilter(detail::WindowsExceptionHandler);
}

#elif EG_PLATFORM == EG_PLATFORM_LINUX
	// #LINUX: See https://gist.github.com/jvranish/4441299 for some tips how to implement this in Linux
	#error "Not implemented yet""
#else
	#error "Unknown Platform"
#endif


