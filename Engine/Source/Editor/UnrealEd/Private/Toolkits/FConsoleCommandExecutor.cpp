// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Toolkits/FConsoleCommandExecutor.h"
#include "EngineGlobals.h"
#include "Editor.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"

#define LOCTEXT_NAMESPACE "SOutputLog"

FName FConsoleCommandExecutor::StaticName()
{
	static const FName CmdExecName = TEXT("Cmd");
	return CmdExecName;
}

FName FConsoleCommandExecutor::GetName() const
{
	return StaticName();
}

FText FConsoleCommandExecutor::GetDisplayName() const
{
	return LOCTEXT("ConsoleCommandExecutorDisplayName", "Cmd");
}

FText FConsoleCommandExecutor::GetDescription() const
{
	return LOCTEXT("ConsoleCommandExecutorDescription", "Execute Unreal Console Commands");
}

FText FConsoleCommandExecutor::GetHintText() const
{
	return LOCTEXT("ConsoleCommandExecutorHintText", "Enter Console Command");
}

void FConsoleCommandExecutor::GetAutoCompleteSuggestions(const TCHAR* Input, TArray<FString>& Out)
{
	auto OnConsoleVariable = [&Out](const TCHAR *Name, IConsoleObject* CVar)
	{
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (CVar->TestFlags(ECVF_Cheat))
		{
			return;
		}
#endif // (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (CVar->TestFlags(ECVF_Unregistered))
		{
			return;
		}

		Out.Add(Name);
	};

	IConsoleManager::Get().ForEachConsoleObjectThatContains(FConsoleObjectVisitor::CreateLambda(OnConsoleVariable), Input);
}

void FConsoleCommandExecutor::GetExecHistory(TArray<FString>& Out)
{
	IConsoleManager::Get().GetConsoleHistory(TEXT(""), Out);
}

bool FConsoleCommandExecutor::Exec(const TCHAR* Input)
{
	IConsoleManager::Get().AddConsoleHistoryEntry(TEXT(""), Input);

	bool bWasHandled = false;
	UWorld* World = nullptr;
	UWorld* OldWorld = nullptr;

	// The play world needs to handle these commands if it exists
	if (GIsEditor && GEditor->PlayWorld && !GIsPlayInEditorWorld)
	{
		World = GEditor->PlayWorld;
		OldWorld = SetPlayInEditorWorld(GEditor->PlayWorld);
	}

	ULocalPlayer* Player = GEngine->GetDebugLocalPlayer();
	if (Player)
	{
		UWorld* PlayerWorld = Player->GetWorld();
		if (!World)
		{
			World = PlayerWorld;
		}
		bWasHandled = Player->Exec(PlayerWorld, Input, *GLog);
	}

	if (!World)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
	if (World)
	{
		if (!bWasHandled)
		{
			AGameModeBase* const GameMode = World->GetAuthGameMode();
			AGameStateBase* const GameState = World->GetGameState();
			if (GameMode && GameMode->ProcessConsoleExec(Input, *GLog, nullptr))
			{
				bWasHandled = true;
			}
			else if (GameState && GameState->ProcessConsoleExec(Input, *GLog, nullptr))
			{
				bWasHandled = true;
			}
		}

		if (!bWasHandled && !Player)
		{
			if (GIsEditor)
			{
				bWasHandled = GEditor->Exec(World, Input, *GLog);
			}
			else
			{
				bWasHandled = GEngine->Exec(World, Input, *GLog);
			}
		}
	}

	// Restore the old world of there was one
	if (OldWorld)
	{
		RestoreEditorWorld(OldWorld);
	}

	return bWasHandled;
}

bool FConsoleCommandExecutor::AllowHotKeyClose() const
{
	return true;
}

bool FConsoleCommandExecutor::AllowMultiLine() const
{
	return false;
}

#undef LOCTEXT_NAMESPACE