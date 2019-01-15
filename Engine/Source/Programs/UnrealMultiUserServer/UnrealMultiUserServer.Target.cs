// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealMultiUserServerTarget : TargetRules
{
	public UnrealMultiUserServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Modular;
		LaunchModuleName = "UnrealMultiUserServer";
        AdditionalPlugins.Add("UdpMessaging");
        AdditionalPlugins.Add("ConcertMain");
        AdditionalPlugins.Add("ConcertSyncServer");

        // Lean and mean also override the build developer switch, so just set all the switch it sets manually since we want developer tools (i.e. concert plugins)
        //bCompileLeanAndMeanUE = true;
        bCompileSpeedTree = false;

        // Editor-only data, however, is needed
        bBuildWithEditorOnlyData = true;

        // Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
        bCompileAgainstEngine = false;

		// enable build tools
        bBuildDeveloperTools = true;

        bCompileAgainstCoreUObject = true;
        bCompileWithPluginSupport = true;

        // UnrealMultiUserServer is a console application, not a Windows app (sets entry point to main(), instead of WinMain())
        bIsBuildingConsoleApplication = true;
	}
}
