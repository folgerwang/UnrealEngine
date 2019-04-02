// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealTargetPlatform.Win64, UnrealTargetPlatform.Mac)]
public class MinidumpDiagnosticsTarget : TargetRules
{
	public MinidumpDiagnosticsTarget( TargetInfo Target ) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		LaunchModuleName = "MinidumpDiagnostics";

		bBuildDeveloperTools = false;
		bCompileICU = false;

		// MinidumpDiagnostics doesn't ever compile with the engine linked in
		bCompileAgainstEngine = false;

		//bCompileICU = false;

		// MinidumpDiagnostics.exe has no exports, so no need to verify that a .lib and .exp file was emitted by the linker.
		bHasExports = false;

		// Do NOT produce additional console app exe
		bIsBuildingConsoleApplication = true;

		// Need to disable the bundled version of dbghelp so that CrashDebugHelper can load dbgeng.dll.
		WindowsPlatform.bUseBundledDbgHelp = false;

		GlobalDefinitions.Add("MINIDUMPDIAGNOSTICS=1");
        GlobalDefinitions.Add("NOINITCRASHREPORTER=1");
    }
}
