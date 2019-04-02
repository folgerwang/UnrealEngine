// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemUtils : ModuleRules
{
	public OnlineSubsystemUtils(ReadOnlyTargetRules Target) : base(Target)
    {
		PublicDefinitions.Add("ONLINESUBSYSTEMUTILS_PACKAGE=1");
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.Add("OnlineSubsystemUtils/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ImageCore",
				"Sockets",
				"Voice",
				"PacketHandler",
				"Json",
				"AudioMixer"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		PublicDependencyModuleNames.Add("OnlineSubsystem");
	}
}
