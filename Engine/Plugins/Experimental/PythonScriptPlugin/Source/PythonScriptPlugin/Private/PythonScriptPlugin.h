// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IncludePython.h"
#include "IPythonScriptPlugin.h"
#include "PyUtil.h"
#include "PyPtr.h"
#include "Misc/CoreMisc.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Commands/InputChord.h"

class FPythonScriptPlugin;

#if WITH_PYTHON

/**
 * Executor for "Python" commands
 */
class FPythonCommandExecutor : public IConsoleCommandExecutor
{
public:
	FPythonCommandExecutor(FPythonScriptPlugin* InPythonScriptPlugin);

	static FName StaticName();
	virtual FName GetName() const override;
	virtual FText GetDisplayName() const override;
	virtual FText GetDescription() const override;
	virtual FText GetHintText() const override;
	virtual void GetAutoCompleteSuggestions(const TCHAR* Input, TArray<FString>& Out) override;
	virtual void GetExecHistory(TArray<FString>& Out) override;
	virtual bool Exec(const TCHAR* Input) override;
	virtual bool AllowHotKeyClose() const override;
	virtual bool AllowMultiLine() const override;
	virtual FInputChord GetHotKey() const override
	{
		return FInputChord();
	}
private:
	FPythonScriptPlugin* PythonScriptPlugin;
};

/**
 *
 */
struct IPythonCommandMenu
{
	virtual ~IPythonCommandMenu() {}

	virtual void OnStartupMenu() = 0;
	virtual void OnShutdownMenu() = 0;

	virtual void OnRunFile(const FString& InFile, bool bAdd) = 0;
};
#endif	// WITH_PYTHON

class FPythonScriptPlugin : public IPythonScriptPlugin, public FSelfRegisteringExec
{
public:
	FPythonScriptPlugin();

	/** Get this module */
	static FPythonScriptPlugin* Get()
	{
		return static_cast<FPythonScriptPlugin*>(IPythonScriptPlugin::Get());
	}

	//~ IPythonScriptPlugin interface
	virtual bool IsPythonAvailable() const override;
	virtual bool ExecPythonCommand(const TCHAR* InPythonCommand) override;
	virtual FSimpleMulticastDelegate& OnPythonInitialized() override;
	virtual FSimpleMulticastDelegate& OnPythonShutdown() override;

	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	//~ FSelfRegisteringExec interface
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

#if WITH_PYTHON
	/** 
	 * Import the given module into the "unreal" package.
	 * This function will take the given name and attempt to import either "unreal_{name}" or "_unreal_{name}" into the "unreal" package as "unreal.{name}".
	 */
	void ImportUnrealModule(const TCHAR* InModuleName);

	bool HandlePythonExecCommand(const TCHAR* InPythonCommand);

	PyObject* EvalString(const TCHAR* InStr, const TCHAR* InContext, const int InMode);
	PyObject* EvalString(const TCHAR* InStr, const TCHAR* InContext, const int InMode, PyObject* InGlobalDict, PyObject* InLocalDict);

	bool RunString(const TCHAR* InStr);

	bool RunFile(const TCHAR* InFile, const TCHAR* InArgs);
#endif	// WITH_PYTHON

private:
#if WITH_PYTHON
	void InitializePython();

	void ShutdownPython();

	void RequestStubCodeGeneration();

	void GenerateStubCode();

	void Tick(const float InDeltaTime);

	void OnModuleDirtied(FName InModuleName);

	void OnModulesChanged(FName InModuleName, EModuleChangeReason InModuleChangeReason);

	void OnContentPathMounted(const FString& InAssetPath, const FString& InFilesystemPath);

	void OnContentPathDismounted(const FString& InAssetPath, const FString& InFilesystemPath);

#if WITH_EDITOR
	void OnPrepareToCleanseEditorObject(UObject* InObject);
#endif	// WITH_EDITOR

	FPythonCommandExecutor CmdExec;
	IPythonCommandMenu* CmdMenu;
	FDelegateHandle TickHandle;
	FDelegateHandle ModuleDelayedHandle;

	PyUtil::FPyApiBuffer PyProgramName;
	PyUtil::FPyApiBuffer PyHomePath;
	FPyObjectPtr PyDefaultGlobalDict;
	FPyObjectPtr PyDefaultLocalDict;
	FPyObjectPtr PyConsoleGlobalDict;
	FPyObjectPtr PyConsoleLocalDict;
	FPyObjectPtr PyUnrealModule;
	bool bInitialized;
	bool bHasTicked;
#endif	// WITH_PYTHON

	FSimpleMulticastDelegate OnPythonInitializedDelegate;
	FSimpleMulticastDelegate OnPythonShutdownDelegate;
};
