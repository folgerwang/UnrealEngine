// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_EntryPoint.h"
#include "LC_ClientStartupThread.h"
#include "LC_CriticalSection.h"
#include "LC_RunMode.h"
#include "LPP_API.h"


namespace
{
	// startup thread. initialized when LppRegisterProcessGroup is called
	static ClientStartupThread* g_startupThread = nullptr;

	// critical section to ensure that startup thread is initialized only once
	static CriticalSection g_ensureOneTimeStartup;

	static RunMode::Enum g_runMode = RunMode::DEFAULT;
}


// BEGIN EPIC MOD - Manually trigger startup/shutdown code
void Startup(HINSTANCE instance)
{
	g_startupThread = new ClientStartupThread(instance);
}


void Shutdown(void)
{
	// wait for the startup thread to finish its work and clean up
	g_startupThread->Join();
	delete g_startupThread;
}

#if 0
BOOL WINAPI DllMain(_In_ HINSTANCE hinstDLL, _In_ DWORD dwReason, _In_ LPVOID /* lpvReserved */)
{
	switch (dwReason)
	{
		case DLL_PROCESS_ATTACH:
			Startup(hinstDLL);
			break;

		case DLL_PROCESS_DETACH:
			Shutdown();
			break;

		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			break;
	}

	return Windows::TRUE;
}
#endif
// END EPIC MOD


// exported Live++ API
// BEGIN EPIC MOD - Internalizing API
#define LPP_DLL_API
//#define LPP_DLL_API extern "C" __declspec(dllexport)
// END EPIC MOD - Internalizing API


LPP_DLL_API void __cdecl LppRegisterProcessGroup(const char* const groupName)
{
	// now that we have the process group name, start Live++.
	// ensure that initialization can happen only once, even if the user calls this more than once.
	{
		CriticalSection::ScopedLock lock(&g_ensureOneTimeStartup);

		static bool firstTime = true;
		if (!firstTime)
		{
			// this was already called once, bail out
			return;
		}

		firstTime = false;
	}

	g_startupThread->Start(groupName, g_runMode);
}


LPP_DLL_API void* __cdecl LppEnableModule(const wchar_t* const nameOfExeOrDll)
{
	// hand command creation to the user command thread
	return g_startupThread->EnableModule(nameOfExeOrDll);
}


LPP_DLL_API void* __cdecl LppEnableAllModules(const wchar_t* const nameOfExeOrDll)
{
	// hand command creation to the user command thread
	return g_startupThread->EnableAllModules(nameOfExeOrDll);
}


LPP_DLL_API void* __cdecl LppDisableModule(const wchar_t* const nameOfExeOrDll)
{
	// hand command creation to the user command thread
	return g_startupThread->DisableModule(nameOfExeOrDll);
}


LPP_DLL_API void* __cdecl LppDisableAllModules(const wchar_t* const nameOfExeOrDll)
{
	// hand command creation to the user command thread
	return g_startupThread->DisableAllModules(nameOfExeOrDll);
}


LPP_DLL_API void __cdecl LppWaitForToken(void* token)
{
	g_startupThread->WaitForToken(token);
}


LPP_DLL_API void __cdecl LppTriggerRecompile(void)
{
	g_startupThread->TriggerRecompile();
}


LPP_DLL_API void __cdecl LppBuildPatch(const wchar_t* moduleNames[], const wchar_t* objPaths[], unsigned int count)
{
	g_startupThread->BuildPatch(moduleNames, objPaths, count);
}


LPP_DLL_API void __cdecl LppInstallExceptionHandler(void)
{
	g_startupThread->InstallExceptionHandler();
}


LPP_DLL_API void __cdecl LppUseExternalBuildSystem(void)
{
	g_runMode = RunMode::EXTERNAL_BUILD_SYSTEM;
}


// BEGIN EPIC MOD - Adding ShowConsole command
LPP_DLL_API void __cdecl LppShowConsole()
{
	g_startupThread->ShowConsole();
}
// END EPIC MOD


// BEGIN EPIC MOD - Adding SetVisible command
LPP_DLL_API void __cdecl LppSetVisible(bool visible)
{
	g_startupThread->SetVisible(visible);
}
// END EPIC MOD



// BEGIN EPIC MOD - Adding SetActive command
LPP_DLL_API void __cdecl LppSetActive(bool active)
{
	g_startupThread->SetActive(active);
}
// END EPIC MOD


// BEGIN EPIC MOD - Adding SetBuildArguments command
LPP_DLL_API void __cdecl LppSetBuildArguments(const wchar_t* arguments)
{
	g_startupThread->SetBuildArguments(arguments);
}
// END EPIC MOD

// BEGIN EPIC MOD - Support for lazy-loading modules
LPP_DLL_API void __cdecl LppEnableLazyLoadedModule(const wchar_t* const nameOfExeOrDll)
{
	HMODULE baseAddress = GetModuleHandle(nameOfExeOrDll);
	g_startupThread->EnableLazyLoadedModule(nameOfExeOrDll, baseAddress);
}
// END EPIC MOD

LPP_DLL_API void __cdecl LppApplySettingBool(const char* const settingName, int value)
{
	// hand command creation to the user command thread
	g_startupThread->ApplySettingBool(settingName, value);
}


LPP_DLL_API void __cdecl LppApplySettingInt(const char* const settingName, int value)
{
	// hand command creation to the user command thread
	g_startupThread->ApplySettingInt(settingName, value);
}


LPP_DLL_API void __cdecl LppApplySettingString(const char* const settingName, const wchar_t* const value)
{
	// hand command creation to the user command thread
	g_startupThread->ApplySettingString(settingName, value);
}


#undef LPP_DLL_API
