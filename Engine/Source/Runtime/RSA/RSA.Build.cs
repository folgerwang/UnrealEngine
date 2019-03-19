// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RSA : ModuleRules
{
	public RSA(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");

        switch (Target.Platform)
        {
			case UnrealTargetPlatform.Win64:
			case UnrealTargetPlatform.Win32:
			case UnrealTargetPlatform.Mac:
			case UnrealTargetPlatform.Linux:
				{
					PrivateDependencyModuleNames.Add("OpenSSL");
					PrivateDefinitions.Add("RSA_USE_OPENSSL=1");
					break;
				}
			default:
				{
					PrivateDefinitions.Add("RSA_USE_OPENSSL=0");
					break;
				}
		}
	}
}
