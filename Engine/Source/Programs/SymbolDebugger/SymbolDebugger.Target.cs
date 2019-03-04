// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealTargetPlatform.Win64, UnrealTargetPlatform.Mac)]
public class SymbolDebuggerTarget : TargetRules
{
	public SymbolDebuggerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;

		LaunchModuleName = "SymbolDebugger";
        ExtraModuleNames.Add("EditorStyle");

		bBuildDeveloperTools = false;

		// SymbolDebugger doesn't ever compile with the engine linked in
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;

		// Need to disable the bundled version of dbghelp so that CrashDebugHelper can load dbgeng.dll.
		WindowsPlatform.bUseBundledDbgHelp = false;

		// SymbolDebugger.exe has no exports, so no need to verify that a .lib and .exp file was emitted by
		// the linker.
		bHasExports = false;
	}
}
