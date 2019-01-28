// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MagicLeap : ModuleRules
	{
		public MagicLeap(ReadOnlyTargetRules Target)
			: base(Target)
		{
			// @todo implement in a better way.
			string EngineSourceDirectory = Path.Combine(ModuleDirectory, "../../../../../Source"); //Path.Combine(BuildConfiguration.RelativeEnginePath, "Source");

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"MagicLeap/Private",
					Path.Combine(EngineSourceDirectory, "Runtime/Renderer/Private")
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"HeadMountedDisplay",
					"ProceduralMeshComponent",
					"InputDevice",
                    "LuminRuntimeSettings",
					"AugmentedReality",
                }
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"EngineSettings",
					"InputCore",
					"RHI",
					"RenderCore",
					"Renderer",
					"Slate",
					"SlateCore",
					"MLSDK",
					"MRMesh",
                    "MagicLeapHelperOpenGL",
					"UtilityShaders",
					// Public headers of MagicLeapHelperVulkan are protected against Mac so this is fine here.
					"MagicLeapHelperVulkan",
					"LuminRuntimeSettings",
					"MagicLeapSecureStorage",
				}
			);

			if (Target.Platform != UnrealTargetPlatform.Mac)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"VulkanRHI"
					}
				);
			}

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}

			// Add direct rendering dependencies on a per-platform basis
			if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "D3D11RHI" });
				PrivateIncludePaths.AddRange(
					new string[] {
						Path.Combine(EngineSourceDirectory, "Runtime/Windows/D3D11RHI/Private"),
						Path.Combine(EngineSourceDirectory, "Runtime/Windows/D3D11RHI/Private/Windows"),
					}
				);
				// TODO: refactor MagicLeap.Build.cs!!! Too much duplicate code.
				PrivateIncludePaths.AddRange(
					new string[] {
						Path.Combine(EngineSourceDirectory, "Runtime/VulkanRHI/Private"),
						Path.Combine(EngineSourceDirectory, "Runtime/VulkanRHI/Private/Windows")
					}
				);
				AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				PrivateIncludePaths.AddRange(
					new string[] {
						Path.Combine(EngineSourceDirectory, "ThirdParty/SDL2/SDL-gui-backend/include"),
					}
				);
				// HACK This is a workaround for a bug introduced in Unreal 4.13 which should be
				// removed if all SensoryWare libs are compiled with libc++ instead of libstdc++
				// https://udn.unrealengine.com/questions/305445/ue-33584-linux-build.html
				PublicAdditionalLibraries.Add("stdc++");
			}
			else if (Target.Platform == UnrealTargetPlatform.Lumin)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "VulkanRHI" });
				PrivateIncludePaths.AddRange(
					new string[] {
						Path.Combine(EngineSourceDirectory, "Runtime/VulkanRHI/Private"),
						Path.Combine(EngineSourceDirectory, "Runtime/VulkanRHI/Private", ((Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64) ? "Windows" : Target.Platform.ToString()))
						// ... add other private include paths required here ...
					}
				);
				AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
			}

			if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Lumin)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "OpenGLDrv" });
				PrivateIncludePaths.AddRange(
					new string[] {
						Path.Combine(EngineSourceDirectory, "Runtime/OpenGLDrv/Private"),
						// ... add other private include paths required here ...
					}
				);
				AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
			}
		}
	}
}
