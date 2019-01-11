// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ConcertSyncCore : ModuleRules
	{
		public ConcertSyncCore(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                }
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Concert",
					"ConcertTransport",
				}
			);

			if (Target.bCompileAgainstEngine)
			{
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"Engine",
					}
				);
			}
		}
    }
}
