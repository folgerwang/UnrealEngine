// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class VivoxVoiceChat : ModuleRules
	{
		public VivoxVoiceChat(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"ApplicationCore"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"VivoxCoreSDK",
					"VivoxClientAPI"
				}
			);

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"ApplicationCore",
						"Launch"
					}
				);
			}
			else if (Target.Platform == UnrealTargetPlatform.XboxOne)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"ApplicationCore"
					}
				);
			}
		}
	}
}
