// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WindowsTargetPlatform : ModuleRules
{
	public WindowsTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"TargetPlatform",
				"DesktopPlatform",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);

		// compile with Engine
		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.AddRange( new string[] {
				"Engine", "RHI"
				}
			);
            PrivateIncludePathModuleNames.Add("TextureCompressor");
        }
    }
}
