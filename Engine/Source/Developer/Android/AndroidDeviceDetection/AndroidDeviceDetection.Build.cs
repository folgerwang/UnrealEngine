// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AndroidDeviceDetection : ModuleRules
{
	public AndroidDeviceDetection( ReadOnlyTargetRules Target ) : base(Target)
	{
		BinariesSubFolder = "Android";

        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Json",
                "JsonUtilities",
                "PIEPreviewDeviceSpecification"
            }
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"TcpMessaging",
			}
		);

		PublicIncludePaths.AddRange(
			new string[]
			{
				"Runtime/Core/Public/Android"
			}
		);

        if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                "TcpMessaging"
            }
        );
    }
}
