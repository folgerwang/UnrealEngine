// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

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
                    "LuminRuntimeSettings"
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
					"ShaderCore",
					"Slate",
					"SlateCore",
					"MLSDK",
					"MRMesh",
                    "MagicLeapHelperOpenGL",
					"UtilityShaders",
					// Public headers of MagicLeapHelperVulkan are protected against Mac so this is fine here.
					"MagicLeapHelperVulkan",
					"LuminRuntimeSettings"
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
