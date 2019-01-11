// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMultiUserServer : ModuleRules
{
	public UnrealMultiUserServer(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Launch/Private",           // for LaunchEngineLoop.cpp include
			}
		);
		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "Core",
				"ApplicationCore",
                "Projects",
                "CoreUObject",
                "Messaging",
                "MessagingCommon",
           }
        );
        
        // Networking Dependency
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Sockets",
                "Networking",
                "UdpMessaging",
                "Concert",
                "ConcertTransport",
                "ConcertSyncServer",
                "SessionServices",
            }
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "Messaging",
            }
        );
    }
}
