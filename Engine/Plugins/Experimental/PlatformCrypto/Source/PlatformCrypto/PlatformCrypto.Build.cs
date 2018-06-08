// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PlatformCrypto : ModuleRules
	{
		public PlatformCrypto(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
				);

			if (Target.Platform == UnrealTargetPlatform.XboxOne)
			{
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoBCrypt",
					}
					);

				PublicIncludePathModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoBCrypt"
					}
					);
			}
			else if (Target.Platform == UnrealTargetPlatform.Switch)
			{
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoSwitch",
					}
					);

				PublicIncludePathModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoSwitch"
					}
					);
			}
			else
			{
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoOpenSSL",
					}
					);

				PublicIncludePathModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoOpenSSL"
					}
					);
			}
		}
	}
}