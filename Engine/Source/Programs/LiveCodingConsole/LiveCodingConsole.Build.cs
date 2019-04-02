// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LiveCodingConsole : ModuleRules
{
	public LiveCodingConsole(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Json",
				"Projects",
				"ApplicationCore",
				"Slate",
				"SlateCore",
				"StandaloneRenderer",
				"MessageLog",
				"LiveCodingServer"
			});

		PublicIncludePaths.Add("Runtime/Launch/Public");
		PrivateIncludePaths.Add("Runtime/Launch/Private");      // For LaunchEngineLoop.cpp include
    }
}
