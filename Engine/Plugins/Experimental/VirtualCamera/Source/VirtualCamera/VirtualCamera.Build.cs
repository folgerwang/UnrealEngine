// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VirtualCamera : ModuleRules
{
	public VirtualCamera(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AugmentedReality",
				"CinematicCamera",
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"LevelSequence",
				"LiveLinkInterface",
				"MovieScene",
				"RemoteSession",
				"TimeManagement",
				"VPUtilities",
			}
		);

		if (Target.bBuildDeveloperTools)
		{
			PrivateDefinitions.Add("VIRTUALCAMERA_WITH_CONCERT=1");
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Concert",
				}
			);
		}
		else
		{
			PrivateDefinitions.Add("VIRTUALCAMERA_WITH_CONCERT=0");
		}


		if (Target.bBuildEditor == true)
		{
			PublicDependencyModuleNames.Add("LevelSequenceEditor");
			PublicDependencyModuleNames.Add("Sequencer");
			PublicDependencyModuleNames.Add("SlateCore");
			PublicDependencyModuleNames.Add("TakeRecorder");
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
