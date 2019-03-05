// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Features/IModularFeature.h"
#include "Delegates/Delegate.h"

#define LIVE_CODING_SERVER_FEATURE_NAME "LiveCodingServer"

enum class ELiveCodingResult
{
	Success,
	Error
};

enum class ELiveCodingLogVerbosity
{
	Info,
	Success,
	Warning,
	Failure,
};

class ILiveCodingServer : public IModularFeature
{
public:
	virtual void Start(const wchar_t* ProcessGroupName) = 0;
	virtual void Stop() = 0;

	virtual void SetLinkerPath(const wchar_t* LinkerPath) = 0;

	DECLARE_DELEGATE(FBringToFrontDelegate);
	virtual FBringToFrontDelegate& GetBringToFrontDelegate() = 0;

	DECLARE_DELEGATE(FClearOutputDelegate);
	virtual FClearOutputDelegate& GetClearOutputDelegate() = 0;

	DECLARE_DELEGATE_OneParam(FStatusChangeDelegate, const wchar_t*);
	virtual FStatusChangeDelegate& GetStatusChangeDelegate() = 0;

	DECLARE_DELEGATE_TwoParams(FLogOutputDelegate, ELiveCodingLogVerbosity, const wchar_t*);
	virtual FLogOutputDelegate& GetLogOutputDelegate() = 0;

	typedef TMap<FString, TArray<FString>> FModuleToObjectFiles;
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FCompileDelegate, const TArray<FString>&, FModuleToObjectFiles&)
	virtual FCompileDelegate& GetCompileDelegate() = 0;

	DECLARE_DELEGATE(FCompileStartedDelegate);
	virtual FCompileStartedDelegate& GetCompileStartedDelegate() = 0;

	DECLARE_DELEGATE_TwoParams(FCompileFinishedDelegate, ELiveCodingResult, const wchar_t*);
	virtual FCompileFinishedDelegate& GetCompileFinishedDelegate() = 0;

	DECLARE_DELEGATE(FShowConsoleDelegate);
	virtual FShowConsoleDelegate& GetShowConsoleDelegate() = 0;

	DECLARE_DELEGATE_OneParam(FSetVisibleDelegate, bool);
	virtual FSetVisibleDelegate& GetSetVisibleDelegate() = 0;
};

