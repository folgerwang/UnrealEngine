// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Modules/BuildVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Modules/SimpleParse.h"
#include "Misc/Paths.h"

FBuildVersion::FBuildVersion()
	: MajorVersion(0)
	, MinorVersion(0)
	, PatchVersion(0)
	, Changelist(0)
	, CompatibleChangelist(0)
	, IsLicenseeVersion(0)
	, IsPromotedBuild(0)
{
}

FString FBuildVersion::GetDefaultFileName()
{
	return FPaths::EngineDir() / TEXT("Build/Build.version");
}

FString FBuildVersion::GetFileNameForCurrentExecutable()
{
	FString AppExecutableName = FPlatformProcess::ExecutableName();
#if PLATFORM_WINDOWS
	if(AppExecutableName.EndsWith(TEXT("-Cmd")))
	{
		AppExecutableName = AppExecutableName.Left(AppExecutableName.Len() - 4);
	}
#endif
	FString Result = FPlatformProcess::GetModulesDirectory() / AppExecutableName + TEXT(".version");
	return Result;
}

bool FBuildVersion::TryRead(const FString& FileName, FBuildVersion& OutVersion)
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

	bool bParsedMajorVersion = false;
	bool bParsedMinorVersion = false;
	bool bParsedPatchVersion = false;
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

		if (Field == TEXT("MajorVersion"))
		{
			if (!FSimpleParse::ParseUnsignedNumber(Ptr, OutVersion.MajorVersion))
			{
				return false;
			}

			bParsedMajorVersion = true;
		}
		else if (Field == TEXT("MinorVersion"))
		{
			if (!FSimpleParse::ParseUnsignedNumber(Ptr, OutVersion.MinorVersion))
			{
				return false;
			}

			bParsedMinorVersion = true;
		}
		else if (Field == TEXT("PatchVersion"))
		{
			if (!FSimpleParse::ParseUnsignedNumber(Ptr, OutVersion.PatchVersion))
			{
				return false;
			}

			bParsedPatchVersion = true;
		}
		else if (Field == TEXT("Changelist"))
		{
			if (!FSimpleParse::ParseUnsignedNumber(Ptr, OutVersion.Changelist))
			{
				return false;
			}
		}
		else if (Field == TEXT("CompatibleChangelist"))
		{
			if (!FSimpleParse::ParseUnsignedNumber(Ptr, OutVersion.CompatibleChangelist))
			{
				return false;
			}
		}
		else if (Field == TEXT("IsLicenseeVersion"))
		{
			if (!FSimpleParse::ParseUnsignedNumber(Ptr, OutVersion.IsLicenseeVersion))
			{
				return false;
			}
		}
		else if (Field == TEXT("IsPromotedBuild"))
		{
			if (!FSimpleParse::ParseUnsignedNumber(Ptr, OutVersion.IsPromotedBuild))
			{
				return false;
			}
		}
		else if (Field == TEXT("BranchName"))
		{
			if (!FSimpleParse::ParseString(Ptr, OutVersion.BranchName))
			{
				return false;
			}
		}
		else if (Field == TEXT("BuildId"))
		{
			if (!FSimpleParse::ParseString(Ptr, OutVersion.BuildId))
			{
				return false;
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
			// Only succeed if we did actually parse these things
			return bParsedMajorVersion && bParsedMinorVersion && bParsedPatchVersion;
		}

		if (!FSimpleParse::MatchChar(Ptr, TEXT(',')) || !FSimpleParse::MatchZeroOrMoreWhitespace(Ptr))
		{
			return false;
		}
	}
}
