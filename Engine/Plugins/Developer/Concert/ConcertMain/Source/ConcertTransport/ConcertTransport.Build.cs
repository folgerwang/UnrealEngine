// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ConcertTransport : ModuleRules
	{
		public ConcertTransport(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                }
			);

            PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"MessagingCommon",
				}
			);

        }
    }
}
