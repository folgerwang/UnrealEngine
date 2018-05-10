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
						Path.Combine(EngineSourceDirectory, "Runtime/VulkanRHI/Private")
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
