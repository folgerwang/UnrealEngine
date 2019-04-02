// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


/**
 * Auxiliary class. Responsible for terminating application.
 */
class FDisplayClusterAppExit
{
public:
	enum class ExitType
	{
		// Kills current process. No resource cleaning performed.
		KillImmediately,
		// UE4 based soft exit (game thread). Full resource cleaning.
		NormalSoft,
		// UE4 game termination. Error window and dump file should appear after exit.
		NormalForce
	};

public:
	static void ExitApplication(ExitType exitType, const FString& strMsg);

private:
	static auto ExitTypeToStr(ExitType type);

private:
	static FCriticalSection InternalsSyncScope;
};
