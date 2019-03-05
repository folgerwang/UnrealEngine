// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LiveCodingServerModule.h"
#include "LiveCodingServer.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "External/LC_Logging.h"

IMPLEMENT_MODULE(FLiveCodingServerModule, LiveCodingServer)

static void ServerOutputHandler(logging::Channel::Enum Channel, logging::Type::Enum Type, const wchar_t* const Text)
{
	ELiveCodingLogVerbosity Verbosity;
	switch (Type)
	{
	case logging::Type::LOG_SUCCESS:
		Verbosity = ELiveCodingLogVerbosity::Success;
		break;
	case logging::Type::LOG_ERROR:
		Verbosity = ELiveCodingLogVerbosity::Failure;
		break;
	case logging::Type::LOG_WARNING:
		Verbosity = ELiveCodingLogVerbosity::Warning;
		break;
	default:
		Verbosity = ELiveCodingLogVerbosity::Info;
		break;
	}
	GLiveCodingServer->GetLogOutputDelegate().ExecuteIfBound(Verbosity, Text);
}


void FLiveCodingServerModule::StartupModule()
{
	logging::SetOutputHandler(&ServerOutputHandler);

	GLiveCodingServer = new FLiveCodingServer();

	IModularFeatures::Get().RegisterModularFeature(LIVE_CODING_SERVER_FEATURE_NAME, GLiveCodingServer);
}

void FLiveCodingServerModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(LIVE_CODING_SERVER_FEATURE_NAME, GLiveCodingServer);

	if(GLiveCodingServer != nullptr)
	{
		delete GLiveCodingServer;
		GLiveCodingServer = nullptr;
	}

	logging::SetOutputHandler(nullptr);
}
