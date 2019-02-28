// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Console.h"
#include "SharedQueue.h"
#include "FileLogOutput.h"
#include "CmdLine.h"
#include "WebRTCLogging.h"
#include "Conductor.h"
#include "StringUtils.h"
#include "ScopeGuard.h"
#include "CrashDetection.h"

const char* Help =
    "\
WebRTCProxy\n\
Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.\n\
\n\
Parameters:\n\
\n\
-help\n\
Shows this help\n\
\n\
-Cirrus=<IP:Port>\n\
The Cirrus server to connect to. If not specified. it defaults to 127.0.0.1:8888\n\
\n\
-StunServer=<IP:Port>\n\
Stun server to use.\n\
\n\
-UE4Port=<Port>\n\
The port UE4 is listening on\n\
\n\
-AutoSetBitrate\n\
If specified, it forcibly sends a bitrate request to UE4 once a client gets\n\
quality control ownership\n\
\n\
-PlanB\n\
If specified, it will use PlanB sdp semantics. Default is UnifiedPlan.\n\
\n\
-dbgwindow=[Proxy|WebRTC|All|None]\n\
If running under the debugger (e.g: Visual Studio), it specifies what logs to\n\
send to the Output Window.\n\
	Proxy - Only logs from WebRTCProxy itself will be displayed.\n\
	WebRTC - Only logs from WebRTC internals will be displayed.\n\
	All - (Default) Both WebRTCProxy and WebRTC internal logs are displayed.\n\
	None - No logs sent to the Output Window\n\
\n\
-LocalTime\n\
If specified, it will use local time in logging, instead of UTC.\n\
\n\
\n\
";

TCHAR GInternalProjectName[64] = TEXT("WebRTCProxy");
IMPLEMENT_FOREIGN_ENGINE_DIR();

std::pair<std::string, uint16_t> PARAM_Cirrus{"127.0.0.1", 8888};
uint16_t PARAM_UE4Port = 8124;
bool PARAM_PlanB = false;
bool PARAM_DbgWindow_Proxy = true;
bool PARAM_DbgWindow_WebRTC = true;
bool PARAM_LocalTime = false;  // By default we use UTC time

bool ParseParameters(int argc, char* argv[])
{
	FCmdLine Params;
	if (!Params.Parse(argc, argv))
	{
		printf(Help);
		return false;
	}

	if (Params.Has("Help"))
	{
		printf(Help);
		return false;
	}

	// Splits a string in the form of "XXXX:NNN" into a pair
	auto ProcessAddressParameter = [&Params](const char* Name, std::pair<std::string, uint16_t>& OutAddr) -> bool {
		if (!Params.Has(Name))
		{
			return true;
		}

		const char* const Param = Params.Get(Name).c_str();
		const char* Ptr = Param;
		// Find separator
		while (!(*Ptr == 0 || *Ptr == ':' || *Ptr == '|'))
		{
			Ptr++;
		}

		OutAddr.first = std::string(Param, Ptr);
		// If at the end of the string, then no separator was found (and no port specified)
		if (*Ptr && OutAddr.first != "")
		{
			int Port = std::atoi(Ptr + 1);
			if (Port < 1 || Port > 65535)
			{
				EG_LOG(LogDefault, Error, "Invalid port number for parameter '%s'", Name);
				return false;
			}
			OutAddr.second = static_cast<uint16_t>(Port);
		}
		else
		{
			EG_LOG(LogDefault, Error, "Invalid format for parameter '%s'", Name);
			OutAddr.second = 0;
			return false;
		}

		return true;
	};

	if (!ProcessAddressParameter("Cirrus", PARAM_Cirrus))
	{
		return false;
	}

	PARAM_UE4Port = Params.GetAsInt("UE4Port", 8124).second;

	PARAM_PlanB = Params.Has("PlanB");

	if (Params.Has("DbgWindow"))
	{
		const std::string& Val = Params.Get("dbgwindow");
		if (CiEquals(Val, std::string("Proxy")))
		{
			PARAM_DbgWindow_Proxy = true;
			PARAM_DbgWindow_WebRTC = false;
		}
		else if (CiEquals(Val, std::string("WebRTC")))
		{
			PARAM_DbgWindow_Proxy = false;
			PARAM_DbgWindow_WebRTC = true;
		}
		else if (CiEquals(Val, std::string("All")))
		{
			PARAM_DbgWindow_Proxy = true;
			PARAM_DbgWindow_WebRTC = true;
		}
		else if (CiEquals(Val, std::string("None")))
		{
			PARAM_DbgWindow_Proxy = false;
			PARAM_DbgWindow_WebRTC = false;
		}
		else
		{
			EG_LOG(LogDefault, Error, "Invalid parameter format for parameter 'dbgwindow'");
			return false;
		}
	}

	PARAM_LocalTime = Params.Has("LocalTime");

	return true;
}

// This is used by the Control handler (set with ConsoleCtrlHandler function)
// to wait for the main thread to finish
std::atomic<bool> bFinished = false;
DWORD MainThreadId = 0;

// Handler function will be called on separate thread!
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:  // Ctrl+C
		break;
	case CTRL_BREAK_EVENT:  // Ctrl+Break
		break;
	case CTRL_CLOSE_EVENT:  // Closing the console window
		break;
	case CTRL_LOGOFF_EVENT:  // User logs off. Passed only to services!
		break;
	case CTRL_SHUTDOWN_EVENT:  // System is shutting down. Passed only to services!
		break;
	}

	EG_LOG(LogDefault, Log, "Console Ctrl Handler: %lu", dwCtrlType);
	EG_LOG(LogDefault, Log, "Waiting to finish UE4WebRTCProxy...");

	if (!MainThreadId)
	{
		return Windows::FALSE;
	}

	PostThreadMessage(MainThreadId, WM_QUIT, 0, 0);
	// Wait for the main thread to finish
	while (!bFinished)
	{
		Sleep(100);
	}

	// Return TRUE if handled this message, further handler functions won't be called.
	// Return FALSE to pass this message to further handlers until default handler calls ExitProcess().
	return Windows::FALSE;
}

int mainImpl(int argc, char* argv[])
{
	FConsole Console;
	Console.Init(120, 40, 400, 2000);

	MainThreadId = GetCurrentThreadId();
	SetConsoleCtrlHandler(ConsoleCtrlHandler, Windows::TRUE);

	// NOTE: Parsing the parameters before creating the file logger, so the log
	// filename takes into account the -LocalTime parameter (if specified)
	if (!ParseParameters(argc, argv))
	{
		return EXIT_FAILURE;
	}

	//
	// Create file loggers
	//
	FFileLogOutput FileLogger(nullptr);  // Our own log file
	// WebRTC logging
	InitializeWebRTCLogging(rtc::LoggingSeverity::LS_VERBOSE);
	// Make sure we stop the webrtc logging, otherwise it crashes on exit
	SCOPE_EXIT
	{
		StopWebRTCLogging();
	};

	// Log the command line parameters, so we know what parameters were used for this run
	{
		std::string Str;
		for (int i = 0; i < argc; i++)
		{
			Str += std::string(argv[i]) + " ";
		}

		EG_LOG(LogDefault, Log, "CmdLine: %s", Str.c_str());
	}

	SetupCrashDetection();
	// If you want to test crash detection when not running a debugger, enable the block below.
	// It will cause an access violation after 1 second.
	// NOTE: If running under the debugger, it will not trigger the crash detection
#if 0
	std::thread([]()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		*reinterpret_cast<uint32_t*>(0) = 1;
	}).detach();
#endif

	// #REFACTOR : Make this cross platform
#if EG_PLATFORM == EG_PLATFORM_WINDOWS
	rtc::EnsureWinsockInit();
	rtc::Win32SocketServer w32_ss;
	rtc::Win32Thread w32_thread(&w32_ss);
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);
#elif EG_PLATFORM_LINUX == EG_PLATFORM_LINUX
#error Not yet implemented
#else
#error Unknown platform
#endif

	rtc::InitializeSSL();
	auto Conductor = std::make_unique<FConductor>();

	// Main loop.
	MSG Msg;
	BOOL Gm;
	while ((Gm = ::GetMessageW(&Msg, NULL, 0, 0)) != 0 && Gm != -1)
	{
		::TranslateMessage(&Msg);
		::DispatchMessage(&Msg);
	}

	rtc::CleanupSSL();

	EG_LOG(LogDefault, Log, "Exiting UE4WebRTCProxy");

	return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
	int ExitCode;
	try
	{
		ExitCode = mainImpl(argc, argv);
	}
	catch (std::exception& e)
	{
		printf("%s\n", e.what());
		ExitCode = EXIT_FAILURE;
	}

	bFinished = true;
	return ExitCode;
}
