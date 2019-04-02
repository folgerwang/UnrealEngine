// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class VulkanRHI : ModuleRules
{
	public VulkanRHI(ReadOnlyTargetRules Target) : base(Target)
	{
		bOutputPubliclyDistributable = true;

		PrivateIncludePaths.Add("Runtime/VulkanRHI/Private");
		if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateIncludePaths.Add("Runtime/VulkanRHI/Private/Windows");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "AMD_AGS");
		}
		else
		{
			PrivateIncludePaths.Add("Runtime/VulkanRHI/Private/" + Target.Platform);
		}

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", 
				"CoreUObject", 
				"Engine", 
				"RHI", 
				"RenderCore", 
				"UtilityShaders",
				"HeadMountedDisplay",
                "PreLoadScreen"
            }
        );

		if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64
			|| Target.Platform == UnrealTargetPlatform.Android || Target.Platform == UnrealTargetPlatform.Lumin)
		{
            AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
        }
        else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");

				string VulkanSDKPath = Environment.GetEnvironmentVariable("VULKAN_SDK");
				bool bSDKInstalled = !String.IsNullOrEmpty(VulkanSDKPath);
				if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Linux || !bSDKInstalled)
				{
					AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
				}
				else
				{
					PrivateIncludePaths.Add(VulkanSDKPath + "/include");
					PrivateIncludePaths.Add(VulkanSDKPath + "/include/vulkan");
					PublicLibraryPaths.Add(VulkanSDKPath + "/lib");
					PublicAdditionalLibraries.Add("vulkan");
				}
			}
			else
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "VkHeadersExternal");
			}
		}
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
			string VulkanSDKPath = Environment.GetEnvironmentVariable("VULKAN_SDK");

			bool bHaveVulkan = false;
			if (!String.IsNullOrEmpty(VulkanSDKPath))
			{
				bHaveVulkan = true;
				PrivateIncludePaths.Add(VulkanSDKPath + "/Include");
			}

			if (bHaveVulkan)
			{
				if (Target.Configuration != UnrealTargetConfiguration.Shipping)
				{
					PrivateIncludePathModuleNames.AddRange(
						new string[]
						{
							"TaskGraph",
						}
					);
				}
			}
			else
			{
				PrecompileForTargets = PrecompileTargetsType.None;
			}
		}
		else
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}

