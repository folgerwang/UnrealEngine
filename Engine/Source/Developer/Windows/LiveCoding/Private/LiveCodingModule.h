// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveCodingModule.h"
#include "Delegates/Delegate.h"
#include "Modules/ModuleManager.h"

struct IConsoleCommand;
class IConsoleVariable;

class FLiveCodingModule final : public ILiveCodingModule
{
public:
	FLiveCodingModule();

	// IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// ILiveCodingModule implementation
	virtual void Enable(bool bInEnabled) override;
	virtual bool IsEnabled() const override;
	virtual void ShowConsole() override;
	virtual void TriggerRecompile() override;

private:
	bool bEnabled;
	bool bShouldStart;
	bool bStarted;
	TSet<FString> EnabledModules;

	IConsoleCommand* EnableCommand;
	IConsoleVariable* ConsolePathVariable;
	FDelegateHandle EndFrameDelegateHandle;
	FDelegateHandle ModulesChangedDelegateHandle;

	bool StartLiveCoding();

	void OnEndFrame();
	void OnModulesChanged(FName ModuleName, EModuleChangeReason Reason);

	void UpdateModules();
	void EnableModule(const FString& FullFilePath);
	void DisableModule(const FString& FullFilePath);

	void ConfigureModule(const FName& Name, bool bIsProjectModule, const FString& FullFilePath);
	bool ShouldEnableModule(const FName& Name, bool bIsProjectModule, const FString& FilePath) const;
};

