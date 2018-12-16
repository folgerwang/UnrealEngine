// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AESHandlerComponent : ModuleRules
{
    public AESHandlerComponent(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"PacketHandler",
				"PlatformCrypto",
			}
			);

		PublicIncludePathModuleNames.AddRange(
			new string[]
			{
				"PlatformCrypto"
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
		}
		else if (Target.Platform == UnrealTargetPlatform.Switch)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"PlatformCryptoSwitch",
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
		}
	}
}