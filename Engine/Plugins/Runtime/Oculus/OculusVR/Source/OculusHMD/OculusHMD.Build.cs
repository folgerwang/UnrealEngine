// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class OculusHMD : ModuleRules
	{
		public OculusHMD(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					// Relative to Engine\Plugins\Runtime\Oculus\OculusVR\Source
					"../../../../../Source/Runtime/Renderer/Private",
					"../../../../../Source/Runtime/OpenGLDrv/Private",
					"../../../../../Source/Runtime/VulkanRHI/Private",
					"../../../../../Source/Runtime/Engine/Classes/Components",
				});

			PublicIncludePathModuleNames.AddRange(
				new string[] {
					"Launch",
					"ProceduralMeshComponent",
				});			

			if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
			{
				PrivateIncludePaths.Add("../../../../../Source/Runtime/VulkanRHI/Private/Windows");
			}
			else
			{
				PrivateIncludePaths.Add("../../../../../Source/Runtime/VulkanRHI/Private/" + Target.Platform);
			}


			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"RHI",
					"RenderCore",
					"Renderer",
					"ShaderCore",
					"HeadMountedDisplay",
					"Slate",
					"SlateCore",
					"ImageWrapper",
					"MediaAssets",
					"Analytics",
					"UtilityShaders",
					"OpenGLDrv",
					"VulkanRHI",
					"OVRPlugin",
					"ProceduralMeshComponent",
                    "Projects",
                });

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}

			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");

			if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
			{
				// D3D
				{
					PrivateDependencyModuleNames.AddRange(
						new string[]
						{
							"D3D11RHI",
							"D3D12RHI",
						});

					PrivateIncludePaths.AddRange(
						new string[]
						{
							"../../../../../Source/Runtime/Windows/D3D11RHI/Private",
							"../../../../../Source/Runtime/Windows/D3D11RHI/Private/Windows",
							"../../../../../Source/Runtime/D3D12RHI/Private",
							"../../../../../Source/Runtime/D3D12RHI/Private/Windows",
						});

					AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
					AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
					AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
					AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11Audio");
					AddEngineThirdPartyPrivateStaticDependencies(Target, "DirectSound");
					AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
                    AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
                }

				// Vulkan
				{
                    AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
				}

				// OVRPlugin
				{
					PublicDelayLoadDLLs.Add("OVRPlugin.dll");
					RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/Oculus/OVRPlugin/OVRPlugin/" + Target.Platform.ToString() + "/OVRPlugin.dll");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Android)
			{
				// Vulkan
				{
                    AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
                }

                // AndroidPlugin
                {
					string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
					AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "GearVR_APL.xml"));
				}
			}
		}
	}
}
