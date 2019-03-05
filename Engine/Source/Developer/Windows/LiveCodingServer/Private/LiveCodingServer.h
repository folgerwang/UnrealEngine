// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "ILiveCodingServer.h"
#include "LC_RunMode.h"
#include <string>

class ServerCommandThread;

class FLiveCodingServer final : public ILiveCodingServer
{
public:
	FLiveCodingServer();
	virtual ~FLiveCodingServer();

	virtual void Start(const wchar_t* ProcessGroupName) override;
	virtual void Stop() override;

	virtual void SetLinkerPath(const wchar_t* LinkerPath) override;

	// ILiveCodingServer implementation
	virtual FBringToFrontDelegate& GetBringToFrontDelegate() override final;
	virtual FClearOutputDelegate& GetClearOutputDelegate() override final;
	virtual FStatusChangeDelegate& GetStatusChangeDelegate() override final;
	virtual FLogOutputDelegate& GetLogOutputDelegate() override final;
	virtual FCompileDelegate& GetCompileDelegate() override final;
	virtual FCompileStartedDelegate& GetCompileStartedDelegate() override final;
	virtual FCompileFinishedDelegate& GetCompileFinishedDelegate() override final;
	virtual FShowConsoleDelegate& GetShowConsoleDelegate() override final;
	virtual FSetVisibleDelegate& GetSetVisibleDelegate() override final;

private:
	std::wstring ProcessGroupName;
	ServerCommandThread* CommandThread;

	FBringToFrontDelegate BringToFrontDelegate;
	FClearOutputDelegate ClearOutputDelegate;
	FStatusChangeDelegate StatusChangeDelegate;
	FLogOutputDelegate LogOutputDelegate;
	FCompileDelegate CompileDelegate;
	FCompileStartedDelegate CompileStartedDelegate;
	FCompileFinishedDelegate CompileFinishedDelegate;
	FShowConsoleDelegate ShowConsoleDelegate;
	FSetVisibleDelegate SetVisibleDelegate;
};

extern FLiveCodingServer *GLiveCodingServer;