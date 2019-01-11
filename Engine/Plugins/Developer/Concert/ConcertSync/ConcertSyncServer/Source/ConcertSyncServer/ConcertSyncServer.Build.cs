// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ConcertSyncServer : ModuleRules
	{
		public ConcertSyncServer(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "ConcertSyncCore",
                }
			);

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    // TODO: Review
                    "Concert",
                    "ConcertTransport",
                    //"ConcertSyncCore",
                }
            );

            PrivateIncludePathModuleNames.AddRange(
                new string[] {
                    "Concert",
                }
            );
        }
    }
}
