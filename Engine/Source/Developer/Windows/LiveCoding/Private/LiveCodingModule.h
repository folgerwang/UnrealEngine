// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveCodingModule.h"
#include "Delegates/Delegate.h"
#include "Modules/ModuleManager.h"

struct IConsoleCommand;
class IConsoleVariable;
class ULiveCodingSettings;

class FLiveCodingModule final : public ILiveCodingModule
{
public:
	FLiveCodingModule();

	// IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// ILiveCodingModule implementation
	virtual void EnableByDefault(bool bInEnabled) override;
	virtual bool IsEnabledByDefault() const override;
	virtual void EnableForSession(bool bInEnabled) override;
	virtual bool IsEnabledForSession() const override;
	virtual void ShowConsole() override;
	virtual void Compile() override;
	virtual bool IsCompiling() const override;
	virtual void Tick() override;

private:
	ULiveCodingSettings* Settings;
	bool bEnabledLastTick;
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

