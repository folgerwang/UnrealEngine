// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GoogleARCoreRendering : ModuleRules
{
	public GoogleARCoreRendering(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"GoogleARCoreRendering/Private",
				"GoogleARCoreBase/Private",
				"../../../../../../Source/Runtime/Renderer/Private",
				// ... add other private include paths required here ...
			}
			);
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Json",
				"CoreUObject",
				"Engine",
				"RHI",
				"Engine",
				"Renderer",
				"RenderCore",
				// ... add private dependencies that you statically link with here ...
			}
			);
		bFasterWithoutUnity = true;
	}
}