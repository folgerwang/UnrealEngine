// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManifest.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Modules/SimpleParse.h"

FModuleManifest::FModuleManifest()
{
}

FString FModuleManifest::GetFileName(const FString& DirectoryName, bool bIsGameFolder)
{
#if UE_BUILD_DEVELOPMENT
	return DirectoryName / ((FApp::GetBuildConfiguration() == EBuildConfigurations::DebugGame && bIsGameFolder)? TEXT(UBT_MODULE_MANIFEST_DEBUGGAME) : TEXT(UBT_MODULE_MANIFEST));
#else
	return DirectoryName / TEXT(UBT_MODULE_MANIFEST);
#endif
}

bool FModuleManifest::TryRead(const FString& FileName, FModuleManifest& OutManifest)
{
	// Read the file to a string
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FileName))
	{
		return false;
	}

	const TCHAR* Ptr = *Text;
	if (!FSimpleParse::MatchZeroOrMoreWhitespace(Ptr) || !FSimpleParse::MatchChar(Ptr, TEXT('{')) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr) || FSimpleParse::MatchChar(Ptr, TEXT('}')))
	{
		return false;
	}

	for (;;)
	{
		FString Field;
		if (!FSimpleParse::ParseString(Ptr, Field))
		{
			return false;
		}

		if (!FSimpleParse::MatchZeroOrMoreWhitespace(Ptr) || !FSimpleParse::MatchChar(Ptr, TEXT(':')) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr))
		{
			return false;
		}

		if (Field == TEXT("BuildId"))
		{
			if (!FSimpleParse::ParseString(Ptr, OutManifest.BuildId))
			{
				return false;
			}
		}
		else if (Field == TEXT("Modules"))
		{
			if (!FSimpleParse::MatchZeroOrMoreWhitespace(Ptr) || !FSimpleParse::MatchChar(Ptr, TEXT('{')) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr))
			{
				return false;
			}

			if (!FSimpleParse::MatchChar(Ptr, TEXT('}')))
			{
				for (;;)
				{
					FString ModuleName;
					FString ModulePath;
					if (!FSimpleParse::ParseString(Ptr, ModuleName) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr) || !FSimpleParse::MatchChar(Ptr, TEXT(':')) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr) || !FSimpleParse::ParseString(Ptr, ModulePath) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr))
					{
						return false;
					}

					OutManifest.ModuleNameToFileName.FindOrAdd(MoveTemp(ModuleName)) = MoveTemp(ModulePath);

					if (FSimpleParse::MatchChar(Ptr, TEXT('}')))
					{
						break;
					}

					if (!FSimpleParse::MatchChar(Ptr, TEXT(',')) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr))
					{
						return false;
					}
				}
			}
		}
		else
		{
			return false;
		}

		if (!FSimpleParse::MatchZeroOrMoreWhitespace(Ptr))
		{
			return false;
		}

		if (FSimpleParse::MatchChar(Ptr, TEXT('}')))
		{
			return true;
		}

		if (!FSimpleParse::MatchChar(Ptr, TEXT(',')) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr))
		{
			return false;
		}
	}
}
