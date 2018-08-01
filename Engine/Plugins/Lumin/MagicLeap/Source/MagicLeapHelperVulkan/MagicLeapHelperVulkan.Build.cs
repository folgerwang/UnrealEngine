// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MagicLeapHelperVulkan : ModuleRules
	{
		public MagicLeapHelperVulkan(ReadOnlyTargetRules Target) : base(Target)
		{
			// Include headers to be public to other modules.
			PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"Engine",
					"RHI",
					"RenderCore",
					"HeadMountedDisplay"
				});

			// TODO: Explore linking Unreal modules against a commong header and
			// having a runtime dll linking against the library according to the platform.
			if (Target.Platform != UnrealTargetPlatform.Mac)
			{
				PrivateDependencyModuleNames.Add("VulkanRHI");
				string EngineSourceDirectory = "../../../../Source";

				if (Target.Platform == UnrealTargetPlatform.Linux)
				{
					PrivateIncludePaths.AddRange(
						new string[] {
							Path.Combine(EngineSourceDirectory, "ThirdParty/SDL2/SDL-gui-backend/include"),
						}
					);
				}

				PrivateIncludePaths.AddRange(
					new string[] {
						"MagicLeapHelperVulkan/Private",
						Path.Combine(EngineSourceDirectory, "Runtime/VulkanRHI/Private"),
						Path.Combine(EngineSourceDirectory, "Runtime/VulkanRHI/Private", ((Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64) ? "Windows" : Target.Platform.ToString()))
					});

				AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");

				// HACK This is to get access to vulkan headers on Lumin.
				// The way Lumininterprets dependency headers is broken.
				if (Target.Platform == UnrealTargetPlatform.Lumin)
				{
					PrivateIncludePaths.AddRange(
						new string[] {
							Path.Combine(System.Environment.GetEnvironmentVariable("MLSDK"), "lumin/usr/include/vulkan"),
						}
					);
				}
			}

		}
	}
}
