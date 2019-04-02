// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_AppSettings.h"
#include "LC_Process.h"
#include "LC_FileUtil.h"
#include "LC_StringUtil.h"
#include "LC_Logging.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <ShlObj.h>
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
	// cache for compiler and linker path
#if LC_64_BIT
	static const wchar_t* const VS2017_COMPILER_PATH = L"bin\\hostx64\\x64\\cl.exe";
	static const wchar_t* const VS2015_AND_EARLIER_COMPILER_PATH = L"bin\\amd64\\cl.exe";
	static const wchar_t* const VS2017_LINKER_PATH = L"bin\\hostx64\\x64\\link.exe";
	static const wchar_t* const VS2015_AND_EARLIER_LINKER_PATH = L"bin\\amd64\\link.exe";
#else
	static const wchar_t* const VS2017_COMPILER_PATH = L"bin\\hostx86\\x86\\cl.exe";
	static const wchar_t* const VS2015_AND_EARLIER_COMPILER_PATH = L"bin\\cl.exe";
	static const wchar_t* const VS2017_LINKER_PATH = L"bin\\hostx86\\x86\\link.exe";
	static const wchar_t* const VS2015_AND_EARLIER_LINKER_PATH = L"bin\\link.exe";
#endif

	static const wchar_t* const COMPILER_EXE = L"cl.exe";
	static const wchar_t* const LINKER_EXE = L"link.exe";

	static std::wstring g_cachedCompilerPath;
	static std::wstring g_cachedLinkerPath;

	static std::wstring DeterminePath(SettingString* setting, const wchar_t* const type, const wchar_t* const vs2017Path, const wchar_t* const vs2015AndEarlierPath, const wchar_t* exeName)
	{
		// absolute paths can be used as they are, relative paths are supposed to be relative to the Live++ executable
		std::wstring path;
		const bool isRelativePath = file::IsRelativePath(setting->GetValue());
		if (isRelativePath)
		{
			path = file::GetDirectory(process::GetImagePath());
			path += L"\\";
			path += setting->GetValue();
		}
		else
		{
			path = setting->GetValue();
		}

		const file::Attributes& attributes = file::GetAttributes(path.c_str());
		if (!file::DoesExist(attributes))
		{
			if (path.length() != 0u)
			{
				LC_ERROR_USER("Cannot determine %S at path %S", type, path.c_str());
			}
			return path;
		}

		if (!file::IsDirectory(attributes))
		{
			// this is not a directory, but a full path
			LC_SUCCESS_USER("Using %S at path %S", type, path.c_str());
			return path;
		}

		// try to find the compiler/linker in the given directory or any of its child directories
		const types::vector<std::wstring>& files = file::EnumerateFiles(path.c_str());

		// walk over all files, grabbing only cl.exe and link.exe
		const size_t count = files.size();
		for (size_t i = 0u; i < count; ++i)
		{
			const std::wstring& originalPath = files[i];
			const std::wstring lowerCaseFilename = string::ToLower(originalPath);
			if (string::Contains(lowerCaseFilename.c_str(), vs2017Path))
			{
				// containing the proper sub-path is not enough, we also need to check the filename,
				// because Visual Studio has files named cl.exe.config and link.exe.config.
				const std::wstring filenameOnly = file::GetFilename(lowerCaseFilename);
				if (string::Matches(filenameOnly.c_str(), exeName))
				{
					LC_SUCCESS_USER("Found %S at path %S", type, files[i].c_str());
					return files[i];
				}
			}
			else if (string::Contains(lowerCaseFilename.c_str(), vs2015AndEarlierPath))
			{
				// containing the proper sub-path is not enough, we also need to check the filename,
				// because Visual Studio has files named cl.exe.config and link.exe.config.
				const std::wstring filenameOnly = file::GetFilename(lowerCaseFilename);
				if (string::Matches(filenameOnly.c_str(), exeName))
				{
					LC_SUCCESS_USER("Found %S at path %S", type, files[i].c_str());
					return files[i];
				}
			}
		}

		LC_ERROR_USER("Could not find %S while recursing directory %S", exeName, path.c_str());
		return path;
	}

	// helper function that tries to apply a new value to any setting
	template <typename SettingType, typename T>
	static SettingType* ApplySetting(SettingType* settings[], unsigned int settingsCount, const char* const settingName, T value)
	{
		const std::wstring wideSettingName(string::ToWideString(settingName));

		for (unsigned int i = 0u; i < settingsCount; ++i)
		{
			const wchar_t* name = settings[i]->GetName();
			if (string::Matches(name, wideSettingName.c_str()))
			{
				// found the correct setting, apply the new value
				settings[i]->SetValue(value);

				// BEGIN EPIC MOD - Removing UI
				// // tell the UI to update
				// g_theApp.GetMainFrame()->RefreshPropertyValue(settings[i]);
				// END EPIC MOD

				return settings[i];
			}
		}

		return nullptr;
	}
}


void appSettings::Startup(const wchar_t* group)
{
	// ensure that the directories exist
	::SHCreateDirectoryExW(NULL, GetLppDirectory().c_str(), NULL);
	::SHCreateDirectoryExW(NULL, GetSymbolsDirectory().c_str(), NULL);

	g_initialWindowMode = new SettingInt
	(
		group,
		L"initial_window_mode",
		L"Initial window mode",
		L"Specifies how Live++ is launched",
		SW_SHOWNORMAL
	);

	g_initialWindowModeProxy = new SettingIntProxy(g_initialWindowMode);
	(*g_initialWindowModeProxy)
		.AddMapping(L"Normal", SW_SHOWNORMAL)
		.AddMapping(L"Minimized", SW_SHOWMINIMIZED)
		.AddMapping(L"Maximized", SW_SHOWMAXIMIZED);

	g_showFullPathInTitle = new SettingBool
	(
		group,
		L"show_full_path_in_title",
		L"Show full path in title",
		L"Specifies whether the full path will be shown in the window title",
		false
	);

	g_showPathFirstInTitle = new SettingBool
	(
		group,
		L"show_path_first_in_title",
		L"Show path first in title",
		L"Specifies whether the path will be shown first in the window title",
		false
	);

	g_receiveFocusOnRecompile = new SettingInt
	(
		group,
		L"receive_focus_on_recompile",
		L"Receive focus on re-compile",
		L"Specifies when Live++ should receive focus",
		FocusOnRecompile::ON_SHORTCUT
	);

	g_receiveFocusOnRecompileProxy = new SettingIntProxy(g_receiveFocusOnRecompile);
	(*g_receiveFocusOnRecompileProxy)
		.AddMapping(L"On error", FocusOnRecompile::ON_ERROR)
		.AddMapping(L"On success", FocusOnRecompile::ON_SUCCESS)
		.AddMapping(L"On shortcut", FocusOnRecompile::ON_SHORTCUT)
		.AddMapping(L"Never", FocusOnRecompile::NEVER);

	g_showNotificationOnRecompile = new SettingBool
	(
		group,
		L"show_notification_on_recompile",
		L"Show notifications on re-compile",
		L"Specifies whether Live++ shows notifications when compiling",
		true
	);

	g_clearLogOnRecompile = new SettingBool
	(
		group,
		L"clear_log_on_recompile",
		L"Clear log on re-compile",
		L"Specifies whether Live++ clears the log when compiling",
		false
	);

	g_minimizeOnClose = new SettingBool
	(
		group,
		L"minimize_to_tray_on_close",
		L"Minimize to tray on close",
		L"Specifies whether Live++ should be minimized into the system tray when being closed",
		false
	);

	g_keepTrayIcon = new SettingBool
	(
		group,
		L"keep_system_tray_icon",
		L"Keep system tray icon",
		L"Specifies whether the Live++ icon should stay in the system tray",
		false
	);

	g_playSoundOnSuccess = new SettingString
	(
		group,
		L"sound_on_success",
		L"Play sound on success",
		L"Specifies a .WAV to play on successful re-compiles",
		L""
	);

	g_playSoundOnError = new SettingString
	(
		group,
		L"sound_on_error",
		L"Play sound on error",
		L"Specifies a .WAV to play on failed re-compiles",
		L""
	);

	g_compileShortcut = new SettingShortcut
	(
		group,
		L"compile_shortcut",
		L"Compile shortcut",
		L"Shortcut that triggers a re-compile",
		0x37A	// Ctrl+Alt+F11
	);

	g_showUndecoratedNames = new SettingBool
	(
		group,
		L"show_undecorated_symbol_names",
		L"Show undecorated symbol names",
		L"Specifies whether output will show undecorated symbol names",
		false
	);

	g_wordWrapOutput = new SettingBool
	(
		group,
		L"enable_word_wrap_output",
		L"Enable word wrap for output",
		L"Specifies whether output will be word-wrapped",
		false
	);

	g_enableDevLog = new SettingBool
	(
		group,
		L"enable_dev_output",
		L"Enable Dev output",
		L"Specifies whether development logs will be generated",
		false
	);

	g_enableTelemetryLog = new SettingBool
	(
		group,
		L"enable_telemetry_output",
		L"Enable Telemetry output",
		L"Specifies whether telemetry logs will be generated",
		false
	);

	g_enableDevLogCompilands = new SettingBool
	(
		group,
		L"enable_dev_compiland_output",
		L"Enable Dev compiland output",
		L"Specifies whether dev logs for compiland info will be generated",
		false
	);

	g_compilerPath = new SettingString
	(
		group,
		L"override_compiler_path",
		L"Override compiler path",
		L"Overrides the compiler path found in the PDB",
		L""
	);

	g_useCompilerOverrideAsFallback = new SettingBool
	(
		group,
		L"override_compiler_path_as_fallback",
		L"Override compiler path only as fallback",
		L"Specifies whether Live++ uses the override compiler path only as fallback",
		false
	);

	g_useCompilerEnvironment = new SettingBool
	(
		group,
		L"use_compiler_environment",
		L"Use compiler environment",
		L"Specifies whether Live++ tries to find and use the compiler environment",
		true
	);

	g_compilerOptions = new SettingString
	(
		group,
		L"additional_compiler_options",
		L"Additional compiler options",
		L"Additional compiler options passed to the compiler when creating a patch",
		L""
	);

	g_compilerForcePchPdbs = new SettingBool
	(
		group,
		L"compiler_force_pch_pdbs",
		L"Force use of PCH PDBs",
		L"Forces Live++ to make each translation unit use the same PDB as the corresponding precompiled header when re-compiling",
		false
	);

	g_linkerPath = new SettingString
	(
		group,
		L"override_linker_path",
		L"Override linker path",
		L"Overrides the linker path found in the PDB",
		L""
	);

	g_useLinkerOverrideAsFallback = new SettingBool
	(
		group,
		L"override_linker_path_as_fallback",
		L"Override linker path only as fallback",
		L"Specifies whether Live++ uses the override linker path only as fallback",
		false
	);

	g_useLinkerEnvironment = new SettingBool
	(
		group,
		L"use_linker_environment",
		L"Use linker environment",
		L"Specifies whether Live++ tries to find and use the linker environment",
		true
	);

	g_linkerOptions = new SettingString
	(
		group,
		L"additional_linker_options",
		L"Additional linker options",
		L"Additional linker options passed to the linker when creating a patch",
		L""
	);	

	g_forceLinkWeakSymbols = new SettingBool
	(
		group,
		L"force_link_weak_symbols",
		L"Force linking of weak symbols",
		L"Specifies whether weak symbols should be forced to link",
		false
	);

	g_continuousCompilationEnabled = new SettingBool
	(
		group,
		L"continuous_compilation_enabled",
		L"Enable continuous compilation",
		L"Specifies whether continuous compilation is enabled",
		false
	);

	g_continuousCompilationPath = new SettingString
	(
		group,
		L"continuous_compilation_path",
		L"Directory to watch",
		L"Directory to watch for changes when using continuous compilation",
		L""
	);

	g_continuousCompilationTimeout = new SettingInt
	(
		group,
		L"continuous_compilation_timeout",
		L"Timeout (ms)",
		L"Timeout in milliseconds used when waiting for changes",
		100
	);

	g_virtualDriveLetter = new SettingString
	(
		group,
		L"virtual_drive_letter",
		L"Virtual drive letter",
		L"Drive letter of the virtual drive to use, e.g. Z:",
		L""
	);

	g_virtualDrivePath = new SettingString
	(
		group,
		L"virtual_drive_path",
		L"Virtual drive path",
		L"Path to map to the virtual drive, e.g. C:\\MyPath",
		L""
	);

	g_installCompiledPatchesMultiProcess = new SettingBool
	(
		group,
		L"install_compiled_patches_multi_process",
		L"Install compiled patches",
		L"Specifies whether compiled patches are installed into launched processes belonging to an existing process group",
		false
	);

	g_amalgamationSplitIntoSingleParts = new SettingBool
	(
		group,
		L"amalgamation_split_into_single_parts",
		L"Split into single parts",
		L"Specifies whether amalgamated/unity files are automatically split into single files",
		false
	);

	g_amalgamationSplitMinCppCount = new SettingInt
	(
		group,
		L"amalgamation_split_min_cpp_count",
		L"Split threshold",
		L"Minimum number of .cpp files that must be included in an amalgamated/unity file before it is split",
		3
	);	
}


void appSettings::Shutdown(void)
{
	delete g_initialWindowMode;
	delete g_initialWindowModeProxy;
	delete g_showFullPathInTitle;
	delete g_showPathFirstInTitle;

	delete g_receiveFocusOnRecompile;
	delete g_receiveFocusOnRecompileProxy;
	delete g_showNotificationOnRecompile;
	delete g_clearLogOnRecompile;
	delete g_minimizeOnClose;
	delete g_keepTrayIcon;
	delete g_playSoundOnSuccess;
	delete g_playSoundOnError;
	delete g_compileShortcut;

	delete g_showUndecoratedNames;
	delete g_wordWrapOutput;
	delete g_enableDevLog;
	delete g_enableTelemetryLog;
	delete g_enableDevLogCompilands;

	delete g_compilerPath;
	delete g_useCompilerOverrideAsFallback;
	delete g_useCompilerEnvironment;
	delete g_compilerOptions;
	delete g_compilerForcePchPdbs;

	delete g_linkerPath;
	delete g_useLinkerOverrideAsFallback;
	delete g_useLinkerEnvironment;
	delete g_linkerOptions;
	delete g_forceLinkWeakSymbols;

	delete g_continuousCompilationEnabled;
	delete g_continuousCompilationPath;
	delete g_continuousCompilationTimeout;

	delete g_virtualDriveLetter;
	delete g_virtualDrivePath;

	delete g_installCompiledPatchesMultiProcess;

	delete g_amalgamationSplitIntoSingleParts;
	delete g_amalgamationSplitMinCppCount;
}


std::wstring appSettings::GetLppDirectory(void)
{
	wchar_t* knownPath = nullptr;
	::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0u, NULL, &knownPath);

	std::wstring directory(knownPath);
	directory += L"\\Live++";

	::CoTaskMemFree(knownPath);

	return directory;
}


std::wstring appSettings::GetSymbolsDirectory(void)
{
	std::wstring directory(GetLppDirectory());
	directory += L"\\Symbols";

	return directory;
}


std::wstring appSettings::GetUserSettingsPath(void)
{
	// user settings are stored in the %localappdata%\Live++ directory
	std::wstring iniPath(GetLppDirectory());

#if LC_64_BIT
	iniPath += L"\\LPP_x64.ini";
#else
	iniPath += L"\\LPP_x86.ini";
#endif

	return file::NormalizePath(iniPath.c_str());
}


std::wstring appSettings::GetProjectSettingsPath(void)
{
	// project settings are stored next to the Live++ executable
	const std::wstring& imagePath = process::GetImagePath();
	std::wstring iniPath(file::GetDirectory(imagePath));

#if LC_64_BIT
	iniPath += L"\\LPP_x64.ini";
#else
	iniPath += L"\\LPP_x86.ini";
#endif

	return file::NormalizePath(iniPath.c_str());
}


std::wstring appSettings::GetCompilerPath(void)
{
	return g_cachedCompilerPath;
}


std::wstring appSettings::GetLinkerPath(void)
{
	return g_cachedLinkerPath;
}


void appSettings::UpdateCompilerPathCache(void)
{
	g_cachedCompilerPath = DeterminePath(g_compilerPath, L"compiler", VS2017_COMPILER_PATH, VS2015_AND_EARLIER_COMPILER_PATH, COMPILER_EXE);
}


void appSettings::UpdateLinkerPathCache(void)
{
	g_cachedLinkerPath = DeterminePath(g_linkerPath, L"linker", VS2017_LINKER_PATH, VS2015_AND_EARLIER_LINKER_PATH, LINKER_EXE);
}


void appSettings::UpdatePathCache(void)
{
	UpdateCompilerPathCache();
	UpdateLinkerPathCache();
}


void appSettings::ApplySettingBool(const char* const settingName, bool value)
{
	const unsigned int COUNT = 20u;
	SettingBool* settings[COUNT] =
	{
		g_showFullPathInTitle,
		g_showPathFirstInTitle,
		g_showNotificationOnRecompile,
		g_clearLogOnRecompile,
		g_minimizeOnClose,
		g_keepTrayIcon,
		g_showUndecoratedNames,
		g_wordWrapOutput,
		g_enableDevLog,
		g_enableTelemetryLog,
		g_enableDevLogCompilands,
		g_useCompilerOverrideAsFallback,
		g_useCompilerEnvironment,
		g_compilerForcePchPdbs,
		g_useLinkerOverrideAsFallback,
		g_useLinkerEnvironment,
		g_forceLinkWeakSymbols,
		g_continuousCompilationEnabled,
		g_installCompiledPatchesMultiProcess,
		g_amalgamationSplitIntoSingleParts
	};

	const SettingBool* setting = ApplySetting(settings, COUNT, settingName, value);
	if (!setting)
	{
		LC_ERROR_USER("Cannot apply value for bool setting %s", settingName);
	}
}


void appSettings::ApplySettingInt(const char* const settingName, int value)
{
	// try int settings first
	{
		const unsigned int COUNT = 4u;
		SettingInt* settings[COUNT] =
		{
			g_initialWindowMode,
			g_receiveFocusOnRecompile,
			g_continuousCompilationTimeout,
			g_amalgamationSplitMinCppCount
		};

		const SettingInt* setting = ApplySetting(settings, COUNT, settingName, value);
		if (setting)
		{
			return;
		}
	}

	// try shortcut setting second
	{
		const std::wstring wideSettingName(string::ToWideString(settingName));
		const wchar_t* name = g_compileShortcut->GetName();
		if (string::Matches(name, wideSettingName.c_str()))
		{
			// found the correct setting, apply the new value
			g_compileShortcut->SetValue(value);

			// BEGIN EPIC MOD - Removing UI
			// // tell the UI to update
			// g_theApp.GetMainFrame()->RefreshPropertyValue(g_compileShortcut);
			// END EPIC MOD
			return;
		}
	}

	LC_ERROR_USER("Cannot apply value for int setting %s", settingName);
}


void appSettings::ApplySettingString(const char* const settingName, const wchar_t* const value)
{
	// try string settings first
	{
		const unsigned int COUNT = 9u;
		SettingString* settings[COUNT] =
		{
			g_playSoundOnSuccess,
			g_playSoundOnError,
			g_compilerPath,
			g_compilerOptions,
			g_linkerPath,
			g_linkerOptions,
			g_continuousCompilationPath,
			g_virtualDriveLetter,
			g_virtualDrivePath
		};

		const SettingString* setting = ApplySetting(settings, COUNT, settingName, value);
		if (setting)
		{
			// update cache when compiler path or linker path changes
			if (setting == g_compilerPath)
			{
				UpdateCompilerPathCache();
			}
			else if (setting == g_linkerPath)
			{
				UpdateLinkerPathCache();
			}

			return;
		}
	}

	const std::wstring wideSettingName(string::ToWideString(settingName));

	// try proxies second
	{
		const unsigned int COUNT = 2u;
		SettingIntProxy* settings[COUNT] =
		{
			g_initialWindowModeProxy,
			g_receiveFocusOnRecompileProxy
		};

		for (unsigned int i = 0u; i < COUNT; ++i)
		{
			const int mappedValue = settings[i]->MapStringToInt(value);
			if (mappedValue != -1)
			{
				// found the correct setting
				settings[i]->GetSetting()->SetValue(mappedValue);

				// BEGIN EPIC MOD - Removing UI
				// // tell the UI to update
				// g_theApp.GetMainFrame()->RefreshPropertyValue(settings[i]);
				// END EPIC MOD
				return;
			}
		}
	}

	LC_ERROR_USER("Cannot apply value for string setting %s", settingName);
}


extern SettingInt* appSettings::g_initialWindowMode = nullptr;
extern SettingIntProxy* appSettings::g_initialWindowModeProxy = nullptr;
extern SettingBool* appSettings::g_showFullPathInTitle = nullptr;
extern SettingBool* appSettings::g_showPathFirstInTitle = nullptr;

extern SettingInt* appSettings::g_receiveFocusOnRecompile = nullptr;
extern SettingIntProxy* appSettings::g_receiveFocusOnRecompileProxy = nullptr;
extern SettingBool* appSettings::g_showNotificationOnRecompile = nullptr;
extern SettingBool* appSettings::g_clearLogOnRecompile = nullptr;
extern SettingBool* appSettings::g_minimizeOnClose = nullptr;
extern SettingBool* appSettings::g_keepTrayIcon = nullptr;

extern SettingString* appSettings::g_playSoundOnSuccess = nullptr;
extern SettingString* appSettings::g_playSoundOnError = nullptr;
extern SettingShortcut* appSettings::g_compileShortcut = nullptr;

extern SettingBool* appSettings::g_showUndecoratedNames = nullptr;
extern SettingBool* appSettings::g_wordWrapOutput = nullptr;
extern SettingBool* appSettings::g_enableDevLog = nullptr;
extern SettingBool* appSettings::g_enableTelemetryLog = nullptr;
extern SettingBool* appSettings::g_enableDevLogCompilands = nullptr;

extern SettingString* appSettings::g_compilerPath = nullptr;
extern SettingBool* appSettings::g_useCompilerOverrideAsFallback = nullptr;
extern SettingBool* appSettings::g_useCompilerEnvironment = nullptr;
extern SettingString* appSettings::g_compilerOptions = nullptr;
extern SettingBool* appSettings::g_compilerForcePchPdbs = nullptr;

extern SettingString* appSettings::g_linkerPath = nullptr;
extern SettingBool* appSettings::g_useLinkerOverrideAsFallback = nullptr;
extern SettingBool* appSettings::g_useLinkerEnvironment = nullptr;
extern SettingString* appSettings::g_linkerOptions = nullptr;
extern SettingBool* appSettings::g_forceLinkWeakSymbols = nullptr;

extern SettingBool* appSettings::g_continuousCompilationEnabled = nullptr;
extern SettingString* appSettings::g_continuousCompilationPath = nullptr;
extern SettingInt* appSettings::g_continuousCompilationTimeout = nullptr;

extern SettingString* appSettings::g_virtualDriveLetter = nullptr;
extern SettingString* appSettings::g_virtualDrivePath = nullptr;

extern SettingBool* appSettings::g_installCompiledPatchesMultiProcess = nullptr;

extern SettingBool* appSettings::g_amalgamationSplitIntoSingleParts = nullptr;
extern SettingInt* appSettings::g_amalgamationSplitMinCppCount = nullptr;
