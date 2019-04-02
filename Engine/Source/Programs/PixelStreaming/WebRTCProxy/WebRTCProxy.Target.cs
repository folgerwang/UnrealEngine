// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealTargetPlatform.Win64)]
public class WebRTCProxyTarget : TargetRules
{
	public WebRTCProxyTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LaunchModuleName = "WebRTCProxy";

        GlobalDefinitions.Add("WEBRTC_WIN");
		GlobalDefinitions.Add("INCL_EXTRA_HTON_FUNCTIONS");

        ExeBinariesSubFolder = "../../Source/Programs/PixelStreaming/WebRTCProxy/bin";
        // Lean and mean
        bBuildDeveloperTools = false;
		
        // No editor needed
		bBuildWithEditorOnlyData = false;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = false;

		// Console application, not a Windows app (sets entry point to main(), instead of WinMain())
		bIsBuildingConsoleApplication = true;
    }
}
