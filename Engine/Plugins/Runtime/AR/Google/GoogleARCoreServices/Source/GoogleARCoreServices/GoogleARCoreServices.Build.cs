// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


using System.IO;


namespace UnrealBuildTool.Rules
{
	public class GoogleARCoreServices : ModuleRules
	{
		public GoogleARCoreServices(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[]
				{
					"GoogleARCoreServices/Private",
				}
			);
			
			PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"HeadMountedDisplay",
						"AugmentedReality",
					}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"GoogleARCoreSDK",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"Settings" // For editor settings panel.
				}
			);


			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				// Register Plugin Language
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "GoogleARCoreServices_APL.xml"));
			}

			bFasterWithoutUnity = false;
		}
	}
}
