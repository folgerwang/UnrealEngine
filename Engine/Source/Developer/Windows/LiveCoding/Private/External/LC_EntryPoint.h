// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

void __cdecl LppRegisterProcessGroup(const char* const groupName);
void* __cdecl LppEnableModule(const wchar_t* const nameOfExeOrDll);
void* __cdecl LppEnableAllModules(const wchar_t* const nameOfExeOrDll);
void* __cdecl LppDisableModule(const wchar_t* const nameOfExeOrDll);
void* __cdecl LppDisableAllModules(const wchar_t* const nameOfExeOrDll);
void __cdecl LppWaitForToken(void* token);
void __cdecl LppTriggerRecompile(void);
void __cdecl LppInstallExceptionHandler(void);
void __cdecl LppUseExternalBuildSystem(void);
void __cdecl LppShowConsole();
void __cdecl LppSetVisible(bool visible);
void __cdecl LppSetActive(bool active);
void __cdecl LppSetBuildArguments(const wchar_t* const arguments);
void __cdecl LppApplySettingBool(const char* const settingName, int value);
void __cdecl LppApplySettingInt(const char* const settingName, int value);
void __cdecl LppApplySettingString(const char* const settingName, const wchar_t* const value);

// BEGIN EPIC MOD - Support for lazy-loading modules
void __cdecl LppEnableLazyLoadedModule(const wchar_t* const nameOfExeOrDll);
// END EPIC MODS