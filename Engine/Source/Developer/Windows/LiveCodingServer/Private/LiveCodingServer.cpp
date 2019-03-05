// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LiveCodingServer.h"
#include "External/LC_Scheduler.h"
#include "External/LC_UniqueId.h"
#include "External/LC_FileUtil.h"
#include "External/LC_AppSettings.h"
#include "External/LC_ServerCommandThread.h"

FLiveCodingServer* GLiveCodingServer = nullptr;

////////////////

FLiveCodingServer::FLiveCodingServer()
{
	CommandThread = nullptr;
}

FLiveCodingServer::~FLiveCodingServer()
{
	check(CommandThread == nullptr);
}

void FLiveCodingServer::Start(const wchar_t* InProcessGroupName)
{
	ProcessGroupName = InProcessGroupName;

	scheduler::Startup();
	file::Startup();
	uniqueId::Startup();
	appSettings::Startup(ProcessGroupName.c_str());

	check(CommandThread == nullptr);
	CommandThread = new ServerCommandThread(nullptr, ProcessGroupName.c_str(), RunMode::EXTERNAL_BUILD_SYSTEM);
}

void FLiveCodingServer::Stop()
{
	delete CommandThread;
	CommandThread = nullptr;

	appSettings::Shutdown();
	uniqueId::Shutdown();
	file::Shutdown();
	scheduler::Shutdown();

	ProcessGroupName.clear();
}

void FLiveCodingServer::SetLinkerPath(const wchar_t* LinkerPath)
{
	appSettings::g_linkerPath->SetValueWithoutSaving(LinkerPath);
	appSettings::UpdateLinkerPathCache();
}

ILiveCodingServer::FBringToFrontDelegate& FLiveCodingServer::GetBringToFrontDelegate()
{
	return BringToFrontDelegate;
}

ILiveCodingServer::FClearOutputDelegate& FLiveCodingServer::GetClearOutputDelegate()
{
	return ClearOutputDelegate;
}

ILiveCodingServer::FStatusChangeDelegate& FLiveCodingServer::GetStatusChangeDelegate()
{
	return StatusChangeDelegate;
}

ILiveCodingServer::FLogOutputDelegate& FLiveCodingServer::GetLogOutputDelegate()
{
	return LogOutputDelegate;
}

ILiveCodingServer::FCompileDelegate& FLiveCodingServer::GetCompileDelegate()
{
	return CompileDelegate;
}

ILiveCodingServer::FCompileStartedDelegate& FLiveCodingServer::GetCompileStartedDelegate()
{
	return CompileStartedDelegate;
}

ILiveCodingServer::FCompileFinishedDelegate& FLiveCodingServer::GetCompileFinishedDelegate()
{
	return CompileFinishedDelegate;
}

ILiveCodingServer::FShowConsoleDelegate& FLiveCodingServer::GetShowConsoleDelegate()
{
	return ShowConsoleDelegate;
}

ILiveCodingServer::FSetVisibleDelegate& FLiveCodingServer::GetSetVisibleDelegate()
{
	return SetVisibleDelegate;
}
