// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class VariantManagerContent : ModuleRules
	{
		public VariantManagerContent(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"MovieScene"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
				}
			);
		}
	}
}