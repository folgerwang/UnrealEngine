// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DisplayClusterInput : ModuleRules
	{
		public DisplayClusterInput(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"InputDevice",          // For IInputDevice.h
					"HeadMountedDisplay",   // For IMotionController.h
					"DisplayCluster"        // For IDisplayCluster
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DisplayCluster",
					"Engine",
					"InputCore",
					"InputDevice"
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ApplicationCore",
					"Core",
					"CoreUObject",
					"DisplayCluster",
					"Engine",
					"InputCore",
					"InputDevice",
					"HeadMountedDisplay"
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					// Relative to Engine\Plugins\Runtime\nDisplay\Source\
					"DisplayCluster/Private",
					"DisplayClusterInput/Private/Controller",
					"DisplayClusterInput/Private/State",
					"../../../../Source/Runtime/Renderer/Private",
					"../../../../Source/Runtime/Engine/Classes/Components",
				}
			);
		}
	}
}
