// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveCodingModule.h"
#include "Delegates/Delegate.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"

struct IConsoleCommand;
class IConsoleVariable;
class ISettingsSection;
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
	virtual bool CanEnableForSession() const override;
	virtual bool HasStarted() const override;
	virtual void ShowConsole() override;
	virtual void Compile() override;
	virtual bool IsCompiling() const override;
	virtual void Tick() override;

private:
	ULiveCodingSettings* Settings;
	TSharedPtr<ISettingsSection> SettingsSection;
	bool bEnabledLastTick;
	bool bEnabledForSession;
	bool bStarted;
	TSet<FName> ConfiguredModules;

	const FString FullEnginePluginsDir;
	const FString FullProjectDir;
	const FString FullProjectPluginsDir;

	IConsoleCommand* EnableCommand;
	IConsoleVariable* ConsolePathVariable;
	FDelegateHandle EndFrameDelegateHandle;
	FDelegateHandle ModulesChangedDelegateHandle;

	bool StartLiveCoding();

	void OnModulesChanged(FName ModuleName, EModuleChangeReason Reason);

	void UpdateModules();

	void ConfigureModule(const FName& Name, const FString& FullFilePath);
	bool ShouldPreloadModule(const FName& Name, const FString& FullFilePath) const;
};

